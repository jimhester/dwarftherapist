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
        m_d->refresh_data();
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

    foreach(QObject *obj, m_cleanup_list) {
        obj->deleteLater();
    }
    m_cleanup_list.clear();

    // SKILLS TABLE
    QVector<Skill> *skills = d->get_skills();
    QTableWidget *tw = new QTableWidget(skills->size(), 3, this);
    ui->vbox_main->addWidget(tw, 10);
    m_cleanup_list << tw;
    tw->setEditTriggers(QTableWidget::NoEditTriggers);
    tw->setGridStyle(Qt::NoPen);
    tw->setAlternatingRowColors(true);
    tw->setHorizontalHeaderLabels(QStringList() << "Skill" << "Level" << "Progress");
    tw->verticalHeader()->hide();
    tw->horizontalHeader()->setStretchLastSection(true);
    tw->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    tw->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    tw->setSortingEnabled(false); // no sorting while we're inserting
    for (int row = 0; row < skills->size(); ++row) {
        tw->setRowHeight(row, 18);
        Skill s = skills->at(row);
        QTableWidgetItem *text = new QTableWidgetItem(s.name());
        QTableWidgetItem *level = new QTableWidgetItem;
        level->setData(0, d->get_rating_by_skill(s.id()));


        QProgressBar *pb = new QProgressBar(tw);
        pb->setRange(s.exp_for_current_level(), s.exp_for_next_level());
        pb->setValue(s.actual_exp());
        pb->setToolTip(s.exp_summary());
        pb->setDisabled(true);// this is to keep them from animating and looking all goofy

        tw->setItem(row, 0, text);
        tw->setItem(row, 1, level);
        tw->setCellWidget(row, 2, pb);
    }
    tw->setSortingEnabled(true); // no sorting while we're inserting
    tw->sortItems(1, Qt::DescendingOrder); // order by level descending


    // TRAITS TABLE
    QHash<int, short> traits = d->traits();
    QTableWidget *tw_traits = new QTableWidget(this);
    ui->vbox_main->addWidget(tw_traits, 10);
    m_cleanup_list << tw_traits;
    tw_traits->setColumnCount(3);
    tw_traits->setEditTriggers(QTableWidget::NoEditTriggers);
	tw_traits->setWordWrap(true);
    tw_traits->setShowGrid(false);
    tw_traits->setGridStyle(Qt::NoPen);
    tw_traits->setAlternatingRowColors(true);
    tw_traits->setHorizontalHeaderLabels(QStringList() << "Trait" << "Raw" << "Message");
    tw_traits->verticalHeader()->hide();
    tw_traits->horizontalHeader()->setStretchLastSection(true);
    tw_traits->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    tw_traits->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    tw_traits->setSortingEnabled(false);
    for (int row = 0; row < traits.size(); ++row) {
        short val = traits[row];
        if (val == -1)
            continue;
        tw_traits->insertRow(0);
        tw_traits->setRowHeight(0, 14);
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
        tw_traits->setItem(0, 0, trait_name);
        tw_traits->setItem(0, 1, trait_score);
        tw_traits->setItem(0, 2, trait_msg);
    }
    tw_traits->setSortingEnabled(true);
    tw_traits->sortItems(1, Qt::DescendingOrder);

    //Likes Table
    const QVector<QString> *likes = d->likes();
    int maxSize = 0;
    for(int i = 0;i<3;i++){
        if(likes[i].size() > maxSize)
            maxSize = likes[i].size();
    }
    QTableWidget *tw_likes = new QTableWidget(maxSize,3,this);
    ui->vbox_main->addWidget(tw_likes, 10);
    m_cleanup_list << tw_likes;
    for(int i = 0;i<tw_likes->rowCount();i++){
        tw_likes->setRowHeight(i,14);
    }
    tw_likes->setEditTriggers(QTableWidget::NoEditTriggers);
	tw_likes->setWordWrap(true);
    tw_likes->setShowGrid(false);
    tw_likes->setGridStyle(Qt::NoPen);
    tw_likes->setAlternatingRowColors(true);
    tw_likes->setHorizontalHeaderLabels(QStringList() << "Material Likes" << "Item Likes" << "Food Likes");
    tw_likes->verticalHeader()->hide();
    tw_likes->horizontalHeader()->setStretchLastSection(true);
    tw_likes->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    tw_likes->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    tw_likes->setSortingEnabled(false);
    for(int j = 0;j<3;j++){
        for(int i = 0;i<likes[j].size();i++){
            QString currentLike = likes[j][i];
            currentLike[0] = currentLike[0].toTitleCase();
            QTableWidgetItem *like = new QTableWidgetItem(currentLike);
            tw_likes->setItem(i,j,like);
        }
    }
}