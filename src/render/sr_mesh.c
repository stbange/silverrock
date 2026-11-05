/* CPU meshes, procedural textures and the bitmap font. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

#include <stdlib.h>
#include <string.h>

void sr_mesh_init(SrMesh *m) { memset(m, 0, sizeof *m); }

void sr_mesh_free(SrMesh *m) {
  free(m->v);
  free(m->idx);
  uint32_t gpu = m->gpu;
  memset(m, 0, sizeof *m);
  m->gpu = gpu; /* the backend releases its own handle */
}

void sr_mesh_clear(SrMesh *m) {
  m->nv = 0;
  m->ni = 0;
  m->dirty = true;
}

static bool reserve(SrMesh *m, int nv, int ni) {
  if (m->nv + nv > m->cap_v) {
    int cap = m->cap_v ? m->cap_v * 2 : 256;
    while (cap < m->nv + nv) cap *= 2;
    SrVertex *v = (SrVertex *)realloc(m->v, sizeof(SrVertex) * (size_t)cap);
    if (!v) return false; /* GCOVR_EXCL_LINE reason: realloc failure not injectable */
    m->v = v;
    m->cap_v = cap;
  }
  if (m->ni + ni > m->cap_i) {
    int cap = m->cap_i ? m->cap_i * 2 : 384;
    while (cap < m->ni + ni) cap *= 2;
    uint32_t *i = (uint32_t *)realloc(m->idx, sizeof(uint32_t) * (size_t)cap);
    if (!i) return false; /* GCOVR_EXCL_LINE reason: realloc failure not injectable */
    m->idx = i;
    m->cap_i = cap;
  }
  return true;
}

void sr_mesh_quad(SrMesh *m, SrVec3 a, SrVec3 b, SrVec3 c, SrVec3 d, float u0, float v0, float u1, float v1,
                  SrColor col, int layer, int emissive, int ground) {
  if (!reserve(m, 4, 6)) return; /* GCOVR_EXCL_LINE reason: see reserve */
  SrVec3 p[4] = {a, b, c, d};
  float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
  uint32_t base = (uint32_t)m->nv;
  for (int i = 0; i < 4; i++) {
    SrVertex *v = &m->v[m->nv++];
    v->x = p[i].x;
    v->y = p[i].y;
    v->z = p[i].z;
    v->u = uv[i][0];
    v->v = uv[i][1];
    v->r = col.r;
    v->g = col.g;
    v->b = col.b;
    v->a = col.a;
    v->layer = (uint8_t)layer;
    v->emissive = (uint8_t)emissive;
    v->ground = (uint8_t)ground;
    v->pad = 0;
  }
  static const uint32_t order[6] = {0, 1, 2, 0, 2, 3};
  for (int i = 0; i < 6; i++) m->idx[m->ni++] = base + order[i];
  m->dirty = true;
}

void sr_mesh_box(SrMesh *m, SrVec3 lo, SrVec3 hi, SrColor col, int layer, float tile, int em) {
  float w = hi.x - lo.x, h = hi.y - lo.y, d = hi.z - lo.z;
  float uw = tile > 0 ? w * tile : 1, uh = tile > 0 ? h * tile : 1, ud = tile > 0 ? d * tile : 1;
  /* front (+z), back (-z), right (+x), left (-x), top */
  sr_mesh_quad(m, sr_v3(lo.x, lo.y, hi.z), sr_v3(hi.x, lo.y, hi.z), sr_v3(hi.x, hi.y, hi.z), sr_v3(lo.x, hi.y, hi.z), 0,
               uh, uw, 0, col, layer, em, 0);
  sr_mesh_quad(m, sr_v3(hi.x, lo.y, lo.z), sr_v3(lo.x, lo.y, lo.z), sr_v3(lo.x, hi.y, lo.z), sr_v3(hi.x, hi.y, lo.z), 0,
               uh, uw, 0, col, layer, em, 0);
  sr_mesh_quad(m, sr_v3(hi.x, lo.y, hi.z), sr_v3(hi.x, lo.y, lo.z), sr_v3(hi.x, hi.y, lo.z), sr_v3(hi.x, hi.y, hi.z), 0,
               uh, ud, 0, col, layer, em, 0);
  sr_mesh_quad(m, sr_v3(lo.x, lo.y, lo.z), sr_v3(lo.x, lo.y, hi.z), sr_v3(lo.x, hi.y, hi.z), sr_v3(lo.x, hi.y, lo.z), 0,
               uh, ud, 0, col, layer, em, 0);
  SrColor roof = {(uint8_t)(col.r * 3 / 4), (uint8_t)(col.g * 3 / 4), (uint8_t)(col.b * 3 / 4), col.a};
  sr_mesh_quad(m, sr_v3(lo.x, hi.y, hi.z), sr_v3(hi.x, hi.y, hi.z), sr_v3(hi.x, hi.y, lo.z), sr_v3(lo.x, hi.y, lo.z), 0,
               0, 1, 1, roof, SR_LAYER_CONCRETE, em, 0);
}

