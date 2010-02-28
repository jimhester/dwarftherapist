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
    ,m_num_creatures(0)
    ,m_DF ("etc/Memory.xml")
	,m_heartbeat_timer(new QTimer(this))
{
    m_codec = new CP437Codec;
    if(!m_DF.Attach())
    {
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
    
    m_DF.InitReadCreatures(m_num_creatures);
    m_DF.InitReadSettlements(m_num_settlements);
    m_DF.ReadCreatureMatgloss(m_creaturestypes);
    m_DF.ReadWoodMatgloss(m_woodstypes);
    m_DF.ReadPlantMatgloss(m_plantstypes);
    m_DF.ReadStoneMatgloss(m_stonestypes);
    m_DF.ReadMetalMatgloss(m_metalstypes);
    m_DF.ReadItemTypes(m_itemstypes);
    m_DF.InitReadNameTables(m_names);

	heartbeat(); // check if a fort is loaded
	if(m_is_ok){
		DFHack::t_settlement current;
		m_DF.ReadCurrentSettlement(current);
		m_generic_fort_name = QString(m_DF.TranslateName(current.name,2,m_names).c_str());
		m_generic_fort_name = m_generic_fort_name.toLower();
		m_generic_fort_name[0] = m_generic_fort_name[0].toTitleCase();
		m_dwarf_fort_name = QString(m_DF.TranslateName(current.name,2,m_names,"DWARF").c_str());
		m_dwarf_fort_name = m_dwarf_fort_name.toLower();
		m_dwarf_fort_name[0] = m_dwarf_fort_name[0].toTitleCase();
	}
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
 	if (m_num_creatures > 0) {
		for (uint offset=0; offset < m_num_creatures; ++offset) {
			Dwarf *d = Dwarf::get_dwarf(this, offset);
			if (d) {
				dwarves.append(d);
				TRACE << "FOUND DWARF" << offset << d->nice_name();
			} else {
				TRACE << "FOUND OTHER CREATURE" << offset;
			}
		}
        m_DF.Resume();
    } else {
        // we lost the fort!
        m_is_ok = false;
    }
	LOGI << "found" << dwarves.size() << "dwarves out of" << m_num_creatures << "creatures";
	return dwarves;
}

void DFInstance::heartbeat() {
	// simple read attempt that will fail if the DF game isn't running a fort, or isn't running at all
    m_DF.Suspend();
    m_DF.FinishReadCreatures(); // free old vector
    m_creatures_inited = m_DF.InitReadCreatures(m_num_creatures); // get new vector
    m_DF.Resume();
	if (m_num_creatures < 1) {
		// no game loaded, or process is gone
		emit connection_interrupted();
		m_is_ok = false;
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
		if(m_creatures_inited){
			m_DF.FinishReadCreatures();
		}
        m_DF.FinishReadNameTables();
        m_DF.FinishReadSettlements();
        m_DF.Detach();
    }
}
