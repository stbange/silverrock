/* The gloom: a sky dome that takes the fog colour (with the dawn and dusk glow), the Pall as a low
 * cloud deck lit by the districts below, halos of big lights seen through the fog, silhouettes of
 * tall buildings beyond it, and the sun of the Sunrise ending. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DECK_Y     260.0f
#define DECK_SPAN  2400.0f
#define DECK_STEPS 32

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }

static uint8_t u8(float x) { return (uint8_t)(clamp01(x) * 255.0f + 0.5f); }

static SrColor rgbf(float r, float g, float b) { return (SrColor){u8(r), u8(g), u8(b), 255}; }

void sr_sky_prepare(SrSceneCache *s, const SrCity *c) {
  if (s->sky_seed == c->seed + 1 && s->beacons) return;
  s->sky_seed = c->seed + 1;
  for (int i = 0; i < 16; i++) s->prop_cache[i].used = 0; /* another city: forget its props */
  s->distant_built = false;
  s->vertical_built = false;
  for (int i = 0; i < 24; i++) s->blocks[i].used = 0;
  s->skyline_built = false;
  free(s->beacons);
  int cap = c->building_count * 3 + c->factions.corp_count + c->grid * c->grid + 8;
  s->beacons = (SrBeacon *)calloc((size_t)cap, sizeof *s->beacons);
  s->nbeacons = 0;
  /* headquarters logos: the brightest marks in the fog */
  for (int f = 0; f < c->factions.corp_count; f++) {
    int hq = c->factions.f[f].hq_building;
    if (hq < 0) continue;
    const SrBuilding *b = &c->buildings[hq];
    SrBeacon *k = &s->beacons[s->nbeacons++];
    k->pos =
        sr_v3((float)(b->x0 + b->x1) / 200.0f, (float)b->height_cm / 100.0f + 14.0f, (float)(b->y0 + b->y1) / 200.0f);
    k->size = 46.0f;
    k->col = sr_corp_color(c->factions.f[f].hue);
  }
  for (int i = 0; i < c->building_count && s->nbeacons < cap - 1; i++) {
    const SrBuilding *b = &c->buildings[i];
    float h = (float)b->height_cm / 100.0f;
    SrFacade fc;
    sr_facade_for(c, i, &fc);
    if (h > 20.0f && (fc.roof == SR_ROOF_ANTENNAS || fc.roof == SR_ROOF_NEON_CROWN)) { /* tower beacons, crowns */
      float px = (float)(b->x0 + b->x1) / 200.0f, pz = (float)(b->y0 + b->y1) / 200.0f;
      SrBeacon *k = &s->beacons[s->nbeacons++];
      bool crown = fc.roof == SR_ROOF_NEON_CROWN;
      k->pos = sr_v3(px, h + (crown ? 2.0f : 12.0f), pz);
      k->size = crown ? 30.0f : 9.0f;
      k->col = crown ? sr_palette(c->districts[c->blocks[b->block].district].type)->neon[b->id % 3]
                     : (SrColor){255, 40, 40, 255};
    }
    if (b->poi != SR_POI_NONE && b->poi != SR_POI_HOME) { /* shop signs */
      const SrPalette *pal = sr_palette(c->districts[c->blocks[b->block].district].type);
      SrBeacon *k = &s->beacons[s->nbeacons++];
      k->pos = sr_v3((float)(b->x0 + b->x1) / 200.0f, 4.3f, (float)b->y1 / 100.0f + 0.3f);
      k->size = 12.0f;
      k->col = pal->neon[b->id % 3];
    }
  }
  /* billboards glow through the fog too */
  for (int blk = 0; blk < c->grid * c->grid; blk++) {
    SrBillboardSlot slots[SR_MAX_BLOCK_SLOTS];
    int ns = sr_billboard_slots(c, blk, slots, SR_MAX_BLOCK_SLOTS);
    for (int i = 0; i < ns && s->nbeacons < cap; i++) {
      if (slots[i].empty) continue;
      SrBeacon *k = &s->beacons[s->nbeacons++];
      k->pos = sr_v3(slots[i].x, slots[i].y + slots[i].h * 0.5f, slots[i].z);
      k->size = slots[i].w * 1.4f;
      k->col = sr_corp_color(c->factions.f[slots[i].seed % (uint32_t)SR_MAX(1, c->factions.corp_count)].hue);
    }
  }
  /* glow each block throws up at the clouds: district neon times how lit the block is */
  memset(s->deck_light, 0, sizeof s->deck_light);
  for (int blk = 0; blk < c->grid * c->grid && blk < SR_MAX_BLOCKS; blk++) {
    const SrBlock *b = &c->blocks[blk];
    if (b->water) continue;
    SrDistrictType t = c->districts[b->district].type;
    static const float k_bright[SR_D_COUNT] = {0.9f, 0.5f, 0.8f, 1.0f, 0.55f, 0.6f, 0.5f, 0.35f, 0.4f, 0.3f, 0.12f};
    int pois = 0;
    for (int i = 0; i < b->building_count; i++) pois += c->buildings[b->first_building + i].poi != SR_POI_NONE;
    float k = k_bright[t] * (0.6f + 0.15f * (float)SR_MIN(pois, 3));
    SrColor n = sr_palette(t)->neon[0];
    s->deck_light[blk] = rgbf((float)n.r / 255.0f * k, (float)n.g / 255.0f * k, (float)n.b / 255.0f * k);
  }
}

