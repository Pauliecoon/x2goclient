/***************************************************************************
 *  Copyright (C) 2012-2015 by Mihai Moldovan <ionic@ionic.de>             *
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program; if not, write to the                          *
 *  Free Software Foundation, Inc.,                                        *
 *  59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.              *
 ***************************************************************************/

#ifndef PULSEMANAGER_H
#define PULSEMANAGER_H

#ifdef NDEBUG
#define NDEBUG_DEFINE
#undef NDEBUG
#endif

#include <iostream>
#include <QDir>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QHostInfo>
#include <QProcess>
#include <QApplication>
#include <QTimer>
#include <assert.h>
#include "x2gologdebug.h"
#include "x2gosettings.h"

class PulseManager: public QObject
{
    Q_OBJECT;

public:
    PulseManager ();
    ~PulseManager ();
    int get_pulse_port ();
    void set_pulse_port (int pulse_port);
    QProcess::ProcessState state ();

public slots:
    void start ();
    void relaunch ();

private:
    PulseManager (const PulseManager &other);
    void start_osx ();
    // FIXME
    void start_win ();
    // FIXME
    void start_linux ();
    void find_port ();
    bool generate_server_config ();
    bool generate_client_config ();
    void cleanup_client_dir ();
    void shutdown ();
    bool is_server_running ();

private slots:
    void on_pulse_finished (int exit_code);
    void slot_play_startup_sound ();

signals:
    void sig_pulse_server_terminated ();

private:
    QString _pulse_X2Go;
    QDir _pulse_dir;
    QProcessEnvironment _env;
    QProcess *_pulse_server;
    int _pulse_port;
    QProcess::ProcessState _state;
    QString _app_dir;
};

#ifdef NDEBUG_DEFINE
#define NDEBUG
#undef NDEBUG_DEFINE
#endif

#endif // PULSEMANAGER_H
