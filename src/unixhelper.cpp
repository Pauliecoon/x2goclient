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
#include <cstdlib>


void unixhelper::kill_pgroup (int signal) {
  if (SIGHUP == signal) {
    /* Try to kill via SIGTERM first. */
    if (0 != killpg (getpgrp (), SIGTERM)) {
      std::cerr << "WARNING: unable to send SIGTERM to process group: " << std::strerror (errno) << std::endl;
    }

    /* Grant a grace period of (at least) 5 seconds. */
    sleep (5);

    /* Don't handle any errors here, because we die anyway. */
    killpg (getpgrp (), SIGKILL);
  }
}


int unixhelper::unix_cleanup () {
  if (SIG_ERR == std::signal (SIGTERM, SIG_IGN)) {
    std::cerr << "Unable to ignore SIGTERM: " << std::strerror (errno) << std::endl;
    std::exit (1);
  }

  std::signal (SIGHUP, kill_pgroup);

  /* Sleep forever... at least one second in each run. */
  for (;;) {
    sleep (1);
  }
}

#endif /* defined (Q_OS_UNIX) */