void sr_build_sky_dome(SrVec3 eye, SrMesh *out) {
  sr_mesh_clear(out);
  const int seg = 24;
  const float r = 300.0f, lo = -60.0f, hi = 320.0f;
  SrColor w = {255, 255, 255, 255};
  for (int i = 0; i < seg; i++) {
    float a0 = (float)i / (float)seg * 2.0f * SR_PI, a1 = (float)(i + 1) / (float)seg * 2.0f * SR_PI;
    SrVec3 p0 = sr_v3(eye.x + cosf(a0) * r, eye.y + lo, eye.z + sinf(a0) * r);
    SrVec3 p1 = sr_v3(eye.x + cosf(a1) * r, eye.y + lo, eye.z + sinf(a1) * r);
    /* not emissive: the shader then paints it the fog colour entirely */
    sr_mesh_quad(out, p1, p0, sr_v3(p0.x, eye.y + hi, p0.z), sr_v3(p1.x, eye.y + hi, p1.z), 0, 0, 1, 1, w,
                 SR_LAYER_WHITE, 0, 0);
    sr_mesh_quad(out, sr_v3(p0.x, eye.y + hi, p0.z), sr_v3(p1.x, eye.y + hi, p1.z), sr_v3(eye.x, eye.y + hi, eye.z),
                 sr_v3(eye.x, eye.y + hi, eye.z), 0, 0, 1, 1, w, SR_LAYER_WHITE, 0, 0);
  }
}

