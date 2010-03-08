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
#include <QVector>
#include "dwarf.h"
#include "dfinstance.h"
#include "skill.h"
#include "labor.h"
#include "trait.h"
#include "dwarfjob.h"
#include "defines.h"
#include "gamedatareader.h"
#include "customprofession.h"
#include "memorylayout.h"
#include "dwarftherapist.h"
#include "dwarfdetailswidget.h"
#include "mainwindow.h"
#include "profession.h"
#include "militarypreference.h"
#include "utils.h"
#include "cp437codec.h"
#include "dfhack/DFMemInfo.h"
#include "dfhack/DFProcess.h"

Dwarf::Dwarf(DFInstance *df, const uint &index, QObject *parent)
	: QObject(parent)
	, m_df(df)
 	, m_total_xp(0)
   , m_index(index)
    , m_raw_profession(-1)
	, m_migration_wave(0)
    , m_current_job_id(-1)
    , m_squad_leader_id(-1)
{
	read_settings();
	refresh_data();
	connect(DT, SIGNAL(settings_changed()), SLOT(read_settings()));

	// setup context actions
	m_actions.clear();
    QAction *show_details = new QAction(tr("Show Details..."), this);
    connect(show_details, SIGNAL(triggered()), SLOT(show_details()));
    m_actions << show_details;

    QAction *zoom_to_dwarf = new QAction(tr("Zoom To Dwarf..."),this);
    connect(zoom_to_dwarf, SIGNAL(triggered()), SLOT(move_view_to()));
    m_actions << zoom_to_dwarf;
}

Dwarf::~Dwarf() {
    m_traits.clear();
    foreach(QAction *a, m_actions) {
        a->deleteLater();
    }
    m_actions.clear();
    m_skills.clear();
}

void Dwarf::refresh_data() {
    m_df->getAPI()->Suspend();
    m_df->getAPI()->ReadCreature(m_index, m_cre);
    m_address = m_cre.origin;
    m_dirty.D_LOCATION =  (m_x != m_cre.x || m_y != m_cre.y || m_z != m_cre.z);
    
    m_x = m_cre.x;
    m_y = m_cre.y;
    m_z = m_cre.z;
    
	m_id = m_cre.id; 
	TRACE << "\tID:" << m_id;
    char sex = m_cre.sex; 
    m_is_male = (int)sex == 1;
    TRACE << "\tMALE?" << m_is_male;

	m_first_name = m_df->convertString(m_cre.name.first_name); 
	if (m_first_name.size() > 1)
		m_first_name[0] = m_first_name[0].toUpper();
	TRACE << "\tFIRSTNAME:" << m_first_name;
	m_nick_name = m_df->convertString(m_cre.name.nickname);
	TRACE << "\tNICKNAME:" << m_nick_name;
	m_pending_nick_name = m_nick_name;
	m_last_name = m_df->convertString(m_df->translateName(m_cre.name,false));
	TRACE << "\tLASTNAME:" << m_last_name;
	m_translated_last_name = m_df->convertString(m_df->translateName(m_cre.name,true));
    calc_names();

	m_custom_profession = m_df->convertString(m_cre.custom_profession); 
	TRACE << "\tCUSTOM PROF:" << m_custom_profession;
	m_pending_custom_profession = m_custom_profession;
	m_race_id = m_cre.type; 
	TRACE << "\tRACE ID:" << m_race_id;
	m_skills = read_skills();
	TRACE << "\tSKILLS: FOUND" << m_skills.size();
	m_profession = read_profession();
	TRACE << "\tPROFESSION:" << m_profession;
	m_strength = m_cre.strength; 
	TRACE << "\tSTRENGTH:" << m_strength;
    m_toughness = m_cre.toughness; 
	TRACE << "\tTOUGHNESS:" << m_toughness;
	m_agility = m_cre.agility; 
	TRACE << "\tAGILITY:" << m_cre.agility; 
    read_labors();
	read_traits();
	TRACE << "\tTRAITS:" << m_traits.size();
	m_money = m_cre.money; 
	TRACE << "\tMONEY:" << m_money;
    m_dirty.D_LOCATION = m_raw_happiness != m_cre.happiness;

	m_raw_happiness = m_cre.happiness; 
	TRACE << "\tRAW HAPPINESS:" << m_raw_happiness;
	m_happiness = happiness_from_score(m_raw_happiness);
	TRACE << "\tHAPPINESS:" << happiness_name(m_happiness);
    
    read_likes();
    read_current_job();
    TRACE << "\tCURRENT JOB:" << m_current_job_id << m_current_job;

    m_squad_leader_id = m_cre.squad_leader_id; 
    TRACE << "\tSQUAD LEADER ID:" << m_squad_leader_id;

    m_squad_name = m_df->convertString(m_df->translateName(m_cre.squad_name,false));
    TRACE << "\tSQUAD NAME:" << m_squad_name;
    m_generic_squad_name = m_df->convertString(m_df->translateName(m_cre.squad_name,true));
    TRACE << "\tGENERIC SQUAD NAME:" << m_generic_squad_name;

	if(m_cre.flags1.bits.had_mood && (m_cre.mood == 0xFFFF || m_cre.mood == 8 ) ) //No idea what 8 is. But it's what DF checks!
		m_artifact_name = m_df->translateName(m_cre.artifact_name,false);
	else
		m_artifact_name = "";

	TRACE << "finished refresh of dwarf data for dwarf:" << m_nice_name << "(" << m_translated_name << ")";


	m_df->getAPI()->Resume();
}

