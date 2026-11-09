/* Scene building: palettes, lights, city chunks, characters, rain, skyline, landmarks, frame. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- palettes ------------------------------------------------------------------------------ */

static const SrPalette k_palettes[SR_D_COUNT] = {
    /* fog, ambient, neon x3, ground */
    {{20, 34, 52, 255},
     {150, 190, 230, 255},
     {{120, 230, 255, 255}, {230, 250, 255, 255}, {80, 160, 255, 255}},
     {70, 80, 95, 255}}, /* spire: cold white & cyan */
    {{28, 30, 44, 255},
     {190, 190, 210, 255},
     {{240, 240, 255, 255}, {70, 110, 255, 255}, {255, 60, 60, 255}},
     {85, 85, 90, 255}}, /* civic: concrete, police blue */
    {{40, 22, 40, 255},
     {200, 160, 200, 255},
     {{255, 200, 40, 255}, {40, 255, 120, 255}, {255, 70, 160, 255}},
     {75, 70, 75, 255}}, /* commercial: every colour */
    {{44, 12, 50, 255},
     {210, 120, 230, 255},
     {{255, 40, 200, 255}, {170, 60, 255, 255}, {255, 120, 240, 255}},
     {60, 45, 65, 255}}, /* strip: magenta & violet */
    {{14, 40, 44, 255},
     {120, 200, 200, 255},
     {{40, 240, 220, 255}, {255, 110, 180, 255}, {120, 255, 230, 255}},
     {65, 75, 78, 255}}, /* residential: teal & pink */
    {{48, 18, 12, 255},
     {230, 160, 110, 255},
     {{255, 60, 40, 255}, {255, 190, 60, 255}, {255, 120, 40, 255}},
     {80, 60, 50, 255}}, /* old quarter: lanterns & gold */
    {{50, 30, 10, 255},
     {240, 170, 90, 255},
     {{255, 160, 40, 255}, {255, 120, 20, 255}, {255, 200, 120, 255}},
     {70, 62, 55, 255}}, /* industrial: sodium orange */
    {{12, 26, 44, 255},
     {120, 160, 220, 255},
     {{60, 140, 255, 255}, {255, 180, 60, 255}, {200, 220, 255, 255}},
     {55, 62, 72, 255}}, /* docks: blue & amber */
    {{20, 40, 28, 255},
     {170, 230, 190, 255},
     {{120, 255, 160, 255}, {240, 255, 240, 255}, {60, 200, 120, 255}},
     {75, 95, 80, 255}}, /* campus: sterile green */
    {{10, 30, 12, 255},
     {110, 200, 100, 255},
     {{80, 255, 60, 255}, {255, 40, 40, 255}, {160, 255, 80, 255}},
     {40, 48, 40, 255}}, /* undercity: toxic green & red */
    {{16, 12, 10, 255},
     {130, 100, 80, 255},
     {{255, 110, 30, 255}, {200, 80, 30, 255}, {255, 160, 80, 255}},
     {45, 40, 36, 255}}, /* barrens: fires */
};

const SrPalette *sr_palette(SrDistrictType t) { return &k_palettes[(unsigned)t < SR_D_COUNT ? t : SR_D_RESIDENTIAL]; }

SrColor sr_corp_color(int hue) {
  /* 24 hand-picked corporate colours; any two that are two steps apart differ clearly */
  static const SrColor k[24] = {{255, 40, 40, 255},   {255, 90, 30, 255},   {255, 150, 30, 255},  {255, 200, 40, 255},
                                {250, 250, 60, 255},  {190, 255, 40, 255},  {110, 255, 60, 255},  {40, 220, 90, 255},
                                {40, 255, 170, 255},  {30, 200, 200, 255},  {40, 240, 255, 255},  {60, 170, 255, 255},
                                {60, 110, 255, 255},  {70, 60, 255, 255},   {130, 60, 255, 255},  {180, 70, 255, 255},
                                {230, 70, 255, 255},  {255, 60, 220, 255},  {255, 60, 170, 255},  {255, 110, 150, 255},
                                {200, 200, 220, 255}, {255, 240, 200, 255}, {160, 120, 255, 255}, {255, 170, 120, 255}};
  return k[((hue % 24) + 24) % 24];
}

float sr_color_distance(SrColor a, SrColor b) {
  float dr = (float)a.r - b.r, dg = (float)a.g - b.g, db = (float)a.b - b.b;
  /* weighted Euclidean ("redmean") approximation of perceptual distance */
  float rm = ((float)a.r + b.r) / 2.0f;
  return sqrtf((2.0f + rm / 256.0f) * dr * dr + 4.0f * dg * dg + (2.0f + (255.0f - rm) / 256.0f) * db * db);
}

/* ---- lights ----------------------------------------------------------------------------------- */

int sr_select_lights(const SrLight *lights, int n, SrVec3 at, int *out) {
  float score[SR_MAX_LIGHTS_PER_DRAW];
  int k = 0;
  for (int i = 0; i < n; i++) {
    SrVec3 d = sr_v3_sub(lights[i].pos, at);
    float dist2 = sr_v3_dot(d, d);
    float r2 = lights[i].radius * lights[i].radius;
    if (dist2 > r2 * 4.0f) continue;
    float s = (lights[i].r + lights[i].g + lights[i].b) * r2 / (dist2 + 1.0f);
    int pos = k;
    while (pos > 0 && score[pos - 1] < s) pos--;
    if (pos >= SR_MAX_LIGHTS_PER_DRAW) continue;
    int last = k < SR_MAX_LIGHTS_PER_DRAW ? k : SR_MAX_LIGHTS_PER_DRAW - 1;
    for (int j = last; j > pos; j--) {
      score[j] = score[j - 1];
      out[j] = out[j - 1];
    }
    score[pos] = s;
    out[pos] = i;
    if (k < SR_MAX_LIGHTS_PER_DRAW) k++;
  }
  return k;
}

static SrColor neon_for(const SrCity *c, int building) {
  const SrBuilding *b = &c->buildings[building];
  const SrPalette *pal = sr_palette(c->districts[c->blocks[b->block].district].type);
  return pal->neon[b->id % 3];
}

static int block_lights(const SrCity *c, int blk, SrLight *out, int max) {
  int n = 0;
  const SrBlock *b = &c->blocks[blk];
  if (b->water) return 0;
  for (int i = 0; i < b->building_count && n < max; i++) { /* neon signs of places */
    int bi = b->first_building + i;
    const SrBuilding *bd = &c->buildings[bi];
    if (bd->poi == SR_POI_NONE) continue;
    SrColor nc = neon_for(c, bi);
    SrLight *l = &out[n++];
    l->pos = sr_v3((float)(bd->x0 + bd->x1) / 200.0f, 4.5f, (float)bd->y1 / 100.0f + 0.5f);
    l->radius = 14.0f;
    l->r = nc.r / 255.0f;
    l->g = nc.g / 255.0f;
    l->b = nc.b / 255.0f;
  }
  /* street lamps, alley lamps, fires, vending machines, booths and stalls */
  return n + sr_block_prop_lights(c, blk, out + n, max - n);
}

void sr_light_grid_build(SrLightGrid *lg, const SrCity *c) {
  sr_light_grid_free(lg);
  int blocks = c->grid * c->grid;
  lg->lights = (SrLight *)malloc(sizeof(SrLight) * (size_t)blocks * SR_BLOCK_LIGHTS);
  if (!lg->lights) return; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  lg->blocks = blocks;
  lg->city_seed = c->seed;
  for (int b = 0; b < blocks; b++) {
    lg->start[b] = (uint16_t)lg->count;
    lg->count += block_lights(c, b, lg->lights + lg->count, SR_BLOCK_LIGHTS);
  }
  lg->start[blocks] = (uint16_t)lg->count;
}

void sr_light_grid_free(SrLightGrid *lg) {
  free(lg->lights);
  memset(lg, 0, sizeof *lg);
}

int sr_light_grid_query(const SrLightGrid *lg, const SrCity *c, SrVec3 cam, float radius, SrLight *out, int max) {
  int n = 0;
  int bx = (int)(cam.x * 100.0f) / SR_BLOCK_CM, by = (int)(cam.z * 100.0f) / SR_BLOCK_CM;
  int rad = (int)(radius * 100.0f) / SR_BLOCK_CM + 1;
  for (int ring = 0; ring <= rad && n < max; ring++) /* nearest blocks first */
    for (int dy = -ring; dy <= ring && n < max; dy++)
      for (int dx = -ring; dx <= ring && n < max; dx++) {
        if (SR_MAX(abs(dx), abs(dy)) != ring) continue;
        int blk = sr_city_block_index(c, bx + dx, by + dy);
        if (blk < 0) continue;
        for (int i = lg->start[blk]; i < lg->start[blk + 1] && n < max; i++) out[n++] = lg->lights[i];
      }
  return n;
}

int sr_collect_lights(const SrCity *c, SrVec3 cam, float radius, SrLight *out, int max) {
  int n = 0;
  int bx = (int)(cam.x * 100.0f) / SR_BLOCK_CM, by = (int)(cam.z * 100.0f) / SR_BLOCK_CM;
  int rad = (int)(radius * 100.0f) / SR_BLOCK_CM + 1;
  for (int dy = -rad; dy <= rad && n < max; dy++)
    for (int dx = -rad; dx <= rad && n < max; dx++) {
      int blk = sr_city_block_index(c, bx + dx, by + dy);
      if (blk >= 0) n += block_lights(c, blk, out + n, max - n);
    }
  return n;
}

/* ---- chunks ----------------------------------------------------------------------------------- */

int sr_chunk_count(const SrCity *c) { return (c->grid + SR_CHUNK_BLOCKS - 1) / SR_CHUNK_BLOCKS; }

static SrColor shade(SrColor c, int pct) {
  return (SrColor){(uint8_t)(c.r * pct / 100), (uint8_t)(c.g * pct / 100), (uint8_t)(c.b * pct / 100), c.a};
}

/* A ground quad over [xa, xb] x [za, zb] (v0 at zb), split into cells of `step` metres when fine,
 * so vertex lighting can show pools of light near the camera. */
static void ground(SrMesh *m, float xa, float za, float xb, float zb, float y, float u0, float v0, float u1, float v1,
                   SrColor col, int layer, float step) {
  int nx = step > 0 ? SR_MAX(1, (int)((xb - xa) / step)) : 1, nz = step > 0 ? SR_MAX(1, (int)((zb - za) / step)) : 1;
  for (int j = 0; j < nz; j++)
    for (int i = 0; i < nx; i++) {
      float x0 = xa + (xb - xa) * (float)i / (float)nx, x1 = xa + (xb - xa) * (float)(i + 1) / (float)nx;
      float z1 = zb - (zb - za) * (float)j / (float)nz, z0 = zb - (zb - za) * (float)(j + 1) / (float)nz;
      float ua = u0 + (u1 - u0) * (float)i / (float)nx, ub = u0 + (u1 - u0) * (float)(i + 1) / (float)nx;
      float va = v0 + (v1 - v0) * (float)j / (float)nz, vb = v0 + (v1 - v0) * (float)(j + 1) / (float)nz;
      sr_mesh_quad(m, sr_v3(x0, y, z1), sr_v3(x1, y, z1), sr_v3(x1, y, z0), sr_v3(x0, y, z0), ua, va, ub, vb, col,
                   layer, 0, 1);
    }
}

static void build_block_detail(const SrCity *c, const SrDeltas *d, int blk, SrMesh *m, bool fine);

