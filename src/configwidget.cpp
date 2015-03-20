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

#include "configwidget.h"
#include "onmainwindow.h"

ConfigWidget::ConfigWidget ( QString id, ONMainWindow * mw,
                             QWidget * parent, Qt::WindowFlags f )
		: QFrame ( parent,f )
{
	sessionId=id;
	mainWindow=mw;
	miniMode=mw->retMiniMode();
	embedMode = mw->getEmbedMode();
	if(embedMode)
		sessionId="embedded";
}


ConfigWidget::~ConfigWidget()
{
}