void Dwarf::read_settings() {
	QSettings *s = DT->user_settings();
	bool new_show_full_name = s->value("options/show_full_dwarf_names", false).toBool();
	if (new_show_full_name != m_show_full_name) {
		calc_names();
		emit name_changed();
	}
	m_show_full_name = new_show_full_name;
}

QString Dwarf::profession() {
	if (!m_pending_custom_profession.isEmpty())
		return m_pending_custom_profession;
	if (!m_custom_profession.isEmpty())
		return m_custom_profession;
	return m_profession;
}

bool Dwarf::active_military() {
    Profession *p = GameDataReader::ptr()->get_profession(m_raw_profession);
    return p && p->is_military();
}

void Dwarf::calc_names() {
	if (m_pending_nick_name.isEmpty()) {
		m_nice_name = QString("%1 %2").arg(m_first_name, m_last_name);
		m_translated_name = QString("%1 %2").arg(m_first_name, m_translated_last_name);
	} else {
		if (m_show_full_name) {
			m_nice_name = QString("%1 '%2' %3").arg(m_first_name, m_pending_nick_name, m_last_name);
			m_translated_name = QString("%1 '%2' %3").arg(m_first_name, m_pending_nick_name, m_translated_last_name);
		} else {
			m_nice_name = QString("'%1' %2").arg(m_pending_nick_name, m_last_name);
			m_translated_name = QString("'%1' %2").arg(m_pending_nick_name, m_translated_last_name);
		}
	}
    // uncomment to put address at front of name
    //m_nice_name = QString("0x%1 %2").arg(m_address, 8, 16, QChar('0')).arg(m_nice_name);
	// uncomment to put internal ID at front of name
	//m_nice_name = QString("%1 %2").arg(m_id).arg(m_nice_name);
    //m_nice_name = codec->toUnicode(m_nice_name.toAscii().data(),m_nice_name.size());
    //m_translated_name = codec->toUnicode(m_translated_name.toAscii().data(),m_translated_name.size());
}

Dwarf::DWARF_HAPPINESS Dwarf::happiness_from_score(int score) {
    if (score < 1)
        return DH_MISERABLE;
    else if (score <= 25)
        return DH_VERY_UNHAPPY;
    else if (score <= 50)
        return DH_UNHAPPY;
    else if (score <= 75)
        return DH_FINE;
    else if (score <= 125)
        return DH_CONTENT;
    else if (score <= 150)
        return DH_HAPPY;
    else
        return DH_ECSTATIC;
}

QString Dwarf::happiness_name(DWARF_HAPPINESS happiness) {
	switch(happiness) {
		case DH_MISERABLE: return tr("Miserable");
		case DH_VERY_UNHAPPY: return tr("Very Unhappy");
		case DH_UNHAPPY: return tr("Unhappy");
		case DH_FINE: return tr("Fine");
		case DH_CONTENT: return tr("Content");
		case DH_HAPPY: return tr("Happy");
		case DH_ECSTATIC: return tr("Ecstatic");
		default: return "UNKNOWN";
	}
}

