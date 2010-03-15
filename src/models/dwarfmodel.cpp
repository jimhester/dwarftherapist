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
#include <QtCore>
#include <QtDebug>

#include "dfinstance.h"
#include "dwarfmodel.h"
#include "dwarf.h"
#include "skill.h"
#include "labor.h"
#include "profession.h"
#include "squad.h"
#include "statetableview.h"
#include "defines.h"
#include "dwarftherapist.h"

#include "columntypes.h"
#include "gridview.h"
#include "viewcolumnset.h"
#include "viewcolumn.h"
#include "laborcolumn.h"
#include "skillcolumn.h"
#include "spacercolumn.h"


DwarfModel::DwarfModel(QObject *parent)
	: QStandardItemModel(parent)
	, m_df(0)
	, m_group_by(GB_NOTHING)
	, m_selected_col(-1)
{}

DwarfModel::~DwarfModel() {
	clear_all();
}

void DwarfModel::clear_all() {
	clear_pending();
	foreach(Dwarf *d, m_dwarves) {
		delete d;
        d=0;
	}
	m_dwarves.clear();
	m_grouped_dwarves.clear();
    foreach(ViewColumnSet *set, m_gridview->sets()) {
        foreach(ViewColumn *col, set->columns()) {
            col->clear_cells();
        }
    }
	clear();
}

void DwarfModel::section_right_clicked(int col) {
	if (col == m_selected_col) {
		m_selected_col = -1; // turn off the guides
	} else {
		m_selected_col = col;
	}
	emit dataChanged(index(0, col), index(rowCount()-1, col));
}

void DwarfModel::load_dwarves() {
	// clear id->dwarf map
	clear_all();
	if (rowCount())
		removeRows(0, rowCount());

//	m_df->attach();
	foreach(Dwarf *d, m_df->load_dwarves()) {
		m_dwarves[d->id()] = d;
	}
	//m_df->detach();
	/*! Let's try to guess the wave a dwarf arrived in based on ID.
	The game appears to assign ids to creates in a serial manner.
	Since at least 1 season seems to pass in between waves there is a
	high likelihood that dwarves arriving in a new wave will be folling
	several other creatures such as groundhogs and the like, thus
	producing a gap in IDs of more than 1 
	*/
	int last_id = 0;
	int wave = 0;
	QList<int> ids = m_dwarves.uniqueKeys(); // get all ids
	qSort(ids); // now in order;
	for (int i = 0; i < ids.size(); ++i) {
		int id = ids.at(i);
		if ((id - last_id) > 5)
			wave++;
		last_id = id;
		m_dwarves[id]->set_migration_wave(wave);
	}
    
    /* build up the squad structures for loaded dwarfs */
    //m_squads.clear();
    foreach(Dwarf *d, m_dwarves) {
        Dwarf *squad_leader = d->get_squad_leader();
        if (squad_leader) { // this dwarf has a squad leader
            squad_leader->add_squad_member(d);
        }
    }
}
void DwarfModel::refresh_dwarves(){
    m_df->get_api()->Suspend();
    if(!Dwarf::can_read){
        Dwarf::can_read = true;
        return;
    }
	foreach(Dwarf *d, m_dwarves) {
        qApp->processEvents();
        if(!Dwarf::can_read){
            Dwarf::can_read = true;
            return;
        }
        d->refresh_data();
    }
    m_df->get_api()->Resume();
	foreach(ViewColumnSet *set, m_gridview->sets()) {
		uint numCol = 0;
		foreach(ViewColumn *col, set->columns()) {
			if(col->type() == CT_IDLE){
				col->redraw_cells();
			}
			numCol++;
		}
	}
}
	
