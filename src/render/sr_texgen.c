/* Procedural textures: every layer of the texture array is generated at start-up from code, so the
 * download carries no art. Layer map in sr_render.h; atlas layers hold 4x4 cells of 32 px. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void px(uint8_t *rgba, int x, int y, int r, int g, int b, int a) {
  uint8_t *p = rgba + ((size_t)y * SR_TEX_SIZE + (size_t)x) * 4;
  p[0] = (uint8_t)SR_CLAMP(r, 0, 255);
  p[1] = (uint8_t)SR_CLAMP(g, 0, 255);
  p[2] = (uint8_t)SR_CLAMP(b, 0, 255);
  p[3] = (uint8_t)SR_CLAMP(a, 0, 255);
}

static int noise8(int layer, int x, int y) {
  return (int)(sr_mix64(((uint64_t)layer << 40) ^ ((uint64_t)x << 20) ^ (uint64_t)y) & 31u) - 16;
}

/* ---- atlases ------------------------------------------------------------------------------------ */

SrAtlasCell sr_atlas_cell(int first_layer, int cell) {
  SrAtlasCell a;
  if (cell < 0) cell = 0;
  a.layer = first_layer + cell / 16;
  int c = cell % 16, cx = c % 4, cy = c / 4;
  const float in = 0.5f / SR_TEX_SIZE; /* half a texel inside, so nearest sampling never bleeds */
  a.u0 = (float)cx / 4.0f + in;
  a.u1 = (float)(cx + 1) / 4.0f - in;
  a.v0 = (float)cy / 4.0f + in;
  a.v1 = (float)(cy + 1) / 4.0f - in;
  return a;
}

SrAtlasCell sr_icon_cell(int poi) { return sr_atlas_cell(SR_LAYER_ICONS, (unsigned)poi < SR_POI_COUNT ? poi : 0); }

/* Pixel inside a 32 px atlas cell. */
static void cell_px(uint8_t *rgba, int cell, int x, int y, int r, int g, int b, int a) {
  if (x < 0 || y < 0 || x > 31 || y > 31) return;
  px(rgba, (cell % 4) * 32 + x, ((cell % 16) / 4) * 32 + y, r, g, b, a);
}

static void icon_shape(int poi, uint16_t rows[16]) {
  memset(rows, 0, sizeof(uint16_t) * 16);
  switch (poi) {
    case SR_POI_FOOD: /* noodle bowl */
      rows[3] = 0x0420;
      rows[4] = 0x0840;
      rows[5] = 0x1080;
      rows[7] = 0xFFFF;
      rows[8] = 0x7FFE;
      rows[9] = 0x3FFC;
      rows[10] = 0x1FF8;
      rows[11] = 0x0FF0;
      break;
    case SR_POI_BAR: /* glass */
      rows[2] = 0x7FFE;
      rows[3] = 0x3FFC;
      rows[4] = 0x1FF8;
      rows[5] = 0x0FF0;
      rows[6] = 0x07E0;
      rows[7] = 0x03C0;
      rows[8] = 0x0180;
      rows[9] = 0x0180;
      rows[10] = 0x0180;
      rows[11] = 0x0FF0;
      break;
    case SR_POI_CLINIC:
    case SR_POI_RIPPERDOC: /* cross */
      for (int i = 3; i < 13; i++) rows[i] = 0x03C0;
      rows[6] = rows[7] = rows[8] = rows[9] = 0x3FFC;
      break;
    case SR_POI_GUNSHOP:
      rows[5] = 0x7FFE;
      rows[6] = 0x7FFE;
      rows[7] = 0x7F80;
      rows[8] = 0x1C00;
      rows[9] = 0x1C00;
      rows[10] = 0x3800;
      rows[11] = 0x3800;
      break;
    case SR_POI_CAPSULE:
    case SR_POI_HOME: /* bed */
      rows[7] = 0x6000;
      rows[8] = 0x7FFE;
      rows[9] = 0x7FFE;
      rows[10] = 0x6006;
      rows[11] = 0x6006;
      break;
    case SR_POI_POLICE: /* star */
      rows[3] = 0x0180;
      rows[4] = 0x03C0;
      rows[5] = 0x7FFE;
      rows[6] = 0x3FFC;
      rows[7] = 0x1FF8;
      rows[8] = 0x3FFC;
      rows[9] = 0x781E;
      rows[10] = 0x6006;
      break;
    default: /* chip: square with pins */
      rows[3] = rows[12] = 0x2AA8;
      for (int i = 4; i < 12; i++) rows[i] = 0x7FFE;
      rows[7] = rows[8] = 0x781E;
      break;
  }
}

/* ---- shared helpers -------------------------------------------------------------------------- */

static uint32_t h32(int layer, int a, int b) {
  return (uint32_t)(sr_mix64(((uint64_t)(unsigned)layer * 0x9E37u) ^ ((uint64_t)(unsigned)a << 24) ^
                             (uint64_t)(unsigned)b) >>
                    16);
}

