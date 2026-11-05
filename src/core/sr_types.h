/* Basic types, result codes and assertions shared by every module. */
#ifndef SR_TYPES_H
#define SR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum SrResult {
  SR_OK = 0,
  SR_ERR_INVALID = 1,
  SR_ERR_NOMEM = 2,
  SR_ERR_IO = 3,
  SR_ERR_CORRUPT = 4,
  SR_ERR_VERSION = 5,
  SR_ERR_DENIED = 6,
  SR_ERR_NOT_FOUND = 7
} SrResult;

#define SR_ARRAY_LEN(a)     (sizeof(a) / sizeof((a)[0]))
#define SR_MIN(a, b)        ((a) < (b) ? (a) : (b))
#define SR_MAX(a, b)        ((a) > (b) ? (a) : (b))
#define SR_CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

void sr_assert_failed(const char *expr, const char *file, int line);

#ifdef NDEBUG
#define SR_ASSERT(e) ((void)0)
#else
#define SR_ASSERT(e) ((e) ? (void)0 : sr_assert_failed(#e, __FILE__, __LINE__))
#endif

/* Marks states that cannot happen; excluded from coverage by policy. */
#define SR_UNREACHABLE() sr_assert_failed("unreachable", __FILE__, __LINE__)

const char *sr_result_name(SrResult r);

#endif