/* Rotated box for rigid character parts: swing about the local x axis around `pivot`, then turn
 * by `yaw` about the vertical axis through `origin`. Local +z faces forward. */
void sr_mesh_part(SrMesh *m, SrVec3 origin, float yaw, SrVec3 pivot, float swing, SrVec3 lo, SrVec3 hi, SrColor col,
                  int layer, int em) {
  float cs = cosf(swing), ss = sinf(swing), cy = cosf(yaw), sy = sinf(yaw);
  SrVec3 c[8];
  for (int i = 0; i < 8; i++) {
    float x = (i & 1) ? hi.x : lo.x, y = (i & 2) ? hi.y : lo.y, z = (i & 4) ? hi.z : lo.z;
    float ry = (y - pivot.y) * cs - (z - pivot.z) * ss + pivot.y;
    float rz = (y - pivot.y) * ss + (z - pivot.z) * cs + pivot.z;
    c[i] = sr_v3(origin.x + x * cy + rz * sy, origin.y + ry, origin.z - x * sy + rz * cy);
  }
  /* front (+z), back, right (+x), left, top, bottom */
  sr_mesh_quad(m, c[4], c[5], c[7], c[6], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[1], c[0], c[2], c[3], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[5], c[1], c[3], c[7], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[0], c[4], c[6], c[2], 0, 1, 1, 0, col, layer, em, 0);
  SrColor top = {(uint8_t)(col.r * 3 / 4), (uint8_t)(col.g * 3 / 4), (uint8_t)(col.b * 3 / 4), col.a};
  sr_mesh_quad(m, c[6], c[7], c[3], c[2], 0, 1, 1, 0, top, layer, em, 0);
  sr_mesh_quad(m, c[0], c[1], c[5], c[4], 0, 1, 1, 0, top, layer, em, 0);
}

uint64_t sr_mesh_digest(const SrMesh *m) {
  uint64_t h = sr_hash_bytes(0, m->v, sizeof(SrVertex) * (size_t)m->nv);
  return sr_hash_bytes(h, m->idx, sizeof(uint32_t) * (size_t)m->ni);
}

/* ---- font ------------------------------------------------------------------------------- */

static const uint8_t k_font[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, {0x00, 0x00, 0x5F, 0x00, 0x00}, {0x00, 0x07, 0x00, 0x07, 0x00},
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, {0x24, 0x2A, 0x7F, 0x2A, 0x12}, {0x23, 0x13, 0x08, 0x64, 0x62},
    {0x36, 0x49, 0x55, 0x22, 0x50}, {0x00, 0x05, 0x03, 0x00, 0x00}, {0x00, 0x1C, 0x22, 0x41, 0x00},
    {0x00, 0x41, 0x22, 0x1C, 0x00}, {0x08, 0x2A, 0x1C, 0x2A, 0x08}, {0x08, 0x08, 0x3E, 0x08, 0x08},
    {0x00, 0x50, 0x30, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08}, {0x00, 0x60, 0x60, 0x00, 0x00},
    {0x20, 0x10, 0x08, 0x04, 0x02}, {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31}, {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E}, {0x00, 0x36, 0x36, 0x00, 0x00},
    {0x00, 0x56, 0x36, 0x00, 0x00}, {0x00, 0x08, 0x14, 0x22, 0x41}, {0x14, 0x14, 0x14, 0x14, 0x14},
    {0x41, 0x22, 0x14, 0x08, 0x00}, {0x02, 0x01, 0x51, 0x09, 0x06}, {0x32, 0x49, 0x79, 0x41, 0x3E},
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x01, 0x01},
    {0x3E, 0x41, 0x41, 0x51, 0x32}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x7F, 0x20, 0x18, 0x20, 0x7F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x51, 0x49, 0x45, 0x43}, {0x00, 0x00, 0x7F, 0x41, 0x41},
    {0x02, 0x04, 0x08, 0x10, 0x20}, {0x41, 0x41, 0x7F, 0x00, 0x00}, {0x04, 0x02, 0x01, 0x02, 0x04},
    {0x40, 0x40, 0x40, 0x40, 0x40}, {0x00, 0x01, 0x02, 0x04, 0x00}, {0x20, 0x54, 0x54, 0x54, 0x78},
    {0x7F, 0x48, 0x44, 0x44, 0x38}, {0x38, 0x44, 0x44, 0x44, 0x20}, {0x38, 0x44, 0x44, 0x48, 0x7F},
    {0x38, 0x54, 0x54, 0x54, 0x18}, {0x08, 0x7E, 0x09, 0x01, 0x02}, {0x08, 0x14, 0x54, 0x54, 0x3C},
    {0x7F, 0x08, 0x04, 0x04, 0x78}, {0x00, 0x44, 0x7D, 0x40, 0x00}, {0x20, 0x40, 0x44, 0x3D, 0x00},
    {0x00, 0x7F, 0x10, 0x28, 0x44}, {0x00, 0x41, 0x7F, 0x40, 0x00}, {0x7C, 0x04, 0x18, 0x04, 0x78},
    {0x7C, 0x08, 0x04, 0x04, 0x78}, {0x38, 0x44, 0x44, 0x44, 0x38}, {0x7C, 0x14, 0x14, 0x14, 0x08},
    {0x08, 0x14, 0x14, 0x18, 0x7C}, {0x7C, 0x08, 0x04, 0x04, 0x08}, {0x48, 0x54, 0x54, 0x54, 0x20},
    {0x04, 0x3F, 0x44, 0x40, 0x20}, {0x3C, 0x40, 0x40, 0x20, 0x7C}, {0x1C, 0x20, 0x40, 0x20, 0x1C},
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, {0x44, 0x28, 0x10, 0x28, 0x44}, {0x0C, 0x50, 0x50, 0x50, 0x3C},
    {0x44, 0x64, 0x54, 0x4C, 0x44}, {0x00, 0x08, 0x36, 0x41, 0x00}, {0x00, 0x00, 0x7F, 0x00, 0x00},
    {0x00, 0x41, 0x36, 0x08, 0x00}, {0x08, 0x08, 0x2A, 0x1C, 0x08}};

