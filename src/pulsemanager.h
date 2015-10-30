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
#endif // defined (NDEBUG)

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
#include <cstdint>
#include "x2gologdebug.h"
#include "x2gosettings.h"

class PulseManager: public QObject {
  Q_OBJECT;

  public:
    PulseManager ();
    ~PulseManager ();
    std::uint16_t get_pulse_port ();
    std::uint16_t get_esd_port ();
    void set_pulse_port (std::uint16_t pulse_port);
    void set_esd_port (std::uint16_t esd_port);
    QProcess::ProcessState state ();

  public slots:
    void start ();
    void restart ();

  private:
    PulseManager (const PulseManager &other);
    void start_osx ();
    // FIXME
    void start_win ();
    // FIXME
    void start_linux ();
    void find_port (bool search_esd = false);
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
    QString pulse_X2Go_;
    QDir pulse_dir_;
    QProcessEnvironment env_;
    QProcess *pulse_server_;
    std::uint16_t pulse_port_;
    std::uint16_t esd_port_;
    QProcess::ProcessState state_;
    QString app_dir_;
    std::uint32_t pulse_version_major_;
    std::uint32_t pulse_version_minor_;
};

#ifdef NDEBUG_DEFINE
#define NDEBUG
#undef NDEBUG_DEFINE
#endif // defined (NDEBUG_DEFINE)

#endif // PULSEMANAGER_H
