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

#include "compat.h"

#ifdef Q_OS_DARWIN
/*
 * strndup() is not available on 10.6 and below, define a compat version here.
 * Shameless copy from libiberty.
 */
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1070
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

inline char *strndup (const char *s, size_t n) {
  char *result;
  size_t len = strlen (s);

  if (n < len) {
    len = n;
  }

  result = (char *) malloc (len + 1);
  if (!result) {
    return (0);
  }

  result[len] = '\0';
  return ((char *) memcpy (result, s, len));
}
#endif /* MAC_OS_X_VERSION_MIN_REQUIRED */
#endif /* defined (Q_OS_DARWIN) */
