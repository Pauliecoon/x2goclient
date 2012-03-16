/***************************************************************************
 *   Copyright (C) 2005-2012 by Oleksandr Shneyder                         *
 *   oleksandr.shneyder@obviously-nice.de                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef APPDIALOG_H
#define APPDIALOG_H

#include <QObject>
#include "ui_appdialog.h"

class QTreeWidgetItem;
class ONMainWindow;

class AppDialog: public QDialog, public Ui_AppDialog
{
    Q_OBJECT
public:
    AppDialog(ONMainWindow *parent = 0);
    ~AppDialog();
private:
    void loadApps();
    QTreeWidgetItem* initTopItem(QString text, QPixmap icon=QPixmap());
    ONMainWindow* mw;
    QTreeWidgetItem* media;
    QTreeWidgetItem* dev;
    QTreeWidgetItem* edu;
    QTreeWidgetItem* game;
    QTreeWidgetItem* graph;
    QTreeWidgetItem* net;
    QTreeWidgetItem* office;
    QTreeWidgetItem* set;
    QTreeWidgetItem* sys;
    QTreeWidgetItem* util;
    QTreeWidgetItem* other;
private slots:
    void slotSelectedChanged();
    void slotStartSelected();
    void slotDoubleClicked(QTreeWidgetItem* item);
    void slotSearchChanged(QString text);
};

#endif // APPDIALOG_H