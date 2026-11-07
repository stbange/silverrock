/* The street: kerbs and gutters, crosswalks, stop lines and arrows, stains and cracks, every street
 * prop of a block, and cables, lantern strings and laundry over narrow streets. Built per block and
 * drawn only near the camera, where the fog lets it show. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

static const SrColor k_lamp_color[SR_D_COUNT] = {
    {200, 230, 255, 255}, /* spire: cold white */
    {220, 225, 240, 255}, /* civic */
    {255, 200, 120, 255}, /* commercial: warm */
    {255, 120, 220, 255}, /* entertainment: pink */
    {150, 240, 230, 255}, /* residential: teal */
    {255, 170, 80, 255},  /* old quarter: lantern gold */
    {255, 150, 50, 255},  /* industrial: sodium */
    {255, 170, 70, 255},  /* docks: sodium */
    {200, 255, 220, 255}, /* campus */
    {170, 255, 120, 255}, /* undercity: sickly green */
    {255, 120, 40, 255},  /* barrens */
};

SrColor sr_lamp_color(SrDistrictType t) { return k_lamp_color[(unsigned)t < SR_D_COUNT ? t : SR_D_RESIDENTIAL]; }

static SrColor dim(SrColor c, int pct) {
  return (SrColor){(uint8_t)(c.r * pct / 100), (uint8_t)(c.g * pct / 100), (uint8_t)(c.b * pct / 100), c.a};
}

/* A box in a prop's frame (local +z faces the street), turned by yaw about the prop's origin. */
static void pbox(SrMesh *m, const SrStreetProp *p, float y0, SrVec3 lo, SrVec3 hi, SrColor c, int layer, int em) {
  sr_mesh_part(m, sr_v3(p->x, y0, p->z), p->yaw, sr_v3(0, 0, 0), 0.0f, lo, hi, c, layer, em);
}

static void decal_q(SrMesh *m, float x, float y, float z, float w, float d, float yaw, int id, SrColor col) {
  SrAtlasCell a = sr_atlas_cell(SR_LAYER_DECALS, id);
  float cy = cosf(yaw), sy = sinf(yaw), hw = w * 0.5f, hd = d * 0.5f;
  float ax = cy * hw, az = -sy * hw, bx = sy * hd, bz = cy * hd;
  sr_mesh_quad(m, sr_v3(x - ax + bx, y, z - az + bz), sr_v3(x + ax + bx, y, z + az + bz),
               sr_v3(x + ax - bx, y, z + az - bz), sr_v3(x - ax - bx, y, z - az - bz), a.u0, a.v1, a.u1, a.v0, col,
               a.layer, 0, 1);
}

