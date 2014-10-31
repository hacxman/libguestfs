#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include <guestfs.h>
#include <guestfs-internal-frontend.h>
#include <rl.h>

int
eq_bracket (char *(*fn)(char*), char * in, char * out)
{
  char * q = fn(in);
  return (q != NULL) && STREQ(q, out);
}

int
test_empty_escape (void)
{
  return eq_bracket(bs_escape_filename, "", "");
}

int
test_empty_unescape (void)
{
  return eq_bracket(bs_unescape_filename, "", "");
}

int
test_singlespace_escape (void)
{
  return eq_bracket(bs_escape_filename, " ", "\\ ");
}

int
test_singlespace_unescape (void)
{
  return eq_bracket(bs_unescape_filename, "\\ ", " ");
}

int
test_singleword_escape (void)
{
  return eq_bracket(bs_escape_filename, "singleword", "singleword");
}

int
test_singleword_unescape (void)
{
  return eq_bracket(bs_unescape_filename, "singleword", "singleword");
}

int
test_multiword_escape (void)
{
  return eq_bracket(bs_escape_filename, "more than one word\n", "more\\ than\\ one\\ word\\n");
}

int
test_nonprinting_escape (void)
{
  return eq_bracket(bs_escape_filename, "\xac\xec\x8", "\\xac\\xec\\b");
}

int
test_multiword_unescape (void)
{
  return eq_bracket(bs_unescape_filename, "more\\ than\\ one\\ word", "more than one word");
}

int
test_nonprinting_unescape (void)
{
  return eq_bracket(bs_unescape_filename, "\\xac\\xec\\b", "\xac\xec\b");
}



struct test_t {
  char *name;
  int (*fn)(void);
  int expect;
};

struct test_t tests[] = {
  { .name = "test empty escape", .fn = test_empty_escape, .expect = 1},
  { .name = "test empty unescape", .fn = test_empty_unescape, .expect = 1},
  { .name = "test single space escape", .fn = test_singlespace_escape, .expect = 1},
  { .name = "test single space unescape", .fn = test_singlespace_unescape, .expect = 1},
  { .name = "test single word escape", .fn = test_singleword_escape, .expect = 1},
  { .name = "test single word unescape", .fn = test_singleword_unescape, .expect = 1},
  { .name = "test multi word escape", .fn = test_multiword_escape, .expect = 1},
  { .name = "test nonprinting escape", .fn = test_nonprinting_escape, .expect = 1},
  { .name = "test multi word unescape", .fn = test_multiword_unescape, .expect = 1},
  { .name = "test nonprinting unescape", .fn = test_nonprinting_unescape, .expect = 1},
};

size_t nr_tests = sizeof(tests) / sizeof(*tests);

int
main (int argc, char *argv[])
{
  int nr_failed = 0; // failed test count
  setbuf(stdout, NULL);

  for (int i = 0; i < nr_tests; i++) {
    fprintf(stdout, "%s: ", tests[i].name);
    int r = tests[i].fn() == tests[i].expect;
    fprintf(stdout, "%s\n", r ? "PASS" : "FAIL");
    nr_failed += !r;
  }

  if (nr_failed > 0) {
    printf ("***** %zu / %zu tests FAILED *****\n", nr_failed, nr_tests);
  }

  return nr_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

