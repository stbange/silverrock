/* Buildings from their facades: a 4.5 m ground-floor band, a shaft of a window module with a texture
 * offset per building, blank walls toward close neighbours, and a crown; shopfronts of open places
 * near the camera are drawn each frame over the shutters baked into the chunks. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

#define BASE_H 4.5f

static SrColor rgb(const uint8_t t[3], int pct) {
  return (SrColor){(uint8_t)(t[0] * pct / 100), (uint8_t)(t[1] * pct / 100), (uint8_t)(t[2] * pct / 100), 255};
}

/* One wall of a box from y0 to y1 between corners (ax, az) and (bx, bz), seen from outside. */
static void wall(SrMesh *m, float ax, float az, float bx, float bz, float y0, float y1, float u0, float u1, float v0,
                 float v1, SrColor col, int layer, int em, uint8_t pad) {
  int first = m->nv;
  sr_mesh_quad(m, sr_v3(ax, y0, az), sr_v3(bx, y0, bz), sr_v3(bx, y1, bz), sr_v3(ax, y1, az), u0, v0, u1, v1, col,
               layer, em, 0);
  for (int i = first; i < m->nv; i++) m->v[i].pad = pad;
}

static int base_em(int base) {
  switch (base) {
    case SR_BASE_SHOPFRONT: return 110;
    case SR_BASE_LOBBY: return 35;
    case SR_BASE_STALL: return 100;
    default: return 0;
  }
}

void sr_build_box_facade(SrMesh *m, const SrFacade *f, float x0, float z0, float x1, float z1, float ybot, float ytop,
                         bool with_base, uint8_t blank) {
  const float ut = 1.0f / 6.4f, vt = 1.0f / (4.0f * f->floor_h), bt = 1.0f / 9.0f; /* windows 1.6 m apart */
  uint8_t pad = (uint8_t)(f->use << 6 | (f->seed & 63u));
  SrColor tint = rgb(f->tint, 100), plain = rgb(f->tint, 80);
  /* corners in counter-clockwise order seen from above, faces: +z (front), +x, -z, -x */
  float cx[4][4] = {{x0, z1, x1, z1}, {x1, z1, x1, z0}, {x1, z0, x0, z0}, {x0, z0, x0, z1}};
  static const uint8_t face_bit[4] = {8u, 2u, 4u, 1u};
  for (int k = 0; k < 4; k++) {
    float ax = cx[k][0], az = cx[k][1], bx = cx[k][2], bz = cx[k][3];
    float len = fabsf(bx - ax) + fabsf(bz - az);
    bool is_blank = (blank & face_bit[k]) != 0;
    float shaft0 = ybot;
    if (with_base && ybot < 0.01f) {
      int base = is_blank ? SR_BASE_POSTERS : f->base;
      if (k == 0 && base == SR_BASE_SHOPFRONT) base = SR_BASE_SHUTTER; /* places: the open front is drawn live */
      wall(m, ax, az, bx, bz, 0.0f, SR_MIN(BASE_H, ytop), f->uv_u, f->uv_u + len * bt, 1.0f, 0.0f, tint,
           SR_LAYER_BASE_FIRST + base, base_em(base), 0);
      shaft0 = BASE_H;
    }
    if (ytop <= shaft0) continue;
    if (is_blank) { /* a firewall: plain material, no windows, a drain pipe and air conditioners */
      int layer = f->winmod == SR_WIN_BRICK ? SR_LAYER_BRICK : SR_LAYER_CONCRETE;
      wall(m, ax, az, bx, bz, shaft0, ytop, 0, len * 0.12f, (ytop - shaft0) * 0.12f, 0, plain, layer, 0, 0);
      float dx = (bx - ax) / len, dz = (bz - az) / len, nx = dz, nz = -dx; /* along the wall, and out of it */
      float px = ax + dx * 0.6f + nx * 0.15f, pz = az + dz * 0.6f + nz * 0.15f;
      SrColor pipe = {70, 72, 78, 255};
      sr_mesh_box(m, sr_v3(px - 0.1f, 0.0f, pz - 0.1f), sr_v3(px + 0.1f, ytop, pz + 0.1f), pipe, SR_LAYER_METAL, 0.0f,
                  0);
      int units = 2 + (int)(f->seed % 4u);
      for (int u = 0; u < units; u++) {
        float t = 0.2f + 0.6f * (float)((f->seed * 7u + (unsigned)u * 13u) % 10u) / 10.0f;
        float y = shaft0 + 1.0f + (float)((f->seed + (unsigned)u * 5u) % 7u) * f->floor_h;
        if (y > ytop - 1.0f) continue;
        float ux = ax + dx * len * t + nx * 0.35f, uz = az + dz * len * t + nz * 0.35f;
        sr_mesh_box(m, sr_v3(ux - 0.45f, y, uz - 0.45f), sr_v3(ux + 0.45f, y + 0.7f, uz + 0.45f),
                    (SrColor){130, 132, 135, 255}, SR_LAYER_METAL, 0.0f, 0);
      }
    } else {
      float vb = f->uv_v + (ytop - shaft0) * vt;
      wall(m, ax, az, bx, bz, shaft0, ytop, f->uv_u, f->uv_u + len * ut, vb, f->uv_v, tint,
           SR_LAYER_WINMOD_FIRST + f->winmod, 0, pad);
    }
  }
}

