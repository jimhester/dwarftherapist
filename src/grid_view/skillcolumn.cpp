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

#include "skillcolumn.h"
#include "gamedatareader.h"
#include "viewcolumnset.h"
#include "columntypes.h"
#include "dwarfmodel.h"
#include "dwarf.h"

SkillColumn::SkillColumn(const QString &title, const int &skill_id, ViewColumnSet *set, QObject *parent) 
	: ViewColumn(title, CT_SKILL, set, parent)
	, m_skill_id(skill_id)
{}

SkillColumn::SkillColumn(QSettings &s, ViewColumnSet *set, QObject *parent) 
    : ViewColumn(s, set, parent)
    , m_skill_id(s.value("skill_id", -1).toInt())
{}

SkillColumn::SkillColumn(const SkillColumn &to_copy) 
    : ViewColumn(to_copy)
    , m_skill_id(to_copy.m_skill_id)
{}

QStandardItem *SkillColumn::build_cell(Dwarf *d) {
	GameDataReader *gdr = GameDataReader::ptr();
	QStandardItem *item = init_cell(d);

	item->setData(CT_SKILL, DwarfModel::DR_COL_TYPE);
	short rating = d->get_rating_by_skill(m_skill_id);
	item->setData(rating, DwarfModel::DR_RATING);
	uint exp = 0;
	if (rating >= 0)
		exp = d->get_skill(m_skill_id).actual_exp();
	item->setData(exp, DwarfModel::DR_SORT_VALUE);

	QString skill_str;
	if (m_skill_id != -1 && rating > -1) {
		QString adjusted_rating = QString::number(rating);
		if (rating > 15)
			adjusted_rating = QString("15 +%1").arg(rating - 15);
		skill_str = tr("%1 %2<br/>[RAW LEVEL: <b><font color=blue>%3</font></b>]<br/><b>Experience:</b><br/>%4")
			.arg(gdr->get_skill_level_name(rating))
			.arg(gdr->get_skill_name(m_skill_id))
			.arg(adjusted_rating)
			.arg(d->get_skill(m_skill_id).exp_summary());
	} else {
		// either the skill isn't a valid id, or they have 0 experience in it
		skill_str = "0 experience";
	}
	item->setToolTip(QString("<h3>%1</h3>%2<h4>%3</h4>").arg(m_title).arg(skill_str).arg(d->nice_name()));
	return item;
}

QStandardItem *SkillColumn::build_aggregate(const QString &, const QVector<Dwarf*> &) {
	QStandardItem *item = new QStandardItem;
	QColor bg;
	if (m_override_set_colors)
		bg = m_bg_color;
	else
		bg = m_set->bg_color();
	item->setData(bg, Qt::BackgroundColorRole);
	item->setData(bg, DwarfModel::DR_DEFAULT_BG_COLOR);
	return item;
}