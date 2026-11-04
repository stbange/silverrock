#include "world/sr_environ.h"

#include "core/sr_rng.h"

#include <stdio.h>

#define WEATHER_SLOT_S (2 * 3600)
#define WEATHER_TRANSITION_S (10 * 60)

void sr_clock_init(SrClock *c) {
  c->scaled = (int64_t)SR_START_TIME_S * SR_STEPS_PER_SECOND;
  c->rate = SR_DEFAULT_TIME_RATE;
  c->paused = false;
}

void sr_clock_step(SrClock *c) {
  if (!c->paused) c->scaled += c->rate;
}

int64_t sr_clock_seconds(const SrClock *c) { return c->scaled / SR_STEPS_PER_SECOND; }

void sr_clock_skip(SrClock *c, int64_t s) { c->scaled += s * SR_STEPS_PER_SECOND; }

void sr_clock_set_seconds(SrClock *c, int64_t s) { c->scaled = s * SR_STEPS_PER_SECOND; }

int sr_time_weekday(int64_t t) { return (int)((t / SR_DAY_S) % 7); }
int sr_time_week(int64_t t) { return (int)(t / SR_WEEK_S) + 1; }
int64_t sr_time_of_day(int64_t t) { return t % SR_DAY_S; }
int sr_time_hour(int64_t t) { return (int)(sr_time_of_day(t) / 3600); }
int sr_time_minute(int64_t t) { return (int)((sr_time_of_day(t) % 3600) / 60); }

const char *sr_weekday_name(int d) {
  static const char *const n[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  return d >= 0 && d < 7 ? n[d] : "?";
}

void sr_time_format(int64_t t, char *out, size_t cap) {
  snprintf(out, cap, "%s %02d:%02d (week %d)", sr_weekday_name(sr_time_weekday(t)),
           sr_time_hour(t), sr_time_minute(t), sr_time_week(t));
}

const char *sr_weather_name(SrWeather w) {
  static const char *const n[SR_WX_COUNT] = {"clear", "drizzle", "downpour", "fog",
                                             "smog alert", "acid-rain advisory"};
  return (unsigned)w < SR_WX_COUNT ? n[w] : "?";
}

float sr_weather_visibility(SrWeather w) {
  static const float v[SR_WX_COUNT] = {400.0f, 300.0f, 160.0f, 110.0f, 200.0f, 220.0f};
  return (unsigned)w < SR_WX_COUNT ? v[w] : 400.0f;
}

static float weather_rain(SrWeather w) {
  switch (w) {
  case SR_WX_DRIZZLE: return 0.35f;
  case SR_WX_DOWNPOUR: return 1.0f;
  case SR_WX_ACID: return 0.6f;
  default: return 0.0f;
  }
}

SrWeather sr_weather_slot(uint64_t seed, int64_t slot) {
  static const int32_t weights[SR_WX_COUNT] = {35, 25, 15, 12, 8, 5};
  SrRng r = sr_rng_stream(seed, "weather", (uint64_t)slot, 0);
  return (SrWeather)sr_rng_weighted(&r, weights, SR_WX_COUNT);
}

SrWeatherState sr_weather_at(uint64_t seed, int64_t t) {
  int64_t slot = t / WEATHER_SLOT_S;
  int64_t into = t - slot * WEATHER_SLOT_S;
  SrWeatherState s;
  s.to = sr_weather_slot(seed, slot);
  s.from = slot > 0 ? sr_weather_slot(seed, slot - 1) : s.to;
  s.blend = into >= WEATHER_TRANSITION_S ? 1.0f : (float)into / (float)WEATHER_TRANSITION_S;
  s.visibility = sr_weather_visibility(s.from) +
                 (sr_weather_visibility(s.to) - sr_weather_visibility(s.from)) * s.blend;
  s.rain = weather_rain(s.from) + (weather_rain(s.to) - weather_rain(s.from)) * s.blend;
  s.wetness = s.rain > 0.0f ? 0.3f + 0.7f * s.rain : 0.25f; /* streets never fully dry */
  return s;
}

SrSky sr_sky_at(int64_t t, bool pall_removed) {
  SrSky s;
  if (pall_removed) {
    float h = (float)sr_time_of_day(t) / 3600.0f;
    float day = (h > 6.0f && h < 20.0f) ? 1.0f : 0.35f; /* even the night after is brighter */
    s.r = 0.45f * day + 0.1f;
    s.g = 0.6f * day + 0.1f;
    s.b = 0.85f * day + 0.1f;
    s.ambient = 0.3f + 0.6f * day;
    s.daylight = true;
    return s;
  }
  (void)t; /* the Pall: same night at every hour */
  s.r = 0.06f;
  s.g = 0.05f;
  s.b = 0.10f;
  s.ambient = 0.12f;
  s.daylight = false;
  return s;
}
