/**************************************************************************
*   Copyright (C) 2005-2015 by Oleksandr Shneyder                         *
*   o.shneyder@phoca-gmbh.de                                              *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
***************************************************************************/
#include "appdialog.h"
#include "onmainwindow.h"

AppDialog::AppDialog(ONMainWindow* parent):QDialog(parent)
{
    setupUi(this);
    mw=parent;

    media=0;
    dev=0;
    edu=0;
    game=0;
    graph=0;
    net=0;
    office=0;
    set=0;
    sys=0;
    util=0;
    other=0;
    startButton->setEnabled(false);

    loadApps();
}

AppDialog::~AppDialog()
{

}

void AppDialog::slotSearchChanged(QString text)
{
    QTreeWidgetItemIterator it(treeWidget);
    while (*it)
    {
        QString exec=(*it)->data(0,Qt::UserRole).toString();
        QString comment=(*it)->data(0,Qt::UserRole+1).toString();
        QString name=(*it)->text(0);
        if ((*it)->childCount()==0)
        {
            if (text.length()<2)
            {
                (*it)->setHidden(false);
                (*it)->setSelected(false);
            }
            else
            {
                if (exec.indexOf(text, 0,Qt::CaseInsensitive)!= -1 ||
                        comment.indexOf(text, 0,Qt::CaseInsensitive)!= -1 ||
                        name.indexOf(text, 0,Qt::CaseInsensitive)!= -1 )
                {
                    treeWidget->clearSelection();
                    (*it)->setSelected(true);
                    (*it)->setHidden(false);
                    treeWidget->scrollToItem((*it));
                }
                else
                {
                    (*it)->setHidden(true);
                    (*it)->setSelected(false);
                }
            }
        }
        ++it;
    }
}

QTreeWidgetItem* AppDialog::initTopItem(QString text, QPixmap icon)
{
    QTreeWidgetItem* item;
    item=new QTreeWidgetItem(treeWidget);
    item->setText(0,text);
    item->setFlags(Qt::ItemIsEnabled);
    item->setIcon(0,icon);
    return item;
}

void AppDialog::loadApps()
{
    QTreeWidgetItem* parent;
    foreach (Application app, mw->getApplications())
    {
        switch (app.category)
        {
        case Application::MULTIMEDIA:
            if (!media)
                media=initTopItem(tr("Multimedia"), QPixmap(":/img/icons/22x22/applications-multimedia.png"));
            parent=media;
            break;
        case Application::DEVELOPMENT:
            if (!dev)
                dev=initTopItem(tr("Development"), QPixmap(":/img/icons/22x22/applications-development.png"));
            parent=dev;
            break;
        case Application::EDUCATION:
            if (!edu)
                edu=initTopItem(tr("Education"), QPixmap(":/img/icons/22x22/applications-education.png"));
            parent=edu;
            break;
        case Application::GAME:
            if (!game)
                game=initTopItem(tr("Game"), QPixmap(":/img/icons/22x22/applications-games.png"));
            parent=game;
            break;
        case Application::GRAPHICS:
            if (!graph)
                graph=initTopItem(tr("Graphics"), QPixmap(":/img/icons/22x22/applications-graphics.png"));
            parent=graph;
            break;
        case Application::NETWORK:
            if (!net)
                net=initTopItem(tr("Network"), QPixmap(":/img/icons/22x22/applications-internet.png"));
            parent=net;
            break;
        case Application::OFFICE:
            if (!office)
                office=initTopItem(tr("Office"), QPixmap(":/img/icons/22x22/applications-office.png"));
            parent=office;
            break;
        case Application::SETTINGS:
            if (!set)
                set=initTopItem(tr("Settings"), QPixmap(":/img/icons/22x22/preferences-system.png"));
            parent=set;
            break;
        case Application::SYSTEM:
            if (!sys)
                sys=initTopItem(tr("System"), QPixmap(":/img/icons/22x22/applications-system.png"));
            parent=sys;
            break;
        case Application::UTILITY:
            if (!util)
                util=initTopItem(tr("Utility"), QPixmap(":/img/icons/22x22/applications-utilities.png"));
            parent=util;
            break;
        case Application::OTHER:
            if (!other)
                other=initTopItem(tr("Other"), QPixmap(":/img/icons/22x22/applications-other.png"));
            parent=other;
            break;
        default:
            break;
        }

        if (parent) {
            QTreeWidgetItem* it;
            if (app.category==Application::TOP)
                it=new QTreeWidgetItem(treeWidget);
            else
                it=new QTreeWidgetItem(parent);
            it->setText(0, app.name);
            it->setToolTip(0,app.comment);
            it->setIcon(0,app.icon);
            it->setData(0, Qt::UserRole, app.exec);
            it->setData(0, Qt::UserRole+1, app.comment);
        }
    }
    treeWidget->sortItems(0,Qt::AscendingOrder);
}

void AppDialog::slotSelectedChanged()
{
    startButton->setEnabled(false);
    if (treeWidget->selectedItems().count())
    {
        startButton->setEnabled(true);
    }
}

void AppDialog::slotDoubleClicked(QTreeWidgetItem* item)
{
    QString exec=item->data(0,Qt::UserRole).toString();
    if (exec.length()>0)
        mw->runApplication(exec);
}

void AppDialog::slotStartSelected()
{
    if (treeWidget->selectedItems().count()>0)
    {
        QString exec=treeWidget->selectedItems()[0]->data(0,Qt::UserRole).toString();
        if (exec.length()>0)
            mw->runApplication(exec);
    }
}
