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
#include "dfhack/DFError.h"

DFInstance::DFInstance(QObject* parent)
	:QObject(parent)
	,m_is_ok(true)
    ,m_num_creatures(0)
    ,m_DF ("etc/Memory.xml")
	,m_heartbeat_timer(new QTimer(this))
{
	try {
		m_is_ok = m_DF.Attach();
	} 
	catch(DFHack::Error::NoProcess &e) 
	{
		QMessageBox::warning(0,tr("Error"),tr("Unable to locate a running copy of Dwarf Fortress"));
		m_is_ok = false;
		return;
	} /*catch(DFHack::Error::CantAttach &e) {
		QMessageBox::warning(0,tr("Error"),tr("Unable to attach to Dwarf Fortress"));
		m_is_ok = false;
		return;
	}*/

    m_codec = new CP437Codec;
    try{
    m_DF.Attach();
    }
    catch(...){
        m_is_ok = false;
        return;
    }
    m_is_ok = true;
    // test if connected with shm
    DFHack::SHMProcess* test = dynamic_cast<DFHack::SHMProcess*>(m_DF.getProcess());
    if(test != NULL){
	    m_has_shm = true;
    }
    else{
	    QMessageBox::warning(0, tr("Warning"), tr("SHM not installed properly\ncustom name and profession writing disabled\nTo enable please see INSTALL.txt"));
	    m_has_shm = false;
    }
    m_mem = m_DF.getMemoryInfo();
    DFHack::t_settlement current;
    try{
        m_DF.InitViewAndCursor(); 
        m_DF.InitViewSize(); 
        m_DF.InitMap();  // for getSize();
        
        m_DF.InitReadCreatures(m_num_creatures); 
        m_DF.InitReadSettlements(m_num_settlements); 
        m_DF.ReadCreatureMatgloss(m_creaturestypes); 
        m_DF.ReadWoodMatgloss(m_woodstypes); 
        m_DF.ReadPlantMatgloss(m_plantstypes); 
        m_DF.ReadStoneMatgloss(m_stonestypes); 
        m_DF.ReadMetalMatgloss(m_metalstypes); 
        m_DF.ReadItemTypes(m_itemstypes); 
        m_DF.InitReadNameTables(m_english,m_foreign);
        heartbeat(); // check if a fort is loaded
        m_DF.Suspend();
	    m_DF.ReadCurrentSettlement(current);
        m_DF.Resume();
        }
    catch(...){
        m_is_ok = false;
        m_DF.Resume();
        m_DF.Detach();
        return;
    }

        m_generic_fort_name = QString(m_DF.TranslateName(current.name, m_english, m_foreign, true).c_str());
		m_dwarf_fort_name = QString(m_DF.TranslateName(current.name, m_english, m_foreign, false).c_str());
        connect(m_heartbeat_timer, SIGNAL(timeout()), SLOT(heartbeat()));
}
QString DFInstance::convert_string(const char * str){
    return m_codec->toUnicode(str,strlen(str));
}
QString DFInstance::convert_string(const QString & str){
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
        m_DF.Suspend();
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
        m_DF.Detach();
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

QString DFInstance::translate_name(const t_name &name , bool in_english)
{
    QString qname(m_DF.TranslateName(name, m_english, m_foreign,in_english).c_str());
    return(qname);
}

QString DFInstance::get_creature_type(uint type)
 {
   if(m_creaturestypes.size() > type)
     {
       return QString(m_creaturestypes[type].name);
     }
     return QString("");
}
QString DFInstance::get_building_type(uint type){
  if(m_buildingtypes.size() > type)
    {
        return QString(m_buildingtypes[type].c_str());
    }
    return QString("");
}
QString DFInstance::get_metal_type(uint type){
  if(m_metalstypes.size() > type)
    {
        return QString(m_metalstypes[type].name);
    }
    return QString("");
}
QString DFInstance::get_stone_type(uint type){
  if(m_stonestypes.size() > type)
    {
        return QString(m_stonestypes[type].name);
    }
    return QString("");
}
QString DFInstance::get_wood_type(uint type){
  if(m_woodstypes.size() > type)
    {
        return QString(m_woodstypes[type].name);
    }
    return QString("");
}
QString DFInstance::get_plant_type(uint type){
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].name);
    }
    return QString("");
}

QString DFInstance::get_plant_drink_type(uint type){
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].drink_name);
    }
    return QString("");
}
QString DFInstance::get_plant_food_type(uint type){
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].food_name);
    }
    return QString("");
}
QString DFInstance::get_plant_extract_type(uint type){
  if(m_plantstypes.size() > type)
    {
        return QString(m_plantstypes[type].extract_name);
    }
    return QString("");
}
QString DFInstance::get_item_type(uint type,uint index){
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
        try{
//		m_DF.FinishReadItems();
		if(m_creatures_inited){
			m_DF.FinishReadCreatures();
		}
        m_DF.FinishReadNameTables();
        m_DF.FinishReadSettlements();
        m_DF.Detach();
        }
        catch(...){}
    }
}
