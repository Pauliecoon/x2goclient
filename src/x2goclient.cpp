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
    return (unixhelper::unix_cleanup ());

    /* Anything here shall be unreachable. */
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
  /*
   * setsid() may succeed and we become a session and process
   * group leader, or it may fail indicating that we already
   * are a process group leader. Either way is fine.
   */
  setsid ();

  /* We should be process group leader by now. */
  return (fork_helper (argc, argv));
#else /* defined (Q_OS_UNIX) */
  return (wrap_x2go_main (argc, argv));
#endif /* defined (Q_OS_UNIX) */
}
