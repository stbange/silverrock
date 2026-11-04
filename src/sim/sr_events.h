/* World event log: crimes, job outcomes, reports. Consumed by rumours, news, Heat, standing. */
#ifndef SR_EVENTS_H
#define SR_EVENTS_H

#include "core/sr_types.h"

#define SR_MAX_EVENTS 128

typedef enum SrEventType {
  SR_EV_NONE, SR_EV_GUNFIRE, SR_EV_CRIME_REPORT, SR_EV_JOB_DONE, SR_EV_JOB_FAILED,
  SR_EV_SABOTAGE, SR_EV_THEFT, SR_EV_KILLING, SR_EV_RENT_MISSED, SR_EV_EVICTION
} SrEventType;

typedef struct SrWorldEvent {
  SrEventType type;
  int64_t time;
  int8_t district;
  int32_t building;
  int8_t victim_faction;   /* -1 none */
  int8_t blamed_faction;   /* -1 unknown; -2 the player */
  bool significant;
  char text[64];
} SrWorldEvent;

typedef struct SrEventLog {
  int count;  /* total ever added */
  SrWorldEvent ring[SR_MAX_EVENTS];
} SrEventLog;

void sr_events_init(SrEventLog *log);
void sr_events_add(SrEventLog *log, const SrWorldEvent *ev);
/* i = 0 is the newest event; returns NULL past the end. */
const SrWorldEvent *sr_events_recent(const SrEventLog *log, int i);
int sr_events_size(const SrEventLog *log);

#endif
