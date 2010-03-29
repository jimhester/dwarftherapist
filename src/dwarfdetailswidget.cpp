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

#include "dwarfdetailswidget.h"
#include "ui_dwarfdetailswidget.h"
#include "dwarftherapist.h"
#include "gamedatareader.h"
#include "dwarf.h"
#include "trait.h"

DwarfDetailsWidget::DwarfDetailsWidget(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags)
    , ui(new Ui::DwarfDetailsWidget)
{
    m_d = 0;
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, SIGNAL(timeout()),this,SLOT(move_dwarf()));
	ui->setupUi(this);

    ui->tw_traits->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui->tw_traits->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    ui->tw_skills->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui->tw_skills->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
}
void DwarfDetailsWidget::set_refresh(){
    if(m_refreshTimer->timerId() == -1){
        m_refreshTimer->start(200);
    }
    else{
        m_refreshTimer->stop();
    }
}
void DwarfDetailsWidget::stop_refresh(){
	ui->checkBox->setCheckState(Qt::Unchecked);
}
void DwarfDetailsWidget::move_dwarf(){
        m_d->move_view_to();
}
void DwarfDetailsWidget::show_dwarf(Dwarf *d) {
    m_d = d;
	// Draw the name/profession text labels...
    ui->lbl_dwarf_name->setText(d->nice_name());
    ui->lbl_translated_name->setText(QString("(%1)").arg(d->translated_name()));
    ui->lbl_profession->setText(d->profession());
    ui->lbl_current_job->setText(QString("%1 %2").arg(d->current_job_id()).arg(d->current_job()));
	ui->lbl_artifact->setText(QString("Creator of %1").arg(d->artifact_name()));
	if(d->artifact_name().length() > 0)
		ui->lbl_artifact->show();
	else
		ui->lbl_artifact->hide();
	
	QMap<QProgressBar*, int> things;
    int str = d->strength();
    int agi = d->agility();
    int tou = d->toughness();
    things.insert(ui->pb_strength, str);
    things.insert(ui->pb_agility, agi);
    things.insert(ui->pb_toughness, tou);

    foreach(QProgressBar *pb, things.uniqueKeys()) {
        int stat = things.value(pb);
        pb->setMaximum(stat > 5 ? stat : 5);
        pb->setValue(stat);
    }
    GameDataReader *gdr = GameDataReader::ptr();
    int xp_for_current = gdr->get_xp_for_next_attribute_level(str + agi + tou - 1);
    int xp_for_next = gdr->get_xp_for_next_attribute_level(str + agi + tou);
    if (xp_for_current && xp_for_next) {// is 0 when we don't know when the next level is...
        ui->pb_next_attribute_gain->setRange(xp_for_current, xp_for_next);
        ui->pb_next_attribute_gain->setValue(d->total_xp());
        ui->pb_next_attribute_gain->setToolTip(QString("%L1xp / %L2xp").arg(d->total_xp()).arg(xp_for_next));
    } else {
        ui->pb_next_attribute_gain->setValue(-1); //turn it off
        ui->pb_next_attribute_gain->setToolTip("Off the charts! (I have no idea when you'll get the next gain)");
    }


    Dwarf::DWARF_HAPPINESS happiness = d->get_happiness();
    ui->lbl_happiness->setText(QString("<b>%1</b> (%2)").arg(d->happiness_name(happiness)).arg(d->get_raw_happiness()));
    QColor color = DT->user_settings()->value(
        QString("options/colors/happiness/%1").arg(static_cast<int>(happiness))).value<QColor>();
    QString style_sheet_color = QString("#%1%2%3")
        .arg(color.red(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(), 2, 16, QChar('0'));
    ui->lbl_happiness->setStyleSheet(QString("background-color: %1;").arg(style_sheet_color));


    // SKILLS TABLE
    QVector<Skill> *skills = d->get_skills();
	ui->tw_skills->setSortingEnabled(false); // no sorting while we're inserting
	ui->tw_skills->setUpdatesEnabled(false);
	int cur_rows = ui->tw_skills->rowCount();
	ui->tw_skills->setRowCount(skills->size());
	for (int row = cur_rows; row < ui->tw_skills->rowCount(); ++row) {
		ui->tw_skills->setRowHeight(row, 18);
        QTableWidgetItem *text = new QTableWidgetItem;
        QTableWidgetItem *level = new QTableWidgetItem;

        QProgressBar *pb = new QProgressBar(ui->tw_skills);
        pb->setDisabled(true);// this is to keep them from animating and looking all goofy
		pb->setGeometry(ui->tw_skills->visualItemRect(ui->tw_skills->item(row,2)));

        ui->tw_skills->setItem(row, 0, text);
        ui->tw_skills->setItem(row, 1, level);
        ui->tw_skills->setCellWidget(row, 2, pb);
	}

    for (int row = 0; row < skills->size(); ++row) {
        Skill s = skills->at(row);
		ui->tw_skills->item(row,0)->setText(s.name());
		ui->tw_skills->item(row,1)->setData(0, d->get_rating_by_skill(s.id()));

		QProgressBar *pb = static_cast<QProgressBar *>(ui->tw_skills->cellWidget(row,2));
        pb->setRange(s.exp_for_current_level(), s.exp_for_next_level());
        pb->setValue(s.actual_exp());
        pb->setToolTip(s.exp_summary());

    }
	ui->tw_skills->setUpdatesEnabled(true);
    ui->tw_skills->setSortingEnabled(true); // no sorting while we're inserting
    ui->tw_skills->sortItems(1, Qt::DescendingOrder); // order by level descending


    // TRAITS TABLE
    QHash<int, short> traits = d->traits();

	ui->tw_traits->clearContents();
	ui->tw_traits->setRowCount(0);
	ui->tw_traits->setSortingEnabled(false);
    for (int row = 0; row < traits.size(); ++row) {
        short val = traits[row];
        if (val == -1)
            continue;
		ui->tw_traits->insertRow(0);
		ui->tw_traits->setRowHeight(0,16);
        Trait *t = gdr->get_trait(row);
        QTableWidgetItem *trait_name = new QTableWidgetItem(t->name);
        QTableWidgetItem *trait_score = new QTableWidgetItem;
        trait_score->setData(0, val);

        int deviation = abs(50 - val);
        if (deviation >= 41) {
            trait_score->setBackground(QColor(0, 0, 128, 255));
            trait_score->setForeground(QColor(255, 255, 255, 255));
        } else if (deviation >= 25) {
            trait_score->setBackground(QColor(220, 220, 255, 255));
            trait_score->setForeground(QColor(0, 0, 128, 255));
        }

		QString lvl_msg = t->level_message(val);
        QTableWidgetItem *trait_msg = new QTableWidgetItem(lvl_msg);
		trait_msg->setToolTip(lvl_msg);
        ui->tw_traits->setItem(0, 0, trait_name);
        ui->tw_traits->setItem(0, 1, trait_score);
        ui->tw_traits->setItem(0, 2, trait_msg);
    }
    ui->tw_traits->setSortingEnabled(true);
    ui->tw_traits->sortItems(1, Qt::DescendingOrder);

    //Likes Table
    const QVector<QString> *likes = d->likes();
 
	ui->tw_likes_mat->setRowCount(likes[0].size());
	ui->tw_likes_item->setRowCount(likes[1].size());
	ui->tw_likes_food->setRowCount(likes[2].size());

	for(int j = 0;j<3;j++){
        for(int i = 0;i<likes[j].size();i++){
            QString currentLike = likes[j][i];
            currentLike[0] = currentLike[0].toTitleCase();
            QTableWidgetItem *like = new QTableWidgetItem(currentLike);
			switch(j) {
				case 0:
					ui->tw_likes_mat->setItem(i,0,like);
					break;
				case 1:
					ui->tw_likes_item->setItem(i,0,like);
					break;
				case 2:
					ui->tw_likes_food->setItem(i,0,like);
			}
        }
    }
}