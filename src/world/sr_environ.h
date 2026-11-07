/* World clock, calendar, the light cycle (long nights, short grey days) and deterministic weather. */
#ifndef SR_ENVIRON_H
#define SR_ENVIRON_H

#include "core/sr_types.h"

#define SR_STEPS_PER_SECOND  60
#define SR_DEFAULT_TIME_RATE 20 /* in-game seconds per real second */
#define SR_DAY_S             86400
#define SR_WEEK_S            (7 * SR_DAY_S)
/* New games start on Monday of week 1 at 21:40. */
#define SR_START_TIME_S (21 * 3600 + 40 * 60)

typedef struct SrClock {
  int64_t scaled; /* sum of time rate per simulation step; game seconds = scaled / 60 */
  int32_t rate;
  bool paused;
} SrClock;

typedef enum SrWeather {
  SR_WX_CLEAR, /* shown as "overcast": no rain, still fog */
  SR_WX_DRIZZLE,
  SR_WX_DOWNPOUR,
  SR_WX_FOG,
  SR_WX_SMOG,
  SR_WX_ACID,
  SR_WX_COUNT
} SrWeather;

typedef struct SrWeatherState {
  SrWeather from, to;
  float blend;      /* 0 = from, 1 = to */
  float visibility; /* metres */
  float rain;       /* 0..1 */
  float wetness;    /* 0..1 */
} SrWeatherState;

typedef enum SrLightPhase { SR_LIGHT_NIGHT, SR_LIGHT_DAWN, SR_LIGHT_DAY, SR_LIGHT_DUSK } SrLightPhase;

/* Hours of the light cycle: dawn, the grey day and dusk; night the rest (19 hours). */
#define SR_DAWN_H  10.0f
#define SR_DAY_H   11.0f
#define SR_DUSK_H  14.0f
#define SR_NIGHT_H 15.0f

typedef struct SrSky {
  float r, g, b; /* sky colour */
  float ambient; /* ambient light level 0..1 */
  bool daylight; /* the clear sky of the Sunrise ending */
  SrLightPhase phase;
  float dayness;    /* 0 night .. 1 grey day */
  float band[3];    /* dawn or dusk glow low on the horizon (colour times strength) */
  float band_dir_x; /* +1 east (dawn), -1 west (dusk) */
} SrSky;

void sr_clock_init(SrClock *c);
void sr_clock_step(SrClock *c); /* one 1/60 s simulation step */
int64_t sr_clock_seconds(const SrClock *c);
void sr_clock_skip(SrClock *c, int64_t game_seconds);
void sr_clock_set_seconds(SrClock *c, int64_t game_seconds);
int sr_time_weekday(int64_t t); /* 0 Monday .. 6 Sunday */
int sr_time_week(int64_t t);    /* 1-based week counter */
int sr_time_hour(int64_t t);
int sr_time_minute(int64_t t);
int64_t sr_time_of_day(int64_t t); /* seconds since midnight */
const char *sr_weekday_name(int d);
/* "Mon 21:40 (week 1)" */
void sr_time_format(int64_t t, char *out, size_t cap);

SrWeather sr_weather_slot(uint64_t seed, int64_t slot);
SrWeatherState sr_weather_at(uint64_t seed, int64_t t);
/* A steady weather state (reference views, tests). */
SrWeatherState sr_weather_fixed(SrWeather w);
const char *sr_weather_name(SrWeather w);
float sr_weather_visibility(SrWeather w);

/* Light phase of a time of day. */
SrLightPhase sr_light_phase(int64_t t);
/* The sky over the city: night for 19 hours, a dark overcast day around noon framed by dawn and
 * dusk; the clear sky of the Sunrise ending once the Pall is removed. */
SrSky sr_sky_at(int64_t t, bool pall_removed);
/* Rain and fog dim the day and the dawn and dusk glow. */
SrSky sr_sky_weather(SrSky s, const SrWeatherState *w);
/* Mean luminance of the sky colour (Rec. 601). */
float sr_sky_luminance(const SrSky *s);

#endif