Dwarf * Dwarf::get_dwarf(DFInstance *df, const uint &index) {
	//  TRACE << "attempting to load dwarf at" << addr << "using memory layout" << mem->game_version();

    Dwarf *unverified_dwarf = new Dwarf(df,index);
    if (df->getCreatureType(unverified_dwarf->m_cre.type) != "dwarf") { // we only care about dwarfs
        TRACE << "Ignoring non-dwarf creature with racial ID of " << hexify(unverified_dwarf->m_cre.type);
        return 0;
    }

    if(unverified_dwarf->m_cre.flags1.bits.zombie)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a zombie";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags1.bits.skeleton)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a skeleton";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags1.bits.merchant)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a merchant";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags1.bits.diplomat)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a diplomat";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags1.bits.active_invader || unverified_dwarf->m_cre.flags1.bits.invader_origin || unverified_dwarf->m_cre.flags1.bits.invades)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be an invader";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags1.bits.marauder)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a marauder";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags2.bits.killed)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be dead, Jim.";
        delete unverified_dwarf;
        return 0;
    }
    else if(unverified_dwarf->m_cre.flags2.bits.underworld)
    {
        LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be from the Underworld. SPOOKY!";
        delete unverified_dwarf;
        return 0;
    }
	//HACK: ugh... so ugly, but this seems to be the best way to filter out kidnapped babies
	short baby_id = -1;
	foreach(Profession *p, GameDataReader::ptr()->get_professions()) {
		if (p->name(true) == "Baby") {
			baby_id = p->id();
			break;
		}
	}
	if (unverified_dwarf->raw_profession() == baby_id) {
		if (!unverified_dwarf->m_cre.flags1.bits.rider) {
			// kidnapped flag? seems like it
			LOGD << "Ignoring" << unverified_dwarf->nice_name() << "who appears to be a kidnapped baby";
			delete unverified_dwarf;
			return 0;
		}
	}
	return unverified_dwarf;
}

void Dwarf::read_traits() {
	m_traits.clear();
	for (int i = 0; i < 30; ++i) {
		short val = m_cre.traits[i];
		int deviation = abs(val - 50); // how far from the norm is this trait?
		if (deviation <= 10) {
            val = -1; // this will cause median scores to not be treated as "active" traits
		}
        m_traits.insert(i, val);
	}
}

QVector<Skill> Dwarf::read_skills() {
	m_total_xp = 0;
	QVector<Skill> skills(0);
    for(int i=0;i < m_cre.numSkills;i++){
        Skill s(m_cre.skills[i].id, m_cre.skills[i].experience, m_cre.skills[i].rating);
		m_total_xp += s.actual_exp();
		skills.append(s);
	}
	return skills;
}

const Skill Dwarf::get_skill(int skill_id) {
	foreach(Skill s, m_skills) {
		if (s.id() == skill_id) {
			return s;
		}
	}
	return Skill(skill_id, 0, -1);
}

short Dwarf::get_rating_by_skill(int skill_id) {
	short retval = -1;
	foreach(Skill s, m_skills) {
		if (s.id() == skill_id) {
			retval = s.rating();
			break;
		}
	}
	return retval;
}

short Dwarf::get_rating_by_labor(int labor_id) {
	GameDataReader *gdr = GameDataReader::ptr();
	Labor *l = gdr->get_labor(labor_id);
    if (l)
	    return get_rating_by_skill(l->skill_id);
    else
        return -1;
}

QString Dwarf::read_profession() {
    m_raw_profession = m_cre.profession;
    Profession *p = GameDataReader::ptr()->get_profession(m_raw_profession);
    QString prof_name = tr("Unknown Profession %1").arg(m_raw_profession);
    if (p) {
        m_can_set_labors = p->can_assign_labors();
        prof_name = p->name(m_is_male);
    } else {
        LOGC << "Read unknown profession with id" << m_raw_profession << "for dwarf" << m_nice_name;
        m_can_set_labors = false;
    }
    if (!m_custom_profession.isEmpty()) {
        return m_custom_profession; 
    } else {
        return prof_name;
    }
}

