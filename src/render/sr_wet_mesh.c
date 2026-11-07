/* Wet streets: reflections of lights on the wet ground, rain splashes, drips from awnings, lamp
 * cones in wet air, steam from the street, ripples in puddles, and rain tinted by nearby lights. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <stdlib.h>

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

static SrColor scale(float r, float g, float b, float k) {
  return (SrColor){(uint8_t)(clamp01(r * k) * 255.0f), (uint8_t)(clamp01(g * k) * 255.0f),
                   (uint8_t)(clamp01(b * k) * 255.0f), 255};
}

int sr_build_reflections(const SrLight *l, int n, SrVec3 eye, float wetness, int max, SrMesh *out) {
  if (wetness <= 0.0f || max <= 0) return 0;
  enum { CAP = 96 };
  int idx[CAP];
  float score[CAP];
  int count = 0;
  for (int i = 0; i < n && i < SR_MAX_FRAME_LIGHTS; i++) {
    float dx = l[i].pos.x - eye.x, dz = l[i].pos.z - eye.z, d = sqrtf(dx * dx + dz * dz);
    if (d > 40.0f || d < 1.0f) continue;
    float s = (l[i].r + l[i].g + l[i].b) * (1.0f - d / 40.0f);
    if (count < max && count < CAP) {
      idx[count] = i;
      score[count++] = s;
      continue;
    }
    int w = 0; /* keep the brightest */
    for (int j = 1; j < count; j++)
      if (score[j] < score[w]) w = j;
    if (s > score[w]) {
      idx[w] = i;
      score[w] = s;
    }
  }
  for (int j = 0; j < count; j++) {
    const SrLight *L = &l[idx[j]];
    float dx = eye.x - L->pos.x, dz = eye.z - L->pos.z, d = sqrtf(dx * dx + dz * dz);
    float ux = dx / d, uz = dz / d; /* toward the camera */
    float len = SR_MIN(14.0f, L->pos.y * 2.2f + 1.0f), w = L->pos.y > 5.5f ? 0.9f : 1.4f;
    float k = wetness * 1.1f * (1.0f - d / 40.0f);
    SrColor c = scale(L->r, L->g, L->b, k);
    float y = 0.04f;
    SrVec3 a = sr_v3(L->pos.x - uz * w, y, L->pos.z + ux * w), b = sr_v3(L->pos.x + uz * w, y, L->pos.z - ux * w);
    SrVec3 bb = sr_v3(b.x + ux * len, y, b.z + uz * len), aa = sr_v3(a.x + ux * len, y, a.z + uz * len);
    SrAtlasCell cell = sr_atlas_cell(SR_LAYER_SPRITES, SR_SPR_STREAK);
    /* bright at the foot of the light, fading toward the camera; ground = 1 makes it ripple */
    sr_mesh_quad(out, a, b, bb, aa, cell.u0, cell.v0, cell.u1, cell.v1, c, cell.layer, 255, 1);
  }
  return count;
}

void sr_build_splashes(const SrCity *c, SrVec3 eye, float rain, uint64_t steps, SrMesh *out) {
  if (rain < 0.3f) return;
  int n = (int)(70.0f * rain);
  uint64_t slot = steps / 5u; /* new splashes twelve times a second */
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < n; i++) {
    uint64_t h = sr_mix64(slot * 0x9E3779B97F4A7C15ull + (uint64_t)i);
    float x = eye.x + ((float)(h % 4000u) / 100.0f - 20.0f), z = eye.z + ((float)((h >> 16) % 4000u) / 100.0f - 20.0f);
    if ((x - eye.x) * (x - eye.x) + (z - eye.z) * (z - eye.z) > 400.0f) continue;
    if (sr_point_covered(c, sr_v3(x, 0.1f, z))) continue;
    int frame = (int)((steps / 3u + (h >> 40)) % 4u);
    sr_mesh_sprite(out, &bb, sr_v3(x, 0.02f, z), 0.5f, 0.35f, SR_SPR_SPLASH + frame, SR_SPRITE_UPRIGHT, 0,
                   (SrColor){38, 44, 56, 255}, 255);
  }
}

void sr_build_drips(const SrCity *c, SrVec3 eye, float rain, float time_s, SrMesh *out) {
  if (rain < 0.15f) return;
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      const SrBlock *b = &c->blocks[by * c->grid + bx];
      for (int i = 0; i < b->building_count; i++) {
        const SrBuilding *bd = &c->buildings[b->first_building + i];
        if (bd->poi == SR_POI_NONE) continue;
        float x0 = (float)bd->x0 / 100.0f, x1 = (float)bd->x1 / 100.0f, z1 = (float)bd->y1 / 100.0f;
        float cx = (x0 + x1) / 2, w = SR_MIN(3.0f, (x1 - x0) * 0.4f) + 1.0f;
        if (fabsf(cx - eye.x) > 30.0f || fabsf(z1 - eye.z) > 30.0f) continue;
        for (int k = 0; k < 8; k++) { /* drips falling off the awning's front edge */
          float x = cx - w + (2.0f * w) * ((float)k + 0.5f) / 8.0f;
          float ph = fmodf(time_s * (1.4f + 0.3f * (float)(k % 3)) + (float)k * 0.37f, 1.0f);
          float y = 3.0f - ph * ph * 3.0f;
          sr_mesh_sprite(out, &bb, sr_v3(x, y, z1 + 2.0f), 0.08f, 0.35f, SR_SPR_DRIP, SR_SPRITE_UPRIGHT, 0,
                         (SrColor){90, 110, 140, 255}, 255);
        }
      }
    }
}

