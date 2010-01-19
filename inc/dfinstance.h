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

    DFHack::API * getAPI(){return &m_DF;}
    DFHack::memory_info * getMem(){return &m_mem;}
    QString getCreatureType(uint type);
    QString getBuildingType(uint type);
    QString getStoneType(uint type);
    QString getMetalType(uint type);
    QString getItemType(uint type, uint index);
    QString getWoodType(uint type);
    QString getPlantType(uint type);
    QString getPlantDrinkType(uint type);
    QString getPlantFoodType(uint type);
    QString getPlantExtractType(uint type);
    QString convertString(const char *);
    QString convertString(const QString &);

	bool is_ok(){return m_is_ok;}
	
    QString translateName(const t_lastname &,string trans);
    QString translateName(const t_squadname &, string trans);
	
	QVector<Dwarf*> load_dwarves();

private:
    bool m_is_ok;
    CP437Codec *m_codec;
    vector<t_matgloss> m_creaturestypes;
    vector<t_matglossPlant> m_plantstypes;
    vector<t_matgloss> m_stonestypes;
    vector<t_matgloss> m_metalstypes;
    vector<t_matgloss> m_woodstypes;
    vector< vector<t_itemType> > m_itemstypes;

    uint m_numCreatures;
    map<string, vector<string> > m_names;
    int m_attach_count;
    QTimer *m_heartbeat_timer;
    DFHack::API m_DF;
    DFHack::memory_info m_mem;
    uint m_numBuildings;
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
