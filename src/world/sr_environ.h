/* World clock, calendar, permanent night and deterministic weather. */
#ifndef SR_ENVIRON_H
#define SR_ENVIRON_H

#include "core/sr_types.h"

#define SR_STEPS_PER_SECOND 60
#define SR_DEFAULT_TIME_RATE 20 /* in-game seconds per real second */
#define SR_DAY_S 86400
#define SR_WEEK_S (7 * SR_DAY_S)
/* New games start on Monday of week 1 at 21:40. */
#define SR_START_TIME_S (21 * 3600 + 40 * 60)

typedef struct SrClock {
  int64_t scaled; /* sum of time rate per simulation step; game seconds = scaled / 60 */
  int32_t rate;
  bool paused;
} SrClock;

typedef enum SrWeather {
  SR_WX_CLEAR, SR_WX_DRIZZLE, SR_WX_DOWNPOUR, SR_WX_FOG, SR_WX_SMOG, SR_WX_ACID, SR_WX_COUNT
} SrWeather;

typedef struct SrWeatherState {
  SrWeather from, to;
  float blend;      /* 0 = from, 1 = to */
  float visibility; /* metres */
  float rain;       /* 0..1 */
  float wetness;    /* 0..1 */
} SrWeatherState;

typedef struct SrSky {
  float r, g, b;   /* sky colour */
  float ambient;   /* ambient light level 0..1 */
  bool daylight;
} SrSky;

void sr_clock_init(SrClock *c);
void sr_clock_step(SrClock *c); /* one 1/60 s simulation step */
int64_t sr_clock_seconds(const SrClock *c);
void sr_clock_skip(SrClock *c, int64_t game_seconds);
void sr_clock_set_seconds(SrClock *c, int64_t game_seconds);
int sr_time_weekday(int64_t t);   /* 0 Monday .. 6 Sunday */
int sr_time_week(int64_t t);      /* 1-based week counter */
int sr_time_hour(int64_t t);
int sr_time_minute(int64_t t);
int64_t sr_time_of_day(int64_t t);/* seconds since midnight */
const char *sr_weekday_name(int d);
/* "Mon 21:40 (week 1)" */
void sr_time_format(int64_t t, char *out, size_t cap);

SrWeather sr_weather_slot(uint64_t seed, int64_t slot);
SrWeatherState sr_weather_at(uint64_t seed, int64_t t);
const char *sr_weather_name(SrWeather w);
float sr_weather_visibility(SrWeather w);

/* Sky is night at every time unless the Pall has been removed (Sunrise ending). */
SrSky sr_sky_at(int64_t t, bool pall_removed);

#endif