void DwarfModel::build_rows() {
	// don't need to go delete the dwarf pointers in here, since the earlier foreach should have
	// deleted them
	m_grouped_dwarves.clear();
	clear();

    /*
    TODO: Move this to the RotatedHeader class
    */
    int start_col = 1;
    setHorizontalHeaderItem(0, new QStandardItem);
    emit clear_spacers();
    QSettings *s = DT->user_settings();
    int width = s->value("options/grid/cell_size", DEFAULT_CELL_SIZE).toInt();
    foreach(ViewColumnSet *set, m_gridview->sets()) {
        foreach(ViewColumn *col, set->columns()) {
            QStandardItem *header = new QStandardItem(col->title());
            header->setData(col->bg_color(), Qt::BackgroundColorRole);
            setHorizontalHeaderItem(start_col++, header);
            switch (col->type()) {
                case CT_SPACER:
                    {
                        SpacerColumn *c = static_cast<SpacerColumn*>(col);
                        emit set_index_as_spacer(start_col - 1);
                        emit preferred_header_size(start_col - 1, c->width());
                    }
                    break;
                default:
                    emit preferred_header_size(start_col - 1, width);
            }
        }
    }

	// populate dwarf maps
	foreach(Dwarf *d, m_dwarves) {
		switch (m_group_by) {
			case GB_NOTHING:
				m_grouped_dwarves[QString::number(d->id())].append(d);
				break;
			case GB_PROFESSION:
				m_grouped_dwarves[d->profession()].append(d);
				break;
			case GB_LEGENDARY:
				{
					int legendary_skills = 0;
					foreach(Skill s, *d->get_skills()) {
						if (s.rating() >= 15)
							legendary_skills++;
					}
					if (legendary_skills)
						m_grouped_dwarves[tr("Legends")].append(d);
					else
						m_grouped_dwarves[tr("Losers")].append(d);
					break;
				}
			case GB_SEX:
				if (d->is_male())
					m_grouped_dwarves[tr("Males")].append(d);
				else
					m_grouped_dwarves[tr("Females")].append(d);
				break;
			case GB_HAPPINESS:
				m_grouped_dwarves[d->happiness_name(d->get_happiness())].append(d);
				break;
			case GB_MIGRATION_WAVE:
				m_grouped_dwarves[QString("Wave %1").arg(d->migration_wave())].append(d);
				break;
			case GB_CURRENT_JOB:
				m_grouped_dwarves[d->current_job()].append(d);
				break;
            case GB_SQUAD:
				{
					QString squad = "None";
					Dwarf *leader = d->get_squad_leader();
					if (leader) {
						squad = leader->squad_name();
					} else if (d->squad_members().size()) {
						squad = d->squad_name();
						//HACK (reorder this to make sure the leader is added first
						QVector<Dwarf*> current_members = m_grouped_dwarves[squad];
						if (current_members.size()) {
							m_grouped_dwarves[squad].clear();
							m_grouped_dwarves[squad].append(d);
							foreach(Dwarf *member, current_members) {
								m_grouped_dwarves[squad].append(member);
							}
							continue;
						}
					}
					m_grouped_dwarves[squad].append(d);
				}
				break;
			case GB_MILITARY_STATUS:
				{
					// groups
					if (d->raw_profession() == 109 || d->raw_profession() == 110) { //child or baby
						m_grouped_dwarves["Juveniles"].append(d);
					} else if (d->active_military() && !d->can_set_labors()) { // epic military
						m_grouped_dwarves["Champions"].append(d);
					} else if (!d->can_set_labors()) {
						m_grouped_dwarves["Nobles"].append(d);
					} else if (d->active_military()) {
						m_grouped_dwarves["Active Military"].append(d);
					} else {
						m_grouped_dwarves["Can Activate"].append(d);
					}
					/*
					4a) Heroes and Champions (who cannot deactivate)
					4b) Non-Heroic Soldiers and Guards (who can deactivate)
					4c) Civilians (who can activate)
					4d) Juveniles (who may one day activate)
					4e) Immigrant Nobles (who are forever off-limits)
					*/
				}
				break;
		}
	}
 
    foreach(QString key, m_grouped_dwarves.uniqueKeys()) {
	    build_row(key);
	}
}

