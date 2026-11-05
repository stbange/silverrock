/* Levelled logging with a replaceable sink (file on desktop, console on web). */
#ifndef SR_LOG_H
#define SR_LOG_H

#include <stdarg.h>
#include <stddef.h>

typedef enum SrLogLevel { SR_LOG_ERROR = 0, SR_LOG_WARN, SR_LOG_INFO, SR_LOG_DEBUG } SrLogLevel;

typedef void (*SrLogSink)(void *user, SrLogLevel level, const char *subsystem, const char *msg);

void sr_log_set_sink(SrLogSink sink, void *user);
void sr_log_set_level(SrLogLevel level);
const char *sr_log_level_name(SrLogLevel level);
void sr_log(SrLogLevel level, const char *subsystem, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
/* Formats one log line: timestamp, level, subsystem, message. */
int sr_log_format(char *out, size_t cap, double timestamp, SrLogLevel level, const char *subsystem, const char *msg);

#endif