void sr_build_deck(const SrSceneCache *s, const SrCity *c, SrVec3 eye, const SrSky *sky, const float fog[3],
                   float time_s, int dark_district, SrMesh *base, SrMesh *glow) {
  sr_mesh_clear(base);
  sr_mesh_clear(glow);
  const float step = DECK_SPAN / DECK_STEPS;
  float ox = floorf(eye.x / step) * step - DECK_SPAN / 2, oz = floorf(eye.z / step) * step - DECK_SPAN / 2;
  float drift = time_s * 0.004f; /* the wind pushes the clouds */
  for (int j = 0; j < DECK_STEPS; j++) {
    for (int i = 0; i < DECK_STEPS; i++) {
      SrVec3 p[4];
      SrColor col[4];
      for (int k = 0; k < 4; k++) {
        float x = ox + (float)(i + (k == 1 || k == 2)) * step, z = oz + (float)(j + (k >= 2)) * step;
        p[k] = sr_v3(x, DECK_Y, z);
        float dx = x - eye.x, dz = z - eye.z, dist = sqrtf(dx * dx + dz * dz);
        float r = 0.07f, g = 0.06f, b = 0.11f; /* night underside */
        int blk = sr_city_block_at(c, (int32_t)(x * 100), (int32_t)(z * 100));
        float lit = 1.0f - sky->dayness;
        if (blk >= 0 && blk < SR_MAX_BLOCKS && c->blocks[blk].district != dark_district) { /* blackouts go dark */
          SrColor l = s->deck_light[blk];
          r += (float)l.r / 255.0f * 0.26f * lit;
          g += (float)l.g / 255.0f * 0.26f * lit;
          b += (float)l.b / 255.0f * 0.26f * lit;
        }
        r += (0.15f - 0.07f) * sky->dayness; /* slate by day, never white */
        g += (0.16f - 0.06f) * sky->dayness;
        b += (0.19f - 0.11f) * sky->dayness;
        float toward = dist > 1 ? SR_MAX(0.0f, dx / dist * sky->band_dir_x) : 0.0f;
        r += sky->band[0] * toward * 0.8f;
        g += sky->band[1] * toward * 0.8f;
        b += sky->band[2] * toward * 0.8f;
        float far = clamp01((dist - 500.0f) / 700.0f); /* blends into the fog at the edges */
        col[k] = rgbf(r + (fog[0] - r) * far, g + (fog[1] - g) * far, b + (fog[2] - b) * far);
      }
      float u0 = (ox + (float)i * step) / 420.0f + drift, v0 = (oz + (float)j * step) / 420.0f;
      float du = step / 420.0f;
      /* quads facing down: corners in the order the deck is seen from below */
      int nv = base->nv;
      sr_mesh_quad(base, p[0], p[3], p[2], p[1], u0, v0, u0 + du, v0 + du, col[0], SR_LAYER_NOISE, 255, 0);
      base->v[nv + 1].r = col[3].r, base->v[nv + 1].g = col[3].g, base->v[nv + 1].b = col[3].b;
      base->v[nv + 2].r = col[2].r, base->v[nv + 2].g = col[2].g, base->v[nv + 2].b = col[2].b;
      base->v[nv + 3].r = col[1].r, base->v[nv + 3].g = col[1].g, base->v[nv + 3].b = col[1].b;
      /* a second, faster layer of lighter cloud adds moving highlights */
      SrColor hl = {(uint8_t)(col[0].r / 3), (uint8_t)(col[0].g / 3), (uint8_t)(col[0].b / 3), 255};
      float u1 = (ox + (float)i * step) / 260.0f - drift * 1.7f, v1 = (oz + (float)j * step) / 260.0f + drift * 0.6f;
      float du1 = step / 260.0f;
      sr_mesh_quad(glow, sr_v3(p[0].x, DECK_Y - 12, p[0].z), sr_v3(p[3].x, DECK_Y - 12, p[3].z),
                   sr_v3(p[2].x, DECK_Y - 12, p[2].z), sr_v3(p[1].x, DECK_Y - 12, p[1].z), u1, v1, u1 + du1, v1 + du1,
                   hl, SR_LAYER_NOISE, 255, 0);
    }
  }
}

