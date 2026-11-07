/* Lighting helpers: pools of light on the ground and splashes on the walls under lamps, signs,
 * vending machines and fires (decals in the glow pass, cheap on every device), the colour grade of
 * each district blended across borders, and the brightness curve of the post pass. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

int sr_build_light_pools(const SrCity *c, const SrLight *l, int n, SrVec3 eye, SrMesh *out) {
  SrBillboard bb = sr_billboard(eye, 0, 0);
  int pools = 0;
  for (int i = 0; i < n; i++) {
    float dx = l[i].pos.x - eye.x, dz = l[i].pos.z - eye.z;
    if (dx * dx + dz * dz > 90.0f * 90.0f) continue;
    int blk = sr_city_block_at(c, (int32_t)(l[i].pos.x * 100.0f), (int32_t)(l[i].pos.z * 100.0f));
    float ground = 0.025f;
    if (blk >= 0) { /* on the pavement the ground is 12 cm higher */
      int32_t ox, oy;
      sr_city_block_origin(blk, c->grid, &ox, &oy);
      float st = (float)SR_STREET_CM / 100.0f;
      if (l[i].pos.x - (float)ox / 100.0f >= st && l[i].pos.z - (float)oy / 100.0f >= st) ground = 0.145f;
    }
    float size = l[i].radius * (l[i].pos.y > 5.5f ? 0.55f : 0.45f);
    SrColor col = {(uint8_t)(clamp01(l[i].r * 0.42f) * 255.0f), (uint8_t)(clamp01(l[i].g * 0.42f) * 255.0f),
                   (uint8_t)(clamp01(l[i].b * 0.42f) * 255.0f), 255};
    sr_mesh_sprite(out, &bb, sr_v3(l[i].pos.x, ground, l[i].pos.z), size, size, SR_SPR_POOL, SR_SPRITE_DECAL, 0, col,
                   255);
    pools++;
    if (l[i].pos.y > 3.0f && l[i].pos.y < 5.5f) { /* a sign: its light splashes up the wall behind it */
      SrAtlasCell a = sr_atlas_cell(SR_LAYER_SPRITES, SR_SPR_GLOW);
      float z = l[i].pos.z - 0.46f, x = l[i].pos.x, w = 3.2f;
      SrColor wc = {(uint8_t)(col.r * 2 / 3), (uint8_t)(col.g * 2 / 3), (uint8_t)(col.b * 2 / 3), 255};
      sr_mesh_quad(out, sr_v3(x - w, l[i].pos.y - 2.6f, z), sr_v3(x + w, l[i].pos.y - 2.6f, z),
                   sr_v3(x + w, l[i].pos.y + 2.6f, z), sr_v3(x - w, l[i].pos.y + 2.6f, z), a.u0, a.v1, a.u1, a.v0, wc,
                   a.layer, 255, 0);
    }
  }
  return pools;
}

static void grade_of(const SrCity *c, int district, float lift[3], float gain[3]) {
  const SrPalette *p = sr_palette(district >= 0 ? c->districts[district].type : SR_D_RESIDENTIAL);
  float fog[3] = {(float)p->fog.r / 255.0f, (float)p->fog.g / 255.0f, (float)p->fog.b / 255.0f};
  float neon[3] = {(float)p->neon[0].r / 255.0f, (float)p->neon[0].g / 255.0f, (float)p->neon[0].b / 255.0f};
  for (int k = 0; k < 3; k++) {
    lift[k] = fog[k] * 0.18f;          /* shadows lean toward the district's fog */
    gain[k] = 0.96f + 0.08f * neon[k]; /* highlights toward its neon */
  }
}

void sr_grade_at(const SrCity *c, float x, float z, float lift[3], float gain[3]) {
  /* the average over a disc of 15 m (three rings and the centre): crossing a border blends over
   * about 30 m */
  float l[3] = {0, 0, 0}, g[3] = {0, 0, 0};
  const int samples = 25;
  for (int i = 0; i < samples; i++) {
    int ring = i / 8;
    float a = (float)(i % 8) / 8.0f * 2.0f * SR_PI + (float)ring * 0.39f, r = i == 24 ? 0.0f : 5.0f * (float)(ring + 1);
    int d = sr_city_district_at(c, (int32_t)((x + cosf(a) * r) * 100.0f), (int32_t)((z + sinf(a) * r) * 100.0f));
    float li[3], gi[3];
    grade_of(c, d, li, gi);
    for (int k = 0; k < 3; k++) {
      l[k] += li[k] / (float)samples;
      g[k] += gi[k] / (float)samples;
    }
  }
  for (int k = 0; k < 3; k++) {
    lift[k] = l[k];
    gain[k] = g[k];
  }
}

float sr_brightness(float v, int level) {
  /* lifts the dark tones; white and the fog distance stay where they are */
  float b = (float)(level < -2 ? -2 : level > 2 ? 2 : level);
  float u = 1.0f - clamp01(v);
  return clamp01(v + b * 0.05f * u * u * u);
}