/* Crown on a flat roof at height h: parapet, cornice or nothing (neon crowns are added with roofs). */
void sr_build_crown(SrMesh *m, const SrFacade *f, float x0, float z0, float x1, float z1, float h) {
  SrColor trim = rgb(f->tint, 90);
  float t = 0.35f;
  if (f->crown == SR_CROWN_PARAPET || f->crown == SR_CROWN_NEON) {
    int layer = SR_LAYER_TRIM_FIRST + 2;
    sr_mesh_box(m, sr_v3(x0, h, z0), sr_v3(x1, h + 1.0f, z0 + t), trim, layer, 0.25f, 0);
    sr_mesh_box(m, sr_v3(x0, h, z1 - t), sr_v3(x1, h + 1.0f, z1), trim, layer, 0.25f, 0);
    sr_mesh_box(m, sr_v3(x0, h, z0 + t), sr_v3(x0 + t, h + 1.0f, z1 - t), trim, layer, 0.25f, 0);
    sr_mesh_box(m, sr_v3(x1 - t, h, z0 + t), sr_v3(x1, h + 1.0f, z1 - t), trim, layer, 0.25f, 0);
  } else if (f->crown == SR_CROWN_CORNICE) { /* a moulded band that juts out at the top */
    sr_mesh_box(m, sr_v3(x0 - 0.4f, h - 1.2f, z0 - 0.4f), sr_v3(x1 + 0.4f, h + 0.3f, z1 + 0.4f), trim,
                SR_LAYER_TRIM_FIRST + 3, 0.0f, 0);
  }
  /* the roof itself: gravel */
  sr_mesh_quad(m, sr_v3(x0, h + 0.02f, z1), sr_v3(x1, h + 0.02f, z1), sr_v3(x1, h + 0.02f, z0),
               sr_v3(x0, h + 0.02f, z0), 0, 0, (x1 - x0) * 0.1f, (z1 - z0) * 0.1f, rgb(f->tint, 55),
               SR_LAYER_TRIM_FIRST + 4, 0, 0);
}

