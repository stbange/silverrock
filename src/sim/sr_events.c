#include "sim/sr_events.h"

#include <string.h>

void sr_events_init(SrEventLog *log) { memset(log, 0, sizeof *log); }

void sr_events_add(SrEventLog *log, const SrWorldEvent *ev) {
  log->ring[log->count % SR_MAX_EVENTS] = *ev;
  log->count++;
}

int sr_events_size(const SrEventLog *log) { return log->count < SR_MAX_EVENTS ? log->count : SR_MAX_EVENTS; }

const SrWorldEvent *sr_events_recent(const SrEventLog *log, int i) {
  if (i < 0 || i >= sr_events_size(log)) return NULL;
  return &log->ring[(log->count - 1 - i) % SR_MAX_EVENTS];
}