/* Glass texel: alpha 200 marks glass for the window shader; red carries the window's own random
 * value (the shader decides lit or dark from it), green a shade inside the pane (blinds, depth). */
static void glass(uint8_t *rgba, int x, int y, uint32_t window, int shade) {
  px(rgba, x, y, (int)(window & 255u), shade, 0, 200);
}

static float smooth01(float x) { return x <= 0 ? 0 : x >= 1 ? 1 : x * x * (3 - 2 * x); }

/* ---- materials -------------------------------------------------------------------------------- */

static void old_windows(int layer, uint8_t *rgba) {
  const int S = SR_TEX_SIZE;
  int cell = layer == SR_LAYER_WIN_A ? 16 : layer == SR_LAYER_WIN_B ? 32 : 8;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y), wx = x % cell, wy = y % cell;
      bool window = wx > cell / 5 && wx < cell - cell / 5 && wy > cell / 4 && wy < cell - cell / 6;
      if (window)
        glass(rgba, x, y, h32(layer, x / cell, y / cell), 60 + (wy * 120) / cell);
      else
        px(rgba, x, y, 110 + n, 110 + n, 118 + n, 255);
    }
}

/* Window modules: a pitch in px and a body drawn per cell. */
static void window_module(int k, uint8_t *rgba) {
  const int S = SR_TEX_SIZE, layer = SR_LAYER_WINMOD_FIRST + k;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y);
      int wall = 108 + n;
      switch (k) {
        case 0: { /* office strip: continuous bands, thin mullions every 16 px */
          int wy = y % 32;
          bool band = wy >= 7 && wy <= 25, mull = x % 16 < 2;
          if (band && !mull)
            glass(rgba, x, y, h32(layer, x / 16, y / 32), 90 + (wy - 7) * 6);
          else
            px(rgba, x, y, wall - 10, wall - 8, wall, 255);
          break;
        }
        case 1: { /* residential with balconies */
          int wx = x % 32, wy = y % 32;
          if (wy >= 27 && wy <= 29)
            px(rgba, x, y, wall + 30, wall + 30, wall + 32, 255); /* slab */
          else if (wy >= 21 && wy < 27 && (wx % 4 == 0 || wy == 21))
            px(rgba, x, y, 70, 72, 78, 255); /* railing */
          else if (wx >= 5 && wx <= 26 && wy >= 4 && wy <= 24)
            glass(rgba, x, y, h32(layer, x / 32, y / 32), wx < 15 ? 70 : 140);
          else
            px(rgba, x, y, wall + 8, wall + 2, wall - 4, 255);
          break;
        }
        case 2: { /* capsule hive: small round windows on a tight grid */
          int wx = x % 16 - 8, wy = y % 16 - 8;
          if (wx * wx + wy * wy <= 18)
            glass(rgba, x, y, h32(layer, x / 16, y / 16), 120);
          else
            px(rgba, x, y, wall - 20 + (x % 16 == 0 ? -20 : 0), wall - 18, wall - 12, 255);
          break;
        }
        case 3: { /* glass curtain wall with mullions */
          bool mull = x % 16 < 1 || y % 32 < 2;
          if (mull)
            px(rgba, x, y, 60, 66, 76, 255);
          else
            glass(rgba, x, y, h32(layer, x / 16, y / 32), 40 + (y % 32) * 4);
          break;
        }
        case 4: { /* brick with stone sills */
          int wx = x % 32, wy = y % 32, row = y / 6, off = (row % 2) * 6;
          if (wx >= 9 && wx <= 22 && wy >= 5 && wy <= 21)
            glass(rgba, x, y, h32(layer, x / 32, y / 32), wx == 15 || wx == 16 ? 30 : 130);
          else if (wx >= 7 && wx <= 24 && wy >= 22 && wy <= 24)
            px(rgba, x, y, 170 + n, 165 + n, 150 + n, 255);
          else if (y % 6 == 0 || (x + off) % 12 == 0)
            px(rgba, x, y, 88, 82, 76, 255);
          else
            px(rgba, x, y, 150 + n, 72 + n / 2, 56 + n / 2, 255);
          break;
        }
        case 5: { /* shuttered: some windows behind closed slats */
          int wx = x % 32, wy = y % 32;
          bool closed = (h32(layer, x / 32, y / 32 + 99) & 3u) == 0;
          if (wx >= 6 && wx <= 25 && wy >= 5 && wy <= 23) {
            if (closed)
              px(rgba, x, y, 80 + (wy % 3 == 0 ? -25 : 0), 74, 66, 255);
            else
              glass(rgba, x, y, h32(layer, x / 32, y / 32), 110);
          } else {
            px(rgba, x, y, wall + 14, wall + 6, wall - 6, 255);
          }
          break;
        }
        case 6: { /* boarded up: planks over most windows, stains */
          int wx = x % 32, wy = y % 32;
          bool open = (h32(layer, x / 32, y / 32 + 7) % 5u) == 0;
          if (wx >= 6 && wx <= 25 && wy >= 5 && wy <= 23) {
            if (open)
              glass(rgba, x, y, h32(layer, x / 32, y / 32) | 1u, 60);
            else
              px(rgba, x, y, 96 + n + (wy % 6 == 0 ? -30 : 0), 70 + n / 2, 44, 255);
          } else {
            int stain = (int)(h32(layer, x / 8, y / 4) % 40u);
            px(rgba, x, y, wall - stain / 2, wall - stain / 2, wall - stain / 3, 255);
          }
          break;
        }
        default: { /* clerestory: corrugated metal with one high glass band per 64 px */
          int wy = y % 64;
          if (wy >= 4 && wy <= 13 && x % 16 > 1)
            glass(rgba, x, y, h32(layer, x / 16, y / 64), 100);
          else
            px(rgba, x, y, 116 + n + (x % 6 < 2 ? 18 : 0), 120 + n, 124 + n, 255);
          break;
        }
      }
    }
}

