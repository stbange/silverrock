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
void sr_build_poi_fronts(const SrCity *c, const SrDeltas *d, SrVec3 eye, int64_t t, SrMesh *out) {
  sr_mesh_clear(out);
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
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
        bool open = sr_deltas_get(d, bd->id) != 0 && sr_poi_open(c, bi, t);
        if (!open) continue;
        float x0 = (float)bd->x0 / 100.0f, x1 = (float)bd->x1 / 100.0f, z1 = (float)bd->y1 / 100.0f + 0.04f;
        wall(out, x0, z1, x1, z1, 0.0f, BASE_H, f.uv_u, f.uv_u + (x1 - x0) / 9.0f, 1.0f, 0.0f, rgb(f.tint, 100),
             SR_LAYER_BASE_FIRST + SR_BASE_SHOPFRONT, 110, 0);
      }
    }
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
