/* guestfish - guest filesystem shell
 * Copyright (C) 2009-2014 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include <c-ctype.h>

#include "rl.h"

static char program_name[] = "fish";

int
hexdigit (char d)
{
  switch (d) {
  case '0'...'9': return d - '0';
  case 'a'...'f': return d - 'a' + 10;
  case 'A'...'F': return d - 'A' + 10;
  default: return -1;
  }
}

// un-backslash quote for readline
char *
debsquote_filename (char *str)
{
  char *p = calloc(strlen(str) + 1, 1);
  strcpy(p, str);
  char *start = p;

  for (; *p; p++) {
    if (*p == '\\') {
      int m = 1, c;

      switch (p[1]) {
      case '\\': break;
      case 'a': *p = '\a'; break;
      case 'b': *p = '\b'; break;
      case 'f': *p = '\f'; break;
      case 'n': *p = '\n'; break;
      case 'r': *p = '\r'; break;
      case 't': *p = '\t'; break;
      case 'v': *p = '\v'; break;
      case '"': *p = '"'; break;
      case '\'': *p = '\''; break;
      case '?': *p = '?'; break;
      case ' ': *p = ' '; break;

      case '0'...'7':           /* octal escape - always 3 digits */
        m = 3;
        if (p[2] >= '0' && p[2] <= '7' &&
            p[3] >= '0' && p[3] <= '7') {
          c = (p[1] - '0') * 0100 + (p[2] - '0') * 010 + (p[3] - '0');
          if (c < 1 || c > 255)
            goto error;
          *p = c;
        }
        else
          goto error;
        break;

      case 'x':                 /* hex escape - always 2 digits */
        m = 3;
        if (c_isxdigit (p[2]) && c_isxdigit (p[3])) {
          c = hexdigit (p[2]) * 0x10 + hexdigit (p[3]);
          if (c < 1 || c > 255)
            goto error;
          *p = c;
        }
        else
          goto error;
        break;

      default:
      error:
        fprintf (stderr, ("%s: invalid escape sequence in string (starting at offset %d)\n"),
                 program_name, (int) (p - start));
        return -1;
      }
      memmove (p+1, p+1+m, strlen (p+1+m) + 1);
    }
  }

  return start;
}

// backslash quote
char *
bsquote_filename (char *p)
{
  char *start = p;
  // four times original length - if all chars are unprintable
  // new string would be \xXY\xWZ
  char *n = calloc(strlen(p) * 4 + 1, 1);
  char *nstart = n;
  if (strlen(p) == 0) {
    n[0] = '\0';
    return n;
  }

  for (; *p; p++, n++) {
      int m = 1;

      switch (*p) {
      case '\\': break;
      case '\a': *(n++) = '\\'; *n = 'a'; break;
      case '\b': *(n++) = '\\'; *n = 'b'; break;
      case '\f': *(n++) = '\\'; *n = 'f'; break;
      case '\n': *(n++) = '\\'; *n = 'n'; break;
      case '\r': *(n++) = '\\'; *n = 'r'; break;
      case '\t': *(n++) = '\\'; *n = 't'; break;
      case '\v': *(n++) = '\\'; *n = 'v'; break;
      case '"':  *(n++) = '\\'; *n = '"'; break;
      case '\'': *(n++) = '\\'; *n = '\''; break;
      case '?':  *(n++) = '\\'; *n = '?'; break;
      case ' ':  *(n++) = '\\'; *n = ' '; break;

      default:
        // Hexadecimal escape unprintable character. This violates identity
        // after composition of bsquote_filename after debsquote_filename
        // (i.e. can escape some characters differently).
        if (!c_isprint(*p)) {
          n += sprintf(n, "\\x%x", (int) (*p & 0xff)) - 1;
        } else {
          *n = *p;
        }
        break;
      error:
        fprintf (stderr, ("%s: invalid escape sequence in string (starting at offset %d)\n"),
                 program_name, (int) (p - start));
        return -1;
      }
  }

  nstart = realloc(nstart, strlen(nstart));
  return nstart;
}