/* Ground-floor modules, 128 px tall for the 4.5 m band (row 0 at the top). */
static void base_module(int k, uint8_t *rgba) {
  const int S = SR_TEX_SIZE, layer = SR_LAYER_BASE_FIRST + k;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y);
      switch (k) {
        case 0: /* shopfront: fascia, lit display, shelves, kick plate */
          if (y < 22)
            px(rgba, x, y, 34 + n / 2, 32 + n / 2, 40 + n / 2, 255);
          else if (y >= 112)
            px(rgba, x, y, 60 + n, 62 + n, 66 + n, 255);
          else if (x % 64 < 3 || y < 25 || y > 109)
            px(rgba, x, y, 40, 42, 48, 255);
          else if ((y - 25) % 28 == 0 || (x % 64 > 40 && x % 64 < 60 && y > 60))
            px(rgba, x, y, 120, 96, 70, 255); /* shelves, a figure */
          else
            px(rgba, x, y, 235 + n / 2, 220 + n / 2, 180 + n / 2, 255);
          break;
        case 1: { /* roller shutter with tags */
          int tag = (int)(h32(layer, x / 10, y / 9) % 9u);
          int v = 72 + n / 2 + (y % 5 == 0 ? -30 : 0);
          if (y > 40 && y < 100 && tag == 0)
            px(rgba, x, y, 200, 60 + n, 160, 255);
          else
            px(rgba, x, y, v, v + 2, v + 6, 255);
          break;
        }
        case 2: /* lobby: tall glass doors, warm interior */
          if (y < 14 || x % 32 < 2 || y >= 124)
            px(rgba, x, y, 50 + n / 2, 50 + n / 2, 58 + n / 2, 255);
          else if (x % 64 == 32 || x % 64 == 33)
            px(rgba, x, y, 30, 30, 34, 255);
          else
            px(rgba, x, y, 120 + n / 2 + (y > 90 ? 20 : 0), 108 + n / 2, 80 + n / 2, 255);
          break;
        case 3: /* loading dock: roll-up door with hazard stripes */
          if (y > 116 && ((x + y) / 6) % 2 == 0)
            px(rgba, x, y, 230, 190, 40, 255);
          else if (y > 116)
            px(rgba, x, y, 30, 30, 30, 255);
          else if (x % 64 < 6 || y < 16)
            px(rgba, x, y, 70 + n, 74 + n, 80 + n, 255);
          else
            px(rgba, x, y, 88 + n / 2 + (y % 8 == 0 ? -30 : 0), 86 + n / 2, 78 + n / 2, 255);
          break;
        case 4: /* stall front: open counter, hanging goods, warm light */
          if (y > 84)
            px(rgba, x, y, 110 + n, 70 + n / 2, 40, 255);
          else if (y < 12)
            px(rgba, x, y, 180, 40 + n, 40, 255); /* canvas edge */
          else if (y < 40 && (x % 16 > 5 && x % 16 < 11) && (x / 16 + y / 8) % 3 != 0)
            px(rgba, x, y, 255, 140 + n, 60, 255); /* lanterns, goods */
          else
            px(rgba, x, y, 200 + n / 2, 150 + n / 2, 90 + n / 2, 255);
          break;
        default: { /* blank concrete with posters */
          uint32_t hp = h32(layer, x / 24, y / 34);
          bool poster = (hp & 3u) != 0 && x % 24 > 2 && x % 24 < 22 && y % 34 > 3 && y % 34 < 31 && y > 30;
          if (poster)
            px(rgba, x, y, 60 + (int)(hp % 130u), 60 + (int)((hp >> 8) % 130u), 60 + (int)((hp >> 16) % 130u), 255);
          else
            px(rgba, x, y, 76 + n, 74 + n, 72 + n, 255);
          break;
        }
      }
    }
}

