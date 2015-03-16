/**************************************************************************
*   Copyright (C) 2005-2015 by Oleksandr Shneyder                         *
*   o.shneyder@phoca-gmbh.de                                              *
*   Copyright (C) 2015 by Mihai Moldovan <ionic@ionic.de>                 *
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

#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

#include "unixhelper.h"
#include "ongetpass.h"

int wrap_x2go_main (int argc, char **argv) {
  return (x2goMain (argc, argv));
}

#ifdef Q_OS_UNIX
int fork_helper (int argc, char **argv) {
  /* Fork off to start helper process. */
  pid_t tmp_pid = fork ();

  /* Child. */
  if (0 == tmp_pid) {
    /* Starting unixhelper. */
    unixhelper cleanup_helper ();
    cleanup_helper.unix_cleanup ();
  }
  /* Error. */
  else if (-1 == tmp_pid) {
    std::cerr << "Unable to create a new process for the UNIX cleanup watchdog: " << std::strerror (errno) << "\n";
    std::cerr << "Terminating. Please report a bug, refer to this documentation: http://wiki.x2go.org/doku.php/wiki:bugs" << std::endl;

    std::exit (EXIT_FAILURE);
  }
  /* Parent. */
  else {
    /* Start real X2Go Client. */
    return (wrap_x2go_main (argc, argv));
  }
}
#endif /* defined (Q_OS_UNIX) */

int main (int argc, char **argv) {
#ifdef Q_OS_UNIX
  if (-1 == setsid ()) {
    std::cerr << "Unable to create a new process session: " << std::strerror (errno) << "\n";

    std::cerr << "Trying to fork." << std::endl;
    pid_t tmp_pid = fork ();

    /* Child. */
    if (0 == tmp_pid) {
        /* Trying to get a new session and become session + process group leader again. */
        if (-1 == setsid ()) {
          std::cerr << "Child was unable to create a new process session: " << std::strerror (errno) << "\n";
          std::cerr << "Terminating. Please report a bug, refer to this documentation: http://wiki.x2go.org/doku.php/wiki:bugs" << std::endl;

          std::exit (EXIT_FAILURE);
        }

        /* By now, we should be session and group leader. */
        return (fork_helper (argc, argv));
    }
    /* Error. */
    else if (-1 == tmp_pid) {
      std::cerr << "Error while forking: " << std::strerror (errno) << std::endl;
      std::cerr << "Terminating. Please report a bug, refer to this documentation: http://wiki.x2go.org/doku.php/wiki:bugs" << std::endl;

      std::exit (EXIT_FAILURE);
    }
    /* Parent. Just die here. */
    else {
      std::exit (EXIT_SUCCESS);
    }
  }
  else {
    /* setsid() worked. Starting helper and main program. */
    return (fork_helper (argc, argv));
  }
#else /* defined (Q_OS_UNIX) */
  return (wrap_x2go_main (argc, argv));
#endif /* defined (Q_OS_UNIX) */
}
