#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <guestfs.h>
#include <guestfs-internal-frontend.h>
#include <rl.h>

int
test_empty_quote (void)
{
  char * q = bsquote_filename("");
  if (q != NULL && q != -1) {
    if (STREQ(q, "")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_empty_dequote (void)
{
  char * q = debsquote_filename("");
  if (q != NULL && q != -1) {
    if (STREQ(q, "")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_singlespace_quote (void)
{
  char * q = bsquote_filename(" ");
  if (q != NULL && q != -1) {
    if (STREQ(q, "\ ")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_singlespace_dequote (void)
{
  char * q = debsquote_filename("\ ");
  if (q != NULL && q != -1) {
    if (STREQ(q, " ")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_singleword_quote (void)
{
  char * q = bsquote_filename("singleword");
  if (q != NULL && q != -1) {
    if (STREQ(q, "singleword")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

int
test_singleword_dequote (void)
{
  char * q = debsquote_filename("singleword");
  if (q != NULL && q != -1) {
    if (STREQ(q, "singleword")) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
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