static void trim_layer(int k, uint8_t *rgba) {
  const int S = SR_TEX_SIZE, layer = SR_LAYER_TRIM_FIRST + k;
  for (int y = 0; y < S; y++)
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y);
      switch (k) {
        case 0: { /* roof tiles */
          int row = y / 8, off = (row % 2) * 8;
          bool edge = y % 8 >= 6 || (x + off) % 16 == 0;
          px(rgba, x, y, edge ? 70 : 130 + n, edge ? 40 : 66 + n / 2, edge ? 34 : 50 + n / 2, 255);
          break;
        }
        case 1: { /* corrugated metal */
          int v = 120 + n / 2 + (int)(18 * sinf((float)x * 0.8f));
          px(rgba, x, y, v, v + 4, v + 8, 255);
          break;
        }
        case 2: /* parapet: concrete with a cap line */
          px(rgba, x, y, (y < 10 ? 160 : 120) + n, (y < 10 ? 160 : 118) + n, (y < 10 ? 162 : 116) + n, 255);
          break;
        case 3: { /* cornice: stone moulding bands */
          int band = (y / 16) % 4;
          int v = band == 0 ? 175 : band == 1 ? 140 : band == 2 ? 158 : 120;
          px(rgba, x, y, v + n, v - 4 + n, v - 12 + n, 255);
          break;
        }
        case 4: /* gravel roof */ px(rgba, x, y, 70 + n * 2, 70 + n * 2, 72 + n * 2, 255); break;
        default: { /* helipad: H in a circle */
          float dx = (float)(x - 64), dy = (float)(y - 64), r = sqrtf(dx * dx + dy * dy);
          bool ring = r > 50 && r < 56;
          bool hbar = (x > 40 && x < 50) || (x > 78 && x < 88) || (y > 59 && y < 69 && x > 40 && x < 88);
          bool h = hbar && y > 36 && y < 92;
          int v = ring || h ? 220 : 52 + n;
          px(rgba, x, y, v, ring ? 200 : v, ring ? 40 : v, 255);
          break;
        }
      }
    }
}

/* ---- icon atlas, sprites, decals, ads, script, noise --------------------------------------------- */