static void build_block(const SrCity *c, const SrDeltas *d, int blk, SrMesh *m) {
  build_block_detail(c, d, blk, m, false);
}

static void build_block_detail(const SrCity *c, const SrDeltas *d, int blk, SrMesh *m, bool fine) {
  const SrBlock *b = &c->blocks[blk];
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f, s = (float)SR_BLOCK_CM / 100.0f;
  float st = (float)SR_STREET_CM / 100.0f;
  SrColor white = {255, 255, 255, 255};
  if (b->water) {
    sr_mesh_quad(m, sr_v3(x0, -0.5f, z0 + s), sr_v3(x0 + s, -0.5f, z0 + s), sr_v3(x0 + s, -0.5f, z0),
                 sr_v3(x0, -0.5f, z0), 0, 0, 2, 2, white, SR_LAYER_WATER, 0, 1);
    return;
  }
  const SrPalette *pal = sr_palette(c->districts[b->district].type);
  int gang = -1;
  for (int f = c->factions.gang_first; f < c->factions.gang_first + c->factions.gang_count && gang < 0; f++)
    if (c->factions.f[f].turf & (1u << b->district)) gang = f;
  /* streets (west and north strips of the block) and the sidewalk-lined lot */
  float cell = fine ? 4.0f : 0.0f;
  ground(m, x0, z0, x0 + st, z0 + s, 0.0f, 0, 0, 1, s / st, white, SR_LAYER_ASPHALT, cell);
  ground(m, x0 + st, z0, x0 + s, z0 + st, 0.0f, 0, 0, (s - st) / st, 1, white, SR_LAYER_ASPHALT, cell);
  ground(m, x0 + st, z0 + st, x0 + s, z0 + s, 0.12f, 0, 0, (s - st) / 4, (s - st) / 4, pal->ground, SR_LAYER_SIDEWALK,
         cell);
  if (b->highway) sr_build_highway(c, blk, m); /* elevated highway deck overhead */
  for (int i = 0; i < b->building_count; i++) {
    int bi = b->first_building + i;
    const SrBuilding *bd = &c->buildings[bi];
    float bx0 = (float)bd->x0 / 100.0f, bx1 = (float)bd->x1 / 100.0f, bz1 = (float)bd->y1 / 100.0f;
    sr_build_building(c, bi, neon_for(c, bi), m);
    if (bd->poi != SR_POI_NONE) sr_build_door(m, (bx0 + bx1) / 2, bz1, neon_for(c, bi));
    if (bd->poi != SR_POI_NONE) {
      /* neon sign with the place's icon over the door (front = +z side) */
      SrColor neon = neon_for(c, bi);
      float cx = (bx0 + bx1) / 2, w = SR_MIN(3.0f, (bx1 - bx0) * 0.4f);
      int open = sr_deltas_get(d, bd->id) != 0; /* deltas can switch signs off */
      int em = open ? 255 : 40;
      int sign_first = m->nv;
      sr_mesh_quad(m, sr_v3(cx - w, 3.2f, bz1 + 0.15f), sr_v3(cx + w, 3.2f, bz1 + 0.15f),
                   sr_v3(cx + w, 5.4f, bz1 + 0.15f), sr_v3(cx - w, 5.4f, bz1 + 0.15f), 0, 1, 1, 0, neon, SR_LAYER_SIGN,
                   em, 0);
      SrAtlasCell ic = sr_icon_cell(bd->poi);
      sr_mesh_quad(m, sr_v3(cx - 1.0f, 3.3f, bz1 + 0.2f), sr_v3(cx + 1.0f, 3.3f, bz1 + 0.2f),
                   sr_v3(cx + 1.0f, 5.3f, bz1 + 0.2f), sr_v3(cx - 1.0f, 5.3f, bz1 + 0.2f), ic.u0, ic.v1, ic.u1, ic.v0,
                   neon, ic.layer, em, 0);
      sr_build_shop_name(m, bd, neon, open != 0);
      uint8_t sign_pad = sr_sign_pad(c->seed, bd->id);
      for (int v = sign_first; v < m->nv; v++) m->v[v].pad = sign_pad; /* about 8 % of signs are broken */
      /* awning: shelter from the rain */
      sr_mesh_quad(m, sr_v3(cx - w - 1, 3.0f, bz1 + 2.0f), sr_v3(cx + w + 1, 3.0f, bz1 + 2.0f),
                   sr_v3(cx + w + 1, 3.0f, bz1), sr_v3(cx - w - 1, 3.0f, bz1), 0, 0, 1, 1, shade(neon, 40),
                   SR_LAYER_WHITE, 0, 0);
    }
    sr_build_blade_signs(c, bi, m);
    /* gang tags on the walls of their turf: each gang's own mark in its colour */
    if (gang >= 0 && bd->poi != SR_POI_CORP_HQ && bd->poi != SR_POI_CORP_FACILITY && bd->id % 3 == 0) {
      SrColor tag = sr_corp_color(c->factions.f[gang].hue);
      SrAtlasCell tc = sr_atlas_cell(SR_LAYER_DECALS, SR_DECAL_TAG + (gang - c->factions.gang_first) % 6);
      float gx = bx0 + (bx1 - bx0) * (0.15f + 0.1f * (float)(bd->id % 4)), gw = 1.6f + 0.3f * (float)(bd->id % 3);
      sr_mesh_quad(m, sr_v3(gx, 0.6f, bz1 + 0.05f), sr_v3(gx + gw, 0.6f, bz1 + 0.05f),
                   sr_v3(gx + gw, 2.0f, bz1 + 0.05f), sr_v3(gx, 2.0f, bz1 + 0.05f), tc.u0, tc.v1, tc.u1, tc.v0, tag,
                   tc.layer, 70, 0);
    }
  }
}

void sr_chunk_build(const SrCity *c, const SrDeltas *d, int cx, int cy, SrMesh *out) {
  sr_mesh_clear(out);
  for (int by = cy * SR_CHUNK_BLOCKS; by < (cy + 1) * SR_CHUNK_BLOCKS; by++)
    for (int bx = cx * SR_CHUNK_BLOCKS; bx < (cx + 1) * SR_CHUNK_BLOCKS; bx++) {
      int blk = sr_city_block_index(c, bx, by);
      if (blk >= 0) build_block(c, d, blk, out);
    }
}

void sr_chunk_cache_init(SrChunkCache *cc, int capacity) {
  memset(cc, 0, sizeof *cc);
  cc->capacity = SR_CLAMP(capacity, 1, SR_CHUNK_CACHE);
}

void sr_chunk_cache_free(SrChunkCache *cc) {
  for (int i = 0; i < SR_CHUNK_CACHE; i++) sr_mesh_free(&cc->chunks[i].mesh);
}

SrChunk *sr_chunk_get(SrChunkCache *cc, const SrCity *c, const SrDeltas *d, int cx, int cy, int budget) {
  cc->tick++;
  int n = sr_chunk_count(c);
  if (cx < 0 || cy < 0 || cx >= n || cy >= n) return NULL;
  int used = 0, lru = 0;
  for (int i = 0; i < cc->capacity; i++) {
    SrChunk *ch = &cc->chunks[i];
    if (ch->valid) {
      used++;
      if (ch->cx == cx && ch->cy == cy) {
        ch->last_used = cc->tick;
        return ch;
      }
      if (cc->chunks[lru].valid && ch->last_used < cc->chunks[lru].last_used) lru = i;
    } else {
      lru = i; /* prefer empty slots */
    }
  }
  if (cc->built_this_frame >= budget) return NULL;
  SrChunk *slot = &cc->chunks[lru];
  if (slot->valid) cc->evictions++;
  (void)used;
  slot->cx = cx;
  slot->cy = cy;
  sr_chunk_build(c, d, cx, cy, &slot->mesh);
  slot->digest = sr_mesh_digest(&slot->mesh);
  slot->valid = true;
  slot->last_used = cc->tick;
  cc->built_this_frame++;
  return slot;
}

/* ---- characters ------------------------------------------------------------------------------- */

SrColor sr_outfit_color(const SrAppearance *a) {
  static const SrColor base[16] = {
      {30, 30, 34, 255},   {200, 200, 205, 255}, {150, 30, 40, 255},  {40, 60, 140, 255},
      {40, 110, 60, 255},  {200, 160, 40, 255},  {120, 40, 140, 255}, {40, 140, 150, 255},
      {170, 90, 40, 255},  {90, 90, 100, 255},   {220, 60, 150, 255}, {60, 200, 220, 255},
      {130, 200, 60, 255}, {240, 120, 40, 255},  {100, 60, 40, 255},  {170, 170, 120, 255}};
  return base[a->outfit_color % 16];
}

float sr_walk_swing(float walk) { return sinf(walk * 2.4f) * 0.55f; }

void sr_build_character_walk(const SrAppearance *a, SrVec3 p, SrVec3 dir, float walk, bool umbrella, SrMesh *m) {
  SrAnimInput in;
  memset(&in, 0, sizeof in);
  in.key = sr_appearance_key(a);
  in.look = a;
  in.role = (uint8_t)(a->outfit / 4);
  in.pos = p;
  in.dir = dir;
  in.speed = walk >= 0 ? 1.4f : 0.0f;
  in.walk = walk >= 0 ? walk : 0.0f;
  in.base = SR_CLIP_IDLE;
  in.prop_r = (uint8_t)(umbrella ? SR_HP_UMBRELLA : sr_anim_accessory(a));
  in.upper = umbrella ? SR_CLIP_UMBRELLA : SR_CLIP_NONE;
  in.expr = SR_EXPR_AUTO;
  sr_anim_character(NULL, &in, m);
}

void sr_build_character(const SrAppearance *a, SrVec3 p, SrVec3 dir, bool umbrella, SrMesh *m) {
  sr_build_character_walk(a, p, dir, -1.0f, umbrella, m);
}

static void body_part(SrMesh *m, SrVec3 lo, SrVec3 hi, int part) {
  int v0 = m->nv;
  sr_mesh_box(m, lo, hi, (SrColor){255, 255, 255, 255}, SR_LAYER_WHITE, 0, 0);
  for (int v = v0; v < m->nv; v++) {
    m->v[v].pad = (uint8_t)part;
    m->v[v].layer = SR_LAYER_WHITE;
  }
}

void sr_build_instance_body(SrMesh *m) {
  sr_mesh_clear(m);
  /* unit body (height 1): thighs, torso, upper arms, head, shins, forearms */
  body_part(m, sr_v3(-0.22f, 0.24f, -0.12f), sr_v3(-0.02f, 0.48f, 0.12f), 0);
  body_part(m, sr_v3(0.02f, 0.24f, -0.12f), sr_v3(0.22f, 0.48f, 0.12f), 1);
  body_part(m, sr_v3(-0.26f, 0.44f, -0.15f), sr_v3(0.26f, 0.82f, 0.15f), 2);
  body_part(m, sr_v3(-0.37f, 0.64f, -0.07f), sr_v3(-0.27f, 0.8f, 0.07f), 3);
  body_part(m, sr_v3(0.27f, 0.64f, -0.07f), sr_v3(0.37f, 0.8f, 0.07f), 4);
  body_part(m, sr_v3(-0.13f, 0.84f, -0.13f), sr_v3(0.13f, 1.0f, 0.13f), 5);
  body_part(m, sr_v3(-0.2f, 0.0f, -0.1f), sr_v3(-0.04f, 0.24f, 0.12f), 6);
  body_part(m, sr_v3(0.04f, 0.0f, -0.1f), sr_v3(0.2f, 0.24f, 0.12f), 7);
  body_part(m, sr_v3(-0.36f, 0.48f, -0.06f), sr_v3(-0.28f, 0.64f, 0.06f), 8);
  body_part(m, sr_v3(0.28f, 0.48f, -0.06f), sr_v3(0.36f, 0.64f, 0.06f), 9);
}

