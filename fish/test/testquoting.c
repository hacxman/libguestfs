#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <guestfs.h>
#include <guestfs-internal-frontend.h>
#include <rl.h>

int
eq_bracket (char *(*fn)(char*), char * in, char * out)
{
  char * q = fn(in);
  if (q != NULL && q != -1) {
    if (STREQ(q, out)) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_empty_quote (void)
{
  return eq_bracket(bsquote_filename, "", "");
}

int
test_empty_dequote (void)
{
  return eq_bracket(debsquote_filename, "", "");
}

int
test_singlespace_quote (void)
{
  return eq_bracket(bsquote_filename, " ", "\\ ");
}

int
test_singlespace_dequote (void)
{
  return eq_bracket(debsquote_filename, "\\ ", " ");
}

int
test_singleword_quote (void)
{
  return eq_bracket(bsquote_filename, "singleword", "singleword");
}

int
test_singleword_dequote (void)
{
  return eq_bracket(debsquote_filename, "singleword", "singleword");
}

int
test_multiword_quote (void)
{
  return eq_bracket(bsquote_filename, "more than one word\n", "more\\ than\\ one\\ word\\n");
}

int
test_nonprinting_quote (void)
{
  return eq_bracket(bsquote_filename, "\xac\xec\x8", "\\xac\\xec\\b");
}

int
test_multiword_dequote (void)
{
  return eq_bracket(bsquote_filename, "more\\ than\\ one\\ word\\n", "more than one word\n");
}

int
test_nonprinting_dequote (void)
{
  return eq_bracket(bsquote_filename, "\\xac\\xec\\b", "\xac\xec\x8");
}



struct test_t {
  char *name;
  int (*fn)(void);
  int expect;
};

struct test_t tests[] = {
  { .name = "test empty quote", .fn = test_empty_quote, .expect = 1},
  { .name = "test empty dequote", .fn = test_empty_dequote, .expect = 1},
  { .name = "test single space quote", .fn = test_singlespace_quote, .expect = 1},
  { .name = "test single space dequote", .fn = test_singlespace_dequote, .expect = 1},
  { .name = "test single word quote", .fn = test_singleword_quote, .expect = 1},
  { .name = "test single word dequote", .fn = test_singleword_dequote, .expect = 1},
  { .name = "test multi word quote", .fn = test_multiword_quote, .expect = 1},
  { .name = "test nonprinting quote", .fn = test_nonprinting_quote, .expect = 1},
  { .name = "test multi word dequote", .fn = test_multiword_dequote, .expect = 1},
  { .name = "test nonprinting dequote", .fn = test_nonprinting_dequote, .expect = 1},
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

