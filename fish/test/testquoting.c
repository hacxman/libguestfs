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

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "guestfs.h"
#include "guestfs-internal-frontend.h"
#include "../rl.h"

struct string_test_data {
  const char *in;
  const char *out;
  int pass;
};

struct string_test_data escape_tests[] = {
  { "", "", 1 },
  { " ", "\\ ", 1 },
  { "singleword", "singleword", 1 },
  { "more than one word\n", "more\\ than\\ one\\ word\\n", 1 },
  { "more than one word\n", "more\\ than\\ one\\ word\\n", 1 },
  { "\xac\xec\x8", "\\xac\\xec\\b", 1 },
};

size_t nr_escape_tests = sizeof (escape_tests) / sizeof (*escape_tests);

struct string_test_data unescape_tests[] = {
  { "", "", 1 },
  { "\\ ", " ", 1 },
  { "singleword", "singleword", 1 },
  { "more\\ than\\ one\\ word\\n", "more than one word\n", 1 },
  { "more\\ than\\ one\\ word\\n", "more than one word\n", 1 },
  { "\\xac\\xec\\b", "\xac\xec\x8", 1 },
};

size_t nr_unescape_tests = sizeof (unescape_tests) / sizeof (*unescape_tests);

int
run_with_test_data (char *(*f) (const char *),
    struct string_test_data *data, size_t len)
{
  int i = 0, nr_failed = 0;

  for (; i < len; i++) {
    char *r = f(data[i].in);
    if (((r != NULL) && STREQ (r, data[i].out)) != data[i].pass) {
      printf ("%d ", i);
      nr_failed ++;
    }
    if (r != NULL) {
      free (r);
    }
  }
  printf ("%s\n", nr_failed == 0 ? "none" : "");
  return nr_failed;
}

int
main (int argc, char *argv[])
{
  int nr_failed = 0;

  printf ("Escaping tests failed ids: ");
  nr_failed += run_with_test_data (
      bs_escape_filename, escape_tests, nr_escape_tests);

  printf ("Un-escaping tests failed ids: ");
  nr_failed += run_with_test_data (
      bs_unescape_filename, unescape_tests, nr_unescape_tests);

  if (nr_failed > 0) {
    printf ("***** %zu / %zu tests FAILED *****\n", nr_failed,
        nr_escape_tests + nr_unescape_tests);
  }

  return nr_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