int sr_build_halos(const SrBeacon *b, int n, const SrBillboard *bb, float fog_end, float vis, SrMesh *out) {
  sr_mesh_clear(out);
  enum { MAX_HALOS = 48 };
  int pick[MAX_HALOS];
  float strength[MAX_HALOS];
  int count = 0;
  for (int i = 0; i < n; i++) {
    float dx = b[i].pos.x - bb->eye.x, dy = b[i].pos.y - bb->eye.y, dz = b[i].pos.z - bb->eye.z;
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    if (d <= fog_end || d > 3.0f * vis) continue;
    float l = d / vis; /* distance in fog lengths, 1..3 */
    float k = 1.0f / (1.0f + 1.6f * (l - 1.0f) * (l - 1.0f)) * clamp01((3.0f - l) * 2.0f);
    float score = k * b[i].size;
    if (count < MAX_HALOS) {
      pick[count] = i;
      strength[count++] = k;
      continue;
    }
    int weakest = 0; /* keep the brightest */
    for (int j = 1; j < MAX_HALOS; j++)
      if (strength[j] * b[pick[j]].size < strength[weakest] * b[pick[weakest]].size) weakest = j;
    if (score > strength[weakest] * b[pick[weakest]].size) {
      pick[weakest] = i;
      strength[weakest] = k;
    }
  }
  for (int j = 0; j < count; j++) {
    const SrBeacon *k = &b[pick[j]];
    float dx = k->pos.x - bb->eye.x, dz = k->pos.z - bb->eye.z, l = sqrtf(dx * dx + dz * dz) / vis;
    float size = k->size * (1.0f + 0.5f * l); /* farther lights blur wider */
    float s = strength[j] * 0.75f;
    SrColor c = {(uint8_t)((float)k->col.r * s), (uint8_t)((float)k->col.g * s), (uint8_t)((float)k->col.b * s), 255};
    sr_mesh_sprite(out, bb, k->pos, size, size, SR_SPR_HALO, SR_SPRITE_FACING, 0, c, 255);
  }
  return count;
}

void sr_build_near_halos(const SrLight *l, int n, const SrBillboard *bb, float wet_air, SrMesh *out) {
  sr_mesh_clear(out);
  float k = clamp01(wet_air) * 0.5f;
  if (k <= 0.01f) return;
  for (int i = 0; i < n; i++) {
    float size = 2.0f + l[i].radius * 0.12f;
    SrColor c = rgbf(l[i].r * k, l[i].g * k, l[i].b * k);
    sr_mesh_sprite(out, bb, l[i].pos, size, size, SR_SPR_HALO, SR_SPRITE_FACING, 0, c, 255);
  }
}

void sr_build_silhouettes(const SrCity *c, SrVec3 eye, float fog_end, const float fog[3], SrMesh *out) {
  sr_mesh_clear(out);
  SrColor col = rgbf(fog[0] * 0.78f, fog[1] * 0.78f, fog[2] * 0.8f);
  int count = 0;
  for (int i = 0; i < c->building_count && count < 96; i++) {
    const SrBuilding *b = &c->buildings[i];
    float h = (float)b->height_cm / 100.0f;
    if (h < 35.0f) continue;
    float cx = (float)(b->x0 + b->x1) / 200.0f, cz = (float)(b->y0 + b->y1) / 200.0f;
    float dx = cx - eye.x, dz = cz - eye.z, d = sqrtf(dx * dx + dz * dz);
    if (d < fog_end * 0.95f || d > fog_end * 1.8f) continue;
    float w = (float)SR_MAX(b->x1 - b->x0, b->y1 - b->y0) / 200.0f;
    float rx = -dz / d * w, rz = dx / d * w;
    sr_mesh_quad(out, sr_v3(cx - rx, 0, cz - rz), sr_v3(cx + rx, 0, cz + rz), sr_v3(cx + rx, h, cz + rz),
                 sr_v3(cx - rx, h, cz - rz), 0, 0, 1, 1, col, SR_LAYER_WHITE, 255, 0);
    count++;
  }
}

void sr_build_sun(SrVec3 eye, const SrBillboard *bb, SrMesh *out) {
  sr_mesh_clear(out);
  SrVec3 dir = sr_v3_norm(sr_v3(0.7f, 0.55f, -0.45f));
  SrVec3 p = sr_v3_add(eye, sr_v3_scale(dir, 900.0f));
  sr_mesh_sprite(out, bb, p, 160, 160, SR_SPR_GLOW, SR_SPRITE_FACING, 0, (SrColor){255, 230, 170, 255}, 255);
  sr_mesh_sprite(out, bb, p, 60, 60, SR_SPR_FLARE, SR_SPRITE_FACING, 0, (SrColor){255, 250, 230, 255}, 255);
}

/* ---- sky details ------------------------------------------------------------------------------- */