void DwarfModel::build_row(const QString &key) {
	QIcon icn_f(":img/female.png");
	QIcon icn_m(":img/male.png");
	QStandardItem *root = 0;
	QList<QStandardItem*> root_row;

	if (m_group_by != GB_NOTHING) {
		// we need a root element to hold group members...
		QString title = QString("%1 (%2)").arg(key).arg(m_grouped_dwarves.value(key).size());
		root = new QStandardItem(title);
		root->setData(true, DR_IS_AGGREGATE);
		root->setData(key, DR_GROUP_NAME);
		root->setData(0, DR_RATING);
		root_row << root;
	}

	if (root) { // we have a parent, so we should draw an aggregate row
		foreach(ViewColumnSet *set, m_gridview->sets()) {
			foreach(ViewColumn *col, set->columns()) {
				QStandardItem *item = col->build_aggregate(key, m_grouped_dwarves[key]);
				root_row << item;
			}
		}
	}
	
	foreach(Dwarf *d, m_grouped_dwarves.value(key)) {
        QStandardItem *i_name = new QStandardItem(d->nice_name());
        if (d->active_military()) {
            QFont f = i_name->font();
            f.setBold(true);
            i_name->setFont(f);
			if (d->squad_members().size() && !d->get_squad_leader()) {
				i_name->setText(QString("[L] %1").arg(i_name->text()));
			}
        }
            
		i_name->setToolTip(d->tooltip_text());
		i_name->setStatusTip(d->nice_name());
		i_name->setData(false, DR_IS_AGGREGATE);
		i_name->setData(0, DR_RATING);
		i_name->setData(d->id(), DR_ID);
        i_name->setData(d->raw_profession(), DR_SORT_VALUE);
		
		if (d->is_male()) {
			i_name->setIcon(icn_m);
		} else {
			i_name->setIcon(icn_f);
		}

		QList<QStandardItem*> items;
		items << i_name;
		foreach(ViewColumnSet *set, m_gridview->sets()) {
			foreach(ViewColumn *col, set->columns()) {
				QStandardItem *item = col->build_cell(d);
				items << item;
			}
		}

		if (root) {
			root->appendRow(items);
		} else {
			appendRow(items);
		}
		d->m_name_idx = indexFromItem(i_name);
	}
	if (root) {
		appendRow(root_row);
	}
}

void DwarfModel::cell_activated(const QModelIndex &idx) {
    QStandardItem *item = itemFromIndex(idx);
    bool is_aggregate = item->data(DR_IS_AGGREGATE).toBool();
    if (idx.column() == 0) {
        if (is_aggregate)
            return; // no double clicking aggregate names
        int dwarf_id = item->data(DR_ID).toInt(); // TODO: handle no id
        if (!dwarf_id) {
            LOGW << "double clicked what should have been a dwarf name, but the ID wasn't set!";
            return;
        }
        Dwarf *d = get_dwarf_by_id(dwarf_id);
        d->show_details();
        return;
    }

	COLUMN_TYPE type = static_cast<COLUMN_TYPE>(idx.data(DwarfModel::DR_COL_TYPE).toInt());
	if (type != CT_LABOR && type != CT_MILITARY_PREFERENCE) 
		return;
	
	Q_ASSERT(item);

	int labor_id = item->data(DR_LABOR_ID).toInt();
	int dwarf_id = item->data(DR_ID).toInt(); // TODO: handle no id
	if (is_aggregate) {
		QModelIndex first_col = idx.sibling(idx.row(), 0);
		
		// first find out how many are enabled...
		int enabled_count = 0;
		QString group_name = idx.data(DwarfModel::DR_GROUP_NAME).toString();
		int children = rowCount(first_col);

		foreach(Dwarf *d, m_grouped_dwarves.value(group_name)) {
			if (d->labor_enabled(labor_id))
				enabled_count++;
		}

		// if none or some are enabled, enable all of them
		bool enabled = (enabled_count < children);
		foreach(Dwarf *d, m_grouped_dwarves.value(group_name)) {
			d->set_labor(labor_id, enabled);
		}

		// tell the view what we touched...
        emit dataChanged(idx, idx);
        QModelIndex left = index(0, 0, first_col);
        QModelIndex right = index(rowCount(first_col) - 1, columnCount(first_col) - 1, first_col);
        emit dataChanged(left, right); // tell the view we changed every dwarf under this agg to pick up implicit exclusive changes
    } else {
        if (!dwarf_id) {
            LOGW << "dwarf_id was 0 for cell at" << idx << "!";
        } else {
            QModelIndex left = index(idx.parent().row(), 0, idx.parent().parent());
            QModelIndex right = index(idx.parent().row(), columnCount(idx.parent()) - 1, idx.parent().parent());
            emit dataChanged(left, right); // update the agg row

            left = index(idx.row(), 0, idx.parent());
            right = index(idx.row(), columnCount(idx.parent()) - 1, idx.parent());
		    emit dataChanged(left, right); // update the dwarf row
            if (type == CT_LABOR)
                m_dwarves[dwarf_id]->toggle_labor(labor_id);
            else if (type == CT_MILITARY_PREFERENCE)
                m_dwarves[dwarf_id]->toggle_pref_value(labor_id);
		    
        }
	}
	calculate_pending();
	TRACE << "toggling" << labor_id << "for dwarf:" << dwarf_id;
}