static int act_clip(int act) {
  switch (act) {
    case SR_SA_SIT: return SR_CLIP_SIT;
    case SR_SA_SLEEP: return SR_CLIP_SLEEP;
    case SR_SA_EAT: return SR_CLIP_EAT;
    case SR_SA_BUY: return SR_CLIP_BUY;
    case SR_SA_LEAN: return SR_CLIP_LEAN;
    case SR_SA_SMOKE: return SR_CLIP_SMOKE;
    case SR_SA_PHONE: return SR_CLIP_PHONE;
    case SR_SA_CALL: return SR_CLIP_CALL;
    case SR_SA_WINDOW: return SR_CLIP_WINDOW;
    case SR_SA_WAIT:
    case SR_SA_QUEUE: return SR_CLIP_WAIT;
    case SR_SA_DANCE: return SR_CLIP_DANCE;
    case SR_SA_WATCH: return SR_CLIP_WATCH;
    case SR_SA_POINT: return SR_CLIP_POINT;
    case SR_SA_WARM: return SR_CLIP_WARM;
    case SR_SA_BEG: return SR_CLIP_BEG;
    case SR_SA_TALK: return SR_CLIP_TALK;
    case SR_SA_FILM: return SR_CLIP_FILM;
    case SR_SA_COWER: return SR_CLIP_COWER;
    default: return SR_CLIP_IDLE;
  }
}

int sr_group_speaker(uint64_t group, int n, double now) {
  if (n <= 0) return -1;
  return (int)(((uint64_t)(now / 3.5) + sr_mix64(group) % 7u) % (uint64_t)n);
}

void sr_agent_anim(const SrAgent *a, const SrCrowdCtx *x, SrAnimInput *in) {
  memset(in, 0, sizeof *in);
  in->key = a->key;
  in->look = &a->look;
  in->role = a->role;
  in->pos = a->pos;
  in->dir = a->dir;
  in->speed = a->vel;
  in->walk = a->walk;
  in->wind_x = x->wind.x;
  in->wind_z = x->wind.z;
  in->once_t0 = -1;
  in->expr = SR_EXPR_AUTO;
  bool standing = a->vel <= 0.3f;
  int act = a->activity;
  in->base = (uint8_t)(standing ? act_clip(act) : SR_CLIP_WALK);
  if (act == SR_SA_TALK && a->group && sr_group_speaker(a->group, a->group_n, x->now) != a->group_i)
    in->base = SR_CLIP_LISTEN;
  int acc = sr_anim_accessory(&a->look);
  in->prop_r = (uint8_t)acc;
  if (standing) {
    if (act == SR_SA_SMOKE) in->prop_r = SR_HP_CIGARETTE;
    if (act == SR_SA_PHONE || act == SR_SA_CALL || act == SR_SA_FILM) in->prop_r = SR_HP_PHONE;
    if (act == SR_SA_EAT) {
      in->prop_r = SR_HP_NONE;
      in->prop_l = SR_HP_BOX;
    }
    if (act == SR_SA_SIT && a->key % 3u == 0) { /* on a bench with a phone */
      in->upper = SR_CLIP_NONE;
      in->prop_r = SR_HP_PHONE;
    }
  }
  bool sitting = in->base == SR_CLIP_SIT || in->base == SR_CLIP_SLEEP || in->base == SR_CLIP_BEG;
  /* umbrellas: open and held up in the rain, closed and carried otherwise */
  if (a->umbrella && !sitting) {
    if (x->rain > 0.05f) {
      if (in->prop_r) in->prop_l = in->prop_r == SR_HP_BOX ? SR_HP_BOX : in->prop_r;
      in->prop_r = SR_HP_UMBRELLA;
      in->upper = SR_CLIP_UMBRELLA;
    } else if (!in->prop_r) {
      in->prop_r = SR_HP_UMBRELLA_CLOSED;
    }
  }
  /* rain without an umbrella: hunched, hood up, or a jacket or bag held over the head */
  if (!a->umbrella && x->rain >= 0.3f && !standing && a->state == SR_AG_WALK) {
    in->hunch = SR_MIN(1.0f, x->rain * 1.5f);
    switch (a->key % 4u) {
      case 0: in->hood = true; break;
      case 1:
        in->upper = SR_CLIP_COVER_HEAD;
        in->prop_r = in->prop_l = SR_HP_NONE;
        break;
      default: break;
    }
  }
  if (a->state == SR_AG_SHELTER && a->covered && a->state_time < 1.3f) {
    in->once = SR_CLIP_SHAKE;
    in->once_t0 = x->now - a->state_time;
  }
  if (a->state == SR_AG_FLEE) {
    in->upper = SR_CLIP_FLEE_ARMS;
    in->once = SR_CLIP_RECOIL;
    in->once_t0 = x->now - a->state_time;
    in->hunch = 0.4f;
  }
  if (a->role ==
      SR_ROLE_COP) { /* officers walk a beat, ready their weapons at high Heat and aim at an armed response */
    in->prop_l = in->prop_r = SR_HP_NONE;
    in->upper = SR_CLIP_BEAT;
    float dx = x->player.x - a->pos.x, dz = x->player.z - a->pos.z;
    bool close = dx * dx + dz * dz < 30.0f * 30.0f;
    if (x->heat >= 3 && close) {
      in->upper = x->heat >= 4 ? SR_CLIP_AIM_PISTOL : SR_CLIP_READY;
      in->prop_r = SR_HP_PISTOL;
      in->look_at = true;
      in->target = x->player;
      if (x->heat >= 4 && standing) in->dir = sr_v3(dx, 0, dz);
    } else if (standing && act == SR_SA_WAIT) {
      in->upper = a->key % 2u ? SR_CLIP_WAVE : SR_CLIP_STOP;
    }
  }
  /* a glance at the player passing close by */
  if (standing && !in->look_at && in->base != SR_CLIP_SLEEP && a->key % 2u == 0) {
    float dx = x->player.x - a->pos.x, dz = x->player.z - a->pos.z;
    if (dx * dx + dz * dz < 16.0f) {
      in->look_at = true;
      in->target = x->player;
    }
  }
}

/* instanced clip codes (mirrored by the vertex shader) */
enum { INST_STAND, INST_WALK, INST_SIT, INST_CROUCH, INST_LYING };

static int inst_code(const SrAnimInput *in) {
  if (in->speed > 0.3f) return INST_WALK;
  switch (in->base) {
    case SR_CLIP_SIT:
    case SR_CLIP_BEG: return INST_SIT;
    case SR_CLIP_COWER:
    case SR_CLIP_CROUCH: return INST_CROUCH;
    case SR_CLIP_SLEEP: return INST_LYING;
    default: return INST_STAND;
  }
}

/* A splash at each footfall on wet ground near the camera. */
static void foot_splash(const SrAnimSlot *before, float phase0, const SrAnimInput *in, const SrCrowdCtx *x,
                        SrMesh *fx) {
  if (!fx || !before || x->wet < 0.3f || in->speed < 0.3f) return;
  const SrAnimSlot *sl = before;
  float p1 = sl->phase, p0 = phase0;
  for (int side = 0; side < 2; side++) {
    float plant = side ? 0.5f : 0.0f;
    float a0 = p0 - plant, a1 = p1 - plant;
    a0 -= floorf(a0);
    a1 -= floorf(a1);
    if (!(a1 < a0 && a0 - a1 > 0.5f)) continue; /* wrapped past the plant point */
    if (x->splashes >= x->max_splashes) return;
    SrVec3 side_v = sr_v3(-in->dir.z, 0, in->dir.x);
    SrVec3 at = sr_v3_add(in->pos, sr_v3_scale(side_v, side ? 0.12f : -0.12f));
    at = sr_v3_add(at, sr_v3_scale(in->dir, 0.25f));
    SrBillboard bb = sr_billboard(x->eye, 0, 0);
    sr_mesh_sprite(fx, &bb, sr_v3(at.x, 0.02f, at.z), 0.35f, 0.25f, SR_SPR_SPLASH + (int)(in->key % 4u),
                   SR_SPRITE_UPRIGHT, 0, (SrColor){46, 52, 64, 255}, 255);
    ((SrCrowdCtx *)x)->splashes++;
  }
}

static void draw_agent(const SrAgent *a, SrAnimCache *anim, const SrCrowdCtx *x, SrMesh *near, SrMesh *fx) {
  SrAnimInput in;
  sr_agent_anim(a, x, &in);
  const SrAnimSlot *sl = anim ? sr_anim_cache_find(anim, a->key) : NULL;
  float p0 = sl ? sl->phase : 0.0f;
  sr_anim_character(anim, &in, near);
  float dx = a->pos.x - x->eye.x, dz = a->pos.z - x->eye.z;
  if (sl && dx * dx + dz * dz < 15.0f * 15.0f) foot_splash(sl, p0, &in, x, fx);
}

int sr_build_crowd_lod(const SrCrowd *cr, SrAnimCache *anim, const SrCrowdCtx *x, SrVec3 cam, float near_m, float far_m,
                       SrMesh *near, float *inst, int max) {
  sr_mesh_clear(near);
  int n = 0, near_n = 0;
  for (int i = 0; i < cr->count; i++) {
    const SrAgent *a = &cr->agents[i];
    float dx = a->pos.x - cam.x, dz = a->pos.z - cam.z, d2 = dx * dx + dz * dz;
    if (d2 > far_m * far_m) continue;
    if ((d2 <= near_m * near_m && near_n < x->max_near) || n >= max) {
      draw_agent(a, anim, x, near, x->fx);
      near_n++;
      continue;
    }
    SrAnimInput in;
    sr_agent_anim(a, x, &in);
    float *f = inst + n * SR_INST_FLOATS;
    SrColor o = sr_anim_cloth(&a->look), sk = sr_anim_skin(&a->look);
    int code = inst_code(&in);
    SrSkel skel;
    sr_anim_skel(&a->look, &skel);
    float phase = code == INST_WALK ? a->walk / sr_anim_stride(&skel, a->vel, SR_CLIP_WALK) : (float)(x->now / 4.0);
    f[0] = a->pos.x;
    f[1] = a->pos.y;
    f[2] = a->pos.z;
    f[3] = (fabsf(in.dir.x) + fabsf(in.dir.z)) > 1e-4f ? atan2f(in.dir.x, in.dir.z) : 0.0f;
    f[4] = phase - floorf(phase);
    f[5] = skel.bw / 0.24f;
    f[6] = skel.tall;
    f[7] = (float)code;
    f[8] = o.r / 255.0f;
    f[9] = o.g / 255.0f;
    f[10] = o.b / 255.0f;
    f[11] = a->vel;
    f[12] = sk.r / 255.0f;
    f[13] = sk.g / 255.0f;
    f[14] = sk.b / 255.0f;
    f[15] = in.hunch + (in.prop_r == SR_HP_PHONE || in.prop_r == SR_HP_CIGARETTE ? 2.0f : 0.0f);
    n++;
  }
  return n;
}