void Dwarf::read_current_job() {
    if (m_cre.current_job.active) {
        m_dirty.D_JOB = m_current_job_id != m_cre.current_job.jobId;
        m_current_job_id = m_cre.current_job.jobId;
		DwarfJob *job = GameDataReader::ptr()->get_job(m_current_job_id);
		if (job)
			m_current_job = job->description;
		else
			m_current_job = tr("Unknown job");
    } else {
        m_current_job = tr("No Job");
    }

}

short Dwarf::pref_value(const int &labor_id) {
    if (!m_pending_labors.contains(labor_id)) {
        LOGW << m_nice_name << "pref_value for labor_id" << labor_id << "was not found in pending labors hash!";
        return 0;
    }
    return m_pending_labors.value(labor_id);
}

void Dwarf::toggle_pref_value(const int &labor_id) {
    short next_val = GameDataReader::ptr()->get_military_preference(labor_id)->next_val(pref_value(labor_id));
    m_pending_labors[labor_id] = next_val;
}

void Dwarf::read_labors() {
    // read a big array of labors in one read, then pick and choose
	// the values we care about
	//uchar buf[102];
	//memset(buf, 0, 102);
    //m_df->read_raw(addr, 102, &buf);

	// get the list of identified labors from game_data.ini
	GameDataReader *gdr = GameDataReader::ptr();
	foreach(Labor *l, gdr->get_ordered_labors()) {
        if (l->is_weapon && l->labor_id < 0) // unarmed
            continue;
		bool enabled = m_cre.labors[l->labor_id] > 0;
		m_labors[l->labor_id] = enabled;
		m_pending_labors[l->labor_id] = enabled;
	}
    // also store prefs in this structure
    foreach(MilitaryPreference *mp, gdr->get_military_preferences()) {
        m_labors[mp->labor_id] = static_cast<ushort>(m_cre.labors[mp->labor_id]);
        m_pending_labors[mp->labor_id] = static_cast<ushort>(m_cre.labors[mp->labor_id]);
    }
}

bool Dwarf::labor_enabled(int labor_id) {
    if (labor_id < 0) {// unarmed
        bool uses_weapon = false;
        foreach(Labor *l, GameDataReader::ptr()->get_ordered_labors()) {
            if (l->is_weapon && l->labor_id > 0) {
                if (m_pending_labors[l->labor_id]) {
                    uses_weapon = true;
                    break;
                }
            }
        }
        return !uses_weapon;
    } else {
        return m_pending_labors.value(labor_id, false);
    }
}

bool Dwarf::is_labor_state_dirty(int labor_id) {
	return m_labors[labor_id] != m_pending_labors[labor_id];
}

QVector<int> Dwarf::get_dirty_labors() {
	QVector<int> labors;
//    vector<int> m_laborIDs = m_labors.uniqueKeys().toVector().toStdVector();
//    vector<int> m_pendingIDs = m_pending_labors.uniqueKeys().toVector().toStdVector();
 //   if(m_labors.size() != m_pending_labors.size()){
 /*   for(int i = 0;i<m_laborIDs.size();i++){
      TRACE << m_laborIDs[i];
    }
    for(int i = 0;i<m_pendingIDs.size();i++){
      TRACE << m_pendingIDs[i];
    }
    }
    */
    	Q_ASSERT(m_labors.size() == m_pending_labors.size());
	foreach(int labor_id, m_pending_labors.uniqueKeys().toVector()) {
		if (is_labor_state_dirty(labor_id))
			labors << labor_id;
	}
	return labors;
}

bool Dwarf::toggle_labor(int labor_id) {
	set_labor(labor_id, !m_pending_labors[labor_id]);
	return true;
}

void Dwarf::set_labor(int labor_id, bool enabled) {
    Labor *l = GameDataReader::ptr()->get_labor(labor_id);
    if (!l) {
        LOGD << "UNKNOWN LABOR: Bailing on set labor of id" << labor_id << "enabled?" << enabled << "for" << m_nice_name;
        return;
    }

    if (!m_can_set_labors && !DT->labor_cheats_allowed() && !l->is_weapon) {
        LOGD << "IGNORING SET LABOR OF ID:" << labor_id << "TO:" << enabled << "FOR:" << m_nice_name << "PROF_ID" << m_raw_profession
             << "PROF_NAME:" << profession() << "CUSTOM:" << m_pending_custom_profession;
        return;
    }

    if (enabled) { // user is turning a labor on, so we must turn off exclusives
        foreach(int excluded, l->get_excluded_labors()) {
            m_pending_labors[excluded] = false;
        }
    }
    if (enabled && l->is_weapon) { // weapon type labors are automatically exclusive
        foreach(Labor *l, GameDataReader::ptr()->get_ordered_labors()) {
            if (l && l->is_weapon && l->labor_id != -1)
                m_pending_labors[l->labor_id] = false;
        }
    }
	m_pending_labors[labor_id] = enabled;
}