/* Open places near the camera: the lit shopfront, lobby or stall over the shutter of the chunk. */
float sr_shutter_open(const SrCity *c, int building, int64_t t, int time_rate) {
  /* 1 open, 0 shut; in between while the shutter rolls, over about three real seconds */
  int open, hours;
  sr_poi_hours(c, building, &open, &hours);
  if (hours >= 24) return 1.0f;
  float roll = 3.0f * (float)SR_MAX(time_rate, 1);
  float since = (float)(((t % 86400) - (int64_t)open * 3600 + 86400) % 86400);
  float until = (float)hours * 3600.0f - since;
  if (until <= 0) return 0.0f;
  return sr_clampf(fminf(since / roll, until / roll), 0, 1);
}

int sr_build_poi_fronts(const SrCity *c, const SrDeltas *d, SrVec3 eye, int64_t t, int time_rate, SrMesh *out) {
  sr_mesh_clear(out);
  int rolling = 0;
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return 0;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      const SrBlock *b = &c->blocks[by * c->grid + bx];
      for (int i = 0; i < b->building_count; i++) {
        int bi = b->first_building + i;
        const SrBuilding *bd = &c->buildings[bi];
        if (bd->poi == SR_POI_NONE) continue;
        SrFacade f;
        sr_facade_for(c, bi, &f);
        if (f.base != SR_BASE_SHOPFRONT) continue; /* lobbies and stalls are always lit in the chunk */
        if (sr_deltas_get(d, bd->id) == 0) continue;
        float u = sr_shutter_open(c, bi, t, time_rate);
        if (u <= 0) continue;
        if (u < 1) { /* rolling: at most two near the camera, the rest snap */
          if (rolling >= 2) u = sr_poi_open(c, bi, t) ? 1.0f : 0.0f;
          if (u <= 0) continue;
          if (u < 1) rolling++;
        }
        float x0 = (float)bd->x0 / 100.0f, x1 = (float)bd->x1 / 100.0f, z1 = (float)bd->y1 / 100.0f + 0.04f;
        float top = BASE_H * u; /* the lit front shows below the rising shutter */
        wall(out, x0, z1, x1, z1, 0.0f, top, f.uv_u, f.uv_u + (x1 - x0) / 9.0f, 1.0f, 1.0f - u, rgb(f.tint, 100),
             SR_LAYER_BASE_FIRST + SR_BASE_SHOPFRONT, 110, 0);
        if (u < 1) /* the shutter's bottom bar */
          sr_mesh_box(out, sr_v3(x0, top - 0.08f, z1), sr_v3(x1, top, z1 + 0.06f), (SrColor){90, 92, 96, 255},
                      SR_LAYER_METAL, 0, 0);
      }
    }
  return rolling;
}

/* The door of a place, under its sign (a dark recess with a lit frame). */
void sr_build_door(SrMesh *m, float cx, float z, SrColor frame) {
  sr_mesh_quad(m, sr_v3(cx - 0.8f, 0.0f, z + 0.06f), sr_v3(cx + 0.8f, 0.0f, z + 0.06f),
               sr_v3(cx + 0.8f, 2.5f, z + 0.06f), sr_v3(cx - 0.8f, 2.5f, z + 0.06f), 0, 0, 1, 1,
               (SrColor){14, 14, 18, 255}, SR_LAYER_WHITE, 0, 0);
  sr_mesh_quad(m, sr_v3(cx - 0.95f, 2.5f, z + 0.07f), sr_v3(cx + 0.95f, 2.5f, z + 0.07f),
               sr_v3(cx + 0.95f, 2.65f, z + 0.07f), sr_v3(cx - 0.95f, 2.65f, z + 0.07f), 0, 0, 1, 1, frame,
               SR_LAYER_WHITE, 200, 0);
}

/* ---- shapes, roofs and landmarks ------------------------------------------------------------------ */

static uint32_t rh(uint32_t seed, uint32_t k) { return (uint32_t)(sr_mix64(((uint64_t)seed << 32) ^ k) >> 20); }