/* A beam from base along dir, facing the camera around its axis, apex at the base. */
static void beam(SrMesh *m, SrVec3 eye, SrVec3 base, SrVec3 dir, float len, float w0, float w1, SrColor col) {
  SrVec3 top = sr_v3_add(base, sr_v3_scale(dir, len));
  SrVec3 mid = sr_v3_scale(sr_v3_add(base, top), 0.5f);
  SrVec3 side = sr_v3_cross(dir, sr_v3_sub(mid, eye));
  float sl = sqrtf(side.x * side.x + side.y * side.y + side.z * side.z);
  if (sl < 1e-4f) return;
  side = sr_v3_scale(side, 1.0f / sl);
  SrAtlasCell c = sr_atlas_cell(SR_LAYER_SPRITES, SR_SPR_CONE);
  sr_mesh_quad(m, sr_v3_sub(base, sr_v3_scale(side, w0)), sr_v3_add(base, sr_v3_scale(side, w0)),
               sr_v3_add(top, sr_v3_scale(side, w1)), sr_v3_sub(top, sr_v3_scale(side, w1)), c.u0, c.v1, c.u1, c.v0,
               col, c.layer, 255, 0);
}

int sr_build_searchlights(const SrCity *c, SrVec3 eye, float time_s, float vis, uint32_t alert_mask, SrMesh *out) {
  sr_mesh_clear(out);
  int n = 0;
  for (int f = 0; f < c->factions.corp_count; f++) {
    int hq = c->factions.f[f].hq_building;
    if (hq < 0) continue;
    const SrBuilding *b = &c->buildings[hq];
    float cx = (float)(b->x0 + b->x1) / 200.0f, cz = (float)(b->y0 + b->y1) / 200.0f, h = (float)b->height_cm / 100.0f;
    float dx = cx - eye.x, dz = cz - eye.z, d = sqrtf(dx * dx + dz * dz);
    float k = 1.0f / (1.0f + (d / vis) * (d / vis) * 0.35f); /* the fog swallows far beams */
    bool alert = (alert_mask >> f) & 1u;
    float bright = (alert ? 0.5f : 0.22f) * k;
    if (bright < 0.01f) continue;
    SrColor col = sr_corp_color(c->factions.f[f].hue);
    SrColor bc = {(uint8_t)(((float)col.r * 0.5f + 128.0f) * bright),
                  (uint8_t)(((float)col.g * 0.5f + 128.0f) * bright),
                  (uint8_t)(((float)col.b * 0.5f + 128.0f) * bright), 255};
    int beams = 2 + (int)(b->id % 3u);
    for (int i = 0; i < beams; i++) {
      float ph = (float)i * 2.1f + (float)f;
      float sweep = time_s * (alert ? 0.9f : 0.35f) + ph;
      float tilt = 0.35f + 0.2f * sinf(sweep * 0.7f);
      SrVec3 dir = sr_v3_norm(sr_v3(cosf(sweep) * tilt, 1.0f, sinf(sweep) * tilt));
      float base = SR_MIN(h + 1.0f, 210.0f); /* towers above the clouds light them from below */
      float len = (262.0f - base) / dir.y;   /* up into the cloud deck */
      beam(out, eye, sr_v3(cx + (float)(i % 2) * 4.0f - 2.0f, base, cz), dir, len, 0.8f, 18.0f, bc);
      n++;
    }
  }
  /* the neon-crowned towers sweep the clouds too */
  int crowns = 0;
  for (int i = 0; i < c->building_count && crowns < 6; i++) {
    const SrBuilding *b = &c->buildings[i];
    if (b->height_cm < 9000) continue;
    float cx = (float)(b->x0 + b->x1) / 200.0f, cz = (float)(b->y0 + b->y1) / 200.0f;
    float dx = cx - eye.x, dz = cz - eye.z, d = sqrtf(dx * dx + dz * dz);
    if (d > 3.0f * vis) continue;
    SrFacade f;
    sr_facade_for(c, i, &f);
    if (f.roof != SR_ROOF_NEON_CROWN) continue;
    float k = 0.18f / (1.0f + (d / vis) * (d / vis) * 0.35f);
    SrColor bc = {(uint8_t)(220.0f * k), (uint8_t)(230.0f * k), (uint8_t)(255.0f * k), 255};
    float sweep = time_s * 0.3f + (float)i;
    SrVec3 dir = sr_v3_norm(sr_v3(cosf(sweep) * 0.3f, 1.0f, sinf(sweep) * 0.3f));
    float base = SR_MIN((float)b->height_cm / 100.0f + 1.0f, 210.0f);
    beam(out, eye, sr_v3(cx, base, cz), dir, (262.0f - base) / dir.y, 0.6f, 14.0f, bc);
    crowns++;
    n++;
  }
  return n;
}