const uint8_t *sr_font_glyph(char ch) {
  if (ch < 32 || ch > 126) ch = '?';
  return k_font[ch - 32];
}

void sr_font_cell(char ch, int *cx, int *cy) {
  if (ch < 32 || ch > 126) ch = '?';
  int i = ch - 32;
  *cx = (i % 16) * 8;
  *cy = (i / 16) * 8;
}

/* ---- procedural textures --------------------------------------------------------------------- */

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

/* 16x16 icon bitmaps (1 bit per pixel, 16 rows of 16 bits) drawn large into icon layers. */
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

void sr_texgen_layer(int layer, uint8_t *rgba) {
  const int S = SR_TEX_SIZE;
  for (int y = 0; y < S; y++) {
    for (int x = 0; x < S; x++) {
      int n = noise8(layer, x, y);
      switch (layer) {
        case SR_LAYER_WHITE: px(rgba, x, y, 255, 255, 255, 255); break;
        case SR_LAYER_CONCRETE: px(rgba, x, y, 150 + n, 150 + n, 155 + n, 255); break;
        case SR_LAYER_WIN_A:
        case SR_LAYER_WIN_B:
        case SR_LAYER_WIN_GLASS: {
          int cell = layer == SR_LAYER_WIN_A ? 16 : layer == SR_LAYER_WIN_B ? 32 : 8;
          int wx = x % cell, wy = y % cell;
          bool window = wx > cell / 5 && wx < cell - cell / 5 && wy > cell / 4 && wy < cell - cell / 6;
          uint64_t h = sr_mix64((uint64_t)layer * 977u + (uint64_t)(x / cell) * 131u + (uint64_t)(y / cell));
          bool lit = (h % 100) < 38;
          if (window && lit) {
            int warm = (int)(h >> 8) % 3;
            px(rgba, x, y, warm == 0 ? 255 : 170, warm == 1 ? 200 : 235, warm == 2 ? 240 : 150, 255);
          } else if (window) {
            px(rgba, x, y, 30 + n / 2, 35 + n / 2, 50 + n / 2, 255);
          } else {
            px(rgba, x, y, 110 + n, 110 + n, 118 + n, 255);
          }
          break;
        }
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
        default: {
          if (layer < SR_LAYER_ICON_FIRST || layer >= SR_LAYER_ICON_FIRST + SR_POI_COUNT) {
            px(rgba, x, y, 255, 0, 255, 255); /* unused layer: magenta, easy to spot */
            break;
          }
          uint16_t rows[16];
          icon_shape(layer - SR_LAYER_ICON_FIRST, rows);
          int ix = x * 16 / S, iy = y * 16 / S;
          bool on = (rows[iy] >> (15 - ix)) & 1u;
          px(rgba, x, y, on ? 255 : 0, on ? 255 : 0, on ? 255 : 0, on ? 255 : 0);
          break;
        }
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
