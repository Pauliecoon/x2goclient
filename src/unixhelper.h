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


#ifndef UNIXHELPER_H
#define UNIXHELPER_H

#include <QtCore/qglobal.h>
#include <unistd.h>

#ifdef Q_OS_UNIX

namespace unixhelper {
  /*
   * Unblocks all signals and installs a signal handler for SIGHUP,
   * which calls kill_pgroup().
   *
   * Should signal unblocking or installing the signal handler fail,
   * an emergency exit is performed and
   * the whole process group killed.
   *
   * Loops indefinitely afterwards.
   *
   * In this loop, the current parent PID is polled and compared against
   * the original value passed via parameter parent.
   * Should they mismatch, the parent died and kill_pgroup() is called.
   */
  int unix_cleanup (const pid_t parent);

  /*
   * Kills the whole process group.
   * First, SIGTERM is sent to the group.
   * A 10 seconds grace period is granted to make sure
   * processes exit cleanly on their own.
   * Lastly, SIGKILL is sent to the group -- which also
   * implies the demise of this program.
   *
   * signal may be any of:
   *   * -1       to indicate an error leading to emergency termination
   *   * SIGHUP   as the standard signal that is sent when the
   *              group leader dies under specific circumstances
   *              (we cannot rely that this always happens, though,
   *               so a polling solution is needed, see unix_cleanup().)
   */
  void kill_pgroup (int signal);
}

#endif /* defined (Q_OS_UNIX) */

#endif /* !defined (UNIXHELPER_H) */
