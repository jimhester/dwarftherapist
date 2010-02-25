/*
Dwarf Therapist
Copyright (c) 2009 Trey Stout (chmod)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <QtGui>
#include <QtDebug>
#include "defines.h"
#include "dfinstance.h"
#include "dwarf.h"
#include "utils.h"
#include "gamedatareader.h"
#include "memorylayout.h"
#include "dwarftherapist.h"
#include "memorysegment.h"
#include "dfhack/DFProcess.h"


DFInstance::DFInstance(QObject* parent)
	:QObject(parent)
	,m_is_ok(true)
    ,m_numCreatures(0)
    ,m_DF ("etc/Memory.xml")
	,m_heartbeat_timer(new QTimer(this))
{
    m_codec = new CP437Codec;
    if(!m_DF.Attach())
    {
    /*    QMessageBox::warning(0, tr("Warning"),
			tr("Unable to locate a running copy of Dwarf "
			"Fortress, are you sure it's running?"));*/
        m_is_ok = false;
    }
    else{
        m_is_ok = true;
		// test if connected with shm
		DFHack::SHMProcess* test = dynamic_cast<DFHack::SHMProcess*>(m_DF.getProcess());
		if(test != NULL){
			m_has_shm = true;
		}
		else{
			QMessageBox::warning(0, tr("Warning"), tr("SDL.dll not installed properly\ncustom name and profession writing disabled\nTo enable please rename the original SDL.dll to SDLreal.dll and copy the DFhack SDL.dll into your Dwarf Fortress directory"));
			m_has_shm = false;
		}
    m_mem = m_DF.getMemoryInfo();
    m_DF.InitViewAndCursor();
    m_DF.InitViewSize();
    m_DF.InitMap(); // for getSize();
    
//    m_numBuildings = m_DF.InitReadBuildings(m_buildingtypes);
    m_DF.InitReadCreatures(m_numCreatures);
    m_DF.ReadCreatureMatgloss(m_creaturestypes);
    m_DF.ReadWoodMatgloss(m_woodstypes);
    m_DF.ReadPlantMatgloss(m_plantstypes);
    m_DF.ReadStoneMatgloss(m_stonestypes);
    m_DF.ReadMetalMatgloss(m_metalstypes);
    m_DF.ReadItemTypes(m_itemstypes);
    m_DF.InitReadNameTables(m_names);
    }
	connect(m_heartbeat_timer, SIGNAL(timeout()), SLOT(heartbeat()));
	// let subclasses start the timer, since we don't want to be checking before we're connected
}
QString DFInstance::convertString(const char * str)
{
    return m_codec->toUnicode(str,strlen(str));
}
QString DFInstance::convertString(const QString & str)
{
    return m_codec->toUnicode(str.toAscii().data(),str.size());
}
bool DFInstance::find_running_copy() {
    if (DT->user_settings()->value("options/alert_on_lost_connection", true).toBool()) {
	    m_heartbeat_timer->start(1000); // check every second for disconnection
    }
	return m_is_ok;
}

QVector<Dwarf*> DFInstance::load_dwarves() {
	QVector<Dwarf*> dwarves;
	if (!m_is_ok) {
		LOGW << "not connected";
		return dwarves;
	}
 	if (m_numCreatures > 0) {
		for (uint offset=0; offset < m_numCreatures; ++offset) {
			Dwarf *d = Dwarf::get_dwarf(this, offset);
			if (d) {
				dwarves.append(d);
				TRACE << "FOUND DWARF" << offset << d->nice_name();
			} else {
				TRACE << "FOUND OTHER CREATURE" << offset;
			}
		}
        m_DF.ForceResume();
    } else {
        // we lost the fort!
        m_is_ok = false;
    }

    /*TEST RELATIONSHIPS
	int relations_found = 0;
    foreach(Dwarf *d, dwarves) {
        // get all bytes for this dwarf...
        qDebug() << "Scanning for ID references in" << d->nice_name() << "(" << hex << d->id() << ")";
        QByteArray data = get_data(d->address(), 0x7e8);
        foreach(Dwarf *other_d, dwarves) {
            if (other_d == d)
                continue; // let's hope there are no pointers to ourselves...
            int offset = 0;
            while (offset < data.size()) {
                int idx = data.indexOf(encode(other_d->id()), offset);
                if (idx != -1) {
                    qDebug() << "\t\tOFFSET:" << hex << idx << "Found ID of" << other_d->nice_name() << "(" << other_d->id() << ")";
					relations_found++;
                    offset = idx + 1;
                } else {
                    offset = data.size();
                }
            }
        }
    }
	LOGD << "relations found" << relations_found;
	*/

	LOGI << "found" << dwarves.size() << "dwarves out of" << m_numCreatures << "creatures"; //creatures.size() << "creatures";
	return dwarves;
}

void DFInstance::heartbeat() {
	// simple read attempt that will fail if the DF game isn't running a fort, or isn't running at all
  m_DF.Suspend();
    uint32_t creaturesSize;
    m_DF.InitReadCreatures(creaturesSize);
    m_DF.Resume();
    //QVector<uint> creatures = enumerate_vector(m_layout->address("creature_vector") + m_memory_correction);
	if (creaturesSize < 1) {
		// no game loaded, or process is gone
		emit connection_interrupted();
	}
}

QString DFInstance::translateName(const t_lastname &name , string trans)
{
    QString qname(m_DF.TranslateName(name, m_names,trans).c_str());
    qname = qname.toLower();
    qname[0]=qname[0].toUpper();
    return(qname);
}
QString DFInstance::translateName(const t_squadname& name, string trans)
{
    QString qname(m_DF.TranslateName(name, m_names,trans).c_str());
    qname = qname.toLower();
    qname[0]=qname[0].toUpper();
    return(qname);
}
QString DFInstance::getCreatureType(uint type)
 {
   if(m_creaturestypes.size() > type)
     {
       return QString(m_creaturestypes[type].name);
     }
     return QString("");
}
QString DFInstance::getBuildingType(uint type)
{
  if(m_buildingtypes.size() > type)
    {
        return QString(m_buildingtypes[type].c_str());
    }
    return QString("");
}
QString DFInstance::getMetalType(uint type)
{
  if(m_metalstypes.size() > type)
    {
        return QString(m_metalstypes[type].name);
    }
    return QString("");
}
QString DFInstance::getStoneType(uint type)
{
  if(m_stonestypes.size() > type)
    {
        return QString(m_stonestypes[type].name);
    }
    return QString("");
}
QString DFInstance::getWoodType(uint type)
{
  if(m_woodstypes.size() > type)
    {
        return QString(m_woodstypes[type].name);
    }
    return QString("");
}
QString DFInstance::getPlantType(uint type)
{
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].name);
    }
    return QString("");
}

QString DFInstance::getPlantDrinkType(uint type)
{
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].drink_name);
    }
    return QString("");
}
QString DFInstance::getPlantFoodType(uint type)
{
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].food_name);
    }
    return QString("");
}
QString DFInstance::getPlantExtractType(uint type)
{
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].extract_name);
    }
    return QString("");
}
QString DFInstance::getItemType(uint type,uint index)
{
  if(m_itemstypes.size() > type)
    {
        if(m_itemstypes[type].size() > index)
        {
            return QString(m_itemstypes[type][index].name);
        }
    }
    return QString("");
}
DFInstance::~DFInstance(){
    if(m_is_ok){
		m_DF.FinishReadItems();
        m_DF.FinishReadCreatures();
        m_DF.FinishReadNameTables();
 //       m_DF.Detach();
    }
}