void sr_build_lamp_cones(const SrLight *l, int n, SrVec3 eye, float wet_air, SrMesh *out) {
  float k = clamp01(wet_air) * 0.22f;
  if (k < 0.01f) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < n; i++) {
    if (l[i].pos.y < 5.5f || l[i].pos.y > 9.0f) continue; /* street lamps */
    float dx = l[i].pos.x - eye.x, dz = l[i].pos.z - eye.z;
    if (dx * dx + dz * dz > 60.0f * 60.0f) continue;
    sr_mesh_sprite(out, &bb, sr_v3(l[i].pos.x, 0.0f, l[i].pos.z), 4.6f, l[i].pos.y, SR_SPR_CONE, SR_SPRITE_UPRIGHT, 0,
                   scale(l[i].r, l[i].g, l[i].b, k), 255);
  }
}

void sr_build_steam(const SrStreetProp *p, int n, SrVec3 eye, float rain, float time_s, float dist, SrMesh *out) {
  SrBillboard bb = sr_billboard(eye, 0, 0);
  int puffs = rain > 0.05f ? 6 : 3; /* twice the steam in the rain */
  for (int i = 0; i < n; i++) {
    if (!(p[i].flags & SR_SPF_STEAM)) continue;
    float dx = p[i].x - eye.x, dz = p[i].z - eye.z;
    if (dx * dx + dz * dz > dist * dist) continue;
    float base = p[i].kind == SR_SP_STALL ? 2.4f : p[i].kind == SR_SP_VENT ? 0.4f : 0.0f;
    for (int k = 0; k < puffs; k++) {
      float ph = fmodf(time_s * 0.35f + (float)k / (float)puffs + (float)(i % 7) * 0.13f, 1.0f);
      float y = base + ph * 3.2f, s = 0.9f + ph * 2.0f;
      float drift = ph * 0.8f;
      int frame = (int)(ph * 4.0f) % 4;
      uint8_t v = (uint8_t)(70.0f * (1.0f - ph));
      sr_mesh_sprite(out, &bb, sr_v3(p[i].x + drift, y, p[i].z), s, s, SR_SPR_STEAM + frame, SR_SPRITE_FACING, 0,
                     (SrColor){v, v, (uint8_t)(v + 6), 255}, 255);
    }
  }
}

void sr_build_ripples(const SrStreetProp *p, int n, SrVec3 eye, float rain, float time_s, SrMesh *out) {
  if (rain < 0.1f) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < n; i++) {
    if (p[i].kind != SR_SP_PUDDLE) continue;
    float dx = p[i].x - eye.x, dz = p[i].z - eye.z;
    if (dx * dx + dz * dz > 30.0f * 30.0f) continue;
    for (int k = 0; k < 3; k++) {
      float ox = ((float)((i * 7 + k * 3) % 5) - 2.0f) * 0.35f, oz = ((float)((i * 3 + k * 5) % 5) - 2.0f) * 0.25f;
      int frame = sr_anim_frame((double)time_s, 6.0f, 3, (uint32_t)(i * 31 + k));
      sr_mesh_sprite(out, &bb, sr_v3(p[i].x + ox, 0.03f, p[i].z + oz), 0.7f, 0.7f, SR_SPR_RIPPLE + frame,
                     SR_SPRITE_DECAL, 0, (SrColor){60, 70, 90, 255}, 255);
    }
  }
}

SrColor sr_rain_tint(const SrLight *l, int n, SrVec3 p, SrColor base) {
  float r = (float)base.r, g = (float)base.g, b = (float)base.b;
  for (int i = 0; i < n; i++) {
    float dx = l[i].pos.x - p.x, dy = l[i].pos.y - p.y, dz = l[i].pos.z - p.z;
    float d2 = dx * dx + dy * dy + dz * dz, rr = l[i].radius * 0.6f;
    if (d2 > rr * rr) continue;
    float a = 1.0f - sqrtf(d2) / rr;
    r += l[i].r * 160.0f * a;
    g += l[i].g * 160.0f * a;
    b += l[i].b * 160.0f * a;
  }
  return (SrColor){(uint8_t)SR_MIN(255.0f, r), (uint8_t)SR_MIN(255.0f, g), (uint8_t)SR_MIN(255.0f, b), base.a};
}
