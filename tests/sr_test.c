#include "sr_test.h"

#include <stdlib.h>

SrTestState sr_test_state;

static const char *g_filter = NULL;

void sr_test_fail(const char *file, int line, const char *msg) {
  sr_test_state.current_failed = 1;
  fprintf(stderr, "  FAIL %s (%s:%d): %s\n", sr_test_state.current, file, line, msg);
}

void sr_test_run(const char *name, void (*fn)(void)) {
  if (g_filter && !strstr(name, g_filter)) return;
  sr_test_state.current = name;
  sr_test_state.current_failed = 0;
  fn();
  sr_test_state.tests_run++;
  if (sr_test_state.current_failed) sr_test_state.tests_failed++;
}

/* Every test translation unit provides one suite function, listed in suites.inc. */
#define SR_SUITE(name) void name(void);
#include "suites.inc"
#undef SR_SUITE

int main(int argc, char **argv) {
  if (argc > 1) g_filter = argv[1];
#define SR_SUITE(name) name();
#include "suites.inc"
#undef SR_SUITE
  printf("%d tests, %d checks, %d failed\n", sr_test_state.tests_run, sr_test_state.checks, sr_test_state.tests_failed);
  return sr_test_state.tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