int Dwarf::pending_changes() {
	int cnt = get_dirty_labors().size();
	if (m_nick_name != m_pending_nick_name)
		cnt++;
	if (m_custom_profession != m_pending_custom_profession)
		cnt++;
	return cnt;
}

void Dwarf::clear_pending() {
	refresh_data();
}

bool Dwarf::commit_pending() {
/*	MemoryLayout *mem = m_df->memory_layout();
	int addr = m_address + mem->dwarf_offset("labors");
*/
    uint8_t buf[NUM_CREATURE_LABORS] = {0};
    for(int i = 0;i<NUM_CREATURE_LABORS;i++){
        buf[i] = m_cre.labors[i];
    }
	
	foreach(int labor_id, m_pending_labors.uniqueKeys()) {
        if (labor_id < 0)
            continue;
		// change values to what's pending
		buf[labor_id] = m_pending_labors.value(labor_id);
	}
//	m_df->getAPI()->Suspend();
    m_df->getAPI()->WriteLabors(m_index,buf);
    bool success = true;
	if (m_pending_nick_name != m_nick_name)
        success = write_string(m_pending_nick_name,true);
	if (m_pending_custom_profession != m_custom_profession)
	  success = write_string(m_pending_custom_profession,false);
	refresh_data();
    return success;
}

bool Dwarf::write_string(const QString &changeString,bool isName){ // if not name, has to be profession

    DFHack::Process* p = m_df->getAPI()->getProcess();
    DFHack::memory_info* mem = m_df->getMem();
    if(isName){
        p->writeSTLString(m_cre.origin+mem->getOffset("creature_nick_name"),changeString.toStdString());
        return(true);
    }
    p->writeSTLString(m_cre.origin+mem->getOffset("creature_custom_profession"),changeString.toStdString());
    return(true);
}

void Dwarf::set_custom_profession_text(const QString &prof_text) {
    m_pending_custom_profession = prof_text;
}

int Dwarf::apply_custom_profession(CustomProfession *cp) {
	foreach(int labor_id, m_pending_labors.uniqueKeys()) {
		set_labor(labor_id, false); // turn off everything...
	}

	foreach(int labor_id, cp->get_enabled_labors()) {
		set_labor(labor_id, true); // only turn on what this prof has enabled...
	}
	m_pending_custom_profession = cp->get_name();
	return get_dirty_labors().size();
}

QTreeWidgetItem *Dwarf::get_pending_changes_tree() {
	QVector<int> labors = get_dirty_labors();
	QTreeWidgetItem *d_item = new QTreeWidgetItem;
	d_item->setText(0, nice_name() + "(" + QString::number(labors.size()) + ")");
	d_item->setData(0, Qt::UserRole, id());
	if (m_pending_nick_name != m_nick_name) {
		QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
		i->setText(0, "nickname change to " + m_pending_nick_name);
		i->setIcon(0, QIcon(":img/book_edit.png"));
		i->setData(0, Qt::UserRole, id());
	}
	if (m_pending_custom_profession != m_custom_profession) {
		QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
		QString prof = m_pending_custom_profession;
		if (prof.isEmpty())
			prof = "DEFAULT";
		i->setText(0, "profession change to " + prof);
		i->setIcon(0, QIcon(":img/book_edit.png"));
		i->setData(0, Qt::UserRole, id());
	}
	foreach(int labor_id, labors) {
		Labor *l = GameDataReader::ptr()->get_labor(labor_id);
        MilitaryPreference *mp = GameDataReader::ptr()->get_military_preference(labor_id);

		if (!l || l->labor_id != labor_id) {
            // this may be a mil pref not a labor..
            if (!mp || mp->labor_id != labor_id) {
                LOGW << "somehow got a change to an unknown labor with id:" << labor_id;
				continue;
            }
        }

	    QTreeWidgetItem *i = new QTreeWidgetItem(d_item);
        if (l) {
	        i->setText(0, l->name);
	        if (labor_enabled(labor_id)) {
		        i->setIcon(0, QIcon(":img/add.png"));
	        } else {
		        i->setIcon(0, QIcon(":img/delete.png"));
	        }
        } else if (mp) {
            i->setText(0, QString("Set %1 to %2").arg(mp->name).arg(mp->value_name(pref_value(labor_id))));
            i->setIcon(0, QIcon(":img/arrow_switch.png"));
        }
	    i->setData(0, Qt::UserRole, id());
	}
	return d_item;
}

