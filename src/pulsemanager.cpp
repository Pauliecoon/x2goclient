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

#include "pulsemanager.h"

#ifndef DEBUG
#define DEBUG_UNDEF
#define DEBUG
#endif

PulseManager::PulseManager () {
    _pulse_X2Go = "/.x2go/pulse";

    _pulse_dir = QDir (QDir::homePath ());
    _pulse_dir.mkpath (_pulse_dir.absolutePath () + _pulse_X2Go + "/tmp");
    _pulse_dir.cd (_pulse_X2Go.mid (1));

    _env = QProcessEnvironment::systemEnvironment ();
    _env.insert ("HOME", _pulse_dir.absolutePath ());
    _env.insert ("TEMP", _pulse_dir.absolutePath () + "/tmp");

    _pulse_port = 4713;

    _state = QProcess::NotRunning;

    _pulse_server = 0;

    _app_dir = QApplication::applicationDirPath ();
}

PulseManager::~PulseManager () {
    if (_pulse_server && is_server_running ())
        shutdown ();

    delete (_pulse_server);
}

void PulseManager::start () {
    assert (!is_server_running ());

    delete (_pulse_server);

    _pulse_server = new QProcess (0);
    _state = QProcess::Starting;

#ifdef Q_OS_DARWIN
    start_osx ();
#elif defined (Q_OS_WIN)
    start_win ();
#elif defined (Q_OS_LINUX)
    start_linux ();
#endif
}

void PulseManager::start_osx () {
    find_port ();

    if (generate_server_config () && generate_client_config ()) {
        cleanup_client_dir ();

        _pulse_server->setProcessEnvironment (_env);

        QStringList args;
        args << "--exit-idle-time=-1" << "-n"
             << "-F" << _pulse_dir.absolutePath () + "/config.pa"
             << "-p"
             << QDir (_app_dir
                      + "/../Frameworks/pulse-2.0/modules").absolutePath ()
             << "--high-priority";
#ifdef DEBUG
        args << "--log-level=debug";
#endif

        _pulse_server->setWorkingDirectory (_app_dir + "/../exe/");
        _pulse_server->start (_app_dir + "/../exe/pulseaudio", args);

        if (_pulse_server->waitForStarted ()) {
            x2goDebug << "pulse started with" << args << "waiting for finish...";
            _state = QProcess::Running;

            connect (_pulse_server, SIGNAL (finished (int)),
                     this,          SLOT (on_pulse_finished (int)));

#ifdef DEBUG
            // Give PA a little time to come up.
            QTimer::singleShot (5000, this, SLOT (slot_play_startup_sound ()));
#endif
        }
    }
}

void PulseManager::find_port () {
    QTcpSocket tcpSocket (0);
    bool free = false;

    do {
        tcpSocket.connectToHost ("127.0.0.1", _pulse_port);

        if (tcpSocket.waitForConnected (1000)) {
            tcpSocket.close ();
            free = false;
            _pulse_port++;
        }
        else
            free = true;
    } while (!free);
}

bool PulseManager::generate_server_config () {
    QString config_file_name = _pulse_dir.absolutePath () + "/config.pa";
    QTemporaryFile config_tmp_file (_pulse_dir.absolutePath () + "/tmp/tmpconfig");
    X2goSettings settings ("settings");
    bool disable_input = false;
    bool ret = false;

    disable_input = settings.setting ()->value ("soundnoinput",
                                                (QVariant) false).toBool ();

    if (config_tmp_file.open ()) {
        QTextStream config_tmp_file_stream (&config_tmp_file);

        config_tmp_file_stream << "load-module module-native-protocol-tcp port=" +
                               QString::number (_pulse_port) << endl;
        config_tmp_file_stream << "load-module module-native-protocol-unix" << endl;
        config_tmp_file_stream << "load-module module-coreaudio-detect";

        if (disable_input)
            config_tmp_file_stream << " record=0";

        config_tmp_file_stream << endl;

        QFile config_file (config_file_name);
        if (QFile::exists (config_file_name))
            QFile::remove (config_file_name);

        config_tmp_file.copy (config_file_name);
        config_tmp_file.remove ();

        ret = true;
    }

    return (ret);
}

bool PulseManager::generate_client_config () {
    QTemporaryFile client_config_tmp_file (_pulse_dir.absolutePath ()
                                           + "/tmp/tmpconfig");
    QString client_config_file_name (_pulse_dir.absolutePath () + "/.pulse/client.conf");
    bool ret = false;

    if (client_config_tmp_file.open ()) {
        QTextStream config_tmp_file_stream (&client_config_tmp_file);

        config_tmp_file_stream << "autospawn=no" << endl;
        config_tmp_file_stream << "daemon-binary="
                               << QDir (_app_dir
                                        + "/../exe/pulseaudio").absolutePath ()
                               << endl;

        if (QFile::exists (client_config_file_name))
            QFile::remove (client_config_file_name);

        client_config_tmp_file.copy (client_config_file_name);
        client_config_tmp_file.remove ();

        ret = true;
    }

    return (ret);
}

void PulseManager::cleanup_client_dir () {
    // PA expects $HOME/.pulse/$HOST-runtime to be a symbolic link
    // and will fail, if it's just a plain directory on Mac OS X.
    // Delete it first.
    QDir machine_dir (_pulse_dir.absolutePath () + "/.pulse/"
                      + QHostInfo::localHostName () + "-runtime");

    if (QFile::exists (machine_dir.absolutePath () + "/pid"))
        QFile::remove (machine_dir.absolutePath () + "/pid");

    if (machine_dir.exists ())
        machine_dir.remove (machine_dir.absolutePath ());
}

void PulseManager::slot_play_startup_sound () {
    QProcess play_file (0);

    play_file.setWorkingDirectory (_pulse_server->workingDirectory ());
    play_file.setProcessEnvironment (_env);
    play_file.start (_app_dir + "/../exe/paplay "
                     + _app_dir + "/../Resources/startup.wav");

    if (play_file.waitForStarted ())
        play_file.waitForFinished ();
}

void PulseManager::on_pulse_finished (int exit_code) {
    if (!exit_code)
        x2goDebug << "Warning! Pulseaudio's exit code is non-zero.";

    QByteArray ba = _pulse_server->readAllStandardOutput ();
    char *data = ba.data ();
    std::cout << data;
    ba = _pulse_server->readAllStandardError ();
    data = ba.data ();
    std::cout << data;

    _state = QProcess::NotRunning;
    emit (sig_pulse_server_terminated ());
}

bool PulseManager::is_server_running () {
    if (_pulse_server)
        return (_pulse_server->state () == QProcess::Running);
    else
      return (false);
}

void PulseManager::shutdown () {
    QEventLoop loop;

    connect (this,  SIGNAL (sig_pulse_server_terminated ()),
             &loop, SLOT (quit ()));

    _pulse_server->terminate ();

    loop.exec ();
}

int PulseManager::get_pulse_port () {
    return (_pulse_port);
}

void PulseManager::set_pulse_port (int pulse_port) {
    _pulse_port = pulse_port;
}

void PulseManager::relaunch () {
    if (_pulse_server && is_server_running ())
        shutdown ();

    x2goDebug << "restarting pulse";
    start ();
}

QProcess::ProcessState PulseManager::state () {
    return (_state);
}

#ifdef DEBUG_UNDEF
#undef DEBUG
#undef DEBUG_UNDEF
#endif
