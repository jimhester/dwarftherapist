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
#ifndef DFINSTANCE_H
#define DFINSTANCE_H

#include <QtGui>
#include "cp437codec.h"
using namespace std;
#include "dfhack/integers.h"
#include "dfhack/DFTypes.h"
#include "dfhack/DFHackAPI.h"
#include "dfhack/DFMemInfo.h"
using namespace DFHack;

class Dwarf;
class MemoryLayout;
struct MemorySegment;

class DFInstance : public QObject {
	Q_OBJECT
public:
	DFInstance(QObject *parent=0);
	~DFInstance();

	bool find_running_copy();

	// accessors

    DFHack::API * get_api(){return &m_DF;}
    DFHack::memory_info * getMem(){return m_mem;}
    uint get_num_creatures() { return m_num_creatures; }
    QString get_creature_type(uint type);
    QString get_building_type(uint type);
    QString get_stone_type(uint type);
    QString get_metal_type(uint type);
    QString get_item_type(uint type, uint index);
    QString get_wood_type(uint type);
    QString get_plant_type(uint type);
    QString get_plant_drink_type(uint type);
    QString get_plant_food_type(uint type);
    QString get_plant_extract_type(uint type);
    QString get_generic_fort_name(){return m_generic_fort_name;}
    QString get_dwarf_fort_name(){return m_dwarf_fort_name;}
    QString convert_string(const char *);
    QString convert_string(const QString &);

	bool is_ok(){return m_is_ok;}
	bool has_shm(){return m_has_shm;}
	
    QString translate_name(const t_name &,bool inEnglish);
	
	QVector<Dwarf*> load_dwarves();

private:
	bool m_has_shm;
    bool m_is_ok;
	bool m_creatures_inited;
    CP437Codec *m_codec;
    vector<t_matgloss> m_creaturestypes;
    vector<t_matglossPlant> m_plantstypes;
    vector<t_matgloss> m_stonestypes;
    vector<t_matgloss> m_metalstypes;
    vector<t_matgloss> m_woodstypes;
    vector< vector<t_itemType> > m_itemstypes;

    vector<vector<string>> m_english;
	vector<vector<string>> m_foreign;
    int m_attach_count;
    QTimer *m_heartbeat_timer;
    DFHack::API m_DF;
    DFHack::memory_info* m_mem;
    uint m_num_buildings;
    uint m_num_creatures;
    uint m_num_settlements;
    QString m_dwarf_fort_name;
    QString m_generic_fort_name;
    vector<string> m_buildingtypes;

private slots:
	void heartbeat();

signals:
	// methods for sending progress information to QWidgets
	void scan_total_steps(int steps);
	void scan_progress(int step);
	void scan_message(const QString &message);
	void connection_interrupted();

};

#endif // DFINSTANCE_H