QString Dwarf::tooltip_text() {
	QString skill_summary, trait_summary;
	QVector<Skill> *skills = get_skills();
	qSort(*skills);
	for (int i = skills->size() - 1; i >= 0; --i) {
		skill_summary.append(QString("<li>%1</li>").arg(skills->at(i).to_string()));
	}
	GameDataReader *gdr = GameDataReader::ptr();
	for (int i = 0; i <= m_traits.size(); ++i) {
		if (m_traits.value(i) == -1)
			continue;
		Trait *t = gdr->get_trait(i);
		if (!t)
			continue;
		trait_summary.append(QString("%1").arg(t->level_message(m_traits.value(i))));
		if (i < m_traits.size() - 1) // second to last
			trait_summary.append(", ");
	}

	return tr("<b><font size=5>%1</font><br/><font size=3>(%2)</font></b><br/><br/>"
		"<b>Happiness:</b> %3 (%4)<br/>"
		"<b>Profession:</b> %5<br/><br/>"
		"<b>Skills:</b><ul>%6</ul><br/>"
		"<b>Traits:</b> %7<br/>")
		.arg(m_nice_name)
		.arg(m_translated_name)
		.arg(happiness_name(m_happiness))
		.arg(m_raw_happiness)
		.arg(profession())
		.arg(skill_summary)
		.arg(trait_summary);
}

Dwarf::LIKETYPE Dwarf::getLikeName(DFHack::t_like & like, QString & retLike)
{ // The function in DF which prints out the likes is a monster, it is a huge switch statement with tons of options and calls a ton of other functions as well, 
    //so I am not going to try and put all the possibilites here, only the low hanging fruit, with stones and metals, as well as items,
    //you can easily find good canidates for military duty for instance
    //The ideal thing to do would be to call the df function directly with the desired likes, the df function modifies a string, so it should be possible to do...
    if(like.active){
        if(like.type ==0){
            switch (like.material.type)
            {
            case 0:
                retLike = m_df->getWoodType(like.material.index);
                return(MATERIAL);
            case 1:
                retLike = m_df->getStoneType(like.material.index);
                return(MATERIAL);
            case 2:
                retLike = m_df->getMetalType(like.material.index);           
                return(MATERIAL);
            case 12: // don't ask me why this has such a large jump, maybe this is not actually the matType for plants, but they all have this set to 12
                retLike = m_df->getPlantType(like.material.index);
                return(MATERIAL);
            case 32:
                retLike = m_df->getPlantType(like.material.index);
                return(MATERIAL);
            case 121:
                retLike = m_df->getCreatureType(like.material.index);
                return(MATERIAL);
            default:
                return(FAIL);
            }
        }
        else if(like.type == 4 && like.itemIndex != -1){
            switch(like.itemClass)
            {
            case 24:
                retLike = m_df->getItemType(0,like.itemIndex);
                return(ITEM);
            case 25:
                retLike = m_df->getItemType(4,like.itemIndex);
                return(ITEM);
            case 26:
                retLike = m_df->getItemType(8,like.itemIndex);
                return(ITEM);
            case 27:
                retLike = m_df->getItemType(9,like.itemIndex);
                return(ITEM);
            case 28:
                retLike = m_df->getItemType(10,like.itemIndex);
                return(ITEM);
            case 29:
                retLike = m_df->getItemType(7,like.itemIndex);
                return(ITEM);
            case 38:
                retLike = m_df->getItemType(5,like.itemIndex);
                return(ITEM);
            case 63:
                retLike = m_df->getItemType(11,like.itemIndex);
                return(ITEM);
            case 68:
            case 69:
                retLike = m_df->getItemType(6,like.itemIndex);
                return(ITEM);
            case 70:
                retLike = m_df->getItemType(1,like.itemIndex);
                return(ITEM);
            default:
                //retLike = QString(like.itemClass << ":" << like.itemIndex;
                return(FAIL);
            }
        }
        else if(like.material.type != -1){// && like.material.index == -1){
            if(like.type == 2){
                switch(like.itemClass)
                {
                case 52:
                case 53:
                case 58:
                    retLike = m_df->getPlantType(like.material.type);
                    return(FOOD);
                case 72:
                    if(like.material.type =! 10){ // 10 is for milk stuff, which I don't know how to do
                        retLike = m_df->getPlantExtractType(like.material.index);
                        return(FOOD);
                    }
                    return(FAIL);
                case 74:
                    retLike = m_df->getPlantDrinkType(like.material.index);
                    return(FOOD);
                case 75:
                    retLike = m_df->getPlantFoodType(like.material.index);
                    return(FOOD);
                case 47:
                case 48:
                    retLike = m_df->getCreatureType(like.material.type);
                    return(FOOD);
                default:
                    return(FAIL);
                }
            }
        }
    }
    return(FAIL);
}

