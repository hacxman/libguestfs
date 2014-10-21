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
debsquote_filename (char *p)
{
  char *start = p;
  fprintf(stderr, "fish.c: debsquote_filename called\n");

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
      case ' ': *p = ' '; fprintf(stderr, "found escaped space\n"); break;

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

  if (!*p) {
    fprintf (stderr, ("%s: unterminated double quote\n"), program_name);
    return -1;
  }

  *p = '\0';
  return p - start;
}

// backslash quote
char *
bsquote_filename (char *p)
{
  char *start = p;
  fprintf(stderr, "fish.c: bsquote_filename called\n");
  // four times original length - if all chars are unprintable
  // new string would be 0xXY0xWZ
  char *n = malloc(strlen(p) * 4 + 1);
  if (strlen(p) == 0) {
    n[0] = '\0';
    return n;
  }

  for (; *p; p++) {
//    if (*p == '\\') {
      int m = 1, c;

      switch (*p) {
      case '\\': break;
      case '\a': *n = '\a'; break;
      case '\b': *n = '\b'; break;
      case '\f': *n = '\f'; break;
      case '\n': *n = '\n'; break;
      case '\r': *n = '\r'; break;
      case '\t': *n = '\t'; break;
      case '\v': *n = '\v'; break;
      case '"': *n = '"'; break;
      case '\'': *n = '\''; break;
      case '?': *n = '?'; break;
      case ' ': *n = ' '; fprintf(stderr, "found space\n"); break;

      default:
        // Octal escape unprintable character. This violates identity
        // after composition of bsquote_filename after debsquote_filename
        // (i.e. can escape some characters differently).
        if (!isprint(*p)) {
          sprintf(n, "%o", *p);
          int l = strlen(n);
          n += l;
        }
      error:
        fprintf (stderr, ("%s: invalid escape sequence in string (starting at offset %d)\n"),
                 program_name, (int) (p - start));
        return -1;
      }
      memmove (p+1, p+1+m, strlen (p+1+m) + 1);
//    }
  }

  if (!*p) {
    fprintf (stderr, ("%s: unterminated double quote\n"), program_name);
    return -1;
  }

  *p = '\0';
  return p - start;
}