static void car_box(SrMesh *m, SrVec3 o, float yaw, float pitch, float roll, SrVec3 lo, SrVec3 hi, SrColor col,
                    int layer, int em) {
  /* a box in the car's frame: pitched and rolled about the centre of the wheelbase */
  float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), cr = cosf(roll), sr = sinf(roll);
  SrVec3 c[8];
  for (int i = 0; i < 8; i++) {
    float x0 = (i & 1) ? hi.x : lo.x, y0 = (i & 2) ? hi.y : lo.y, z0 = (i & 4) ? hi.z : lo.z;
    float y1 = y0 - 0.6f;
    float x1 = x0 * cr - y1 * sr, y2 = x0 * sr + y1 * cr;
    float y3 = y2 * cp - z0 * sp, z1 = y2 * sp + z0 * cp;
    c[i] = sr_v3(o.x + x1 * cy + z1 * sy, o.y + y3 + 0.6f, o.z - x1 * sy + z1 * cy);
  }
  sr_mesh_quad(m, c[4], c[5], c[7], c[6], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[1], c[0], c[2], c[3], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[5], c[1], c[3], c[7], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[0], c[4], c[6], c[2], 0, 1, 1, 0, col, layer, em, 0);
  SrColor top = shade(col, 75);
  sr_mesh_quad(m, c[6], c[7], c[3], c[2], 0, 1, 1, 0, top, layer, em, 0);
  sr_mesh_quad(m, c[0], c[1], c[5], c[4], 0, 1, 1, 0, top, layer, em, 0);
}

static void aerodyne(SrMesh *out, SrMesh *fx, const SrBillboard *bb, const SrAerodyne *v, double t, bool blink) {
  SrVec3 p = sr_v3(v->pos.x, v->pos.y - 0.6f, v->pos.z); /* car_box pitches about 0.6 m up */
  float yaw = atan2f(v->dir.x, v->dir.z);
  SrColor body = shade(sr_corp_color(v->color), 50);
  car_box(out, p, yaw, v->pitch, v->roll, sr_v3(-1.3f, 0, -2.6f), sr_v3(1.3f, 0.9f, 2.6f), body, SR_LAYER_METAL, 0);
  car_box(out, p, yaw, v->pitch, v->roll, sr_v3(-0.9f, 0.9f, -1.0f), sr_v3(0.9f, 1.5f, 1.2f),
          (SrColor){40, 60, 80, 255}, SR_LAYER_WIN_GLASS, 40);
  car_box(out, p, yaw, v->pitch, v->roll, sr_v3(-1.1f, 0.3f, 2.6f), sr_v3(1.1f, 0.6f, 2.7f),
          (SrColor){255, 250, 220, 255}, SR_LAYER_WHITE, 255);
  car_box(out, p, yaw, v->pitch, v->roll, sr_v3(-1.1f, 0.3f, -2.7f), sr_v3(1.1f, 0.6f, -2.6f),
          (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE, 255);
  if (blink || v->mode == SR_AIR_LANDED) /* navigation beacon underneath, steady while on a pad */
    car_box(out, p, yaw, v->pitch, v->roll, sr_v3(-0.2f, -0.25f, -0.2f), sr_v3(0.2f, 0, 0.2f),
            (SrColor){40, 220, 255, 255}, SR_LAYER_WHITE, 255);
  if (!fx || v->mode == SR_AIR_LANDED) return;
  /* four thrusters glowing underneath, pulsing; brighter while hovering down or up */
  float pulse = 0.75f + 0.25f * sinf((float)t * 17.0f + (float)v->color);
  int b = (int)((v->mode == SR_AIR_CRUISE ? 110.0f : 200.0f) * pulse);
  float cy = cosf(yaw), sy = sinf(yaw);
  for (int k = 0; k < 4; k++) {
    float lx = k & 1 ? 1.0f : -1.0f, lz = k & 2 ? 1.8f : -1.8f;
    SrVec3 q = sr_v3(v->pos.x + lx * cy + lz * sy, v->pos.y - 0.7f, v->pos.z - lx * sy + lz * cy);
    sr_mesh_sprite(fx, bb, q, 1.1f, 1.1f, SR_SPR_GLOW, SR_SPRITE_FACING, 0,
                   (SrColor){(uint8_t)(b / 3), (uint8_t)(b * 2 / 3), (uint8_t)b, 255}, 255);
  }
}

void sr_build_aerodynes(const SrAirTraffic *air, const SrCity *c, double t, SrVec3 cam, float radius, SrMesh *out,
                        SrMesh *fx) {
  sr_mesh_clear(out);
  SrAerodyne v[SR_AIR_MAX_LANES * SR_AIR_PER_LANE + 16];
  int n = sr_air_near(air, t, cam, radius, v, SR_AIR_MAX_LANES * SR_AIR_PER_LANE);
  if (c) n += sr_air_landers(c, t, cam, 200.0f, v + n, 16);
  bool blink = fmod(t, 1.0) < 0.5;
  SrBillboard bb = sr_billboard(cam, 0, 0);
  for (int i = 0; i < n; i++) aerodyne(out, fx, &bb, &v[i], t, blink);
}

float sr_wheel_angle(const SrVehicle *v) { return v->odo / 0.34f; }

void sr_build_vehicles(const SrCrowd *cr, const SrCrowdCtx *x, SrVec3 cam, float max_dist, SrMesh *out) {
  double now = x ? x->now : 0.0;
  bool blink = fmod(now, 0.7) < 0.35;
  for (int i = 0; i < cr->vehicle_count; i++) {
    const SrVehicle *v = &cr->vehicles[i];
    float dx = v->pos.x - cam.x, dz = v->pos.z - cam.z;
    if (dx * dx + dz * dz > max_dist * max_dist) continue;
    float yaw = atan2f(v->dir.x, v->dir.z);
    /* the body pitches back when pulling away, dips when braking, rolls in a turn */
    float bob = 0.012f * sinf(v->odo * 1.7f + (float)(v->key % 13u));
    float pitch = sr_clampf(v->accel * 0.012f, -0.07f, 0.05f);
    float roll = v->turning ? (v->indicator > 0 ? -0.04f : 0.04f) * SR_MIN(1.0f, v->vel / 6.0f) : 0.0f;
    SrVec3 o = sr_v3(v->pos.x, bob, v->pos.z);
    SrColor body = shade(sr_corp_color(v->color), 45);
    car_box(out, o, yaw, pitch, roll, sr_v3(-0.9f, 0.35f, -2.2f), sr_v3(0.9f, 0.95f, 2.2f), body, SR_LAYER_METAL, 0);
    car_box(out, o, yaw, pitch, roll, sr_v3(-0.8f, 0.95f, -1.2f), sr_v3(0.8f, 1.4f, 0.9f), (SrColor){30, 40, 55, 255},
            SR_LAYER_WHITE, 0);
    /* wheels with a two-tone hub, turning with the distance driven */
    float wa = sr_wheel_angle(v);
    for (int w = 0; w < 4; w++) {
      float wx = w & 1 ? 0.92f : -0.92f, wz = w & 2 ? 1.45f : -1.45f;
      SrVec3 wc = sr_v3(v->pos.x + wx * cosf(yaw) + wz * sinf(yaw), 0.34f, v->pos.z - wx * sinf(yaw) + wz * cosf(yaw));
      sr_mesh_part(out, wc, yaw, sr_v3(0, 0, 0), wa, sr_v3(-0.12f, -0.34f, -0.34f), sr_v3(0.12f, 0.34f, 0.34f),
                   (SrColor){18, 18, 20, 255}, SR_LAYER_WHITE, 0);
      sr_mesh_part(out, wc, yaw, sr_v3(0, 0, 0), wa, sr_v3(w & 1 ? 0.12f : -0.13f, -0.06f, -0.2f),
                   sr_v3(w & 1 ? 0.13f : -0.12f, 0.06f, 0.2f), (SrColor){150, 150, 160, 255}, SR_LAYER_WHITE, 0);
    }
    car_box(out, o, yaw, pitch, roll, sr_v3(-0.75f, 0.6f, 2.2f), sr_v3(-0.35f, 0.82f, 2.25f),
            (SrColor){255, 250, 220, 255}, SR_LAYER_WHITE, 255);
    car_box(out, o, yaw, pitch, roll, sr_v3(0.35f, 0.6f, 2.2f), sr_v3(0.75f, 0.82f, 2.25f),
            (SrColor){255, 250, 220, 255}, SR_LAYER_WHITE, 255);
    /* brake lights brighten while slowing */
    bool braking = v->stopped || v->accel < -0.5f;
    SrColor brake = braking ? (SrColor){255, 30, 30, 255} : (SrColor){150, 20, 20, 255};
    car_box(out, o, yaw, pitch, roll, sr_v3(-0.8f, 0.65f, -2.25f), sr_v3(-0.4f, 0.85f, -2.2f), brake, SR_LAYER_WHITE,
            braking ? 255 : 160);
    car_box(out, o, yaw, pitch, roll, sr_v3(0.4f, 0.65f, -2.25f), sr_v3(0.8f, 0.85f, -2.2f), brake, SR_LAYER_WHITE,
            braking ? 255 : 160);
    if (v->indicator && blink) {                      /* amber indicator on the side of the turn, front and back */
      float side = v->indicator > 0 ? -0.86f : 0.86f; /* +x of the car frame is its left */
      SrColor amber = {255, 160, 20, 255};
      car_box(out, o, yaw, pitch, roll, sr_v3(side - 0.06f, 0.62f, 2.15f), sr_v3(side + 0.06f, 0.75f, 2.27f), amber,
              SR_LAYER_WHITE, 255);
      car_box(out, o, yaw, pitch, roll, sr_v3(side - 0.06f, 0.62f, -2.27f), sr_v3(side + 0.06f, 0.75f, -2.15f), amber,
              SR_LAYER_WHITE, 255);
    }
    if (x && x->rain > 0.2f) { /* a wiper sweeping the windscreen */
      float wsw = sinf((float)now * 4.0f + (float)(v->key % 7u)) * 0.6f;
      car_box(out, o, yaw, pitch, roll, sr_v3(-0.6f + wsw * 0.3f, 1.0f, 0.92f),
              sr_v3(-0.55f + wsw * 0.3f + 0.6f, 1.03f, 0.95f), (SrColor){10, 10, 10, 255}, SR_LAYER_WHITE, 0);
    }
    if (sr_vehicle_is_patrol(v)) /* a light bar on the patrol cars (it flashes at Heat 3) */
      car_box(out, o, yaw, pitch, roll, sr_v3(-0.5f, 1.4f, -0.3f), sr_v3(0.5f, 1.55f, 0.1f), (SrColor){60, 60, 90, 255},
              SR_LAYER_WHITE, 40);
  }
}

int sr_build_headlights(const SrCrowd *cr, SrVec3 eye, float wet, uint64_t steps, int max, SrMesh *fx) {
  /* beams cut through the wet air in front of every nearby car, and rear wheels throw spray */
  SrBillboard bb = sr_billboard(eye, 0, 0);
  int beams = 0;
  for (int i = 0; i < cr->vehicle_count; i++) {
    const SrVehicle *v = &cr->vehicles[i];
    float dx = v->pos.x - eye.x, dz = v->pos.z - eye.z;
    if (dx * dx + dz * dz > 60.0f * 60.0f) continue;
    for (int side = 0; side < 2 && beams < max; side++) {
      SrVec3 perp = sr_v3(-v->dir.z, 0, v->dir.x);
      SrVec3 lamp = sr_v3_add(sr_v3_add(v->pos, sr_v3_scale(v->dir, 2.3f)), sr_v3_scale(perp, side ? 0.55f : -0.55f));
      for (int k = 1; k <= 3; k++) { /* a fading cone of glows along the beam */
        SrVec3 p = sr_v3_add(lamp, sr_v3_scale(v->dir, 2.2f * (float)k));
        float sz = 0.8f + 0.9f * (float)k;
        int b = (int)(70.0f / (float)k * (0.5f + 0.5f * wet));
        sr_mesh_sprite(fx, &bb, sr_v3(p.x, 0.75f, p.z), sz, sz * 0.6f, SR_SPR_GLOW, SR_SPRITE_FACING, 0,
                       (SrColor){(uint8_t)b, (uint8_t)b, (uint8_t)(b * 9 / 10), 255}, 255);
      }
      beams++;
    }
    if (wet > 0.4f && v->vel > 6.0f) {
      for (int side = 0; side < 2; side++) {
        SrVec3 perp = sr_v3(-v->dir.z, 0, v->dir.x);
        SrVec3 p = sr_v3_add(sr_v3_sub(v->pos, sr_v3_scale(v->dir, 1.9f)), sr_v3_scale(perp, side ? 0.9f : -0.9f));
        int frame = (int)((steps / 4u + v->key + (uint64_t)side) % 4u);
        sr_mesh_sprite(fx, &bb, sr_v3(p.x, 0.02f, p.z), 0.8f, 0.5f, SR_SPR_SPLASH + frame, SR_SPRITE_UPRIGHT, 0,
                       (SrColor){40, 46, 58, 255}, 255);
      }
    }
  }
  return beams;
}

void sr_build_characters(const SrCrowd *cr, SrAnimCache *anim, const SrCrowdCtx *x, SrVec3 cam, float max_dist,
                         SrMesh *out) {
  sr_mesh_clear(out);
  for (int i = 0; i < cr->count; i++) {
    const SrAgent *a = &cr->agents[i];
    float dx = a->pos.x - cam.x, dz = a->pos.z - cam.z;
    if (dx * dx + dz * dz > max_dist * max_dist) continue;
    draw_agent(a, anim, x, out, NULL);
  }
  sr_build_vehicles(cr, x, cam, max_dist, out);
}

/* ---- rain, skyline, landmarks ----------------------------------------------------------------- */

bool sr_point_covered(const SrCity *c, SrVec3 p) {
  int blk = sr_city_block_at(c, (int32_t)(p.x * 100.0f), (int32_t)(p.z * 100.0f));
  if (blk < 0) return false;
  const SrBlock *b = &c->blocks[blk];
  if (b->highway && p.y < 11.0f) {
    int32_t ox, oy;
    sr_city_block_origin(blk, c->grid, &ox, &oy);
    float z0 = (float)oy / 100.0f;
    if (p.z >= z0 + 2 && p.z <= z0 + (float)SR_STREET_CM / 100.0f - 2) return true;
  }
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    float x0 = (float)bd->x0 / 100.0f, x1 = (float)bd->x1 / 100.0f, z1 = (float)bd->y1 / 100.0f;
    float h = (float)bd->height_cm / 100.0f;
    if (p.x >= x0 && p.x <= x1 && p.z >= (float)bd->y0 / 100.0f && p.z <= z1 && p.y < h) return true;
    if (bd->poi != SR_POI_NONE) { /* awning in front of places */
      float cx = (x0 + x1) / 2, w = SR_MIN(3.0f, (x1 - x0) * 0.4f) + 1.0f;
      if (p.x >= cx - w && p.x <= cx + w && p.z >= z1 && p.z <= z1 + 2.0f && p.y < 3.0f) return true;
    }
  }
  return false;
}

