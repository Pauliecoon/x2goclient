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
#endif // defined (NDEBUG)

PulseManager::PulseManager () : pulse_X2Go_ ("/.x2go/pulse"),
                                pulse_port_ (4713),
                                esd_port_ (4714),
                                state_ (QProcess::NotRunning),
                                pulse_server_ (NULL),
                                app_dir_ (QApplication::applicationDirPath ()),
                                server_args_ (QStringList ()),
                                server_binary_ (QString ("")),
                                server_working_dir (QString ("")),
                                pulse_version_major_ (0),
                                pulse_version_minor_ (0) {
  pulse_dir_ = QDir (QDir::homePath ());
  pulse_dir_.mkpath (pulse_dir_.absolutePath () + pulse_X2Go_ + "/tmp");
  pulse_dir_.cd (pulse_X2Go_.mid (1));

  env_ = QProcessEnvironment::systemEnvironment ();
  env_.insert ("HOME", pulse_dir_.absolutePath ());
  env_.insert ("TEMP", pulse_dir_.absolutePath () + "/tmp");
#ifdef Q_OS_WIN
  env_.insert ("USERPROFILE", pulse_dir_.absolutePath ());
  env_.insert ("USERNAME", "pulseuser");
#endif // defined (Q_OS_WIN)
}

PulseManager::~PulseManager () {
  if (pulse_server_ && is_server_running ())
    shutdown ();

  delete (pulse_server_);
}

void PulseManager::start () {
  assert (!is_server_running ());

  delete (pulse_server_);

  pulse_server_ = new QProcess (0);
  state_ = QProcess::Starting;

  // Search for a free Pulse and EsounD port.
  // Note that there is no way we could find
  // an esd port, if the pulse port detection
  // failed. Better trust your compiler to
  // optimize this statement and save some
  // cycles.
  if ((findPort (false)) && (findPort (true))) {
#ifdef Q_OS_DARWIN
    start_osx ();
#elif defined (Q_OS_WIN)
    start_win ();
#elif defined (Q_OS_LINUX)
    start_linux ();
#endif // defined (Q_OS_DARWIN)
  }
}

void PulseManager::start_generic () {
  pulse_server_->setProcessEnvironment (env_);
  pulse_server_->setWorkingDirectory (server_working_dir_);

  pulse_server_->start (server_binary_, server_args_);
  if (pulse_server_->waitForStarted ()) {
    x2goDebug << "pulse started with arguments" << server_args_ << "; waiting for finish...";
    state_ = QProcess::Running;

    connect (pulse_server_, SIGNAL (finished (int)),
             this,          SLOT (slot_on_pulse_finished (int)));

#ifdef DEBUG
    // Give PA a little time to come up.
    QTimer::singleShot (3000, this, SLOT (slot_play_startup_sound ()));
#endif // defined (DEBUG)
  }
}

void PulseManager::start_osx () {
  if (generate_server_config () && generate_client_config ()) {
    cleanup_client_dir ();

    pulse_server_->setProcessEnvironment (env_);

    QStringList args;
    args << "--exit-idle-time=-1" << "-n"
         << "-F" << pulse_dir_.absolutePath () + "/config.pa"
         << "-p"
         << QDir (app_dir_
                  + "/../Frameworks/pulse-2.0/modules").absolutePath ()
         << "--high-priority";
#ifdef DEBUG
    args << "--log-level=debug";
#endif

    pulse_server_->setWorkingDirectory (app_dir_ + "/../exe/");
    pulse_server_->start (app_dir_ + "/../exe/pulseaudio", args);

    if (pulse_server_->waitForStarted ()) {
      x2goDebug << "pulse started with" << args << "waiting for finish...";
      state_ = QProcess::Running;

      connect (pulse_server_, SIGNAL (finished (int)),
               this,          SLOT (on_pulse_finished (int)));

#ifdef DEBUG
      // Give PA a little time to come up.
      QTimer::singleShot (5000, this, SLOT (slot_play_startup_sound ()));
#endif // defined (DEBUG)
    }
  }
}

void PulseManager::find_port (bool search_esd) {
  QTcpSocket tcpSocket (0);
  bool free = false;
  std::uint16_t ret = pulse_port_;
  std::uint16_t other_port = esd_port_;

  // If the search_esd parameter is true, find a free port
  // for the PulseAudio emulation.
  if (search_esd) {
    ret = esd_port_;
    other_port = pulse_port_;
  }

  do {
    // Skip this port, if it's reserved for the counterpart.
    if (ret == other_port) {
      ++ret;
      continue;
    }

    tcpSocket.connectToHost ("127.0.0.1", ret);

    if (tcpSocket.waitForConnected (1000)) {
      tcpSocket.close ();
      free = false;
      ++ret;
    }
    else
      free = true;
  } while ((!free) && (port > 1023));

  if (!search_esd) {
    pulse_port_ = ret;
  }
  else {
    esd_port_ = ret;
  }

  return (free);
}

