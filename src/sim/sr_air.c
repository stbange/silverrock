#include "sim/sr_air.h"

#include <math.h>
#include <string.h>

void sr_air_build(SrAirTraffic *a, const SrCity *c) {
  memset(a, 0, sizeof *a);
  float extent = (float)(c->grid * SR_BLOCK_CM) / 100.0f;
  float street = (float)SR_STREET_CM / 200.0f; /* centre of the street strip */
  for (int axis = 0; axis < 2; axis++)
    for (int row = 1; row < c->grid && a->lane_count < SR_AIR_MAX_LANES; row += 4) {
      SrAirLane *l = &a->lanes[a->lane_count];
      l->along_x = axis == 0;
      l->line = (float)(row * SR_BLOCK_CM) / 100.0f + street;
      l->from = 0;
      l->to = extent;
      l->alt = axis == 0 ? SR_AIR_ALT_X : SR_AIR_ALT_Z;
      l->dir = (int8_t)((a->lane_count % 2) ? -1 : 1);
      a->lane_count++;
    }
}

SrAerodyne sr_air_vehicle(const SrAirTraffic *a, int lane, int k, double t) {
  const SrAirLane *l = &a->lanes[lane];
  double len = (double)(l->to - l->from);
  double spacing = len / SR_AIR_PER_LANE;
  double s = fmod(t * SR_AIR_SPEED + spacing * k + lane * 37.0, len);
  if (s < 0) s += len; /* GCOVR_EXCL_LINE reason: t >= 0 in play */
  float along = l->dir > 0 ? l->from + (float)s : l->to - (float)s;
  SrAerodyne v;
  v.lane = lane;
  v.color = (uint8_t)((lane * 7 + k * 5) % 24);
  v.pos = l->along_x ? sr_v3(along, l->alt, l->line) : sr_v3(l->line, l->alt, along);
  v.dir = l->along_x ? sr_v3((float)l->dir, 0, 0) : sr_v3(0, 0, (float)l->dir);
  return v;
}

int sr_air_near(const SrAirTraffic *a, double t, SrVec3 cam, float radius, SrAerodyne *out, int max) {
  int n = 0;
  for (int l = 0; l < a->lane_count; l++)
    for (int k = 0; k < SR_AIR_PER_LANE && n < max; k++) {
      SrAerodyne v = sr_air_vehicle(a, l, k, t);
      float dx = v.pos.x - cam.x, dz = v.pos.z - cam.z;
      if (dx * dx + dz * dz <= radius * radius) out[n++] = v;
    }
  return n;
}