static void icon_atlas(int k, uint8_t *rgba) {
  memset(rgba, 0, (size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  for (int c = 0; c < 16; c++) {
    int poi = k * 16 + c;
    if (poi >= SR_POI_COUNT) break;
    uint16_t rows[16];
    icon_shape(poi, rows);
    for (int y = 0; y < 32; y++)
      for (int x = 0; x < 32; x++) {
        bool on = (rows[y / 2] >> (15 - x / 2)) & 1u;
        cell_px(rgba, c, x, y, 255, 255, 255, on ? 255 : 0);
      }
  }
}

/* One sprite into its cell: white, alpha is the strength (glow sprites) or a cut-out mask. */
static void sprite_cell(int id, uint8_t *rgba) {
  int c = id % 16;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++) {
      float dx = ((float)x - 15.5f) / 16.0f, dy = ((float)y - 15.5f) / 16.0f, d = sqrtf(dx * dx + dy * dy);
      float a = 0;
      uint32_t hn = h32(SR_LAYER_SPRITES + id, x, y);
      float nz = (float)(hn % 100u) / 100.0f;
      if (id == SR_SPR_GLOW) {
        a = (1 - d) * (1 - d);
      } else if (id == SR_SPR_POOL) {
        a = powf(SR_MAX(0.0f, 1 - d), 1.6f);
      } else if (id == SR_SPR_HALO) {
        a = expf(-4.0f * d * d) * (d < 1 ? 1.0f : 0.0f);
      } else if (id == SR_SPR_CONE) { /* apex at the bottom, opening upwards */
        float t = 1.0f - (float)y / 31.0f, w = 0.08f + 0.92f * t;
        a = SR_MAX(0.0f, 1 - fabsf(dx) / w) * (0.25f + 0.75f * (1 - t));
      } else if (id >= SR_SPR_STEAM && id < SR_SPR_STEAM + 4) {
        float f = (float)(id - SR_SPR_STEAM), r = 0.45f + 0.15f * f;
        a = SR_MAX(0.0f, 1 - d / r) * (0.75f - 0.15f * f) * (0.6f + 0.4f * nz);
      } else if (id >= SR_SPR_SPLASH && id < SR_SPR_SPLASH + 4) {
        float f = (float)(id - SR_SPR_SPLASH), r = 0.25f + 0.22f * f;
        a = (fabsf(d - r) < 0.09f && dy > -0.2f) ? 1.0f - 0.2f * f : 0.0f;
      } else if (id >= SR_SPR_SMOKE && id < SR_SPR_SMOKE + 4) {
        float f = (float)(id - SR_SPR_SMOKE), r = 0.55f + 0.12f * f;
        a = SR_MAX(0.0f, 1 - d / r) * (0.9f - 0.15f * f) * (0.5f + 0.5f * nz);
      } else if (id >= SR_SPR_SPARK && id < SR_SPR_SPARK + 4) {
        a = (hn % 37u) == (unsigned)(id - SR_SPR_SPARK) && d < 0.8f ? 1.0f : 0.0f;
      } else if (id == SR_SPR_DRIP) {
        a = fabsf(dx) < 0.07f ? SR_MAX(0.0f, 1 - fabsf(dy)) : 0.0f;
      } else if (id >= SR_SPR_LITTER && id < SR_SPR_LITTER + 4) {
        int v = id - SR_SPR_LITTER;
        a = (fabsf(dx + 0.1f * (float)v) < 0.35f && fabsf(dy - 0.05f * (float)v) < 0.25f + 0.05f * (float)v) ? 1.0f
                                                                                                             : 0.0f;
      } else if (id == SR_SPR_BIRD || id == SR_SPR_BIRD + 1) {
        float wing = id == SR_SPR_BIRD ? 0.45f : -0.2f;
        a = fabsf(dy - fabsf(dx) * wing) < 0.08f && fabsf(dx) < 0.8f ? 1.0f : 0.0f;
      } else if (id == SR_SPR_DRONE) {
        a = (fabsf(dy) < 0.12f && fabsf(dx) < 0.7f) || d < 0.18f ? 1.0f : 0.0f;
      } else if (id == SR_SPR_FLARE) {
        a = SR_MAX(SR_MAX(0.0f, 1 - fabsf(dx) * 12) * (1 - fabsf(dy)),
                   SR_MAX(0.0f, 1 - fabsf(dy) * 12) * (1 - fabsf(dx)));
        a = SR_MAX(a, (1 - d) * (1 - d) * 0.8f);
      } else if (id >= SR_SPR_RIPPLE && id < SR_SPR_RIPPLE + 3) {
        float r = 0.3f + 0.3f * (float)(id - SR_SPR_RIPPLE);
        a = fabsf(d - r) < 0.06f ? 0.8f : 0.0f;
      } else if (id == SR_SPR_STREAK) { /* reflection: bright at the top, broken lines below */
        float t = (float)y / 31.0f;
        a = SR_MAX(0.0f, 1 - fabsf(dx) * 2.2f) * (1 - t) * (y % 3 == 0 ? 0.55f : 1.0f);
      } else if (id == SR_SPR_HOLO) {
        bool frame = fabsf(dx) > 0.82f || fabsf(dy) > 0.82f;
        a = d < 1.3f ? (frame ? 1.0f : (y % 2 ? 0.45f : 0.2f)) : 0.0f;
      } else if (id == SR_SPR_TREE) {
        bool trunk = fabsf(dx) < 0.08f && dy > 0.2f;
        float cr = sqrtf(dx * dx + (dy + 0.25f) * (dy + 0.25f));
        a = trunk ? 0.9f : cr < 0.6f ? (0.5f + 0.5f * nz) * (1 - cr / 0.6f) + 0.2f : 0.0f;
      }
      cell_px(rgba, c, x, y, 255, 255, 255, (int)(smooth01(a) * 255.0f + 0.5f));
    }
}

static void sprite_atlas(int k, uint8_t *rgba) {
  memset(rgba, 0, (size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  for (int c = 0; c < 16; c++)
    if (k * 16 + c < SR_SPR_COUNT) sprite_cell(k * 16 + c, rgba);
}

static void decal_cell(int id, uint8_t *rgba) {
  int c = id % 16;
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++) {
      float dx = ((float)x - 15.5f) / 16.0f, dy = ((float)y - 15.5f) / 16.0f, d = sqrtf(dx * dx + dy * dy);
      uint32_t hn = h32(SR_LAYER_DECALS + id, x, y);
      int r = 255, g = 255, b = 255, a = 0;
      switch (id) {
        case SR_DECAL_MANHOLE:
          if (d < 0.92f) {
            a = 255;
            r = g = b = (d > 0.8f || (x % 6 < 2 && d < 0.7f)) ? 60 : 38;
          }
          break;
        case SR_DECAL_DRAIN:
          if (fabsf(dx) < 0.9f && fabsf(dy) < 0.5f) {
            a = 255;
            r = g = b = (x % 5 < 2 && fabsf(dy) < 0.4f) ? 10 : 70;
          }
          break;
        case SR_DECAL_CROSSWALK: a = (x % 16) < 9 ? 230 : 0; break;
        case SR_DECAL_ARROW:
          a = ((fabsf(dx) < 0.12f && dy > -0.3f && dy < 0.8f) ||
               (dy < -0.2f && dy > -0.8f && fabsf(dx) < (dy + 0.8f) * 0.8f))
                  ? 220
                  : 0;
          break;
        case SR_DECAL_CRACK:
        case SR_DECAL_CRACK + 1: {
          float line = sinf((float)x * 0.35f + (float)(id - SR_DECAL_CRACK) * 2.0f) * 0.3f;
          a = fabsf(dy - line) < 0.05f + 0.03f * (float)(hn % 3u) ? 255 : 0;
          r = g = b = 20;
          break;
        }
        case SR_DECAL_STAIN:
        case SR_DECAL_STAIN + 1:
          a = d < 0.7f + 0.25f * (float)(hn % 100u) / 100.0f ? 255 : 0;
          r = 20;
          g = 18;
          b = 16;
          break;
        case SR_DECAL_PUDDLE:
        case SR_DECAL_PUDDLE + 1: {
          float wob = 0.15f * sinf(atan2f(dy, dx) * (id == SR_DECAL_PUDDLE ? 3.0f : 5.0f));
          a = d < 0.78f + wob ? 255 : 0;
          r = 12;
          g = 14;
          b = 20;
          break;
        }
        case SR_DECAL_STOPLINE: a = fabsf(dy) < 0.3f ? 230 : 0; break;
        default: /* graffiti tags and posters */
          if (id >= SR_DECAL_TAG && id < SR_DECAL_TAG + 6) {
            int v = id - SR_DECAL_TAG;
            float s = sinf((float)x * (0.3f + 0.07f * (float)v)) * 0.35f + 0.15f * sinf((float)x * 1.3f + (float)v);
            a = (fabsf(dy - s) < 0.14f && fabsf(dx) < 0.9f) ||
                        (fabsf(dx - 0.6f + 0.2f * (float)v) < 0.08f && fabsf(dy) < 0.6f)
                    ? 255
                    : 0;
          } else if (id >= SR_DECAL_POSTER && id < SR_DECAL_POSTER + 4) {
            if (fabsf(dx) < 0.7f && fabsf(dy) < 0.92f) {
              a = 255;
              bool art = fabsf(dx) < 0.55f && dy > -0.75f && dy < 0.3f && (hn % 3u) != 0;
              r = art ? 220 : 240;
              g = art ? 120 + (id - SR_DECAL_POSTER) * 30 : 236;
              b = art ? 160 - (id - SR_DECAL_POSTER) * 30 : 220;
              if (dy > 0.45f && dy < 0.8f && x % 3 == 0) r = g = b = 40; /* text lines */
            }
          } else if (id == SR_DECAL_WANTED) {
            if (fabsf(dx) < 0.7f && fabsf(dy) < 0.92f) {
              a = 255;
              float fx = dx, fy = dy + 0.2f, head = sqrtf(fx * fx + (fy + 0.15f) * (fy + 0.15f));
              bool face = head < 0.28f || (fy > 0.1f && fy < 0.45f && fabsf(fx) < 0.45f - (0.45f - fy) * 0.3f);
              bool title = dy < -0.62f && x % 4 != 0;
              r = face ? 255 : title ? 30 : 236; /* the face is tinted by the vertex colour */
              g = face ? 255 : title ? 30 : 226;
              b = face ? 255 : title ? 30 : 200;
            }
          }
          break;
      }
      cell_px(rgba, c, x, y, r, g, b, a);
    }
}

static void decal_atlas(int k, uint8_t *rgba) {
  memset(rgba, 0, (size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  for (int c = 0; c < 16; c++)
    if (k * 16 + c < SR_DECAL_COUNT) decal_cell(k * 16 + c, rgba);
}

/* Advert layouts: 2x2 cells of 64 px per layer. White and greys, tinted by the corp colour; the
 * product name and logo are drawn on top as separate quads. Layer 44 holds the vandalised
 * overlays, layer 45 the LED ticker and marquee strips. */
static void ads_layer(int k, uint8_t *rgba) {
  for (int y = 0; y < SR_TEX_SIZE; y++)
    for (int x = 0; x < SR_TEX_SIZE; x++) {
      int cell = (y / 64) * 2 + x / 64, lx = x % 64, ly = y % 64, n = noise8(SR_LAYER_ADS + k, x, y);
      int layout = k * 4 + cell, v = 0, a = 255;
      bool border = lx < 2 || ly < 2 || lx > 61 || ly > 61;
      if (k < 2) {
        switch (layout % 8) {
          case 0: v = ly < 40 ? 255 - ly * 3 : 60; break;                                    /* gradient + band */
          case 1: v = ((lx / 8 + ly / 8) % 2) ? 200 : 90; break;                             /* checker */
          case 2: v = (lx - 32) * (lx - 32) + (ly - 26) * (ly - 26) < 380 ? 255 : 70; break; /* sun disc */
          case 3: v = ly > 44 ? 230 : (lx + ly) % 16 < 8 ? 170 : 110; break;                 /* stripes */
          case 4: v = lx < 22 ? 240 : 80 + ly; break;                                        /* side bar */
          case 5: v = abs(lx - 32) + abs(ly - 24) < 20 ? 255 : 60; break;                    /* diamond */
          case 6: v = ly % 12 < 6 ? 210 : 120; break;                                        /* bands */
          default: v = 140 + n * 2; break;                                                   /* photo grain */
        }
        if (border) v = 255;
      } else if (k == 2) { /* vandalism: spray, torn, cracks, dead panel */
        uint32_t hv = h32(SR_LAYER_ADS + 2, x / 3, y / 3);
        if (cell == 0)
          a = (fabsf(sinf((float)lx * 0.2f) * 18 + 30 - (float)ly) < 4) || (hv % 11u == 0) ? 255 : 0;
        else if (cell == 1)
          a = (ly > 30 + (int)(hv % 12u) && lx < 40) ? 255 : 0;
        else if (cell == 2)
          a = abs(lx - ly) < 2 || abs(lx + ly - 64) < 2 ? 255 : 0;
        else
          a = 255;
        v = cell == 1 ? 140 : cell == 3 ? 20 : 30;
      } else { /* LED dots (cell 0), marquee bulbs (cell 1), plain (2, 3) */
        if (cell == 0) {
          v = (lx % 4 < 3 && ly % 4 < 3) ? 255 : 40;
        } else if (cell == 1) {
          int bx = lx % 8 - 4, by = ly % 8 - 4;
          v = bx * bx + by * by < 6 ? 255 : 30;
        } else {
          v = 255;
        }
      }
      px(rgba, x, y, v, v, v, a);
    }
}

SrAtlasCell sr_ad_cell(int first_layer, int cell) {
  SrAtlasCell a;
  a.layer = first_layer + cell / 4;
  int c = cell % 4, cx = c % 2, cy = c / 2;
  const float in = 0.5f / SR_TEX_SIZE;
  a.u0 = (float)cx / 2.0f + in;
  a.u1 = (float)(cx + 1) / 2.0f - in;
  a.v0 = (float)cy / 2.0f + in;
  a.v1 = (float)(cy + 1) / 2.0f - in;
  return a;
}

/* 64 glyphs of a made-up neon script, 16 px each: strokes on a 3x4 point grid. */
static void script_layer(uint8_t *rgba) {
  memset(rgba, 0, (size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  static const int8_t pts[12][2] = {{3, 2},  {8, 2},  {13, 2},  {3, 6},  {8, 6},  {13, 6},
                                    {3, 10}, {8, 10}, {13, 10}, {3, 14}, {8, 14}, {13, 14}};
  for (int gl = 0; gl < 64; gl++) {
    uint64_t h = sr_mix64(0x5C21u + (uint64_t)gl);
    int strokes = 2 + (int)(h % 3u);
    int gx = (gl % 8) * 16, gy = (gl / 8) * 16;
    for (int s = 0; s < strokes; s++) {
      int a = (int)((h >> (8 + s * 8)) % 12u), b = (int)((h >> (12 + s * 8)) % 12u);
      if (a == b) b = (b + 4) % 12;
      for (int t = 0; t <= 24; t++) {
        float f = (float)t / 24.0f;
        int x = (int)((float)pts[a][0] + ((float)pts[b][0] - (float)pts[a][0]) * f + 0.5f);
        int y = (int)((float)pts[a][1] + ((float)pts[b][1] - (float)pts[a][1]) * f + 0.5f);
        for (int oy = 0; oy < 2; oy++)
          for (int ox = 0; ox < 2; ox++)
            if (x + ox < 16 && y + oy < 16) px(rgba, gx + x + ox, gy + y + oy, 255, 255, 255, 255);
      }
    }
  }
}

SrAtlasCell sr_script_cell(int glyph) {
  SrAtlasCell a;
  glyph &= 63;
  a.layer = SR_LAYER_SCRIPT;
  const float in = 0.5f / SR_TEX_SIZE;
  a.u0 = (float)(glyph % 8) / 8.0f + in;
  a.u1 = (float)(glyph % 8 + 1) / 8.0f - in;
  int row = glyph / 8;
  a.v0 = (float)row / 8.0f + in;
  a.v1 = (float)(row + 1) / 8.0f - in;
  return a;
}

/* Tileable value noise, three octaves (clouds). */
static float vnoise(int x, int y, int period, int octave) {
  int x0 = x / period, y0 = y / period, n = SR_TEX_SIZE / period;
  float fx = (float)(x % period) / (float)period, fy = (float)(y % period) / (float)period;
  float v[4];
  for (int k = 0; k < 4; k++) {
    int cx = (x0 + (k & 1)) % n, cy = (y0 + (k >> 1)) % n;
    v[k] = (float)(h32(SR_LAYER_NOISE + octave, cx, cy) % 1000u) / 1000.0f;
  }
  fx = smooth01(fx);
  fy = smooth01(fy);
  return (v[0] * (1 - fx) + v[1] * fx) * (1 - fy) + (v[2] * (1 - fx) + v[3] * fx) * fy;
}

static void noise_layer(uint8_t *rgba) {
  for (int y = 0; y < SR_TEX_SIZE; y++)
    for (int x = 0; x < SR_TEX_SIZE; x++) {
      float v = 0.55f * vnoise(x, y, 32, 0) + 0.3f * vnoise(x, y, 16, 1) + 0.15f * vnoise(x, y, 8, 2);
      int c = (int)(v * 255.0f);
      px(rgba, x, y, c, c, c, 255);
    }
}

void sr_texgen_layer(int layer, uint8_t *rgba) {
  const int S = SR_TEX_SIZE;
  if (layer == SR_LAYER_WIN_A || layer == SR_LAYER_WIN_B || layer == SR_LAYER_WIN_GLASS) {
    old_windows(layer, rgba);
    return;
  }
  if (layer >= SR_LAYER_WINMOD_FIRST && layer < SR_LAYER_BASE_FIRST) {
    window_module(layer - SR_LAYER_WINMOD_FIRST, rgba);
    return;
  }
  if (layer >= SR_LAYER_BASE_FIRST && layer < SR_LAYER_TRIM_FIRST) {
    base_module(layer - SR_LAYER_BASE_FIRST, rgba);
    return;
  }
  if (layer >= SR_LAYER_TRIM_FIRST && layer < SR_LAYER_ICONS) {
    trim_layer(layer - SR_LAYER_TRIM_FIRST, rgba);
    return;
  }
  if (layer >= SR_LAYER_ICONS && layer < SR_LAYER_SPRITES) {
    icon_atlas(layer - SR_LAYER_ICONS, rgba);
    return;
  }
  if (layer >= SR_LAYER_SPRITES && layer < SR_LAYER_DECALS) {
    sprite_atlas(layer - SR_LAYER_SPRITES, rgba);
    return;
  }
  if (layer >= SR_LAYER_DECALS && layer < SR_LAYER_ADS) {
    decal_atlas(layer - SR_LAYER_DECALS, rgba);
    return;
  }
  if (layer >= SR_LAYER_ADS && layer < SR_LAYER_SCRIPT) {
    ads_layer(layer - SR_LAYER_ADS, rgba);
    return;
  }
  if (layer == SR_LAYER_SCRIPT) {
    script_layer(rgba);
    return;
  }
  if (layer == SR_LAYER_NOISE) {
    noise_layer(rgba);
    return;
  }
  for (int y = 0; y < S; y++) {
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y);
      switch (layer) {
        case SR_LAYER_WHITE: px(rgba, x, y, 255, 255, 255, 255); break;
        case SR_LAYER_CONCRETE: px(rgba, x, y, 150 + n, 150 + n, 155 + n, 255); break;
        case SR_LAYER_BRICK: {
          int row = y / 8, off = (row % 2) * 8;
          bool mortar = (y % 8) == 0 || ((x + off) % 16) == 0;
          if (mortar)
            px(rgba, x, y, 90, 85, 80, 255);
          else
            px(rgba, x, y, 150 + n, 70 + n / 2, 55 + n / 2, 255);
          break;
        }
        case SR_LAYER_METAL: {
          bool seam = (x % 32) == 0 || (y % 32) == 0;
          bool rivet = (x % 32 == 3 || x % 32 == 28) && (y % 32 == 3 || y % 32 == 28);
          int v = seam ? 60 : rivet ? 200 : 120 + n;
          px(rgba, x, y, v, v + 5, v + 12, 255);
          break;
        }
        case SR_LAYER_ASPHALT: {
          bool line = (x >= 62 && x <= 65) && (y % 32) < 16;
          if (line)
            px(rgba, x, y, 220, 200, 90, 255);
          else
            px(rgba, x, y, 45 + n / 2, 45 + n / 2, 50 + n / 2, 255);
          break;
        }
        case SR_LAYER_SIDEWALK: {
          bool joint = (x % 32) == 0 || (y % 32) == 0;
          int v = joint ? 70 : 105 + n / 2;
          px(rgba, x, y, v, v, v + 4, 255);
          break;
        }
        case SR_LAYER_SIGN: {
          bool border = x < 4 || y < 4 || x >= S - 4 || y >= S - 4;
          px(rgba, x, y, border ? 255 : 40, border ? 255 : 40, border ? 255 : 48, 255);
          break;
        }
        case SR_LAYER_WATER: px(rgba, x, y, 10 + n / 4, 20 + n / 4, 40 + n / 2, 255); break;
        case SR_LAYER_FONT: px(rgba, x, y, 255, 255, 255, 0); break;
        default: px(rgba, x, y, 255, 0, 255, 255); break; /* reserved layer: magenta, easy to spot */
      }
    }
  }
  if (layer == SR_LAYER_FONT) {
    for (int ch = 32; ch < 127; ch++) {
      int cx, cy;
      sr_font_cell((char)ch, &cx, &cy);
      const uint8_t *g = sr_font_glyph((char)ch);
      for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
          if ((g[col] >> row) & 1u) px(rgba, cx + col, cy + row, 255, 255, 255, 255);
    }
  }
}

uint64_t sr_texgen_digest(void) {
  uint8_t *buf = (uint8_t *)malloc((size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  uint64_t h = 0;
  for (int l = 0; l < SR_TEX_LAYERS && buf; l++) {
    sr_texgen_layer(l, buf);
    h = sr_hash_bytes(h, buf, (size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  }
  free(buf);
  return h;
}
