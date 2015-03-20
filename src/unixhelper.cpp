/***************************************************************************
 *  Copyright (C) 2015 by Mihai Moldovan <ionic@ionic.de> +49 721 14595728 *
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

/* Necessary for Q_OS_UNIX! */

#include "unixhelper.h"

#ifdef Q_OS_UNIX

#include <csignal>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <vector>
#include <cstdlib>

/* For documentation please see unixhelper.h. */

namespace unixhelper {
  void kill_pgroup (int signal) {
    if (SIGHUP == signal) {
      /* Try to kill via SIGTERM first. */
      if (0 != killpg (getpgrp (), SIGTERM)) {
        std::cerr << "WARNING: unable to send SIGTERM to process group: " << std::strerror (errno) << std::endl;
      }

      /* Grant a grace period of (at least) 10 seconds. */
      sleep (10);

      /* Don't handle any errors here, because we die anyway. */
      killpg (getpgrp (), SIGKILL);
    }
  }


  int unix_cleanup (const pid_t parent) {
    /*
     * Unblock all signals first.
     * Signal blocks are inherited, so you never you what is currently set.
     */
    sigset_t empty_set;
    if (0 != sigemptyset (&empty_set)) {
      std::cerr << "Unable to fetch empty signal set: " << std::strerror (errno) << std::endl;
      kill_pgroup (-1);

      /* Anything here shall be unreachable. */
    }

    if (0 != sigprocmask (SIG_SETMASK, &empty_set, NULL)) {
      std::cerr << "Unable to set empty signal set: " << std::strerror (errno) << std::endl;
      kill_pgroup (-1);

      /* Anything here shall be unreachable. */
    }

    std::vector<int> ignore_signals;
    ignore_signals.push_back (SIGINT);
    ignore_signals.push_back (SIGTERM);
    ignore_signals.push_back (SIGPIPE);
    ignore_signals.push_back (SIGQUIT);
    ignore_signals.push_back (SIGUSR1);
    ignore_signals.push_back (SIGUSR2);

    for (std::vector<int>::iterator it = ignore_signals.begin (); it != ignore_signals.end (); ++it) {
      struct sigaction sig_action;
      sig_action.sa_handler = SIG_IGN;
      sig_action.sa_mask = empty_set;
      sig_action.sa_flags = SA_RESTART;

      /* Set up signal handler to ignore the current signal. */
      if (0 != sigaction (*it, &sig_action, NULL)) {
        std::cerr << "Unable to ignore signal " << strsignal (*it) << ": " << std::strerror (errno) << std::endl;
        kill_pgroup (-1);

        /* Anything here shall be unreachable. */
      }
    }

    {
      struct sigaction sig_action;
      sig_action.sa_handler = kill_pgroup;
      sig_action.sa_mask = empty_set;
      sig_action.sa_flags = SA_RESTART;

      if (0 != sigaction (SIGHUP, &sig_action, NULL)) {
        std::cerr << "Unable to set up signal handler for SIGHUP: " << std::strerror (errno) << std::endl;
        kill_pgroup (-1);

        /* Anything here shall be unreachable. */
      }
    }

    /* Sleep forever... at least two seconds in each run. */
    for (;;) {
      pid_t cur_ppid = getppid ();

      /* cur_ppid should match parent, otherwise the parent died. */
      if (cur_ppid != parent) {
        kill_pgroup (SIGHUP);
      }

      sleep (2);
    }

    /*
     * Anything here shall be unreachable.
     * But make compilers happy by returning something.
     */
    return (EXIT_SUCCESS);
  }
}

#endif /* defined (Q_OS_UNIX) */
