#include "core/sr_types.h"
#include "core/sr_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- assertions --------------------------------------------------------- */

void sr_assert_failed(const char *expr, const char *file, int line) {
  /* GCOVR_EXCL_START reason: aborts the process, cannot return to a test */
  fprintf(stderr, "assertion failed: %s (%s:%d)\n", expr, file, line);
  abort();
  /* GCOVR_EXCL_STOP */
}

const char *sr_result_name(SrResult r) {
  switch (r) {
  case SR_OK: return "ok";
  case SR_ERR_INVALID: return "invalid";
  case SR_ERR_NOMEM: return "out of memory";
  case SR_ERR_IO: return "io error";
  case SR_ERR_CORRUPT: return "corrupt";
  case SR_ERR_VERSION: return "version";
  case SR_ERR_DENIED: return "denied";
  case SR_ERR_NOT_FOUND: return "not found";
  }
  return "unknown";
}

/* ---- logging ------------------------------------------------------------
 * The log sink is the one piece of process-wide state in the engine: logging
 * must be reachable from every module without threading a context through. */

static SrLogSink g_sink = NULL;
static void *g_sink_user = NULL;
static SrLogLevel g_min_level = SR_LOG_INFO;

void sr_log_set_sink(SrLogSink sink, void *user) {
  g_sink = sink;
  g_sink_user = user;
}

void sr_log_set_level(SrLogLevel level) { g_min_level = level; }

const char *sr_log_level_name(SrLogLevel level) {
  switch (level) {
  case SR_LOG_ERROR: return "error";
  case SR_LOG_WARN: return "warning";
  case SR_LOG_INFO: return "info";
  case SR_LOG_DEBUG: return "debug";
  }
  return "?";
}

void sr_log(SrLogLevel level, const char *subsystem, const char *fmt, ...) {
  if (level > g_min_level || g_sink == NULL) {
    return;
  }
  char msg[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  g_sink(g_sink_user, level, subsystem, msg);
}

int sr_log_format(char *out, size_t cap, double timestamp, SrLogLevel level,
                  const char *subsystem, const char *msg) {
  return snprintf(out, cap, "[%10.3f] %-7s %-10s %s", timestamp, sr_log_level_name(level),
                  subsystem, msg);
}