void Dwarf::read_likes()
{
    m_likes[0].clear();
    m_likes[1].clear();
    m_likes[2].clear();
    for(int i = 0; i < m_cre.numLikes; i++){
        QString typeName;
        LIKETYPE type = getLikeName(m_cre.likes[i],typeName);
        if(type){
            m_likes[type-1].push_back(typeName);
        }
    }
}

bool Dwarf::has_like(QString like)
{
    QRegExp regex(like,Qt::CaseInsensitive,QRegExp::Wildcard); // only do wildcard matching
    for(int i = 0; i < 3;i++){
        for(int j = 0;j<m_likes[i].size();j++){
            if(regex.exactMatch(m_likes[i][j])){
                return true;
            }
        }
    }
    return false;
}   
/*bool Dwarf::hasLikeRegex(QString like)
{
    QRegExp regex(like,Qt::CaseInsensitive,QRegExp::RegExp); // use perl like regex
    for(int i = 0; i < 3;i++){
        for(int j = 0;j<m_likes[i].size();j++){
            if(regex.exactMatch(m_likes[i][j])){
                return true;
            }
        }
    }
    return false;
}*/
void Dwarf::show_details() {
	DT->get_main_window()->show_dwarf_details_dock(this);
}

void Dwarf::move_view_to(){
    int width, height;
    DFHack::API *DF = m_df->getAPI();
    DF->Suspend();
    DF->getWindowSize(width,height);
    uint mapx,mapy,mapz;
    DF->getSize(mapx,mapy,mapz);
    mapx *= 16;
    mapy *= 16;
    int cursor_x, cursor_y, cursor_z;
    DF->getCursorCoords(cursor_x,cursor_y,cursor_z);
    if(cursor_x != -30000){
        DF->setCursorCoords(m_x,m_y,m_z);
    }
    int zoom_x, zoom_y, zoom_z;
    zoom_x = (std::min)(int(m_x)-int(width/2),int(mapx)-int(width+2));
    if(width > (int)mapx || zoom_x < 0){
        zoom_x = 0;
    }
    zoom_y = (std::min)(int(m_y)-int(height/2),int(mapy)-int(height-2));
    if(height > (int)mapy || zoom_y < 0){
        zoom_y = 0;
    }
    zoom_z = (std::min)(m_z,mapz);
    DF->setViewCoords(zoom_x,zoom_y,zoom_z);
    DF->Resume();
}

/************************************************************************/
/* SQUAD STUFF                                                          */
/************************************************************************/
Dwarf *Dwarf::get_squad_leader() {
    // Oh I will be...
    if (m_squad_leader_id <= 0) {
        // I'll be squad leader!
        return 0;
    } else {
        return DT->get_dwarf_by_id(m_squad_leader_id); // Tony Danza
    }
}

QString Dwarf::squad_name() {
    if (m_squad_leader_id > 0) // follow the chain up
        return get_squad_leader()->squad_name();
    else
        return m_squad_name;
}

void Dwarf::add_squad_member(Dwarf *d) {
    if (d && !m_squad_members.contains(d))
        m_squad_members << d;
}