bool PulseManager::generate_server_config () {
  QString config_file_name = pulse_dir_.absolutePath () + "/config.pa";
  QTemporaryFile config_tmp_file (pulse_dir_.absolutePath () + "/tmp/tmpconfig");
  X2goSettings settings ("settings");
  bool disable_input = false;
  bool ret = false;

  disable_input = settings.setting ()->value ("soundnoinput",
                                              (QVariant) false).toBool ();

  if (config_tmp_file.open ()) {
    QTextStream config_tmp_file_stream (&config_tmp_file);

    config_tmp_file_stream << "load-module module-native-protocol-tcp port="
                            + QString::number (pulse_port_) << endl;
    config_tmp_file_stream << "load-module module-native-protocol-unix" << endl;
    config_tmp_file_stream << "load-module module-esound-protocol-unix" << endl;
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
  QTemporaryFile client_config_tmp_file (pulse_dir_.absolutePath ()
                                         + "/tmp/tmpconfig");
  QString client_config_file_name (pulse_dir_.absolutePath () + "/.pulse/client.conf");
  bool ret = false;

  if (client_config_tmp_file.open ()) {
    QTextStream config_tmp_file_stream (&client_config_tmp_file);

    config_tmp_file_stream << "autospawn=no" << endl;
    config_tmp_file_stream << "daemon-binary="
                           << QDir (app_dir_
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
  QDir machine_dir (pulse_dir_.absolutePath () + "/.pulse/"
                    + QHostInfo::localHostName () + "-runtime");

  if (QFile::exists (machine_dir.absolutePath () + "/pid"))
    QFile::remove (machine_dir.absolutePath () + "/pid");

  if (machine_dir.exists ())
    machine_dir.remove (machine_dir.absolutePath ());
}

void PulseManager::create_client_dir () {
  QDir machine_dir (pulse_dir_.absolutePath () + "/.pulse/"
                    + QHostInfo::localHostName () + "-runtime");

  if (!machine_dir.exists ())
    machine_dir.mkpath (machine_dir.absolutePath ());

  if (QFile::exists (machine_dir.absolutePath () + "/pid"))
    QFile::remove (machine_dir.absolutePath () + "/pid");
}

void PulseManager::slot_play_startup_sound () {
  QProcess play_file (0);

  play_file.setWorkingDirectory (pulse_server_->workingDirectory ());
  play_file.setProcessEnvironment (env_);
  play_file.start (app_dir_ + "/../exe/paplay "
                   + app_dir_ + "/../Resources/startup.wav");

  if (play_file.waitForStarted ())
    play_file.waitForFinished ();
}

void PulseManager::on_pulse_finished (int exit_code) {
  if (!exit_code)
    x2goDebug << "Warning! Pulseaudio's exit code is non-zero.";

  QByteArray ba = pulse_server_->readAllStandardOutput ();
  char *data = ba.data ();
  std::cout << data;
  ba = pulse_server_->readAllStandardError ();
  data = ba.data ();
  std::cout << data;

  // Clean up
  QDir work_dir (app_dir_);

#ifdef Q_OS_WIN
  work_dir.remove (pulse_dir_.absolutePath ()
                   + "/pulse-pulseuser/pid");
  work_dir.rmdir (pulse_dir_.absolutePath ()
                  + "/pulse-pulseuser");
#endif // defined (Q_OS_WIN)

  work_dir.rmdir (pulse_dir_.absolutePath ());

  state_ = QProcess::NotRunning;
  emit (sig_pulse_server_terminated ());
}

bool PulseManager::is_server_running () {
  if (pulse_server_)
    return (pulse_server_->state () == QProcess::Running);
  else
    return (false);
}

void PulseManager::shutdown () {
  QEventLoop loop;

  connect (this,  SIGNAL (sig_pulse_server_terminated ()),
           &loop, SLOT (quit ()));

  // Console applications without an event loop can only be terminated
  // by QProcess::kill() on Windows (unless they handle WM_CLOSE, which
  // PA obviously doesn't.)
#ifdef Q_OS_WIN
  pulse_server_->kill ();
#else // defined (Q_OS_WIN)
  pulse_server_->terminate ();
#endif // defined (Q_OS_WIN)

  loop.exec ();
}

std::uint16_t PulseManager::get_pulse_port () {
  return (pulse_port_);
}

std::uint16_t PulseManager::get_esd_port () {
  return (esd_port_);
}

void PulseManager::set_pulse_port (std::uint16_t pulse_port) {
  pulse_port_ = pulse_port;
}

void PulseManager::set_esd_port (std::uint16_t esd_port) {
  esd_port_ = esd_port;
}

void PulseManager::restart () {
  if (pulse_server_ && is_server_running ())
    shutdown ();

  x2goDebug << "restarting pulse";
  start ();
}

QProcess::ProcessState PulseManager::state () {
  return (state_);
}

#ifdef DEBUG_UNDEF
#undef DEBUG
#undef DEBUG_UNDEF
#endif // defined (DEBUG_UNDEF)