void sr_build_rain(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, SrMesh *out) {
  sr_build_rain_lit(cam, intensity, frame, c, NULL, 0, (SrColor){120, 135, 165, 160}, out);
}

void sr_build_rain_lit(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, const SrLight *l, int nl,
                       SrColor base, SrMesh *out) {
  sr_mesh_clear(out);
  int drops = (int)(1500.0f * sqrtf(intensity)); /* even a drizzle shows */
  float len = 0.5f + 0.7f * intensity;           /* longer streaks in heavy rain */
  SrRng r = sr_rng_stream(frame, "rain", 0, 0);
  int near = SR_MIN(nl, 8); /* the lights nearest the camera tint the drops */
  for (int i = 0; i < drops; i++) {
    float x = cam.x + (sr_rng_float(&r) - 0.5f) * 40.0f;
    float z = cam.z + (sr_rng_float(&r) - 0.5f) * 40.0f;
    float y = cam.y + (sr_rng_float(&r) - 0.3f) * 14.0f;
    SrVec3 p = sr_v3(x, y, z);
    if (sr_point_covered(c, p)) continue;
    float dx = x - cam.x, dz = z - cam.z, d = sqrtf(dx * dx + dz * dz);
    float rx = d > 0.01f ? dz / d * 0.015f : 0.015f, rz = d > 0.01f ? -dx / d * 0.015f : 0.0f; /* faces the eye */
    SrColor col = near ? sr_rain_tint(l, near, p, base) : base;
    sr_mesh_quad(out, sr_v3(x - rx, y, z - rz), sr_v3(x + rx, y, z + rz), sr_v3(x + rx + 0.03f, y + len, z + rz),
                 sr_v3(x - rx + 0.03f, y + len, z - rz), 0, 0, 1, 1, col, SR_LAYER_WHITE, 90, 0);
  }
}

/* The camera's chunk is drawn block by block from finely tessellated meshes. */
static SrMesh *scene_block(SrSceneCache *s, const SrGame *g, int block) {
  s->block_tick++;
  int oldest = 0;
  for (int i = 0; i < 24; i++) {
    if (s->blocks[i].used && s->blocks[i].block == block && s->blocks[i].turf == g->chess.turf_version) {
      s->blocks[i].used = s->block_tick;
      return &s->blocks[i].mesh;
    }
    if (s->blocks[i].used < s->blocks[oldest].used) oldest = i;
  }
  s->blocks[oldest].block = block;
  s->blocks[oldest].turf = g->chess.turf_version;
  s->blocks[oldest].used = s->block_tick;
  sr_mesh_clear(&s->blocks[oldest].mesh);
  build_block_detail(g->city, &g->deltas, block, &s->blocks[oldest].mesh, true);
  return &s->blocks[oldest].mesh;
}

SrMesh *sr_scene_street(SrSceneCache *s, const SrCity *c, int block) {
  s->street_tick++;
  int oldest = 0;
  for (int i = 0; i < 32; i++) {
    if (s->street[i].used && s->street[i].block == block && s->street[i].detail == s->street_detail) {
      s->street[i].used = s->street_tick;
      return &s->street[i].mesh;
    }
    if (s->street[i].used < s->street[oldest].used) oldest = i;
  }
  s->street[oldest].block = block;
  s->street[oldest].detail = s->street_detail;
  s->street[oldest].used = s->street_tick;
  sr_build_block_props(c, block, s->street_detail, &s->ads, &s->street[oldest].mesh);
  return &s->street[oldest].mesh;
}

const SrStreetProp *sr_scene_props(SrSceneCache *s, const SrCity *c, int block, int *n) {
  s->prop_tick++;
  int oldest = 0;
  for (int i = 0; i < 16; i++) {
    if (s->prop_cache[i].used && s->prop_cache[i].block == block) {
      s->prop_cache[i].used = s->prop_tick;
      *n = s->prop_cache[i].n;
      return s->prop_cache[i].props;
    }
    if (s->prop_cache[i].used < s->prop_cache[oldest].used) oldest = i;
  }
  s->prop_cache[oldest].block = block;
  s->prop_cache[oldest].used = s->prop_tick;
  s->prop_cache[oldest].n = sr_dressing_block(c, block, s->prop_cache[oldest].props, SR_MAX_BLOCK_PROPS);
  *n = s->prop_cache[oldest].n;
  return s->prop_cache[oldest].props;
}

