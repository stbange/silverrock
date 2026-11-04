/* Minimal C test harness. Runs natively, under sanitizers and as WebAssembly in Node.
 * SR_SPEC(capability, scenario) tags a test with the OpenSpec scenario it verifies; it
 * compiles to nothing and is read by tools/spectrace.py. */
#ifndef SR_TEST_H
#define SR_TEST_H

#include <stdio.h>
#include <string.h>

typedef struct SrTestState {
  int tests_run;
  int tests_failed;
  int checks;
  int current_failed;
  const char *current;
} SrTestState;

extern SrTestState sr_test_state;

void sr_test_fail(const char *file, int line, const char *msg);
void sr_test_run(const char *name, void (*fn)(void));

#define SR_SPEC(capability, scenario) ((void)0)

#define SR_CHECK(c)                                                                            \
  do {                                                                                         \
    sr_test_state.checks++;                                                                    \
    if (!(c)) sr_test_fail(__FILE__, __LINE__, #c);                                            \
  } while (0)

#define SR_CHECK_EQ(a, b)                                                                      \
  do {                                                                                         \
    long long sr_a_ = (long long)(a), sr_b_ = (long long)(b);                                  \
    sr_test_state.checks++;                                                                    \
    if (sr_a_ != sr_b_) {                                                                      \
      char sr_m_[256];                                                                         \
      snprintf(sr_m_, sizeof sr_m_, "%s == %s (%lld != %lld)", #a, #b, sr_a_, sr_b_);          \
      sr_test_fail(__FILE__, __LINE__, sr_m_);                                                 \
    }                                                                                          \
  } while (0)

#define SR_CHECK_NEAR(a, b, eps)                                                               \
  do {                                                                                         \
    double sr_a_ = (double)(a), sr_b_ = (double)(b);                                           \
    sr_test_state.checks++;                                                                    \
    if (sr_a_ - sr_b_ > (eps) || sr_b_ - sr_a_ > (eps)) {                                      \
      char sr_m_[256];                                                                         \
      snprintf(sr_m_, sizeof sr_m_, "%s ~= %s (%g vs %g)", #a, #b, sr_a_, sr_b_);              \
      sr_test_fail(__FILE__, __LINE__, sr_m_);                                                 \
    }                                                                                          \
  } while (0)

#define SR_CHECK_STR(a, b)                                                                     \
  do {                                                                                         \
    sr_test_state.checks++;                                                                    \
    if (strcmp((a), (b)) != 0) {                                                               \
      char sr_m_[256];                                                                         \
      snprintf(sr_m_, sizeof sr_m_, "\"%s\" == \"%s\"", (a), (b));                             \
      sr_test_fail(__FILE__, __LINE__, sr_m_);                                                 \
    }                                                                                          \
  } while (0)

#define SR_RUN(fn) sr_test_run(#fn, fn)

#endif
