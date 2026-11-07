/* Sprites: quads from the sprite atlas that face the camera (upright or fully) or lie flat as
 * decals, animation frames from game time, the glow fade that mirrors the shader, and the share of
 * lit windows by building use and hour. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

SrBillboard sr_billboard(SrVec3 eye, float yaw, float pitch) {
  SrBillboard b;
  b.eye = eye;
  SrVec3 f = sr_dir_from_yaw_pitch(yaw, pitch);
  b.right = sr_v3_norm(sr_v3_cross(f, sr_v3(0, 1, 0)));
  b.up = sr_v3_cross(b.right, f);
  return b;
}

void sr_mesh_sprite(SrMesh *m, const SrBillboard *bb, SrVec3 p, float w, float h, int sprite, SrSpriteMode mode,
                    float yaw, SrColor col, int em) {
  SrAtlasCell c = sr_atlas_cell(SR_LAYER_SPRITES, sprite);
  float hw = w * 0.5f, hh = h * 0.5f;
  if (mode == SR_SPRITE_DECAL) {
    float cy = cosf(yaw), sy = sinf(yaw);
    SrVec3 ax = sr_v3(cy * hw, 0, -sy * hw), az = sr_v3(sy * hh, 0, cy * hh);
    sr_mesh_quad(m, sr_v3(p.x - ax.x + az.x, p.y, p.z - ax.z + az.z), sr_v3(p.x + ax.x + az.x, p.y, p.z + ax.z + az.z),
                 sr_v3(p.x + ax.x - az.x, p.y, p.z + ax.z - az.z), sr_v3(p.x - ax.x - az.x, p.y, p.z - ax.z - az.z),
                 c.u0, c.v1, c.u1, c.v0, col, c.layer, em, 0);
    return;
  }
  SrVec3 r, u;
  if (mode == SR_SPRITE_UPRIGHT) {
    float dx = bb->eye.x - p.x, dz = bb->eye.z - p.z, len = sqrtf(dx * dx + dz * dz);
    r = len > 1e-4f ? sr_v3(dz / len, 0, -dx / len) : bb->right;
    u = sr_v3(0, 1, 0);
    p = sr_v3(p.x, p.y + hh, p.z); /* bottom centre to centre */
  } else {
    r = bb->right;
    u = bb->up;
  }
  SrVec3 rx = sr_v3_scale(r, hw), uy = sr_v3_scale(u, hh);
  sr_mesh_quad(m, sr_v3_sub(sr_v3_sub(p, rx), uy), sr_v3_sub(sr_v3_add(p, rx), uy), sr_v3_add(sr_v3_add(p, rx), uy),
               sr_v3_add(sr_v3_sub(p, rx), uy), c.u0, c.v1, c.u1, c.v0, col, c.layer, em, 0);
}

int sr_anim_frame(double t, float fps, int frames, uint32_t seed) {
  if (frames <= 1) return 0;
  double f = t * (double)fps + (double)(seed % 997u) * 0.37;
  long long k = (long long)floor(f);
  int r = (int)(k % frames);
  return r < 0 ? r + frames : r;
}

float sr_glow_fade(float z, float fog_start, float fog_end) {
  float f = (z - fog_start) / (fog_end - fog_start);
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  return 1.0f - f;
}

float sr_fog_with_mist(float f, float mist, float y, float z) {
  float low = (3.0f - y) / 3.0f, near = z / 25.0f;
  low = low < 0 ? 0 : low > 1 ? 1 : low;
  near = near < 0 ? 0 : near > 1 ? 1 : near;
  f += mist * low * near;
  return f < 0 ? 0 : f > 1 ? 1 : f;
}

/* Smooth bump between two hours (wrapping at midnight). */
static float hour_curve(float h, float rise, float peak_from, float peak_to, float fall) {
  if (h < rise) h += 24.0f;
  if (peak_from < rise) peak_from += 24.0f;
  if (peak_to < rise) peak_to += 24.0f;
  if (fall < rise) fall += 24.0f;
  if (h <= rise || h >= fall) return 0.0f;
  if (h < peak_from) return (h - rise) / (peak_from - rise);
  if (h <= peak_to) return 1.0f;
  return (fall - h) / (fall - peak_to);
}

float sr_window_share(int usage, float hour) {
  hour = fmodf(hour, 24.0f);
  if (hour < 0) hour += 24.0f;
  switch (usage) {
    case SR_USE_OFFICE: /* busiest during the grey day, a few late workers at night */
      return 0.10f + 0.60f * hour_curve(hour, 7.0f, 9.0f, 18.0f, 23.0f);
    case SR_USE_HOME: /* lit in the evening, dark before dawn */
      return 0.18f + 0.57f * hour_curve(hour, 16.0f, 20.0f, 23.0f, 3.5f);
    case SR_USE_INDUSTRIAL: return 0.40f; /* shifts around the clock */
    default: return 0.02f;                /* vacant */
  }
}