static void prism_gable(SrMesh *m, float x0, float z0, float x1, float z1, float y, float ridge, SrColor col,
                        int layer) {
  float zm = (z0 + z1) * 0.5f, top = y + ridge, slope = sqrtf(ridge * ridge + (zm - z0) * (zm - z0));
  float u = (x1 - x0) * 0.12f, v = slope * 0.12f;
  /* two slopes, ridge along x */
  sr_mesh_quad(m, sr_v3(x0, y, z1), sr_v3(x1, y, z1), sr_v3(x1, top, zm), sr_v3(x0, top, zm), 0, v, u, 0, col, layer, 0,
               0);
  sr_mesh_quad(m, sr_v3(x1, y, z0), sr_v3(x0, y, z0), sr_v3(x0, top, zm), sr_v3(x1, top, zm), 0, v, u, 0, col, layer, 0,
               0);
  /* gable ends as degenerate quads */
  SrColor end = {(uint8_t)(col.r * 3 / 4), (uint8_t)(col.g * 3 / 4), (uint8_t)(col.b * 3 / 4), 255};
  sr_mesh_quad(m, sr_v3(x0, y, z0), sr_v3(x0, y, z1), sr_v3(x0, top, zm), sr_v3(x0, top, zm), 0, 1, 1, 0, end,
               SR_LAYER_CONCRETE, 0, 0);
  sr_mesh_quad(m, sr_v3(x1, y, z1), sr_v3(x1, y, z0), sr_v3(x1, top, zm), sr_v3(x1, top, zm), 0, 1, 1, 0, end,
               SR_LAYER_CONCRETE, 0, 0);
}