static void prop_mesh(SrMesh *m, const SrStreetProp *p, SrDistrictType t, float ground, uint32_t h) {
  static const SrColor dark = {50, 52, 58, 255}, steel = {110, 114, 122, 255};
  float g = ground;
  switch (p->kind) {
    case SR_SP_LAMP: {
      SrColor lc = sr_lamp_color(t);
      if (p->variant == 3) { /* wall lamp in an alley: a bracket and a bare bulb */
        pbox(m, p, g + 3.4f, sr_v3(-0.08f, 0, -0.5f), sr_v3(0.08f, 0.08f, 0.5f), dark, SR_LAYER_METAL, 0);
        pbox(m, p, g + 3.2f, sr_v3(-0.15f, 0, 0.4f), sr_v3(0.15f, 0.2f, 0.6f), lc, SR_LAYER_WHITE, 255);
        break;
      }
      pbox(m, p, g, sr_v3(-0.08f, 0, -0.08f), sr_v3(0.08f, 6.5f, 0.08f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g + 6.3f, sr_v3(-0.06f, 0, 0), sr_v3(0.06f, 0.12f, 1.6f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g + 6.1f, sr_v3(-0.3f, 0, 1.2f), sr_v3(0.3f, 0.2f, 1.9f), lc, SR_LAYER_WHITE, 255);
      break;
    }
    case SR_SP_TRAFFIC_LIGHT: {
      pbox(m, p, g, sr_v3(-0.07f, 0, -0.07f), sr_v3(0.07f, 4.2f, 0.07f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g + 3.0f, sr_v3(-0.2f, 0, 0.05f), sr_v3(0.2f, 1.1f, 0.35f), (SrColor){30, 30, 32, 255}, SR_LAYER_WHITE,
           0);
      int on = (int)(h % 3u); /* one of red, amber, green (they cycle in phase motion) */
      static const SrColor lights[3] = {{255, 40, 40, 255}, {255, 180, 30, 255}, {40, 255, 110, 255}};
      pbox(m, p, g + 3.75f - 0.33f * (float)on, sr_v3(-0.12f, 0, 0.36f), sr_v3(0.12f, 0.24f, 0.4f), lights[on],
           SR_LAYER_WHITE, 255);
      break;
    }
    case SR_SP_BOLLARD:
      pbox(m, p, g, sr_v3(-0.13f, 0, -0.13f), sr_v3(0.13f, 0.8f, 0.13f), steel, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.8f, sr_v3(-0.13f, 0, -0.13f), sr_v3(0.13f, 0.1f, 0.13f), (SrColor){220, 180, 40, 255},
           SR_LAYER_WHITE, 0);
      break;
    case SR_SP_HYDRANT:
      pbox(m, p, g, sr_v3(-0.17f, 0, -0.17f), sr_v3(0.17f, 0.7f, 0.17f), (SrColor){170, 40, 30, 255}, SR_LAYER_METAL,
           0);
      break;
    case SR_SP_BIN:
      pbox(m, p, g, sr_v3(-0.28f, 0, -0.28f), sr_v3(0.28f, 0.95f, 0.28f), (SrColor){40, 70, 50, 255}, SR_LAYER_METAL,
           0);
      break;
    case SR_SP_METER:
      pbox(m, p, g, sr_v3(-0.04f, 0, -0.04f), sr_v3(0.04f, 1.1f, 0.04f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g + 1.1f, sr_v3(-0.12f, 0, -0.1f), sr_v3(0.12f, 0.3f, 0.1f), steel, SR_LAYER_METAL, 20);
      break;
    case SR_SP_TREE:
      pbox(m, p, g, sr_v3(-0.12f, 0, -0.12f), sr_v3(0.12f, 2.4f, 0.12f), (SrColor){60, 45, 35, 255}, SR_LAYER_WHITE, 0);
      pbox(m, p, g + 2.2f, sr_v3(-1.2f, 0, -1.2f), sr_v3(1.2f, 1.6f, 1.2f), (SrColor){30, 60, 40, 255},
           SR_LAYER_TRIM_FIRST + 4, 0);
      pbox(m, p, g + 3.6f, sr_v3(-0.7f, 0, -0.7f), sr_v3(0.7f, 1.0f, 0.7f), (SrColor){34, 70, 44, 255},
           SR_LAYER_TRIM_FIRST + 4, 0);
      break;
    case SR_SP_PLANTER:
      pbox(m, p, g, sr_v3(-0.6f, 0, -0.6f), sr_v3(0.6f, 0.6f, 0.6f), (SrColor){120, 120, 125, 255}, SR_LAYER_CONCRETE,
           0);
      pbox(m, p, g + 0.6f, sr_v3(-0.5f, 0, -0.5f), sr_v3(0.5f, 0.35f, 0.5f), (SrColor){40, 90, 50, 255},
           SR_LAYER_TRIM_FIRST + 4, 0);
      break;
    case SR_SP_NEWSBOX:
      pbox(m, p, g, sr_v3(-0.28f, 0, -0.25f), sr_v3(0.28f, 1.05f, 0.25f), (SrColor){40, 70, 140, 255}, SR_LAYER_METAL,
           0);
      pbox(m, p, g + 0.6f, sr_v3(-0.2f, 0, 0.25f), sr_v3(0.2f, 0.3f, 0.27f), (SrColor){200, 210, 230, 255},
           SR_LAYER_WHITE, 120);
      break;
    case SR_SP_CAR:
    case SR_SP_BURNED_CAR: {
      static const SrColor paint[4] = {{60, 64, 80, 255}, {110, 30, 34, 255}, {40, 80, 90, 255}, {150, 150, 150, 255}};
      bool burned = p->kind == SR_SP_BURNED_CAR;
      SrColor body = burned ? (SrColor){40, 32, 28, 255} : paint[p->variant & 3];
      pbox(m, p, g + 0.25f, sr_v3(-0.9f, 0, -2.2f), sr_v3(0.9f, 0.65f, 2.2f), body, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.9f, sr_v3(-0.8f, 0, -1.1f), sr_v3(0.8f, 0.5f, 0.9f),
           burned ? dim(body, 70) : (SrColor){24, 28, 36, 255}, burned ? SR_LAYER_METAL : SR_LAYER_WHITE, 0);
      if (!burned) /* tail lights, dim while parked */
        pbox(m, p, g + 0.55f, sr_v3(-0.8f, 0, -2.25f), sr_v3(0.8f, 0.12f, -2.2f), (SrColor){120, 20, 20, 255},
             SR_LAYER_WHITE, 120);
      break;
    }
    case SR_SP_VENDING: {
      static const SrColor glow[4] = {
          {40, 200, 255, 255}, {255, 60, 140, 255}, {255, 210, 60, 255}, {120, 255, 120, 255}};
      pbox(m, p, g, sr_v3(-0.5f, 0, -0.4f), sr_v3(0.5f, 2.0f, 0.4f), (SrColor){60, 60, 66, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.5f, sr_v3(-0.4f, 0, 0.4f), sr_v3(0.4f, 1.3f, 0.42f), glow[p->variant & 3], SR_LAYER_BASE_FIRST,
           220);
      break;
    }
    case SR_SP_BENCH:
      pbox(m, p, g + 0.4f, sr_v3(-0.9f, 0, -0.25f), sr_v3(0.9f, 0.08f, 0.25f), (SrColor){90, 70, 50, 255},
           SR_LAYER_WHITE, 0);
      pbox(m, p, g + 0.45f, sr_v3(-0.9f, 0, -0.3f), sr_v3(0.9f, 0.45f, -0.24f), (SrColor){90, 70, 50, 255},
           SR_LAYER_WHITE, 0);
      pbox(m, p, g, sr_v3(-0.8f, 0, -0.2f), sr_v3(-0.7f, 0.4f, 0.2f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g, sr_v3(0.7f, 0, -0.2f), sr_v3(0.8f, 0.4f, 0.2f), dark, SR_LAYER_METAL, 0);
      break;
    case SR_SP_STALL: {
      static const SrColor tarp[4] = {{170, 40, 40, 255}, {40, 110, 160, 255}, {200, 150, 40, 255}, {60, 140, 80, 255}};
      pbox(m, p, g, sr_v3(-1.3f, 0, -0.9f), sr_v3(1.3f, 1.0f, 0.9f), (SrColor){110, 80, 55, 255},
           SR_LAYER_BASE_FIRST + 4, 90);
      pbox(m, p, g + 2.3f, sr_v3(-1.5f, 0, -1.1f), sr_v3(1.5f, 0.15f, 1.3f), tarp[p->variant & 3], SR_LAYER_WHITE, 30);
      pbox(m, p, g + 1.9f, sr_v3(-0.12f, 0, 1.1f), sr_v3(0.12f, 0.3f, 1.25f), (SrColor){255, 150, 60, 255},
           SR_LAYER_WHITE, 255);
      break;
    }
    case SR_SP_DUMPSTER:
      pbox(m, p, g, sr_v3(-1.0f, 0, -0.6f), sr_v3(1.0f, 1.2f, 0.6f),
           p->variant & 1 ? (SrColor){40, 70, 60, 255} : (SrColor){40, 50, 90, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 1.2f, sr_v3(-1.0f, 0, -0.65f), sr_v3(1.0f, 0.1f, 0.6f), dark, SR_LAYER_METAL, 0);
      break;
    case SR_SP_CRATES:
      pbox(m, p, g, sr_v3(-0.6f, 0, -0.6f), sr_v3(0.6f, 0.6f, 0.6f), (SrColor){130, 100, 60, 255},
           SR_LAYER_BASE_FIRST + 3, 0);
      pbox(m, p, g + 0.6f, sr_v3(-0.35f, 0, -0.4f), sr_v3(0.35f, 0.5f, 0.3f), (SrColor){120, 92, 56, 255},
           SR_LAYER_BASE_FIRST + 3, 0);
      break;
    case SR_SP_TRASH:
      pbox(m, p, g, sr_v3(-0.5f, 0, -0.3f), sr_v3(0.0f, 0.5f, 0.2f), (SrColor){25, 25, 28, 255}, SR_LAYER_WHITE, 0);
      pbox(m, p, g, sr_v3(0.0f, 0, -0.4f), sr_v3(0.45f, 0.4f, 0.3f), (SrColor){30, 30, 34, 255}, SR_LAYER_WHITE, 0);
      break;
    case SR_SP_BOOTH:
      pbox(m, p, g, sr_v3(-0.55f, 0, -0.55f), sr_v3(0.55f, 2.4f, 0.55f), (SrColor){50, 55, 70, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.9f, sr_v3(-0.4f, 0, 0.55f), sr_v3(0.4f, 1.2f, 0.57f), (SrColor){60, 230, 255, 255},
           SR_LAYER_WHITE, 200);
      break;
    case SR_SP_VENT: pbox(m, p, g, sr_v3(-0.4f, 0, -0.4f), sr_v3(0.4f, 0.4f, 0.4f), steel, SR_LAYER_METAL, 0); break;
    case SR_SP_BARREL:
      pbox(m, p, g, sr_v3(-0.35f, 0, -0.35f), sr_v3(0.35f, 0.9f, 0.35f), (SrColor){70, 50, 40, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.9f, sr_v3(-0.28f, 0, -0.28f), sr_v3(0.28f, 0.45f, 0.28f), (SrColor){255, 140, 40, 255},
           SR_LAYER_WHITE, 255);
      break;
    case SR_SP_BARRIER:
      pbox(m, p, g, sr_v3(-1.2f, 0, -0.25f), sr_v3(1.2f, 0.9f, 0.25f), (SrColor){150, 150, 150, 255}, SR_LAYER_CONCRETE,
           0);
      pbox(m, p, g + 0.55f, sr_v3(-1.2f, 0, 0.25f), sr_v3(1.2f, 0.15f, 0.27f), (SrColor){230, 120, 30, 255},
           SR_LAYER_WHITE, 40);
      break;
    case SR_SP_FENCE:
      for (int k = 0; k < 3; k++)
        pbox(m, p, g, sr_v3(-2.0f + 2.0f * (float)k - 0.04f, 0, -0.04f),
             sr_v3(-2.0f + 2.0f * (float)k + 0.04f, 2.4f, 0.04f), steel, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.1f, sr_v3(-2.0f, 0, -0.01f), sr_v3(2.0f, 2.2f, 0.01f), (SrColor){90, 95, 100, 255},
           SR_LAYER_WINMOD_FIRST + SR_WIN_HIVE, 0);
      break;
    case SR_SP_CONTAINER: {
      static const SrColor box[4] = {{150, 60, 40, 255}, {40, 90, 140, 255}, {60, 120, 70, 255}, {170, 140, 50, 255}};
      pbox(m, p, g, sr_v3(-1.25f, 0, -3.05f), sr_v3(1.25f, 2.6f, 3.05f), box[p->variant & 3], SR_LAYER_TRIM_FIRST + 1,
           0);
      if (p->variant & 2) /* stacked two high */
        pbox(m, p, g + 2.6f, sr_v3(-1.25f, 0, -3.05f), sr_v3(1.25f, 2.6f, 3.05f), box[(p->variant + 1) & 3],
             SR_LAYER_TRIM_FIRST + 1, 0);
      break;
    }
    case SR_SP_RUBBLE:
      for (int k = 0; k < 4; k++) {
        float ox = (float)((h >> (k * 4)) % 9u) * 0.2f - 0.8f, oz = (float)((h >> (k * 4 + 2)) % 9u) * 0.2f - 0.8f;
        pbox(m, p, g, sr_v3(ox - 0.35f, 0, oz - 0.3f), sr_v3(ox + 0.35f, 0.3f + 0.1f * (float)k, oz + 0.3f),
             (SrColor){90, 86, 80, 255}, SR_LAYER_CONCRETE, 0);
      }
      break;
    case SR_SP_SHRINE:
      pbox(m, p, g, sr_v3(-0.45f, 0, -0.35f), sr_v3(0.45f, 1.1f, 0.35f), (SrColor){120, 40, 30, 255}, SR_LAYER_WHITE,
           0);
      pbox(m, p, g + 1.1f, sr_v3(-0.6f, 0, -0.5f), sr_v3(0.6f, 0.15f, 0.5f), (SrColor){60, 30, 25, 255},
           SR_LAYER_TRIM_FIRST, 0);
      pbox(m, p, g + 0.75f, sr_v3(-0.25f, 0, 0.35f), sr_v3(0.25f, 0.15f, 0.4f), (SrColor){255, 180, 80, 255},
           SR_LAYER_WHITE, 255);
      break;
    case SR_SP_FLAG:
      pbox(m, p, g, sr_v3(-0.05f, 0, -0.05f), sr_v3(0.05f, 7.0f, 0.05f), steel, SR_LAYER_METAL, 0);
      pbox(m, p, g + 5.2f, sr_v3(0.05f, 0, -0.02f), sr_v3(1.6f, 1.6f, 0.02f), (SrColor){60, 90, 200, 255},
           SR_LAYER_WHITE, 30);
      break;
    case SR_SP_HOLOTREE:
      pbox(m, p, g, sr_v3(-0.4f, 0, -0.4f), sr_v3(0.4f, 0.5f, 0.4f), (SrColor){60, 64, 72, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.5f, sr_v3(-0.06f, 0, -0.06f), sr_v3(0.06f, 1.4f, 0.06f), (SrColor){90, 255, 200, 255},
           SR_LAYER_WHITE, 200);
      break;
    case SR_SP_FIRE_ESCAPE: { /* platforms and ladders up a wall; the frame's +z points out of the wall */
      for (int fl = 1; fl <= 5; fl++) {
        float y = g + 4.5f + 3.4f * (float)(fl - 1);
        pbox(m, p, y, sr_v3(-1.5f, 0, 0.05f), sr_v3(1.5f, 0.08f, 1.2f), dark, SR_LAYER_METAL, 0);
        pbox(m, p, y + 0.9f, sr_v3(-1.5f, 0, 1.15f), sr_v3(1.5f, 0.05f, 1.2f), dark, SR_LAYER_METAL, 0);
        float lx = fl % 2 ? -1.2f : 1.0f;
        pbox(m, p, y, sr_v3(lx, 0, 0.5f), sr_v3(lx + 0.2f, 3.4f, 0.6f), dark, SR_LAYER_METAL, 0);
      }
      break;
    }
    case SR_SP_VENDROW: {
      static const SrColor glow[3] = {{40, 200, 255, 255}, {255, 60, 140, 255}, {255, 210, 60, 255}};
      for (int k = 0; k < 3; k++) {
        float ox = -1.1f + 1.1f * (float)k;
        pbox(m, p, g, sr_v3(ox - 0.5f, 0, -0.4f), sr_v3(ox + 0.5f, 2.0f, 0.4f), (SrColor){60, 60, 66, 255},
             SR_LAYER_METAL, 0);
        pbox(m, p, g + 0.5f, sr_v3(ox - 0.4f, 0, 0.4f), sr_v3(ox + 0.4f, 1.3f, 0.42f), glow[(k + p->variant) % 3],
             SR_LAYER_BASE_FIRST, 220);
      }
      break;
    }
    case SR_SP_MARQUEE: { /* a canopy edged with bulbs over a club door, a rope for the queue */
      pbox(m, p, g + 3.0f, sr_v3(-1.5f, 0, -1.0f), sr_v3(1.5f, 0.25f, 1.0f), (SrColor){120, 20, 80, 255},
           SR_LAYER_WHITE, 60);
      SrAtlasCell bulbs = sr_ad_cell(SR_LAYER_ADS, SR_AD_BULBS);
      SrVec3 o = sr_v3(p->x, g + 3.0f, p->z);
      float cy = cosf(p->yaw), sy = sinf(p->yaw);
      SrVec3 a = sr_v3(o.x - cy * 1.5f + sy * 1.02f, o.y, o.z + sy * 1.5f + cy * 1.02f);
      SrVec3 b = sr_v3(o.x + cy * 1.5f + sy * 1.02f, o.y, o.z - sy * 1.5f + cy * 1.02f);
      int first = m->nv;
      sr_mesh_quad(m, a, b, sr_v3(b.x, b.y + 0.25f, b.z), sr_v3(a.x, a.y + 0.25f, a.z), bulbs.u0, bulbs.v1,
                   bulbs.u0 + (bulbs.u1 - bulbs.u0) * 3.0f, bulbs.v0, (SrColor){255, 220, 140, 255}, bulbs.layer, 255,
                   0);
      for (int v = first; v < m->nv; v++) m->v[v].pad = (uint8_t)(0xC0u | (h & 63u)); /* chasing lights (motion) */
      for (int k = 0; k < 4; k++)
        pbox(m, p, g, sr_v3(-1.2f + 0.8f * (float)k - 0.04f, 0, 1.6f),
             sr_v3(-1.2f + 0.8f * (float)k + 0.04f, 0.9f, 1.68f), (SrColor){200, 180, 60, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.8f, sr_v3(-1.2f, 0, 1.62f), sr_v3(1.2f, 0.06f, 1.66f), (SrColor){160, 20, 40, 255},
           SR_LAYER_WHITE, 0);
      break;
    }
    case SR_SP_KIOSK:
      pbox(m, p, g, sr_v3(-1.1f, 0, -1.0f), sr_v3(1.1f, 2.4f, 1.0f), (SrColor){70, 90, 80, 255}, SR_LAYER_METAL, 0);
      pbox(m, p, g + 0.9f, sr_v3(-0.9f, 0, 1.0f), sr_v3(0.9f, 1.0f, 1.03f), (SrColor){240, 200, 130, 255},
           SR_LAYER_BASE_FIRST, 150);
      pbox(m, p, g + 2.4f, sr_v3(-1.3f, 0, -1.2f), sr_v3(1.3f, 0.2f, 1.4f), (SrColor){150, 40, 40, 255}, SR_LAYER_WHITE,
           0);
      break;
    case SR_SP_TANK: /* an octagonal tank: two crossed boxes and a ladder */
      pbox(m, p, g, sr_v3(-3.0f, 0, -1.25f), sr_v3(3.0f, 8.0f, 1.25f), (SrColor){130, 130, 120, 255},
           SR_LAYER_TRIM_FIRST + 1, 0);
      pbox(m, p, g, sr_v3(-1.25f, 0, -3.0f), sr_v3(1.25f, 8.0f, 3.0f), (SrColor){125, 125, 116, 255},
           SR_LAYER_TRIM_FIRST + 1, 0);
      pbox(m, p, g, sr_v3(-2.2f, 0, -2.2f), sr_v3(2.2f, 8.0f, 2.2f), (SrColor){128, 128, 118, 255},
           SR_LAYER_TRIM_FIRST + 1, 0);
      pbox(m, p, g, sr_v3(2.9f, 0, -0.2f), sr_v3(3.1f, 8.4f, 0.2f), dark, SR_LAYER_METAL, 0);
      pbox(m, p, g + 8.0f, sr_v3(-0.2f, 0, -0.2f), sr_v3(0.2f, 0.5f, 0.2f), (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE,
           220);
      break;
    case SR_SP_CRANE: { /* a gantry: two legs, a beam and a boom out over the water */
      SrColor yellow = {190, 150, 40, 255};
      for (int k = 0; k < 2; k++) {
        float lx = k ? 4.5f : -4.5f;
        pbox(m, p, g, sr_v3(lx - 0.5f, 0, -1.6f), sr_v3(lx + 0.5f, 30.0f, -0.6f), yellow, SR_LAYER_METAL, 0);
        pbox(m, p, g, sr_v3(lx - 0.5f, 0, 0.6f), sr_v3(lx + 0.5f, 30.0f, 1.6f), yellow, SR_LAYER_METAL, 0);
      }
      pbox(m, p, g + 30.0f, sr_v3(-5.2f, 0, -1.8f), sr_v3(5.2f, 2.0f, 1.8f), yellow, SR_LAYER_METAL, 0);
      pbox(m, p, g + 30.5f, sr_v3(-0.8f, 0, 1.8f), sr_v3(0.8f, 1.2f, 22.0f), yellow, SR_LAYER_METAL, 0);
      pbox(m, p, g + 32.0f, sr_v3(-0.3f, 0, -0.3f), sr_v3(0.3f, 0.4f, 0.3f), (SrColor){255, 40, 40, 255},
           SR_LAYER_WHITE, 230);
      pbox(m, p, g + 18.0f, sr_v3(-0.03f, 0, 14.0f), sr_v3(0.03f, 12.5f, 14.06f), dark, SR_LAYER_WHITE,
           0); /* the cable */
      break;
    }
    case SR_SP_GLOWTREE:
      pbox(m, p, g, sr_v3(-0.12f, 0, -0.12f), sr_v3(0.12f, 2.2f, 0.12f), (SrColor){70, 60, 50, 255}, SR_LAYER_WHITE, 0);
      pbox(m, p, g + 2.0f, sr_v3(-1.1f, 0, -1.1f), sr_v3(1.1f, 1.8f, 1.1f), (SrColor){60, 220, 150, 255},
           SR_LAYER_TRIM_FIRST + 4, 150);
      break;
    case SR_SP_SHANTY: {
      static const SrColor tarp[3] = {{40, 80, 130, 255}, {130, 60, 40, 255}, {90, 100, 60, 255}};
      pbox(m, p, g, sr_v3(-1.5f, 0, -1.2f), sr_v3(1.5f, 2.0f, 1.2f), (SrColor){90, 80, 70, 255},
           SR_LAYER_TRIM_FIRST + 1, 0);
      pbox(m, p, g + 2.0f, sr_v3(-1.7f, 0, -1.4f), sr_v3(1.7f, 0.15f, 1.6f), tarp[p->variant % 3], SR_LAYER_WHITE, 0);
      pbox(m, p, g + 1.0f, sr_v3(-0.3f, 0, 1.2f), sr_v3(0.3f, 0.5f, 1.24f), (SrColor){255, 190, 110, 255},
           SR_LAYER_WHITE, 200);
      break;
    }
    case SR_SP_BULB:
      pbox(m, p, g + 10.4f, sr_v3(-0.02f, 0, -0.02f), sr_v3(0.02f, 0.6f, 0.02f), dark, SR_LAYER_WHITE, 0);
      pbox(m, p, g + 10.2f, sr_v3(-0.15f, 0, -0.15f), sr_v3(0.15f, 0.25f, 0.15f), (SrColor){255, 200, 120, 255},
           SR_LAYER_WHITE, 255);
      break;
    case SR_SP_PILLAR:
      if (p->variant != 1) /* metro pillars are part of the track mesh */
        pbox(m, p, g, sr_v3(-0.7f, 0, -0.7f), sr_v3(0.7f, 11.0f, 0.7f), (SrColor){110, 110, 112, 255},
             SR_LAYER_CONCRETE, 0);
      break;
    default: break;
  }
}

/* Kerb faces and gutters around the lot, crosswalks, stop lines and arrows at the block's corner,
 * stains and cracks. */
static void street_surface(const SrCity *c, int blk, SrMesh *m) {
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f, st = (float)SR_STREET_CM / 100.0f,
        s = (float)SR_BLOCK_CM / 100.0f;
  float lx0 = x0 + st, lz0 = z0 + st, lx1 = x0 + s, lz1 = z0 + s, k = 0.12f;
  SrColor kerb = {120, 120, 126, 255}, gutter = {28, 28, 32, 255};
  /* kerb faces: the pavement drops 12 cm to the road on all four sides */
  sr_mesh_quad(m, sr_v3(lx0, 0, lz0), sr_v3(lx0, 0, lz1), sr_v3(lx0, k, lz1), sr_v3(lx0, k, lz0), 0, 0, (lz1 - lz0) / 2,
               0.1f, kerb, SR_LAYER_CONCRETE, 0, 0);
  sr_mesh_quad(m, sr_v3(lx1, 0, lz0), sr_v3(lx0, 0, lz0), sr_v3(lx0, k, lz0), sr_v3(lx1, k, lz0), 0, 0, (lx1 - lx0) / 2,
               0.1f, kerb, SR_LAYER_CONCRETE, 0, 0);
  sr_mesh_quad(m, sr_v3(lx1, 0, lz1), sr_v3(lx1, 0, lz0), sr_v3(lx1, k, lz0), sr_v3(lx1, k, lz1), 0, 0, (lz1 - lz0) / 2,
               0.1f, kerb, SR_LAYER_CONCRETE, 0, 0);
  sr_mesh_quad(m, sr_v3(lx0, 0, lz1), sr_v3(lx1, 0, lz1), sr_v3(lx1, k, lz1), sr_v3(lx0, k, lz1), 0, 0, (lx1 - lx0) / 2,
               0.1f, kerb, SR_LAYER_CONCRETE, 0, 0);
  /* gutters: a dark wet strip along the kerb on the west and north streets */
  float y = 0.006f;
  sr_mesh_quad(m, sr_v3(lx0 - 0.5f, y, lz1), sr_v3(lx0, y, lz1), sr_v3(lx0, y, lz0), sr_v3(lx0 - 0.5f, y, lz0), 0, 0, 1,
               1, gutter, SR_LAYER_WHITE, 0, 1);
  sr_mesh_quad(m, sr_v3(lx0, y, lz0), sr_v3(lx1, y, lz0), sr_v3(lx1, y, lz0 - 0.5f), sr_v3(lx0, y, lz0 - 0.5f), 0, 0, 1,
               1, gutter, SR_LAYER_WHITE, 0, 1);
  /* the intersection at the block's corner (x0..x0+st, z0..z0+st): a crosswalk on each of its four
   * sides, a stop line and an arrow in each approaching lane */
  SrColor paint = {200, 200, 190, 255};
  float cw = 3.0f, ye = 0.012f;
  float mx = x0 + st * 0.5f, mz = z0 + st * 0.5f;
  struct {
    float x, z, yaw;
  } sides[4] = {{mx, z0 - cw * 0.5f - 0.5f, 0.0f},
                {mx, z0 + st + cw * 0.5f + 0.5f, 0.0f},
                {x0 - cw * 0.5f - 0.5f, mz, SR_PI / 2},
                {x0 + st + cw * 0.5f + 0.5f, mz, SR_PI / 2}};
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) { /* four 4 m tiles across the 16 m street */
      float off = -6.0f + 4.0f * (float)j;
      float cx = sides[i].x + (i < 2 ? off : 0.0f), cz = sides[i].z + (i < 2 ? 0.0f : off);
      decal_q(m, cx, ye, cz, 4.0f, cw, sides[i].yaw, SR_DECAL_CROSSWALK, paint);
    }
    float dir = (i == 0 || i == 2) ? -1.0f : 1.0f; /* away from the intersection */
    float lane = st * 0.25f * (i == 0 || i == 3 ? 1.0f : -1.0f);
    float sx = sides[i].x + (i < 2 ? lane : dir * (cw * 0.5f + 1.0f));
    float sz = sides[i].z + (i < 2 ? dir * (cw * 0.5f + 1.0f) : lane);
    decal_q(m, sx, ye, sz, st * 0.5f - 0.5f, 0.6f, sides[i].yaw, SR_DECAL_STOPLINE, paint);
    float ax = sides[i].x + (i < 2 ? lane : dir * 12.0f), az = sides[i].z + (i < 2 ? dir * 12.0f : lane);
    decal_q(m, ax, ye, az, 1.6f, 3.0f, sides[i].yaw + (dir > 0 ? SR_PI : 0.0f), SR_DECAL_ARROW, paint);
  }
  SrDistrictType t = c->districts[c->blocks[blk].district].type;
  if (t == SR_D_CAMPUS || t == SR_D_SPIRE) { /* lawns on campus; glowing lines across Spire plazas */
    float pav[4];
    sr_block_pavements(c, blk, pav);
    float ix0 = lx0 + sr_walk_lane(pav[0]) + 1.5f, iz0 = lz0 + sr_walk_lane(pav[1]) + 1.5f;
    float ix1 = lx1 - sr_walk_lane(pav[2]) - 1.5f, iz1 = lz1 - sr_walk_lane(pav[3]) - 1.5f;
    if (t == SR_D_CAMPUS && ix1 > ix0 && iz1 > iz0)
      sr_mesh_quad(m, sr_v3(ix0, 0.13f, iz1), sr_v3(ix1, 0.13f, iz1), sr_v3(ix1, 0.13f, iz0), sr_v3(ix0, 0.13f, iz0), 0,
                   0, (ix1 - ix0) * 0.2f, (iz1 - iz0) * 0.2f, (SrColor){40, 90, 50, 255}, SR_LAYER_TRIM_FIRST + 4, 0,
                   0);
    if (t == SR_D_SPIRE)
      for (int li = 0; li < 4; li++) {
        float zz = lz0 + 6.0f + (lz1 - lz0 - 12.0f) * (float)li / 3.0f;
        sr_mesh_quad(m, sr_v3(lx0 + 2.0f, 0.13f, zz + 0.08f), sr_v3(lx1 - 2.0f, 0.13f, zz + 0.08f),
                     sr_v3(lx1 - 2.0f, 0.13f, zz - 0.08f), sr_v3(lx0 + 2.0f, 0.13f, zz - 0.08f), 0, 0, 1, 1,
                     (SrColor){90, 200, 255, 255}, SR_LAYER_WHITE, 160, 0);
      }
  }
  /* stains and cracks, more in poor districts */
  int n = t == SR_D_SPIRE || t == SR_D_CAMPUS ? 2 : t == SR_D_BARRENS || t == SR_D_UNDERCITY ? 12 : 6;
  for (int i = 0; i < n; i++) {
    uint64_t h = sr_mix64(c->seed ^ ((uint64_t)blk << 20) ^ (uint64_t)(i * 7919));
    float px = x0 + (float)(h % 9600u) / 100.0f, pz = z0 + (float)((h >> 16) % 9600u) / 100.0f;
    bool lot = px >= lx0 && pz >= lz0;
    int id = (h >> 32) & 1u ? SR_DECAL_CRACK + (int)((h >> 33) & 1u) : SR_DECAL_STAIN + (int)((h >> 33) & 1u);
    decal_q(m, px, lot ? 0.128f : 0.008f, pz, 1.5f + (float)((h >> 40) % 3u), 1.2f, (float)((h >> 44) % 6u), id,
            (SrColor){120, 120, 120, 255});
  }
}

/* Cables, lantern strings and laundry across the street north of the block. */
static void overhead(const SrCity *c, int blk, SrMesh *m) {
  SrDistrictType t = c->districts[c->blocks[blk].district].type;
  if (t != SR_D_OLDQUARTER && t != SR_D_UNDERCITY && t != SR_D_RESIDENTIAL && t != SR_D_ENTERTAINMENT) return;
  int north = blk - c->grid;
  if (north < 0 || c->blocks[north].water || c->blocks[north].building_count == 0) return;
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float z0 = (float)oy / 100.0f, st = (float)SR_STREET_CM / 100.0f;
  const SrBlock *b = &c->blocks[blk];
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    float zs = (float)bd->y0 / 100.0f; /* this building's north face */
    float zn = z0 - 2.0f;              /* the far side of the street (approximately the facing pavement) */
    const SrBlock *nb = &c->blocks[north];
    for (int j = 0; j < nb->building_count; j++) {
      const SrBuilding *o = &c->buildings[nb->first_building + j];
      if (o->x1 <= bd->x0 || o->x0 >= bd->x1) continue;
      zn = (float)o->y1 / 100.0f;
    }
    if (zs - zn > st + 14.0f) continue; /* too wide to string a line across */
    for (int k = 0; k < 3; k++) {
      uint64_t h = sr_mix64(c->seed ^ ((uint64_t)bd->id << 8) ^ (uint64_t)k);
      float x = (float)bd->x0 / 100.0f + (float)(h % 1000u) / 1000.0f * (float)(bd->x1 - bd->x0) / 100.0f;
      float y0 = 6.0f + (float)((h >> 12) % 6u), sag = 1.2f;
      int segs = 6;
      for (int sgi = 0; sgi < segs; sgi++) { /* a sagging line */
        float t0 = (float)sgi / (float)segs, t1 = (float)(sgi + 1) / (float)segs;
        float za = zs + (zn - zs) * t0, zb = zs + (zn - zs) * t1;
        float ya = y0 - sag * 4.0f * t0 * (1 - t0), yb = y0 - sag * 4.0f * t1 * (1 - t1);
        sr_mesh_quad(m, sr_v3(x, ya, za), sr_v3(x, yb, zb), sr_v3(x, yb + 0.05f, zb), sr_v3(x, ya + 0.05f, za), 0, 0, 1,
                     1, (SrColor){20, 20, 22, 255}, SR_LAYER_WHITE, 0, 0);
        float mz = (za + zb) * 0.5f, my = (ya + yb) * 0.5f;
        if (t == SR_D_OLDQUARTER || t == SR_D_ENTERTAINMENT) { /* lanterns or bulbs on the line */
          SrColor lc = t == SR_D_OLDQUARTER ? (SrColor){255, 80, 50, 255} : (SrColor){255, 200, 120, 255};
          sr_mesh_box(m, sr_v3(x - 0.15f, my - 0.45f, mz - 0.15f), sr_v3(x + 0.15f, my - 0.05f, mz + 0.15f), lc,
                      SR_LAYER_WHITE, 0, 230);
        } else if (t == SR_D_RESIDENTIAL && sgi % 2 == 0) { /* laundry */
          static const SrColor cloth[3] = {{150, 60, 70, 255}, {70, 90, 140, 255}, {170, 170, 160, 255}};
          sr_mesh_quad(m, sr_v3(x - 0.02f, my - 0.9f, mz - 0.4f), sr_v3(x - 0.02f, my - 0.9f, mz + 0.4f),
                       sr_v3(x - 0.02f, my, mz + 0.4f), sr_v3(x - 0.02f, my, mz - 0.4f), 0, 0, 1, 1,
                       cloth[(h >> (sgi * 2)) % 3u], SR_LAYER_WHITE, 0, 0);
        }
      }
    }
  }
}

void sr_build_block_props(const SrCity *c, int blk, int detail, const SrAdContext *ads, SrMesh *out) {
  sr_mesh_clear(out);
  if (blk < 0 || blk >= c->grid * c->grid || c->blocks[blk].water) return;
  SrDistrictType t = c->districts[c->blocks[blk].district].type;
  street_surface(c, blk, out);
  SrStreetProp props[SR_MAX_BLOCK_PROPS];
  int n = sr_dressing_block(c, blk, props, SR_MAX_BLOCK_PROPS);
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float st = (float)SR_STREET_CM / 100.0f;
  int other = 0;
  for (int i = 0; i < n; i++) {
    const SrStreetProp *p = &props[i];
    bool lot = p->x - (float)ox / 100.0f >= st && p->z - (float)oy / 100.0f >= st;
    float ground = lot ? 0.12f : 0.0f;
    if (p->kind == SR_SP_MANHOLE || p->kind == SR_SP_DRAIN || p->kind == SR_SP_PUDDLE) {
      int id = p->kind == SR_SP_MANHOLE ? SR_DECAL_MANHOLE
               : p->kind == SR_SP_DRAIN ? SR_DECAL_DRAIN
                                        : SR_DECAL_PUDDLE + (p->variant & 1);
      float w = p->kind == SR_SP_PUDDLE ? 2.4f + (float)p->variant * 0.4f : 0.9f;
      float d = p->kind == SR_SP_PUDDLE ? 1.6f : p->kind == SR_SP_DRAIN ? 0.4f : 0.9f;
      decal_q(out, p->x, ground + 0.015f + (p->kind == SR_SP_PUDDLE ? 0.004f : 0.0f), p->z, w, d, p->yaw, id,
              p->kind == SR_SP_PUDDLE ? (SrColor){120, 130, 150, 255} : (SrColor){150, 150, 160, 255});
      continue;
    }
    /* lower detail keeps every lit prop and a share of the others */
    if (!(p->flags & SR_SPF_LIT) && detail < 2) {
      other++;
      if (detail == 1 ? other % 3 == 0 : other % 3 != 0) continue;
    }
    prop_mesh(out, p, t, ground, (uint32_t)(sr_mix64(c->seed ^ ((uint64_t)blk << 16) ^ (uint64_t)i) >> 20));
  }
  overhead(c, blk, out);
  if (ads) { /* billboards for the city's corporations, following the board */
    SrBillboardSlot slots[SR_MAX_BLOCK_SLOTS];
    int ns = sr_billboard_slots(c, blk, slots, SR_MAX_BLOCK_SLOTS);
    for (int i = 0; i < ns; i++)
      sr_build_billboard(out, c, &slots[i], sr_ad_for_slot(ads, &slots[i], c->blocks[blk].district));
  }
}

int sr_block_prop_lights(const SrCity *c, int blk, SrLight *out, int max) {
  if (blk < 0 || blk >= c->grid * c->grid || c->blocks[blk].water) return 0;
  SrDistrictType t = c->districts[c->blocks[blk].district].type;
  SrStreetProp props[SR_MAX_BLOCK_PROPS];
  int n = sr_dressing_block(c, blk, props, SR_MAX_BLOCK_PROPS), k = 0;
  for (int i = 0; i < n && k < max; i++) {
    const SrStreetProp *p = &props[i];
    if (!(p->flags & SR_SPF_LIT) || p->kind == SR_SP_TRAFFIC_LIGHT) continue;
    SrLight *l = &out[k++];
    SrColor col = sr_lamp_color(t);
    float y = 2.0f, r = 8.0f;
    float fx = sinf(p->yaw), fz = cosf(p->yaw); /* toward the street */
    float px = p->x, pz = p->z;
    switch (p->kind) {
      case SR_SP_LAMP:
        if (p->variant == 3) {
          y = 3.3f;
          r = 9.0f;
          col = (SrColor){255, 210, 150, 255};
        } else {
          y = 6.2f;
          r = 18.0f;
          px += fx * 1.5f;
          pz += fz * 1.5f;
        }
        break;
      case SR_SP_BARREL:
        y = 1.6f;
        r = 16.0f;
        col = (SrColor){255, 115, 30, 255};
        break;
      case SR_SP_VENDING: col = (SrColor){90, 200, 255, 255}; break;
      case SR_SP_BOOTH: col = (SrColor){60, 230, 255, 255}; break;
      case SR_SP_STALL: col = (SrColor){255, 150, 60, 255}; break;
      case SR_SP_SHRINE: col = (SrColor){255, 160, 70, 255}; break;
      case SR_SP_HOLOTREE: col = (SrColor){90, 255, 200, 255}; break;
      case SR_SP_VENDROW: col = (SrColor){120, 160, 255, 255}; break;
      case SR_SP_MARQUEE:
        y = 3.2f;
        r = 12.0f;
        col = (SrColor){255, 200, 120, 255};
        break;
      case SR_SP_KIOSK: col = (SrColor){255, 200, 130, 255}; break;
      case SR_SP_GLOWTREE: col = (SrColor){60, 230, 150, 255}; break;
      case SR_SP_SHANTY: col = (SrColor){255, 180, 100, 255}; break;
      case SR_SP_BULB:
        y = 10.0f;
        r = 10.0f;
        col = (SrColor){255, 190, 110, 255};
        break;
      default: break;
    }
    l->pos = sr_v3(px, y, pz);
    l->radius = r;
    l->r = (float)col.r / 255.0f * 0.9f;
    l->g = (float)col.g / 255.0f * 0.9f;
    l->b = (float)col.b / 255.0f * 0.9f;
  }
  return k;
}
