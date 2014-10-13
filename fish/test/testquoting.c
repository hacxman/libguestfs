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
  printf("%zu kokot\n", q);
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

struct test_t {
  char *name;
  int (*fn)(void);
  int expect;
};

struct test_t tests[] = {
  { .name = "test empty quote", .fn = test_empty_quote, .expect = 1},
  { .name = "test empty dequote", .fn = test_empty_dequote, .expect = 1},
};

size_t nr_tests = sizeof(tests) / sizeof(*tests);

int
main (int argc, char *argv[])
{
  int nr_failed = 0; // failed test count
  printf("%zu\n", nr_tests);

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