void sr_build_blimps(const SrCity *c, const SrAdContext *ads, SrVec3 eye, float time_s, const float fog[3],
                     float fog_end, float vis, SrMesh *body, SrMesh *glow) {
  sr_mesh_clear(body);
  sr_mesh_clear(glow);
  float cx = (float)c->grid * SR_BLOCK_CM / 200.0f, cz = cx;
  int count = 2 + (int)(c->seed % 2u);
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < count; i++) {
    float r = cx * (0.35f + 0.25f * (float)i), ang = time_s * 0.004f * (i % 2 ? -1.0f : 1.0f) + (float)i * 2.4f;
    float y = 150.0f + 40.0f * (float)i;
    SrVec3 p = sr_v3(cx + cosf(ang) * r, y, cz + sinf(ang) * r);
    float yaw = -ang + (i % 2 ? -SR_PI / 2 : SR_PI / 2); /* nose along the loop */
    float dx = p.x - eye.x, dz = p.z - eye.z, d = sqrtf(dx * dx + dz * dz);
    float far = d <= fog_end ? 0.0f : SR_MIN(1.0f, (d - fog_end) / (2.0f * vis));
    if (far >= 1.0f) continue;
    SrColor hull = {(uint8_t)(50.0f + (fog[0] * 255.0f - 50.0f) * far),
                    (uint8_t)(52.0f + (fog[1] * 255.0f - 52.0f) * far),
                    (uint8_t)(60.0f + (fog[2] * 255.0f - 60.0f) * far), 255};
    SrStreetProp frame = {0, 0, 0, 0, p.x, p.z, yaw};
    (void)frame;
    float cy = cosf(yaw), sy = sinf(yaw);
    /* hull: three boxes, the middle one longest */
    for (int k = 0; k < 3; k++) {
      float len = k == 1 ? 34.0f : 24.0f, hh = k == 1 ? 9.0f : 7.0f, ww = k == 1 ? 9.0f : 7.0f;
      sr_mesh_part(body, p, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(-ww / 2, -hh / 2, -len / 2),
                   sr_v3(ww / 2, hh / 2, len / 2), hull, SR_LAYER_METAL, 0);
    }
    /* screens on both flanks with an advert */
    SrBillboardSlot slot = {0};
    slot.seed = (uint32_t)(c->seed >> (i * 5)) + (uint32_t)i * 977u;
    SrAd ad = ads ? sr_ad_for_slot(ads, &slot, -1) : (SrAd){0, false, false};
    if (ad.corp < 0) ad.corp = 0;
    SrColor sc = sr_corp_color(c->factions.f[ad.corp].hue);
    float k = 1.0f - far * 0.7f;
    SrColor scr = {(uint8_t)((float)sc.r * k), (uint8_t)((float)sc.g * k), (uint8_t)((float)sc.b * k), 255};
    SrAtlasCell lay = sr_ad_cell(SR_LAYER_ADS, (int)(slot.seed % SR_AD_LAYOUTS));
    for (int side = -1; side <= 1; side += 2) {
      float ox = (float)side * 4.6f;
      SrVec3 a = sr_v3(p.x + cy * ox + sy * -11.0f, p.y - 3.0f, p.z - sy * ox + cy * -11.0f);
      SrVec3 b = sr_v3(p.x + cy * ox + sy * 11.0f, p.y - 3.0f, p.z - sy * ox + cy * 11.0f);
      if (side < 0) {
        SrVec3 t = a;
        a = b;
        b = t;
      }
      sr_mesh_quad(body, a, b, sr_v3(b.x, b.y + 6.0f, b.z), sr_v3(a.x, a.y + 6.0f, a.z), lay.u0, lay.v1, lay.u1, lay.v0,
                   scr, lay.layer, 230, 0);
    }
    /* navigation lights, blinking, and the screen's glow in the haze */
    bool on = fmodf(time_s + (float)i * 0.4f, 1.6f) < 0.25f;
    if (on) {
      sr_mesh_sprite(glow, &bb, sr_v3(p.x + cy * 5.0f, p.y, p.z - sy * 5.0f), 4.0f, 4.0f, SR_SPR_GLOW, SR_SPRITE_FACING,
                     0, (SrColor){(uint8_t)(255 * k), 30, 30, 255}, 255);
      sr_mesh_sprite(glow, &bb, sr_v3(p.x - cy * 5.0f, p.y, p.z + sy * 5.0f), 4.0f, 4.0f, SR_SPR_GLOW, SR_SPRITE_FACING,
                     0, (SrColor){30, (uint8_t)(255 * k), 60, 255}, 255);
    }
    SrColor halo = {(uint8_t)(scr.r / 3), (uint8_t)(scr.g / 3), (uint8_t)(scr.b / 3), 255};
    sr_mesh_sprite(glow, &bb, sr_v3(p.x, p.y, p.z), 48.0f, 30.0f, SR_SPR_HALO, SR_SPRITE_FACING, 0, halo, 255);
  }
}

