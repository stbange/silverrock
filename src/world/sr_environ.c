#include "world/sr_environ.h"

#include "core/sr_rng.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WEATHER_SLOT_S       (2 * 3600)
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
  snprintf(out, cap, "%s %02d:%02d (week %d)", sr_weekday_name(sr_time_weekday(t)), sr_time_hour(t), sr_time_minute(t),
           sr_time_week(t));
}

const char *sr_weather_name(SrWeather w) {
  static const char *const n[SR_WX_COUNT] = {"overcast", "drizzle",    "downpour",
                                             "fog",      "smog alert", "acid-rain advisory"};
  return (unsigned)w < SR_WX_COUNT ? n[w] : "?";
}

/* The climate: fog always, rain nearly always, and the player sees one or two blocks. */
float sr_weather_visibility(SrWeather w) {
  static const float v[SR_WX_COUNT] = {95.0f, 72.0f, 55.0f, 34.0f, 55.0f, 65.0f}; /* metres: a heavy, wet haze */
  return (unsigned)w < SR_WX_COUNT ? v[w] : v[0];
}

static float weather_rain(SrWeather w) {
  switch (w) {
    case SR_WX_DRIZZLE: return 0.35f;
    case SR_WX_DOWNPOUR: return 1.0f;
    case SR_WX_FOG: return 0.2f; /* misty drizzle */
    case SR_WX_SMOG: return 0.15f;
    case SR_WX_ACID: return 0.6f;
    default: return 0.0f;
  }
}

static float wetness_for(float rain) { return SR_MAX(0.6f, 0.3f + 0.7f * rain); /* the street is never dry */ }

SrWeather sr_weather_slot(uint64_t seed, int64_t slot) {
  static const int32_t weights[SR_WX_COUNT] = {6, 34, 22, 18, 10, 10};
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
  s.visibility =
      sr_weather_visibility(s.from) + (sr_weather_visibility(s.to) - sr_weather_visibility(s.from)) * s.blend;
  s.rain = weather_rain(s.from) + (weather_rain(s.to) - weather_rain(s.from)) * s.blend;
  s.wetness = wetness_for(s.rain);
  return s;
}

SrWeatherState sr_weather_fixed(SrWeather w) {
  SrWeatherState s;
  s.from = s.to = (unsigned)w < SR_WX_COUNT ? w : SR_WX_CLEAR;
  s.blend = 1.0f;
  s.visibility = sr_weather_visibility(s.to);
  s.rain = weather_rain(s.to);
  s.wetness = wetness_for(s.rain);
  return s;
}

SrLightPhase sr_light_phase(int64_t t) {
  float h = (float)sr_time_of_day(t) / 3600.0f;
  if (h >= SR_DAWN_H && h < SR_DAY_H) return SR_LIGHT_DAWN;
  if (h >= SR_DAY_H && h < SR_DUSK_H) return SR_LIGHT_DAY;
  if (h >= SR_DUSK_H && h < SR_NIGHT_H) return SR_LIGHT_DUSK;
  return SR_LIGHT_NIGHT;
}

static float smooth(float x) { return x <= 0 ? 0 : x >= 1 ? 1 : x * x * (3 - 2 * x); }

SrSky sr_sky_at(int64_t t, bool pall_removed) {
  SrSky s;
  memset(&s, 0, sizeof s);
  float h = (float)sr_time_of_day(t) / 3600.0f;
  if (pall_removed) {                                   /* the clouds have broken: a clear sky, the sun by day */
    float day = (h > 6.0f && h < 20.0f) ? 1.0f : 0.35f; /* even the night after is brighter */
    s.r = 0.45f * day + 0.1f;
    s.g = 0.6f * day + 0.1f;
    s.b = 0.85f * day + 0.1f;
    s.ambient = 0.3f + 0.7f * day;
    s.daylight = true;
    s.phase = day >= 1.0f ? SR_LIGHT_DAY : SR_LIGHT_NIGHT;
    s.dayness = day >= 1.0f ? 1.0f : 0.0f;
    return s;
  }
  static const float night[3] = {0.06f, 0.05f, 0.10f}, grey[3] = {0.17f, 0.18f, 0.21f};
  s.phase = sr_light_phase(t);
  float band = 0;
  switch (s.phase) {
    case SR_LIGHT_DAWN:
      s.dayness = smooth(h - SR_DAWN_H);
      band = sinf((h - SR_DAWN_H) * 3.14159265f);
      s.band[0] = 0.62f * band;
      s.band[1] = 0.30f * band;
      s.band[2] = 0.14f * band;
      s.band_dir_x = 1.0f;
      break;
    case SR_LIGHT_DAY: s.dayness = 1.0f; break;
    case SR_LIGHT_DUSK: /* the deepest, longest colour of the day */
      s.dayness = 1.0f - smooth(h - SR_DUSK_H);
      band = sinf((h - SR_DUSK_H) * 3.14159265f);
      s.band[0] = 0.72f * band;
      s.band[1] = 0.20f * band;
      s.band[2] = 0.10f * band;
      s.band_dir_x = -1.0f;
      break;
    default: s.dayness = 0.0f; break;
  }
  s.r = night[0] + (grey[0] - night[0]) * s.dayness;
  s.g = night[1] + (grey[1] - night[1]) * s.dayness;
  s.b = night[2] + (grey[2] - night[2]) * s.dayness;
  s.ambient = 0.62f * (1.0f + 0.5f * s.dayness); /* at most 1.5 times the night */
  s.daylight = false;
  return s;
}

SrSky sr_sky_weather(SrSky s, const SrWeatherState *w) {
  if (s.daylight) return s;
  float dim = 1.0f - 0.5f * w->rain - (w->visibility < 80.0f ? 0.2f : 0.0f);
  s.ambient = 0.62f + (s.ambient - 0.62f) * SR_MAX(0.2f, dim);
  float bandk = SR_MAX(0.1f, 1.0f - 0.85f * w->rain) * (w->visibility < 80.0f ? 0.5f : 1.0f);
  for (int k = 0; k < 3; k++) s.band[k] *= bandk;
  return s;
}

float sr_sky_luminance(const SrSky *s) { return 0.299f * s->r + 0.587f * s->g + 0.114f * s->b; }