void DwarfModel::set_group_by(int group_by) {
	LOGD << "group_by now set to" << group_by;
	m_group_by = static_cast<GROUP_BY>(group_by);
	if (m_df)
		build_rows();
}

void DwarfModel::calculate_pending() {
	int changes = 0;
	foreach(Dwarf *d, m_dwarves) {
		changes += d->pending_changes();
	}
	emit new_pending_changes(changes);
}

void DwarfModel::clear_pending() {
	foreach(Dwarf *d, m_dwarves) {
		if (d->pending_changes()) {
			d->clear_pending();
		}
	}
	//reset();
	emit new_pending_changes(0);
	emit need_redraw();
}

void DwarfModel::commit_pending() {
    bool success = true;

	foreach(Dwarf *d, m_dwarves) {
        qApp->processEvents();
		if (d->pending_changes()) {
            if(!d->commit_pending()){
                success = false;
                break;
            }
            
		}
	}
	load_dwarves();
    emit new_pending_changes(0);
	emit need_redraw();
}

QVector<Dwarf*> DwarfModel::get_dirty_dwarves() {
	QVector<Dwarf*> dwarves;
	foreach(Dwarf *d, m_dwarves) {
		if (d->pending_changes())
			dwarves.append(d);
	}
	return dwarves;
}

QModelIndex DwarfModel::findOne(const QVariant &needle, int role, int column, const QModelIndex &start_index) {
    QModelIndex ret_val;
    if (data(start_index, role) == needle) {
        ret_val = start_index;
        return ret_val;
    }
    for (int i = 0; i < rowCount(start_index); ++i) {
        ret_val = findOne(needle, role, column, index(i, column, start_index));
        if (ret_val.isValid())
            return ret_val;
    }
    return ret_val;
}

QList<QPersistentModelIndex> DwarfModel::findAll(const QVariant &needle, int role, int column, QModelIndex start_index) {
	QList<QPersistentModelIndex> ret_val;
    if (data(start_index, role) == needle)
		ret_val.append(QPersistentModelIndex(start_index));
    for (int i = 0; i < rowCount(start_index); ++i) {
		if (column == -1) {
			for (int j = 0; j < columnCount(start_index); ++j) {
				ret_val.append(findAll(needle, role, -1, index(i, j, start_index)));
			}
		} else {
			ret_val.append(findAll(needle, role, column, index(i, column, start_index)));
		}
    }
    return ret_val;
}

void DwarfModel::dwarf_group_toggled(const QString &group_name) {
	QModelIndex agg_cell = findOne(group_name, DR_GROUP_NAME);
	if (agg_cell.isValid()) {
		QModelIndex left = agg_cell;
		QModelIndex right = index(agg_cell.row(), columnCount(agg_cell.parent()) -1, agg_cell.parent());
		emit dataChanged(left, right);
	}
	foreach (Dwarf *d, m_grouped_dwarves[group_name]) {
		foreach(QModelIndex idx, findAll(d->id(), DR_ID, 0, agg_cell)) {
			QModelIndex left = idx;
			QModelIndex right = index(idx.row(), columnCount(idx.parent()) - 1, idx.parent());
			emit dataChanged(left, right);
		}
	}
}

void DwarfModel::dwarf_set_toggled(Dwarf *d) {
	// just update all cells we can find with this dwarf's id
	QList<QPersistentModelIndex> cells = findAll(d->id(), DR_ID, 0);
	foreach(QPersistentModelIndex idx, cells) {
		QModelIndex left = idx;
		QModelIndex right = index(idx.row(), columnCount(idx.parent()) - 1, idx.parent());
		emit dataChanged(left, right);
	}
	if (cells.size()) {
		QPersistentModelIndex first_idx = cells.at(0);
		QModelIndex left = first_idx.parent();
		QModelIndex right = index(left.row(), columnCount(left) - 1, left.parent());
		emit dataChanged(left, right);
	}
}