void sr_build_skyline(const SrCity *c, SrMesh *out) {
  sr_mesh_clear(out);
  SrRng r = sr_rng_stream(c->seed, "skyline", 0, 0);
  float cx = (float)c->grid * SR_BLOCK_CM / 200.0f, cz = cx;
  float radius = cx * 1.6f + 600.0f;
  const int segments = 180;
  for (int i = 0; i < segments; i++) {
    float a0 = (float)i / segments * 2.0f * SR_PI, a1 = (float)(i + 1) / segments * 2.0f * SR_PI;
    float h = 40.0f + sr_rng_float(&r) * 220.0f;
    SrVec3 p0 = sr_v3(cx + cosf(a0) * radius, -5, cz + sinf(a0) * radius);
    SrVec3 p1 = sr_v3(cx + cosf(a1) * radius, -5, cz + sinf(a1) * radius);
    SrColor col = {18, 16, 26, 255};
    sr_mesh_quad(out, p1, p0, sr_v3(p0.x, h, p0.z), sr_v3(p1.x, h, p1.z), 0, 8, 2, 0, col, SR_LAYER_WIN_GLASS, 40, 0);
    if (h > 200.0f) { /* a red beacon on the tallest towers of the skyline */
      SrVec3 m = sr_v3((p0.x + p1.x) * 0.5f, h, (p0.z + p1.z) * 0.5f);
      int first = out->nv;
      sr_mesh_quad(out, sr_v3(m.x - 2.0f, h, m.z), sr_v3(m.x + 2.0f, h, m.z), sr_v3(m.x + 2.0f, h + 4.0f, m.z),
                   sr_v3(m.x - 2.0f, h + 4.0f, m.z), 0, 0, 1, 1, (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE, 255, 0);
      for (int v = first; v < out->nv; v++) out->v[v].pad = (uint8_t)(0x80 | (56 + i % 8)); /* blinking */
    }
  }
}

void sr_build_landmarks(const SrCity *c, float time_s, SrMesh *out) {
  sr_mesh_clear(out);
  for (int f = 0; f < c->factions.corp_count; f++) {
    int hq = c->factions.f[f].hq_building;
    if (hq < 0) continue;
    const SrBuilding *b = &c->buildings[hq];
    float x = (float)(b->x0 + b->x1) / 200.0f, z = (float)(b->y0 + b->y1) / 200.0f;
    float y = (float)b->height_cm / 100.0f + 14.0f;
    float ang = time_s * 0.4f + (float)f;
    float s = 10.0f, dx = cosf(ang) * s, dz = sinf(ang) * s;
    SrColor col = sr_corp_color(c->factions.f[f].hue);
    /* rotating double-sided holo-logo */
    SrAtlasCell ic = sr_icon_cell(SR_POI_CORP_HQ);
    sr_mesh_quad(out, sr_v3(x - dx, y - s, z - dz), sr_v3(x + dx, y - s, z + dz), sr_v3(x + dx, y + s, z + dz),
                 sr_v3(x - dx, y + s, z - dz), ic.u0, ic.v1, ic.u1, ic.v0, col, ic.layer, 255, 0);
    sr_mesh_quad(out, sr_v3(x + dx, y - s, z + dz), sr_v3(x - dx, y - s, z - dz), sr_v3(x - dx, y + s, z - dz),
                 sr_v3(x + dx, y + s, z + dz), ic.u0, ic.v1, ic.u1, ic.v0, col, ic.layer, 255, 0);
  }
}

/* ---- frame ------------------------------------------------------------------------------------ */

void sr_scene_init(SrSceneCache *s, bool mobile) {
  memset(s, 0, sizeof *s);
  sr_chunk_cache_init(&s->chunks, mobile ? 48 : SR_CHUNK_CACHE);
  s->view_chunks = mobile ? 2 : 3;
  s->street_detail = mobile ? 1 : 2;
}

void sr_scene_free(SrSceneCache *s) {
  sr_chunk_cache_free(&s->chunks);
  sr_mesh_free(&s->characters);
  sr_mesh_free(&s->rain);
  sr_mesh_free(&s->skyline);
  sr_mesh_free(&s->landmarks);
  for (int i = 0; i < SR_INT_MAX_ROOMS; i++) sr_mesh_free(&s->rooms[i]);
  sr_light_grid_free(&s->light_grid);
  sr_mesh_free(&s->site_dyn);
  sr_mesh_free(&s->guards);
  sr_mesh_free(&s->inst_body);
  sr_mesh_free(&s->air);
  sr_mesh_free(&s->grid);
  sr_mesh_free(&s->host);
  SrMesh *sky2[] = {&s->beams, &s->blimps, &s->blimp_glow, &s->distant, &s->skyline_glow, &s->vertical, &s->trains};
  for (size_t i = 0; i < sizeof sky2 / sizeof sky2[0]; i++) sr_mesh_free(sky2[i]);
  SrMesh *gloom[] = {&s->sky,         &s->deck, &s->deck_glow, &s->halos,  &s->near_glow,
                     &s->silhouettes, &s->sun,  &s->wet,       &s->fronts, &s->route_beacon};
  for (size_t i = 0; i < sizeof gloom / sizeof gloom[0]; i++) sr_mesh_free(gloom[i]);
  for (int i = 0; i < 32; i++) sr_mesh_free(&s->street[i].mesh);
  for (int i = 0; i < 24; i++) sr_mesh_free(&s->blocks[i].mesh);
  sr_mesh_free(&s->pools);
  sr_mesh_free(&s->crowd_fx);
  sr_mesh_free(&s->view_model);
  free(s->beacons);
  s->beacons = NULL;
}

static void add_draw(SrFrame *f, SrMesh *m, SrPass pass, SrVec3 at) {
  if (f->ndraws >= SR_MAX_DRAWS || !m || m->ni == 0) return;
  SrDraw *d = &f->draws[f->ndraws++];
  d->mesh = m;
  d->pass = pass;
  d->nlights =
      pass == SR_PASS_WORLD || pass == SR_PASS_VIEW ? sr_select_lights(f->lights, f->nlights, at, d->lights) : 0;
}

/* Matrix frame: black void, the grid or the current host, from the persona's eyes. */
static void build_matrix_frame(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f) {
  const SrDive *d = &g->dive;
  f->eye = d->pos;
  f->view = sr_m4_fps_view(d->pos, d->yaw, d->pitch);
  f->proj = sr_m4_perspective(80.0f * SR_PI / 180.0f, aspect > 0 ? aspect : 16.0f / 9.0f, 0.05f, 600.0f);
  f->fog[0] = 0.0f;
  f->fog[1] = 0.01f;
  f->fog[2] = 0.04f;
  f->sky[0] = 0.0f;
  f->sky[1] = 0.01f;
  f->sky[2] = 0.04f;
  f->ambient[0] = f->ambient[1] = f->ambient[2] = 1.0f;
  f->nlights = 0;
  f->wetness = 0;
  f->daylight = false;
  if (d->in_host) {
    f->fog_start = 18.0f;
    f->fog_end = 70.0f;
    sr_build_host(g, (float)g->steps / 60.0f, &s->host);
    add_draw(f, &s->host, SR_PASS_WORLD, d->pos);
  } else {
    f->fog_start = 25.0f;
    f->fog_end = 110.0f;
    if (s->grid_seed != g->city->seed + 1 || s->grid.nv == 0) {
      sr_build_grid(g->city, &s->grid);
      s->grid_seed = g->city->seed + 1;
    }
    add_draw(f, &s->grid, SR_PASS_WORLD, d->pos);
  }
}

/* The weapon sways after the look and settles over about 0.08 s. */
static void update_sway(SrSceneCache *s, const SrGame *g) {
  float tx = SR_CLAMP(g->fx.look_x * 0.6f, -0.05f, 0.05f), ty = SR_CLAMP(g->fx.look_y * 0.6f, -0.04f, 0.04f);
  float k = 1.0f / 60.0f / 0.08f;
  s->sway_x += (tx - s->sway_x) * k;
  s->sway_y += (ty - s->sway_y) * k;
}

/* A shot lights the surroundings for about two frames (before the draws pick their lights). */
static void muzzle_light(SrSceneCache *s, const SrGame *g, SrViewCam vc, SrFrame *f) {
  SrVec3 m;
  if (!sr_view_muzzle(g, vc, s->sway_x, s->sway_y, &m)) return;
  int i = f->nlights < SR_MAX_FRAME_LIGHTS ? f->nlights++ : SR_MAX_FRAME_LIGHTS - 1;
  for (int k = i; k > 0; k--) f->lights[k] = f->lights[k - 1]; /* first, so every draw nearby picks it */
  SrVec3 ahead = sr_v3_add(m, sr_v3_scale(sr_dir_from_yaw_pitch(vc.yaw, vc.pitch), 1.5f)); /* the room, not the hand */
  f->lights[0] = (SrLight){ahead, 8.0f, 1.1f, 0.8f, 0.4f};
}

/* The hands and weapon, drawn last over a cleared depth buffer so walls never cut them. */
static void scene_body(SrSceneCache *s, const SrGame *g, SrViewCam vc, SrFrame *f) {
  s->view_tris = sr_build_view_model(g, vc, s->sway_x, s->sway_y, &s->view_model);
  sr_build_muzzle_fx(g, vc, s->sway_x, s->sway_y, &s->crowd_fx);
  add_draw(f, &s->view_model, SR_PASS_VIEW, vc.eye);
}

/* Interior frame: room meshes (rebuilt when doors, devices or lights change), guards, effects. */
static void build_site_frame(SrSceneCache *s, const SrGame *g, SrFrame *f, SrViewCam vc) {
  const SrSite *site = &g->site;
  uint64_t key = sr_site_mesh_key(site, &g->deltas);
  int rooms = site->in.w * site->in.h;
  if (key != s->site_key) {
    for (int r = 0; r < rooms; r++) sr_build_interior_room(site, r, &s->rooms[r]);
    s->site_key = key;
  }
  float amb = site->lights_off ? 0.08f : 0.3f;
  f->ambient[0] = amb;
  f->ambient[1] = amb;
  f->ambient[2] = amb * 1.1f;
  f->fog[0] = f->fog[1] = f->fog[2] = 0.02f;
  f->sky[0] = f->sky[1] = f->sky[2] = 0.0f;
  f->fog_start = 12.0f;
  f->fog_end = 45.0f;
  f->wetness = 0;
  f->nlights = sr_site_collect_lights(site, f->lights, SR_MAX_FRAME_LIGHTS);
  /* enemy muzzle flashes and the player's own, before the draws pick their lights */
  sr_mesh_clear(&s->crowd_fx);
  static const int k_flash[3] = {4, 6, 8}; /* muzzle lights a frame may add */
  f->nlights =
      sr_build_enemy_fire(site, (double)site->time, g->player.pos, g->player.pos, &s->crowd_fx, f->lights, f->nlights,
                          SR_MIN(SR_MAX_FRAME_LIGHTS, f->nlights + k_flash[SR_CLAMP(s->street_detail, 0, 2)]));
  if (!g->dive.active) muzzle_light(s, g, vc, f);
  for (int r = 0; r < rooms; r++) add_draw(f, &s->rooms[r], SR_PASS_WORLD, sr_site_room_center(site, r));
  sr_build_site_dynamic(site, &g->deltas, (float)g->steps / 60.0f, &s->site_dyn);
  bool snap = s->door_building != site->building + 1;
  s->door_building = site->building + 1;
  sr_build_doors(site, s->door_angle, 1.0f / 60.0f, snap, &s->site_dyn);
  add_draw(f, &s->site_dyn, SR_PASS_WORLD, g->player.pos);
  sr_anim_cache_begin(&s->anim, (double)site->time);
  sr_build_guards(site, &s->anim, (double)site->time, g->player.pos, &s->guards);
  sr_anim_cache_end(&s->anim);
  add_draw(f, &s->crowd_fx, SR_PASS_GLOW, g->player.pos);
  if (g->thermal) { /* warm bodies glow through smoke and darkness */
    sr_mesh_thermal(&s->guards, 0);
    add_draw(f, &s->guards, SR_PASS_XRAY, g->player.pos);
  } else {
    add_draw(f, &s->guards, SR_PASS_WORLD, g->player.pos);
  }
}

static void scene_build(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f, bool matrix) {
  memset(f, 0, sizeof *f);
  s->frame_no++;
  s->chunks.built_this_frame = 0;
  const SrCity *c = g->city;
  const SrPlayer *p = &g->player;
  int64_t t = sr_game_time(g);
  update_sway(s, g);
  SrViewCam vc = sr_view_camera(g); /* head bob, landing dip, eased crouch, recoil, flinch, shake */
  f->eye = vc.eye;
  f->view = sr_m4_fps_view(vc.eye, vc.yaw, vc.pitch);
  f->proj = sr_m4_perspective(70.0f * SR_PI / 180.0f, aspect > 0 ? aspect : 16.0f / 9.0f, 0.1f, 4000.0f);
  SrWeatherState w = sr_game_weather(g);
  SrSky sky = sr_sky_weather(sr_sky_at(t, g->pall_removed), &w);
  int district = sr_city_district_at(c, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  const SrPalette *pal = sr_palette(district >= 0 ? c->districts[district].type : SR_D_RESIDENTIAL);
  /* the district's fog, turning slate grey-blue in the short grey day */
  static const float slate[3] = {0.20f, 0.22f, 0.26f};
  float fogc[3] = {pal->fog.r / 255.0f, pal->fog.g / 255.0f, pal->fog.b / 255.0f};
  for (int k = 0; k < 3; k++) f->fog[k] = fogc[k] + (slate[k] - fogc[k]) * sky.dayness * 0.6f;
  if (sky.daylight) {
    f->fog[0] = sky.r;
    f->fog[1] = sky.g;
    f->fog[2] = sky.b;
  }
  for (int k = 0; k < 3; k++) {
    f->sky[k] = f->fog[k]; /* the sky is the fog: the dome and the clear colour match it */
    f->band[k] = sky.band[k];
  }
  f->band_dir_x = sky.band_dir_x;
  /* each weather has its own look: brown smog, sickly green acid rain, pale fog, dark downpours */
  static const float wx_tint[SR_WX_COUNT][4] = {{0.12f, 0.10f, 0.13f, 0.45f}, /* dry, dusky overcast */
                                                {0.06f, 0.09f, 0.17f, 0.55f}, {0.03f, 0.04f, 0.07f, 0.45f},
                                                {0.17f, 0.18f, 0.20f, 0.45f}, {0.30f, 0.23f, 0.10f, 0.6f},
                                                {0.10f, 0.25f, 0.09f, 0.6f}};
  if (!sky.daylight) {
    const float *wt = wx_tint[w.to];
    for (int k = 0; k < 3; k++) f->fog[k] += (wt[k] - f->fog[k]) * wt[3] * w.blend;
    for (int k = 0; k < 3; k++) f->sky[k] = f->fog[k];
  }
  /* ground mist: low-lying districts and foggy weather */
  SrDistrictType dtype = district >= 0 ? c->districts[district].type : SR_D_RESIDENTIAL;
  f->mist = 0.06f + (dtype == SR_D_UNDERCITY || dtype == SR_D_DOCKS ? 0.3f : 0.0f) +
            (w.to == SR_WX_FOG ? 0.3f : 0.0f) * w.blend;
  if (sky.daylight) f->mist = 0.0f;
  /* 35 % less ambient than M1: walls stay darker than the neon on them */
  f->ambient[0] = pal->ambient.r / 255.0f * sky.ambient * 0.65f;
  f->ambient[1] = pal->ambient.g / 255.0f * sky.ambient * 0.65f;
  f->ambient[2] = pal->ambient.b / 255.0f * sky.ambient * 0.65f;
  int under = sr_city_block_at(c, (int32_t)(p->pos.x * 100.0f), (int32_t)(p->pos.z * 100.0f));
  if (under >= 0 && c->blocks[under].highway && p->pos.y < 11.0f) { /* under the highway deck it is darker */
    int32_t hox, hoy;
    sr_city_block_origin(under, c->grid, &hox, &hoy);
    float dz = p->pos.z - (float)hoy / 100.0f;
    if (dz >= 2.0f && dz <= (float)SR_STREET_CM / 100.0f - 2.0f)
      for (int k = 0; k < 3; k++) f->ambient[k] *= 0.6f;
  }
  f->daylight = sky.daylight;
  f->time_s = (float)sr_time_of_day(t) + (float)(g->steps % 60) / 60.0f;
  f->hour = (float)sr_time_of_day(t) / 3600.0f;
  for (int u = 0; u < 4; u++) f->win_share[u] = sr_window_share(u, f->hour);
  f->fog_end = SR_MIN(w.visibility, (float)(s->view_chunks + 1) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f);
  f->fog_start = f->fog_end * 0.15f;
  f->wetness = w.wetness;
  f->dither = g->settings.dither;
  f->quantize = g->settings.quantize;
  f->vertex_snap = g->settings.vertex_snap;
  f->affine = g->settings.affine;
  f->crt = g->settings.crt;
  f->bloom = g->settings.bloom;
  s->street_detail = SR_CLAMP(g->settings.street_detail, 0, 2);
  f->brightness = g->settings.brightness;
  f->pixel_lighting = g->settings.pixel_lighting;
  f->vignette = true;
  sr_grade_at(c, p->pos.x, p->pos.z, f->grade_lift, f->grade_gain);
  if (matrix && g->dive.active) {
    const SrDive *d = &g->dive;
    if (d->feed_device >= 0 && g->site.active && g->site.building == d->building &&
        d->feed_device < g->site.in.device_count) { /* a camera feed: the room through the lens */
      SrVec3 cp = sr_site_camera_pos(&g->site, d->feed_device);
      SrVec3 rc = sr_site_room_center(&g->site, g->site.in.devices[d->feed_device].room);
      float yaw = atan2f(-(rc.x - cp.x), -(rc.z - cp.z));
      f->eye = cp;
      f->view = sr_m4_fps_view(cp, yaw, -0.45f);
      f->crt = true;
      build_site_frame(s, g, f, vc);
      return;
    }
    build_matrix_frame(s, g, aspect, f);
    return;
  }
  if (g->site.active) {
    build_site_frame(s, g, f, vc);
    scene_body(s, g, vc, f);
    return;
  }
  if (s->light_grid.city_seed != c->seed || s->light_grid.blocks != c->grid * c->grid || !s->light_grid.lights)
    sr_light_grid_build(&s->light_grid, c);
  f->nlights = sr_light_grid_query(&s->light_grid, c, p->pos, 120.0f, f->lights, SR_MAX_FRAME_LIGHTS);
  if (g->net.blackout_district >= 0 && sr_game_blackout_at(g, g->net.blackout_district)) { /* lights out */
    int k = 0;
    for (int i = 0; i < f->nlights; i++) {
      SrVec3 lp = f->lights[i].pos;
      if (sr_city_district_at(c, (int32_t)(lp.x * 100), (int32_t)(lp.z * 100)) != g->net.blackout_district)
        f->lights[k++] = f->lights[i];
    }
    f->nlights = k;
    if (district == g->net.blackout_district)
      for (int j = 0; j < 3; j++) f->ambient[j] *= 0.45f;
  }
  muzzle_light(s, g, vc, f);
  if (s->turf_version != g->chess.turf_version) { /* gangs moved: repaint the walls */
    s->turf_version = g->chess.turf_version;
    for (int i = 0; i < SR_CHUNK_CACHE; i++) s->chunks.chunks[i].valid = false;
  }
  /* city chunks around the camera, nearest first, limited build budget */
  int ccx = (int)(p->pos.x * 100.0f) / (SR_BLOCK_CM * SR_CHUNK_BLOCKS);
  int ccy = (int)(p->pos.z * 100.0f) / (SR_BLOCK_CM * SR_CHUNK_BLOCKS);
  /* the fog hides everything past one or two blocks: stream only what can show through it */
  const float chunk_m = (float)SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f;
  int R = SR_MIN(s->view_chunks, 1 + (int)(f->fog_end * 1.3f / chunk_m));
  for (int ring = 0; ring <= R; ring++) {
    for (int dy = -ring; dy <= ring; dy++) {
      for (int dx = -ring; dx <= ring; dx++) {
        if (SR_MAX(abs(dx), abs(dy)) != ring) continue;
        if (ring == 0) { /* the camera's chunk: a draw per block, so each gets the lights nearest to it */
          for (int by = 0; by < SR_CHUNK_BLOCKS; by++)
            for (int bx = 0; bx < SR_CHUNK_BLOCKS; bx++) {
              int gx = ccx * SR_CHUNK_BLOCKS + bx, gy = ccy * SR_CHUNK_BLOCKS + by;
              if (gx >= c->grid || gy >= c->grid) continue;
              int32_t box, boy;
              sr_city_block_origin(gy * c->grid + gx, c->grid, &box, &boy);
              add_draw(f, scene_block(s, g, gy * c->grid + gx), SR_PASS_WORLD,
                       sr_v3((float)box / 100.0f + 48.0f, 2.0f, (float)boy / 100.0f + 48.0f));
            }
          continue;
        }
        SrChunk *ch = sr_chunk_get(&s->chunks, c, &g->deltas, ccx + dx, ccy + dy, 2);
        if (!ch) continue;
        float mx = ((float)(ccx + dx) + 0.5f) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f;
        float mz = ((float)(ccy + dy) + 0.5f) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f;
        add_draw(f, &ch->mesh, SR_PASS_WORLD, sr_v3(mx, 2, mz));
      }
    }
  }
  SrCrowdCtx cx;
  memset(&cx, 0, sizeof cx);
  cx.now = (double)g->steps / 60.0;
  cx.rain = w.rain;
  cx.wet = w.wetness;
  cx.wind = sr_wind(g->seed, t, w.rain);
  for (int k = 0; k < SR_MAX_FACTIONS; k++) cx.heat = SR_MAX(cx.heat, g->heat.level[k]);
  cx.player = p->pos;
  cx.eye = p->pos;
  static const int k_near[3] = {24, 36, 48};
  static const int k_splash[3] = {0, 24, 48};
  cx.max_near = k_near[s->street_detail];
  cx.max_splashes = k_splash[s->street_detail];
  cx.fx = &s->crowd_fx;
  sr_mesh_clear(&s->crowd_fx);
  sr_anim_cache_begin(&s->anim, cx.now);
  if (g->thermal) { /* thermal vision draws every body as a detailed heat signature */
    sr_build_characters(&g->crowd, &s->anim, &cx, p->pos, f->fog_end, &s->characters);
    sr_mesh_thermal(&s->characters, 0);
  } else {
    if (s->inst_body.nv == 0) sr_build_instance_body(&s->inst_body);
    float near_m = s->street_detail == 0 ? 16.0f : SR_CROWD_NEAR_M;
    f->inst_count = sr_build_crowd_lod(&g->crowd, &s->anim, &cx, p->pos, near_m, f->fog_end, &s->characters, s->inst,
                                       SR_MAX_AGENTS);
    f->inst_mesh = &s->inst_body;
    f->inst = s->inst;
    sr_build_vehicles(&g->crowd, &cx, p->pos, f->fog_end, &s->characters); /* vehicles join the near mesh */
    static const int k_beams[3] = {8, 12, 16};
    s->beams_drawn =
        sr_build_headlights(&g->crowd, p->pos, w.wetness, g->steps, k_beams[s->street_detail], &s->crowd_fx);
  }
  sr_anim_cache_end(&s->anim);
  s->near_drawn = 0;
  for (int i = 0; i < g->crowd.count; i++) {
    float dx = g->crowd.agents[i].pos.x - p->pos.x, dz = g->crowd.agents[i].pos.z - p->pos.z;
    s->near_drawn += dx * dx + dz * dz <= SR_CROWD_NEAR_M * SR_CROWD_NEAR_M;
  }
  s->near_drawn = SR_MIN(s->near_drawn, cx.max_near);
  sr_mesh_clear(&s->route_beacon);
  s->route_drawn = g->settings.nav_path /* the route on the pavement and the beacon over the destination */
                       ? sr_build_route_ar(&g->route, p->pos, sr_game_fx_now(g), &s->crowd_fx, &s->route_beacon)
                       : 0;
  add_draw(f, &s->route_beacon, SR_PASS_HALO, p->pos);
  add_draw(f, &s->crowd_fx, SR_PASS_GLOW, p->pos);
  add_draw(f, &s->characters, g->thermal ? SR_PASS_XRAY : SR_PASS_WORLD, p->pos);
  if (w.rain > 0.05f) { /* game time: same time, same drops */
    SrColor rc = w.to == SR_WX_ACID ? (SrColor){120, 200, 100, 170} : (SrColor){140, 155, 185, 170};
    static const float k_rain[3] = {0.4f, 0.67f, 1.0f}; /* 600, 1000 or 1500 streaks in a downpour */
    sr_build_rain_lit(p->pos, w.rain * k_rain[s->street_detail] * k_rain[s->street_detail], g->steps, c, f->lights,
                      f->nlights, rc, &s->rain);
    add_draw(f, &s->rain, SR_PASS_WORLD, p->pos);
  }
  sr_build_aerodynes(&g->air, c, (double)g->steps / 60.0, p->pos, 600.0f, &s->air, &s->crowd_fx);
  add_draw(f, &s->air, SR_PASS_WORLD, p->pos);
  if (!s->skyline_built) {
    sr_build_skyline(c, &s->skyline);
    s->skyline_built = true;
  }
  bool clear_view = w.visibility > 150.0f || p->pos.y > 40.0f;
  if (clear_view) { /* the skyline and the distant city only show on clearer nights or from up high */
    add_draw(f, &s->skyline, SR_PASS_NOFOG, p->pos);
    if (!s->distant_built) {
      sr_build_distant_city(c, &s->distant);
      sr_build_skyline_glow(c, &s->skyline_glow);
      s->distant_built = true;
    }
    add_draw(f, &s->distant, SR_PASS_FAR, p->pos);
    if (!g->pall_removed) add_draw(f, &s->skyline_glow, SR_PASS_HALO, p->pos);
  }
  sr_build_landmarks(c, (float)(g->steps) / 60.0f, &s->landmarks);
  for (int v = 0; v + 7 < s->landmarks.nv; v += 8) { /* logos dim with distance; their halos carry them */
    float dx = s->landmarks.v[v].x - p->pos.x, dz = s->landmarks.v[v].z - p->pos.z, d = sqrtf(dx * dx + dz * dz);
    float k = d <= f->fog_end ? 1.0f : SR_MAX(0.3f, 1.0f - (d - f->fog_end) / (2.5f * w.visibility));
    for (int j = v; j < v + 8; j++) {
      s->landmarks.v[j].r = (uint8_t)((float)s->landmarks.v[j].r * k);
      s->landmarks.v[j].g = (uint8_t)((float)s->landmarks.v[j].g * k);
      s->landmarks.v[j].b = (uint8_t)((float)s->landmarks.v[j].b * k);
    }
  }
  add_draw(f, &s->landmarks, SR_PASS_NOFOG, p->pos);
  /* the gloom: sky dome, the Pall overhead, halos through the fog, silhouettes beyond it */
  sr_sky_prepare(s, c);
  SrBillboard bb = sr_billboard(p->pos, p->yaw, p->pitch);
  sr_build_sky_dome(p->pos, &s->sky);
  add_draw(f, &s->sky, SR_PASS_SKY, p->pos);
  if (!g->pall_removed) {
    int dark = g->net.blackout_district >= 0 && sr_game_blackout_at(g, g->net.blackout_district)
                   ? g->net.blackout_district
                   : -2;
    sr_build_deck(s, c, p->pos, &sky, f->fog, f->time_s, dark, &s->deck, &s->deck_glow);
    add_draw(f, &s->deck, SR_PASS_NOFOG, p->pos);
    add_draw(f, &s->deck_glow, SR_PASS_HALO, p->pos);
  } else if (sky.phase == SR_LIGHT_DAY) {
    sr_build_sun(p->pos, &bb, &s->sun);
    add_draw(f, &s->sun, SR_PASS_HALO, p->pos);
  }
  if (!g->pall_removed) { /* searchlights cut up into the clouds; blimps drift just under them */
    uint32_t alert = 0;
    for (int k = 0; k < c->factions.corp_count; k++) {
      int hq = c->factions.f[k].hq_building;
      if (g->chess.f[k].retaliate_against >= 0 || (hq >= 0 && sr_game_host_alert(g, hq) > 0)) alert |= 1u << k;
    }
    sr_build_searchlights(c, p->pos, f->time_s, w.visibility, alert, &s->beams);
    add_draw(f, &s->beams, SR_PASS_HALO, p->pos);
    sr_ads_context(&s->ads, &g->chess, c, t);
    sr_build_blimps(c, &s->ads, p->pos, f->time_s, f->fog, f->fog_end, w.visibility, &s->blimps, &s->blimp_glow);
    add_draw(f, &s->blimps, SR_PASS_NOFOG, p->pos);
    add_draw(f, &s->blimp_glow, SR_PASS_HALO, p->pos);
  }
  f->halo_count = sr_build_halos(s->beacons, s->nbeacons, &bb, f->fog_end, w.visibility, &s->halos);
  add_draw(f, &s->halos, SR_PASS_HALO, p->pos);
  float wet_air = (1.0f - SR_MIN(1.0f, w.visibility / 220.0f)) + 0.5f * w.rain;
  sr_build_near_halos(f->lights, f->nlights, &bb, wet_air, &s->near_glow);
  add_draw(f, &s->near_glow, SR_PASS_GLOW, p->pos);
  float moved = fabsf(p->pos.x - s->sil_at.x) + fabsf(p->pos.z - s->sil_at.z);
  if (moved > 8.0f || fabsf(s->sil_fog_end - f->fog_end) > 2.0f || s->silhouettes.nv == 0) {
    sr_build_silhouettes(c, p->pos, f->fog_end, f->fog, &s->silhouettes);
    s->sil_at = p->pos;
    s->sil_fog_end = f->fog_end;
  }
  add_draw(f, &s->silhouettes, SR_PASS_NOFOG, p->pos);
  if (!g->pall_removed && (w.to == SR_WX_SMOG || w.to == SR_WX_ACID)) { /* an orange or sickly Pall */
    float tr = w.to == SR_WX_SMOG ? 1.25f : 0.8f, tg = w.to == SR_WX_SMOG ? 1.0f : 1.2f, tb = 0.7f;
    for (int v = 0; v < s->deck.nv; v++) {
      s->deck.v[v].r = (uint8_t)SR_MIN(255.0f, (float)s->deck.v[v].r * tr);
      s->deck.v[v].g = (uint8_t)SR_MIN(255.0f, (float)s->deck.v[v].g * tg);
      s->deck.v[v].b = (uint8_t)((float)s->deck.v[v].b * tb);
    }
  }
  /* the street around the camera: kerbs, markings and props, a draw (and lights) per block */
  sr_ads_context(&s->ads, &g->chess, c, t);
  if (s->sky_seed != s->street_seed || s->ads.key != s->ads_key) { /* another city, or the adverts changed */
    for (int i = 0; i < 32; i++) s->street[i].used = 0;
    s->street_seed = s->sky_seed;
    s->ads_key = s->ads.key;
  }
  int sring = s->street_detail >= 2 ? 2 : 1;
  int cb = sr_city_block_at(c, (int32_t)(p->pos.x * 100.0f), (int32_t)(p->pos.z * 100.0f));
  for (int dy = -sring; cb >= 0 && dy <= sring; dy++)
    for (int dx = -sring; dx <= sring; dx++) {
      int bx = cb % c->grid + dx, by = cb / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      SrMesh *sm = sr_scene_street(s, c, by * c->grid + bx);
      int32_t sox, soy;
      sr_city_block_origin(by * c->grid + bx, c->grid, &sox, &soy);
      add_draw(f, sm, SR_PASS_WORLD, sr_v3((float)sox / 100.0f + 48.0f, 2.0f, (float)soy / 100.0f + 48.0f));
    }
  sr_build_poi_fronts(c, &g->deltas, p->pos, t, g->settings.time_rate,
                      &s->fronts); /* open shops light up their fronts */
  const SrNews *latest = g->chess.news.count > 0 ? sr_news_latest(&g->chess.news, 0) : NULL;
  sr_build_tickers(c, latest ? latest->headline : "", p->pos, f->time_s, &s->fronts);
  int heat = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat = SR_MAX(heat, g->heat.level[k]);
  int home_d = c->blocks[c->buildings[c->home_building].block].district;
  sr_build_wanted(c, home_d, heat, sr_outfit_color(&g->player.look), p->pos, &s->fronts);
  add_draw(f, &s->fronts, SR_PASS_WORLD, p->pos);
  /* wet streets: reflections, splashes, drips, lamp cones, steam and ripples */
  sr_mesh_clear(&s->wet);
  static const int k_refl[3] = {16, 32, 64};
  s->reflections = g->settings.reflections ? sr_build_reflections(f->lights, f->nlights, p->pos, w.wetness,
                                                                  k_refl[s->street_detail], &s->wet)
                                           : 0;
  sr_build_splashes(c, p->pos, w.rain, g->steps, &s->wet);
  sr_build_drips(c, p->pos, w.rain, f->time_s, &s->wet);
  sr_build_lamp_cones(f->lights, f->nlights, p->pos, wet_air, &s->wet);
  int cblk = sr_city_block_at(c, (int32_t)(p->pos.x * 100.0f), (int32_t)(p->pos.z * 100.0f));
  if (cblk >= 0) {
    float steam_rain = w.rain + (w.to == SR_WX_ACID ? 0.5f : 0.0f);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int bx = cblk % c->grid + dx, by = cblk / c->grid + dy;
        if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
        int n = 0;
        const SrStreetProp *pr = sr_scene_props(s, c, by * c->grid + bx, &n);
        sr_build_steam(pr, n, p->pos, steam_rain, f->time_s, 30.0f + 15.0f * (float)s->street_detail, &s->wet);
        sr_build_ripples(pr, n, p->pos, w.rain, f->time_s, &s->wet);
      }
  }
  sr_build_stack_smoke(c, p->pos, f->time_s, &s->wet);
  sr_mesh_clear(&s->pools); /* pools of light under lamps, signs, vending machines and fires */
  sr_build_light_pools(c, f->lights, f->nlights, p->pos, &s->pools);
  add_draw(f, &s->pools, SR_PASS_GLOW, p->pos);
  sr_build_holo_ads(c, &s->ads, p->pos, f->time_s, &s->wet);
  sr_build_highway_traffic(c, p->pos, f->time_s, &s->wet);
  sr_build_flyers(c, p->pos, f->time_s, &s->wet);
  sr_build_litter(c, p->pos, f->time_s, sr_wind(g->seed, t, w.rain), &s->wet);
  sr_build_sparks(c, p->pos, f->time_s, &s->wet);
  sr_build_police_lights(&g->crowd, p->pos, f->time_s, heat, &s->wet);
  sr_build_fans(c, p->pos, f->time_s, &s->fronts);
  if (!s->vertical_built) { /* skyways and the elevated metro */
    sr_mesh_clear(&s->vertical);
    sr_build_skyways(c, &s->vertical);
    sr_build_metro_tracks(c, &s->vertical);
    s->vertical_built = true;
  }
  add_draw(f, &s->vertical, SR_PASS_WORLD, p->pos);
  sr_mesh_clear(&s->trains);
  sr_build_trains(c, t, p->pos, &s->trains);
  add_draw(f, &s->trains, SR_PASS_WORLD, p->pos);
  add_draw(f, &s->wet, SR_PASS_GLOW, p->pos);
  scene_body(s, g, vc, f);
}

void sr_scene_build(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f) { scene_build(s, g, aspect, f, true); }

void sr_scene_build_body(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f) {
  scene_build(s, g, aspect, f, false);
  f->bloom = false;
  f->internal_w = 160;
  f->internal_h = 90;
}

float sr_fog_factor(float z, float start, float end, float emissive) {
  float f = (z - start) / (end - start);
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  return f * (1.0f - 0.85f * emissive);
}

/* ---- view math ------------------------------------------------------------------------------ */

SrViewport sr_letterbox(int ww, int wh, int sw, int sh, bool integer_scale) {
  SrViewport v = {0, 0, ww, wh};
  if (sw <= 0 || sh <= 0 || ww <= 0 || wh <= 0) return v;
  float sx = (float)ww / (float)sw, sy = (float)wh / (float)sh;
  float s = sx < sy ? sx : sy;
  if (integer_scale && s >= 1.0f) s = floorf(s);
  v.w = (int)lroundf((float)sw * s);
  v.h = (int)lroundf((float)sh * s);
  if (v.w > ww) v.w = ww;
  if (v.h > wh) v.h = wh;
  v.x = (ww - v.w) / 2;
  v.y = (wh - v.h) / 2;
  return v;
}

float sr_projected_pixels(float size_m, float dist_m, float fovy, int screen_h) {
  if (dist_m <= 0.0f) return (float)screen_h;
  return size_m / (2.0f * dist_m * tanf(fovy * 0.5f)) * (float)screen_h;
}

bool sr_dynres_update(SrDynRes *d, float frame_ms, float budget_ms, float dt) {
  if (frame_ms > budget_ms) {
    d->over_time += dt;
    d->under_time = 0;
    if (d->over_time >= 2.0f && d->level < 3) {
      d->level++;
      d->over_time = 0;
      return true;
    }
  } else {
    d->under_time += dt;
    d->over_time = 0;
    if (d->under_time >= 10.0f && d->level > 0) {
      d->level--;
      d->under_time = 0;
      return true;
    }
  }
  return false;
}

void sr_dynres_apply(const SrDynRes *d, int *w, int *h) {
  static const int pct[4] = {100, 85, 70, 55};
  *w = *w * pct[SR_CLAMP(d->level, 0, 3)] / 100;
  *h = *h * pct[SR_CLAMP(d->level, 0, 3)] / 100;
}