static void roof_kit(SrMesh *m, const SrFacade *f, const SrVolume *v, uint32_t seed, SrColor neon) {
  float x0 = v->x0, z0 = v->z0, x1 = v->x1, z1 = v->z1, y = v->y1, w = x1 - x0, d = z1 - z0;
  SrColor metal = {120, 124, 130, 255}, dark = {70, 72, 78, 255};
  switch (f->roof) {
    case SR_ROOF_TANKS:
      for (int i = 0; i < 1 + (int)(seed % 2u); i++) {
        float cx = x0 + w * (0.25f + 0.4f * (float)i), cz = z0 + d * 0.35f;
        for (int l = 0; l < 4; l++) { /* legs */
          float lx = (float)(l % 2) * 2.0f, lz = (float)(l >> 1) * 2.0f;
          sr_mesh_box(m, sr_v3(cx - 1.1f + lx, y, cz - 1.1f + lz), sr_v3(cx - 0.9f + lx, y + 2.0f, cz - 0.9f + lz),
                      dark, SR_LAYER_METAL, 0, 0);
        }
        sr_mesh_box(m, sr_v3(cx - 1.3f, y + 2.0f, cz - 1.3f), sr_v3(cx + 1.3f, y + 5.0f, cz + 1.3f),
                    (SrColor){110, 92, 70, 255}, SR_LAYER_TRIM_FIRST + 1, 0.4f, 0);
        sr_mesh_box(m, sr_v3(cx - 0.9f, y + 5.0f, cz - 0.9f), sr_v3(cx + 0.9f, y + 5.6f, cz + 0.9f), dark,
                    SR_LAYER_METAL, 0, 0);
      }
      break;
    case SR_ROOF_HVAC:
      for (int i = 0; i < 2 + (int)(seed % 3u); i++) {
        float cx = x0 + w * (0.2f + 0.2f * (float)i), cz = z0 + d * (0.3f + 0.15f * (float)(i % 2));
        sr_mesh_box(m, sr_v3(cx - 1.0f, y, cz - 0.8f), sr_v3(cx + 1.0f, y + 1.3f, cz + 0.8f), metal, SR_LAYER_METAL,
                    0.5f, 0);
      }
      sr_mesh_box(m, sr_v3(x0 + w * 0.2f, y + 0.4f, z0 + d * 0.62f), sr_v3(x0 + w * 0.8f, y + 1.0f, z0 + d * 0.68f),
                  dark, SR_LAYER_METAL, 0.5f, 0);
      break;
    case SR_ROOF_ANTENNAS:
      for (int i = 0; i < 3 + (int)(seed % 3u); i++) {
        float cx = x0 + w * (0.2f + 0.15f * (float)i), cz = z0 + d * (0.3f + 0.1f * (float)(i % 3));
        float hh = 6.0f + (float)(rh(seed, (uint32_t)i) % 9u);
        sr_mesh_box(m, sr_v3(cx - 0.08f, y, cz - 0.08f), sr_v3(cx + 0.08f, y + hh, cz + 0.08f), dark, SR_LAYER_WHITE, 0,
                    0);
        int bfirst = m->nv;
        sr_mesh_box(m, sr_v3(cx - 0.22f, y + hh, cz - 0.22f), sr_v3(cx + 0.22f, y + hh + 0.4f, cz + 0.22f),
                    (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE, 0, 230);
        for (int vv = bfirst; vv < m->nv; vv++) m->v[vv].pad = (uint8_t)(0x80u | (56u + seed % 8u)); /* blink in sync */
      }
      break;
    case SR_ROOF_HELIPAD: {
      float s = SR_MIN(w, d) * 0.7f, cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
      sr_mesh_quad(m, sr_v3(cx - s / 2, y + 0.06f, cz + s / 2), sr_v3(cx + s / 2, y + 0.06f, cz + s / 2),
                   sr_v3(cx + s / 2, y + 0.06f, cz - s / 2), sr_v3(cx - s / 2, y + 0.06f, cz - s / 2), 0, 1, 1, 0,
                   (SrColor){200, 200, 200, 255}, SR_LAYER_TRIM_FIRST + 5, 0, 0);
      for (int k = 0; k < 4; k++) {
        float lx = cx + (k % 2 ? s / 2 : -s / 2), lz = cz + ((k >> 1) ? s / 2 : -s / 2);
        sr_mesh_box(m, sr_v3(lx - 0.2f, y, lz - 0.2f), sr_v3(lx + 0.2f, y + 0.3f, lz + 0.2f),
                    (SrColor){80, 255, 120, 255}, SR_LAYER_WHITE, 0, 255);
      }
      break;
    }
    case SR_ROOF_NEON_CROWN: { /* a band of light around the top and fins at the corners */
      float t = 2.5f;
      SrColor c = neon;
      sr_mesh_quad(m, sr_v3(x0, y - t, z1 + 0.1f), sr_v3(x1, y - t, z1 + 0.1f), sr_v3(x1, y - 0.5f, z1 + 0.1f),
                   sr_v3(x0, y - 0.5f, z1 + 0.1f), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
      sr_mesh_quad(m, sr_v3(x1, y - t, z0 - 0.1f), sr_v3(x0, y - t, z0 - 0.1f), sr_v3(x0, y - 0.5f, z0 - 0.1f),
                   sr_v3(x1, y - 0.5f, z0 - 0.1f), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
      sr_mesh_quad(m, sr_v3(x1 + 0.1f, y - t, z1), sr_v3(x1 + 0.1f, y - t, z0), sr_v3(x1 + 0.1f, y - 0.5f, z0),
                   sr_v3(x1 + 0.1f, y - 0.5f, z1), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
      sr_mesh_quad(m, sr_v3(x0 - 0.1f, y - t, z0), sr_v3(x0 - 0.1f, y - t, z1), sr_v3(x0 - 0.1f, y - 0.5f, z1),
                   sr_v3(x0 - 0.1f, y - 0.5f, z0), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
      for (int k = 0; k < 4; k++) {
        float fx = k % 2 ? x1 - 0.4f : x0, fz = k / 2 ? z1 - 0.4f : z0;
        sr_mesh_box(m, sr_v3(fx, y, fz), sr_v3(fx + 0.4f, y + 8.0f, fz + 0.4f), c, SR_LAYER_WHITE, 0, 200);
      }
      break;
    }
    case SR_ROOF_SAWTOOTH: { /* teeth along x: glass facing north, slopes facing south */
      int teeth = SR_MAX(2, (int)(w / 6.0f));
      for (int i = 0; i < teeth; i++) {
        float tx0 = x0 + w * (float)i / (float)teeth, tx1 = x0 + w * (float)(i + 1) / (float)teeth;
        (void)tx1;
        float zz = z0 + d * (float)i / (float)teeth, zz1 = z0 + d * (float)(i + 1) / (float)teeth;
        (void)tx0;
        sr_mesh_quad(m, sr_v3(x1, y, zz), sr_v3(x0, y, zz), sr_v3(x0, y + 2.5f, zz), sr_v3(x1, y + 2.5f, zz), 0, 1, 1,
                     0, (SrColor){180, 190, 160, 255}, SR_LAYER_WINMOD_FIRST + SR_WIN_CLERESTORY, 80, 0);
        sr_mesh_quad(m, sr_v3(x0, y, zz1), sr_v3(x1, y, zz1), sr_v3(x1, y + 2.5f, zz), sr_v3(x0, y + 2.5f, zz), 0, 1,
                     w * 0.1f, 0, metal, SR_LAYER_TRIM_FIRST + 1, 0, 0);
      }
      break;
    }
    case SR_ROOF_CORRUGATED: prism_gable(m, x0, z0, x1, z1, y, 1.6f, metal, SR_LAYER_TRIM_FIRST + 1); break;
    case SR_ROOF_DISHES:
      for (int i = 0; i < 2 + (int)(seed % 2u); i++) {
        float cx = x0 + w * (0.25f + 0.25f * (float)i), cz = z0 + d * 0.7f;
        sr_mesh_box(m, sr_v3(cx - 0.06f, y, cz - 0.06f), sr_v3(cx + 0.06f, y + 1.4f, cz + 0.06f), dark, SR_LAYER_WHITE,
                    0, 0);
        sr_mesh_quad(m, sr_v3(cx - 0.7f, y + 1.0f, cz + 0.2f), sr_v3(cx + 0.7f, y + 1.0f, cz + 0.2f),
                     sr_v3(cx + 0.7f, y + 2.2f, cz - 0.4f), sr_v3(cx - 0.7f, y + 2.2f, cz - 0.4f), 0, 1, 1, 0,
                     (SrColor){190, 190, 195, 255}, SR_LAYER_CONCRETE, 0, 0);
      }
      break;
    case SR_ROOF_STACK: {
      float cx = x0 + w * 0.7f, cz = z0 + d * 0.3f, hh = 10.0f + (float)(seed % 7u);
      sr_mesh_box(m, sr_v3(cx - 0.8f, y, cz - 0.8f), sr_v3(cx + 0.8f, y + hh, cz + 0.8f), (SrColor){140, 80, 60, 255},
                  SR_LAYER_BRICK, 0.5f, 0);
      sr_mesh_box(m, sr_v3(cx - 0.85f, y + hh - 2.0f, cz - 0.85f), sr_v3(cx + 0.85f, y + hh - 1.2f, cz + 0.85f),
                  (SrColor){230, 230, 230, 255}, SR_LAYER_WHITE, 0, 0);
      break;
    }
    default: break;
  }
}

/* Crowns of corporate headquarters, from the tower's silhouette: spire, ring, blade or dome. */
static void hq_crown(SrMesh *m, int kind, float x0, float z0, float x1, float z1, float y, SrColor c) {
  float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f, w = x1 - x0, d = z1 - z0;
  SrColor steel = {90, 96, 110, 255};
  switch (kind % 4) {
    case 0: /* spire: a tapering mast */
      for (int i = 0; i < 4; i++) {
        float s = 2.0f - 0.45f * (float)i;
        sr_mesh_box(m, sr_v3(cx - s, y + (float)i * 9.0f, cz - s), sr_v3(cx + s, y + (float)(i + 1) * 9.0f, cz + s),
                    steel, SR_LAYER_METAL, 0.3f, 0);
      }
      sr_mesh_box(m, sr_v3(cx - 0.3f, y + 36.0f, cz - 0.3f), sr_v3(cx + 0.3f, y + 38.0f, cz + 0.3f), c, SR_LAYER_WHITE,
                  0, 255);
      break;
    case 1: /* ring: a floating square of light */
      for (int k = 0; k < 4; k++) {
        float r = 0.6f;
        SrVec3 lo = k == 0   ? sr_v3(x0, y + 8, z0)
                    : k == 1 ? sr_v3(x0, y + 8, z1 - r)
                    : k == 2 ? sr_v3(x0, y + 8, z0)
                             : sr_v3(x1 - r, y + 8, z0);
        SrVec3 hi = k == 0   ? sr_v3(x1, y + 9, z0 + r)
                    : k == 1 ? sr_v3(x1, y + 9, z1)
                    : k == 2 ? sr_v3(x0 + r, y + 9, z1)
                             : sr_v3(x1, y + 9, z1);
        sr_mesh_box(m, lo, hi, c, SR_LAYER_WHITE, 0, 230);
      }
      sr_mesh_box(m, sr_v3(cx - 0.5f, y, cz - 0.5f), sr_v3(cx + 0.5f, y + 8, cz + 0.5f), steel, SR_LAYER_METAL, 0, 0);
      break;
    case 2: /* blade: a thin slab with lit edges */
      sr_mesh_box(m, sr_v3(cx - w * 0.35f, y, cz - 0.4f), sr_v3(cx + w * 0.35f, y + 24.0f, cz + 0.4f), steel,
                  SR_LAYER_WINMOD_FIRST + SR_WIN_CURTAIN, 0.08f, 0);
      sr_mesh_box(m, sr_v3(cx - w * 0.35f, y + 24.0f, cz - 0.45f), sr_v3(cx + w * 0.35f, y + 24.6f, cz + 0.45f), c,
                  SR_LAYER_WHITE, 0, 255);
      break;
    default: /* dome: stacked shrinking boxes */
      for (int i = 0; i < 4; i++) {
        float s = 0.42f - 0.09f * (float)i;
        sr_mesh_box(m, sr_v3(cx - w * s, y + (float)i * 2.5f, cz - d * s),
                    sr_v3(cx + w * s, y + (float)(i + 1) * 2.5f, cz + d * s), i == 3 ? c : steel, SR_LAYER_METAL, 0.3f,
                    i == 3 ? 200 : 0);
      }
      break;
  }
}

void sr_build_building(const SrCity *c, int bi, SrColor neon, SrMesh *m) {
  const SrBuilding *bd = &c->buildings[bi];
  SrFacade fc;
  sr_facade_for(c, bi, &fc);
  float bx0 = (float)bd->x0 / 100.0f, bz0 = (float)bd->y0 / 100.0f, bx1 = (float)bd->x1 / 100.0f,
        bz1 = (float)bd->y1 / 100.0f, h = (float)bd->height_cm / 100.0f;
  uint32_t seed = (uint32_t)(sr_mix64(c->seed ^ ((uint64_t)bd->id << 17)) >> 24);
  if (bd->poi == SR_POI_CORP_HQ && bd->owner >= 0) { /* landmark tower: stacked tiers, then its crown */
    int tiers = 1 + bd->silhouette[0];
    float taper = 0.06f + 0.05f * bd->silhouette[1];
    float lx0 = bx0, lz0 = bz0, lx1 = bx1, lz1 = bz1, y = 0;
    for (int t = 0; t < tiers; t++) {
      float th = h / (float)tiers;
      sr_build_box_facade(m, &fc, lx0, lz0, lx1, lz1, y, y + th, t == 0, t == 0 ? fc.blank : 0);
      sr_build_crown(m, &fc, lx0, lz0, lx1, lz1, y + th);
      float dx = (lx1 - lx0) * taper, dz = (lz1 - lz0) * taper;
      if (t < tiers - 1) {
        lx0 += dx;
        lx1 -= dx;
        lz0 += dz;
        lz1 -= dz;
      }
      y += th;
    }
    hq_crown(m, bd->silhouette[2], lx0, lz0, lx1, lz1, h + 1.0f, sr_corp_color(c->factions.f[bd->owner].hue));
    return;
  }
  SrVolume v[SR_MAX_VOLUMES];
  int n = sr_building_volumes(c, bi, &fc, v);
  for (int i = 0; i < n; i++) {
    /* faces on the footprint edge keep their blank walls; inner volumes have windows all round */
    uint8_t blank = 0;
    if (fabsf(v[i].x0 - bx0) < 0.01f) blank |= fc.blank & 1u;
    if (fabsf(v[i].x1 - bx1) < 0.01f) blank |= fc.blank & 2u;
    if (fabsf(v[i].z0 - bz0) < 0.01f) blank |= fc.blank & 4u;
    if (fabsf(v[i].z1 - bz1) < 0.01f) blank |= fc.blank & 8u;
    sr_build_box_facade(m, &fc, v[i].x0, v[i].z0, v[i].x1, v[i].z1, v[i].y0, v[i].y1, (v[i].flags & SR_VOL_BASE) != 0,
                        blank);
    if (v[i].flags & SR_VOL_PITCHED) {
      float ridge = SR_MAX(2.5f, (v[i].z1 - v[i].z0) * 0.3f);
      prism_gable(m, v[i].x0, v[i].z0, v[i].x1, v[i].z1, v[i].y1, SR_MIN(ridge, 6.0f), (SrColor){150, 80, 60, 255},
                  SR_LAYER_TRIM_FIRST + 0);
    } else if ((v[i].flags & SR_VOL_ROOF) && (fc.roof == SR_ROOF_SAWTOOTH || fc.roof == SR_ROOF_CORRUGATED)) {
      roof_kit(m, &fc, &v[i], seed, neon);
    } else {
      SrFacade crown = fc;
      if (!(v[i].flags & SR_VOL_CROWN)) crown.crown = SR_CROWN_NONE;
      sr_build_crown(m, &crown, v[i].x0, v[i].z0, v[i].x1, v[i].z1, v[i].y1);
      if (v[i].flags & SR_VOL_ROOF) roof_kit(m, &fc, &v[i], seed, neon);
    }
  }
  if (bd->poi == SR_POI_CITY_HALL) { /* columns and steps in front of the hall */
    for (int k = 0; k < 6; k++) {
      float cx = bx0 + (bx1 - bx0) * (0.12f + 0.152f * (float)k);
      if (fabsf(cx - (bx0 + bx1) * 0.5f) < 1.5f) continue; /* the door stays clear */
      sr_mesh_box(m, sr_v3(cx - 0.4f, 0.3f, bz1 + 1.0f), sr_v3(cx + 0.4f, 9.0f, bz1 + 1.8f),
                  (SrColor){190, 186, 176, 255}, SR_LAYER_CONCRETE, 0.3f, 0);
    }
    sr_mesh_box(m, sr_v3(bx0, 9.0f, bz1), sr_v3(bx1, 10.2f, bz1 + 2.2f), (SrColor){180, 176, 168, 255},
                SR_LAYER_TRIM_FIRST + 3, 0.0f, 0);
    sr_mesh_box(m, sr_v3(bx0 + 2.0f, 0.0f, bz1), sr_v3(bx1 - 2.0f, 0.3f, bz1 + 2.4f), (SrColor){150, 150, 150, 255},
                SR_LAYER_CONCRETE, 0.3f, 0);
  }
}
