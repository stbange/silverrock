#include "sim/sr_air.h"

#include "core/sr_rng.h"
#include "world/sr_dressing.h"

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
  memset(&v, 0, sizeof v);
  v.lane = lane;
  v.color = (uint8_t)((lane * 7 + k * 5) % 24);
  /* a gentle weave across the lane: the aerodyne banks into each curve and bobs as it hovers */
  float ph = (float)s * 0.015f + (float)k * 1.7f, side = 6.0f * sinf(ph);
  float slope = 6.0f * 0.015f * cosf(ph), lat_acc = -6.0f * 0.015f * 0.015f * sinf(ph) * SR_AIR_SPEED * SR_AIR_SPEED;
  float bob = 0.2f * sinf((float)t * 3.1416f + (float)(lane * 3 + k));
  SrVec3 fwd = l->along_x ? sr_v3((float)l->dir, 0, 0) : sr_v3(0, 0, (float)l->dir);
  SrVec3 perp = sr_v3(-fwd.z, 0, fwd.x);
  v.pos = l->along_x ? sr_v3(along, l->alt + bob, l->line) : sr_v3(l->line, l->alt + bob, along);
  v.pos = sr_v3_add(v.pos, sr_v3_scale(perp, side));
  v.dir = sr_v3_norm(sr_v3_add(fwd, sr_v3_scale(perp, slope)));
  v.roll = sr_clampf(lat_acc * 0.3f, -0.44f, 0.44f);
  v.mode = SR_AIR_CRUISE;
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

static float smooth01(float x) {
  x = sr_clampf(x, 0, 1);
  return x * x * (3 - 2 * x);
}

bool sr_air_lander(const SrCity *c, int building, double t, SrAerodyne *out) {
  SrFacade f;
  sr_facade_for(c, building, &f);
  if (f.roof != SR_ROOF_HELIPAD) return false;
  SrVolume v[SR_MAX_VOLUMES];
  int nv = sr_building_volumes(c, building, &f, v);
  const SrVolume *top = NULL;
  for (int i = 0; i < nv; i++)
    if ((v[i].flags & SR_VOL_ROOF) && (!top || v[i].y1 > top->y1)) top = &v[i];
  if (!top) return false;
  uint64_t h = sr_mix64(c->seed ^ ((uint64_t)building * 0x9E3779B97F4A7C15ull));
  double period = 150.0 + (double)(h % 5u) * 20.0, wait = 20.0 + (double)((h >> 8) % 41u);
  double ph = fmod(t + (double)((h >> 16) % 1000u), period);
  float L = SR_AIR_LAND_TIME;
  if (ph > L + wait + L) return false; /* away */
  float ang = (float)((h >> 32) % 628u) / 100.0f;
  SrVec3 pad = sr_v3((top->x0 + top->x1) * 0.5f, top->y1 + 0.9f, (top->z0 + top->z1) * 0.5f);
  SrVec3 from = sr_v3(sinf(ang), 0, cosf(ang)); /* the side it comes in from */
  memset(out, 0, sizeof *out);
  out->lane = -1;
  out->color = (uint8_t)(h % 24u);
  float alt = fmaxf(SR_AIR_ALT_X, pad.y + 25.0f);
  float u;
  if (ph < L) { /* descending along an S-curve, the nose rising to settle */
    u = smooth01((float)ph / L);
    out->mode = SR_AIR_DESCEND;
    out->dir = sr_v3_scale(from, -1.0f);
    out->pitch = -0.25f * sinf(u * 3.1416f);
  } else if (ph < L + wait) {
    u = 1.0f;
    out->mode = SR_AIR_LANDED;
    out->dir = sr_v3_scale(from, -1.0f);
  } else { /* lifting off and leaving the way it faces */
    u = 1.0f - smooth01((float)(ph - L - wait) / L);
    out->mode = SR_AIR_LIFT;
    out->dir = sr_v3_scale(from, -1.0f);
    from = sr_v3_scale(from, -1.0f);
    out->pitch = 0.2f * sinf(u * 3.1416f);
  }
  float away = (1.0f - u) * 60.0f, up = (1.0f - u) * (1.0f - u) * (alt - pad.y);
  out->pos = sr_v3(pad.x + from.x * away, pad.y + up, pad.z + from.z * away);
  return true;
}

int sr_air_landers(const SrCity *c, double t, SrVec3 cam, float radius, SrAerodyne *out, int max) {
  int n = 0;
  int bx = (int)(cam.x * 100.0f) / SR_BLOCK_CM, by = (int)(cam.z * 100.0f) / SR_BLOCK_CM;
  int r = (int)(radius * 100.0f) / SR_BLOCK_CM + 1;
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      int b = sr_city_block_index(c, bx + dx, by + dy);
      if (b < 0) continue;
      const SrBlock *blk = &c->blocks[b];
      for (int i = 0; i < blk->building_count && n < max; i++) {
        int bi = blk->first_building + i;
        int32_t cx, cy;
        sr_building_center(&c->buildings[bi], &cx, &cy);
        float ddx = (float)cx / 100.0f - cam.x, ddz = (float)cy / 100.0f - cam.z;
        if (ddx * ddx + ddz * ddz > radius * radius) continue;
        if (sr_air_lander(c, bi, t, &out[n])) n++;
      }
    }
  return n;
}