void sr_build_distant_city(const SrCity *c, SrMesh *out) {
  sr_mesh_clear(out);
  for (int i = 0; i < c->building_count; i++) {
    const SrBuilding *b = &c->buildings[i];
    float h = (float)b->height_cm / 100.0f;
    if (h < 14.0f) continue;
    SrFacade f;
    sr_facade_for(c, i, &f);
    /* a little inside the real footprint, so the streamed chunks always win where both exist */
    float x0 = (float)b->x0 / 100.0f + 0.4f, z0 = (float)b->y0 / 100.0f + 0.4f;
    float x1 = (float)b->x1 / 100.0f - 0.4f, z1 = (float)b->y1 / 100.0f - 0.4f;
    int first = out->nv;
    sr_mesh_box(out, sr_v3(x0, 0, z0), sr_v3(x1, h - 0.5f, z1), (SrColor){f.tint[0], f.tint[1], f.tint[2], 255},
                SR_LAYER_WINMOD_FIRST + SR_WIN_OFFICE, 0.15f, 0);
    for (int v = first; v < out->nv; v++) out->v[v].pad = (uint8_t)(f.use << 6 | (f.seed & 63u));
  }
}

void sr_build_skyline_glow(const SrCity *c, SrMesh *out) {
  sr_mesh_clear(out);
  float cx = (float)c->grid * SR_BLOCK_CM / 200.0f, cz = cx;
  float radius = cx * 1.6f + 600.0f;
  const int segments = 90;
  SrColor glow = {70, 40, 90, 255}, none = {0, 0, 0, 255};
  for (int i = 0; i < segments; i++) { /* the city's light caught in the clouds above the horizon */
    float a0 = (float)i / segments * 2.0f * SR_PI, a1 = (float)(i + 1) / segments * 2.0f * SR_PI;
    SrVec3 p0 = sr_v3(cx + cosf(a0) * radius, 120.0f, cz + sinf(a0) * radius);
    SrVec3 p1 = sr_v3(cx + cosf(a1) * radius, 120.0f, cz + sinf(a1) * radius);
    int nv = out->nv;
    sr_mesh_quad(out, p1, p0, sr_v3(p0.x, 420.0f, p0.z), sr_v3(p1.x, 420.0f, p1.z), 0, 0, 1, 1, glow, SR_LAYER_WHITE,
                 255, 0);
    for (int k = 2; k < 4; k++) { /* fading upward */
      out->v[nv + k].r = none.r;
      out->v[nv + k].g = none.g;
      out->v[nv + k].b = none.b;
    }
  }
}
