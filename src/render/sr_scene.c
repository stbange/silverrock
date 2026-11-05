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

/* Barrens and Undercity blocks with empty lots get burning barrels: a light where there is none. */
static int barrels(const SrCity *c, int blk, SrVec3 out[2]) {
  const SrBlock *b = &c->blocks[blk];
  SrDistrictType t = c->districts[b->district].type;
  if (b->water || (t != SR_D_BARRENS && t != SR_D_UNDERCITY) || b->building_count >= 3) return 0;
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  int n = 0;
  for (int k = 0; k < 2 - (b->building_count > 1); k++) {
    uint64_t h = sr_mix64(c->seed ^ ((uint64_t)blk * 31u + (uint64_t)k));
    float x = (float)ox / 100.0f + 22.0f + (float)(h % 50), z = (float)oy / 100.0f + 22.0f + (float)((h >> 16) % 50);
    bool inside = false;
    for (int i = 0; i < b->building_count; i++) {
      const SrBuilding *bd = &c->buildings[b->first_building + i];
      inside = inside ||
               (x * 100 > bd->x0 - 200 && x * 100 < bd->x1 + 200 && z * 100 > bd->y0 - 200 && z * 100 < bd->y1 + 200);
    }
    if (!inside) out[n++] = sr_v3(x, 0, z);
  }
  return n;
}

static int block_lights(const SrCity *c, int blk, SrLight *out, int max) {
  int n = 0;
  const SrBlock *b = &c->blocks[blk];
  if (b->water) return 0;
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  SrColor col = sr_palette(c->districts[b->district].type)->neon[blk % 3];
  if (n < max) { /* street lamp */
    SrLight *l = &out[n++];
    float st = (float)SR_STREET_CM / 100.0f;
    l->pos = sr_v3((float)ox / 100.0f + st - 1.1f, 6.2f, (float)oy / 100.0f + st - 1.1f);
    l->radius = 26.0f;
    l->r = col.r / 255.0f * 0.9f;
    l->g = col.g / 255.0f * 0.9f;
    l->b = col.b / 255.0f * 0.9f;
  }
  for (int i = 0; i < b->building_count && n < max; i++) {
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
  SrVec3 fire[2];
  int nf = barrels(c, blk, fire);
  for (int i = 0; i < nf && n < max; i++) {
    SrLight *l = &out[n++];
    l->pos = sr_v3(fire[i].x, 1.6f, fire[i].z);
    l->radius = 16.0f;
    l->r = 1.0f;
    l->g = 0.45f;
    l->b = 0.12f;
  }
  return n;
}

void sr_light_grid_build(SrLightGrid *lg, const SrCity *c) {
  sr_light_grid_free(lg);
  int blocks = c->grid * c->grid;
  lg->lights = (SrLight *)malloc(sizeof(SrLight) * (size_t)blocks * 7);
  if (!lg->lights) return; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  lg->blocks = blocks;
  lg->city_seed = c->seed;
  for (int b = 0; b < blocks; b++) {
    lg->start[b] = (uint16_t)lg->count;
    lg->count += block_lights(c, b, lg->lights + lg->count, 7);
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

static void build_block(const SrCity *c, const SrDeltas *d, int blk, SrMesh *m) {
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
  /* streets (west and north strips of the block) and the sidewalk-lined lot */
  sr_mesh_quad(m, sr_v3(x0, 0, z0 + s), sr_v3(x0 + st, 0, z0 + s), sr_v3(x0 + st, 0, z0), sr_v3(x0, 0, z0), 0, 0, 1,
               s / st, white, SR_LAYER_ASPHALT, 0, 1);
  sr_mesh_quad(m, sr_v3(x0 + st, 0, z0 + st), sr_v3(x0 + s, 0, z0 + st), sr_v3(x0 + s, 0, z0), sr_v3(x0 + st, 0, z0), 0,
               0, (s - st) / st, 1, white, SR_LAYER_ASPHALT, 0, 1);
  sr_mesh_quad(m, sr_v3(x0 + st, 0.12f, z0 + s), sr_v3(x0 + s, 0.12f, z0 + s), sr_v3(x0 + s, 0.12f, z0 + st),
               sr_v3(x0 + st, 0.12f, z0 + st), 0, 0, (s - st) / 4, (s - st) / 4, pal->ground, SR_LAYER_SIDEWALK, 0, 1);
  { /* street lamp at the intersection */
    SrColor lamp = pal->neon[blk % 3];
    sr_mesh_box(m, sr_v3(x0 + st - 1.2f, 0, z0 + st - 1.2f), sr_v3(x0 + st - 1.0f, 6.5f, z0 + st - 1.0f),
                (SrColor){60, 60, 66, 255}, SR_LAYER_WHITE, 0, 0);
    sr_mesh_box(m, sr_v3(x0 + st - 1.6f, 6.5f, z0 + st - 1.6f), sr_v3(x0 + st - 0.6f, 6.8f, z0 + st - 0.6f), lamp,
                SR_LAYER_WHITE, 0, 255);
  }
  SrVec3 fire[2];
  for (int i = 0, nf = barrels(c, blk, fire); i < nf; i++) { /* burning barrels */
    sr_mesh_box(m, sr_v3(fire[i].x - 0.35f, 0, fire[i].z - 0.35f), sr_v3(fire[i].x + 0.35f, 0.9f, fire[i].z + 0.35f),
                (SrColor){70, 50, 40, 255}, SR_LAYER_METAL, 0.5f, 0);
    sr_mesh_box(m, sr_v3(fire[i].x - 0.28f, 0.9f, fire[i].z - 0.28f),
                sr_v3(fire[i].x + 0.28f, 1.35f, fire[i].z + 0.28f), (SrColor){255, 140, 40, 255}, SR_LAYER_WHITE, 0,
                255);
  }
  if (b->highway) { /* elevated highway deck overhead */
    sr_mesh_box(m, sr_v3(x0, 11.0f, z0 + 2), sr_v3(x0 + s, 12.0f, z0 + st - 2), shade(pal->ground, 80),
                SR_LAYER_CONCRETE, 0.2f, 0);
  }
  for (int i = 0; i < b->building_count; i++) {
    int bi = b->first_building + i;
    const SrBuilding *bd = &c->buildings[bi];
    float bx0 = (float)bd->x0 / 100.0f, bz0 = (float)bd->y0 / 100.0f;
    float bx1 = (float)bd->x1 / 100.0f, bz1 = (float)bd->y1 / 100.0f;
    float h = (float)bd->height_cm / 100.0f;
    static const int facade[4] = {SR_LAYER_WIN_A, SR_LAYER_WIN_B, SR_LAYER_WIN_GLASS, SR_LAYER_WIN_A};
    int layer = facade[bd->style % 4];
    SrDistrictType dt = c->districts[b->district].type;
    if (dt == SR_D_SPIRE || dt == SR_D_CAMPUS) layer = SR_LAYER_WIN_GLASS;
    if (dt == SR_D_OLDQUARTER && bd->style % 2) layer = SR_LAYER_BRICK;
    if ((dt == SR_D_INDUSTRIAL || dt == SR_D_DOCKS) && bd->style % 2) layer = SR_LAYER_METAL;
    /* bright base material lightly tinted by the district; lighting darkens it */
    SrColor tint = {(uint8_t)((200 + pal->ambient.r) / 2 - bd->style * 4),
                    (uint8_t)((200 + pal->ambient.g) / 2 - bd->style * 4),
                    (uint8_t)((200 + pal->ambient.b) / 2 - bd->style * 4), 255};
    if (bd->poi == SR_POI_CORP_HQ && bd->owner >= 0) {
      /* landmark tower: stacked tiers by silhouette */
      int tiers = 1 + bd->silhouette[0];
      float taper = 0.06f + 0.05f * bd->silhouette[1];
      float lx0 = bx0, lz0 = bz0, lx1 = bx1, lz1 = bz1, y = 0;
      for (int t = 0; t < tiers; t++) {
        float th = h / (float)tiers;
        sr_mesh_box(m, sr_v3(lx0, y, lz0), sr_v3(lx1, y + th, lz1), tint, layer, 0.08f, 0);
        float dx = (lx1 - lx0) * taper, dz = (lz1 - lz0) * taper;
        lx0 += dx;
        lx1 -= dx;
        lz0 += dz;
        lz1 -= dz;
        y += th;
      }
    } else {
      sr_mesh_box(m, sr_v3(bx0, 0, bz0), sr_v3(bx1, h, bz1), tint, layer, 0.08f, 0);
    }
    if (bd->poi != SR_POI_NONE) {
      /* neon sign with the place's icon over the door (front = +z side) */
      SrColor neon = neon_for(c, bi);
      float cx = (bx0 + bx1) / 2, w = SR_MIN(3.0f, (bx1 - bx0) * 0.4f);
      int open = sr_deltas_get(d, bd->id) != 0; /* deltas can switch signs off */
      int em = open ? 255 : 40;
      sr_mesh_quad(m, sr_v3(cx - w, 3.2f, bz1 + 0.15f), sr_v3(cx + w, 3.2f, bz1 + 0.15f),
                   sr_v3(cx + w, 5.4f, bz1 + 0.15f), sr_v3(cx - w, 5.4f, bz1 + 0.15f), 0, 1, 1, 0, neon, SR_LAYER_SIGN,
                   em, 0);
      sr_mesh_quad(m, sr_v3(cx - 1.0f, 3.3f, bz1 + 0.2f), sr_v3(cx + 1.0f, 3.3f, bz1 + 0.2f),
                   sr_v3(cx + 1.0f, 5.3f, bz1 + 0.2f), sr_v3(cx - 1.0f, 5.3f, bz1 + 0.2f), 0, 1, 1, 0, neon,
                   SR_LAYER_ICON_FIRST + bd->poi, em, 0);
      /* awning: shelter from the rain */
      sr_mesh_quad(m, sr_v3(cx - w - 1, 3.0f, bz1 + 2.0f), sr_v3(cx + w + 1, 3.0f, bz1 + 2.0f),
                   sr_v3(cx + w + 1, 3.0f, bz1), sr_v3(cx - w - 1, 3.0f, bz1), 0, 0, 1, 1, shade(neon, 40),
                   SR_LAYER_WHITE, 0, 0);
    }
    /* rooftop props */
    if (h > 20.0f && (bd->id % 3) == 0) {
      float px = bx0 + (bx1 - bx0) * 0.3f, pz = bz0 + (bz1 - bz0) * 0.3f;
      sr_mesh_box(m, sr_v3(px, h, pz), sr_v3(px + 2, h + 1.5f, pz + 2), shade(tint, 70), SR_LAYER_METAL, 0.5f, 0);
      sr_mesh_box(m, sr_v3(px + 3, h, pz), sr_v3(px + 3.2f, h + 6.0f, pz + 0.2f), (SrColor){255, 40, 40, 255},
                  SR_LAYER_WHITE, 0, 200); /* antenna beacon */
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

static SrColor palette16(int i, int sat) {
  static const SrColor k[16] = {{40, 40, 46, 255},    {200, 200, 205, 255}, {150, 30, 40, 255},  {30, 60, 140, 255},
                                {40, 110, 60, 255},   {200, 160, 40, 255},  {90, 50, 120, 255},  {20, 20, 22, 255},
                                {230, 90, 30, 255},   {60, 160, 170, 255},  {170, 40, 140, 255}, {110, 80, 60, 255},
                                {240, 240, 240, 255}, {80, 90, 100, 255},   {255, 40, 120, 255}, {40, 220, 255, 255}};
  SrColor c = k[i & 15];
  if (!sat) c = shade(c, 80);
  return c;
}

float sr_walk_swing(float walk) { return sinf(walk * 2.4f) * 0.55f; }

void sr_build_character_walk(const SrAppearance *a, SrVec3 p, SrVec3 dir, float walk, bool umbrella, SrMesh *m) {
  static const SrColor skin[8] = {{250, 210, 180, 255}, {230, 180, 140, 255}, {200, 150, 110, 255}, {170, 120, 80, 255},
                                  {140, 95, 60, 255},   {110, 70, 45, 255},   {80, 52, 35, 255},    {60, 40, 28, 255}};
  static const SrColor hair[12] = {{20, 18, 16, 255},   {70, 45, 25, 255},    {160, 120, 60, 255}, {220, 210, 170, 255},
                                   {150, 40, 30, 255},  {200, 200, 200, 255}, {255, 40, 160, 255}, {40, 255, 200, 255},
                                   {120, 60, 255, 255}, {255, 230, 40, 255},  {40, 120, 255, 255}, {255, 90, 20, 255}};
  float yaw = (fabsf(dir.x) + fabsf(dir.z)) > 1e-4f ? atan2f(dir.x, dir.z) : 0.0f;
  float bw = 0.22f + 0.04f * a->body; /* body type changes the silhouette width */
  float tall = 1.55f + 0.08f * (float)(a->body % 3);
  SrColor outfit = palette16(a->outfit_color, a->outfit % 4 != 0);
  SrColor legs = shade(outfit, 60);
  SrColor sk = skin[a->skin & 7];
  float hip = tall * 0.48f, shoulder = tall * 0.8f;
  float sw = walk >= 0 ? sr_walk_swing(walk) : 0.0f;
  SrVec3 o = sr_v3(p.x, p.y, p.z);
  /* legs swing around the hips, arms around the shoulders, opposite phase */
  sr_mesh_part(m, o, yaw, sr_v3(0, hip, 0), sw, sr_v3(-bw, 0, -0.12f), sr_v3(-0.02f, hip, 0.12f), legs, SR_LAYER_WHITE,
               0);
  sr_mesh_part(m, o, yaw, sr_v3(0, hip, 0), -sw, sr_v3(0.02f, 0, -0.12f), sr_v3(bw, hip, 0.12f), legs, SR_LAYER_WHITE,
               0);
  sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-bw - 0.04f, hip, -0.15f), sr_v3(bw + 0.04f, tall * 0.82f, 0.15f),
               outfit, SR_LAYER_WHITE, 0);
  sr_mesh_part(m, o, yaw, sr_v3(0, shoulder, 0), -sw * 0.8f, sr_v3(-bw - 0.15f, tall * 0.5f, -0.07f),
               sr_v3(-bw - 0.05f, shoulder, 0.07f), outfit, SR_LAYER_WHITE, 0);
  sr_mesh_part(m, o, yaw, sr_v3(0, shoulder, 0), sw * 0.8f, sr_v3(bw + 0.05f, tall * 0.5f, -0.07f),
               sr_v3(bw + 0.15f, shoulder, 0.07f), outfit, SR_LAYER_WHITE, 0);
  float hs = 0.12f + 0.01f * (a->head % 3);
  float hy = tall * 0.84f;
  sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-hs, hy, -hs), sr_v3(hs, hy + 2 * hs, hs), sk, SR_LAYER_WHITE, 0);
  float top = hy + 2 * hs;
  int style = a->hair % 12;
  SrColor hc = hair[a->hair_color % 12];
  int hem = a->hair_color >= 6 ? 120 : 0;
  if (style < 4) { /* mohawk / crest */
    sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-0.03f, top, -hs), sr_v3(0.03f, top + 0.12f + 0.03f * style, hs),
                 hc, SR_LAYER_WHITE, hem);
  } else if (style < 9) { /* cap of hair */
    sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-hs - 0.01f, top - 0.06f, -hs - 0.01f),
                 sr_v3(hs + 0.01f, top + 0.03f, hs + 0.01f), hc, SR_LAYER_WHITE, hem);
  }
  if (a->cyber) /* glowing optics, on the face */
    sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-hs * 0.8f, hy + hs * 1.1f, hs),
                 sr_v3(hs * 0.8f, hy + hs * 1.35f, hs + 0.02f), (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE, 255);
  if (a->marker) { /* faction marker: halo, shoulder plates or mask by marker id */
    SrColor mk = sr_corp_color(a->marker * 5);
    switch (a->marker % 3) {
      case 0:
        sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-0.2f, top + 0.15f, -0.2f), sr_v3(0.2f, top + 0.18f, 0.2f), mk,
                     SR_LAYER_WHITE, 255);
        break;
      case 1:
        sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-bw - 0.12f, tall * 0.78f, -0.16f),
                     sr_v3(bw + 0.12f, tall * 0.84f, 0.16f), mk, SR_LAYER_WHITE, 60);
        break;
      default:
        sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-hs, hy, hs), sr_v3(hs, hy + hs, hs + 0.02f), mk,
                     SR_LAYER_WHITE, 80);
        break;
    }
  }
  if (umbrella) {
    SrColor uc = palette16(a->accessory + 3, 1);
    sr_mesh_part(m, o, yaw, sr_v3(0, 0, 0), 0, sr_v3(-0.55f, tall + 0.25f, -0.55f), sr_v3(0.55f, tall + 0.3f, 0.55f),
                 uc, SR_LAYER_WHITE, 30);
  }
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
  body_part(m, sr_v3(-0.22f, 0, -0.12f), sr_v3(-0.02f, 0.48f, 0.12f), 0);
  body_part(m, sr_v3(0.02f, 0, -0.12f), sr_v3(0.22f, 0.48f, 0.12f), 1);
  body_part(m, sr_v3(-0.26f, 0.48f, -0.15f), sr_v3(0.26f, 0.82f, 0.15f), 2);
  body_part(m, sr_v3(-0.37f, 0.5f, -0.07f), sr_v3(-0.27f, 0.8f, 0.07f), 3);
  body_part(m, sr_v3(0.27f, 0.5f, -0.07f), sr_v3(0.37f, 0.8f, 0.07f), 4);
  body_part(m, sr_v3(-0.13f, 0.84f, -0.13f), sr_v3(0.13f, 1.0f, 0.13f), 5);
}

int sr_build_crowd_lod(const SrCrowd *cr, SrVec3 cam, float near_m, float far_m, SrMesh *near, float *inst, int max) {
  static const SrColor skin[8] = {{250, 210, 180, 255}, {230, 180, 140, 255}, {200, 150, 110, 255}, {170, 120, 80, 255},
                                  {140, 95, 60, 255},   {110, 70, 45, 255},   {80, 52, 35, 255},    {60, 40, 28, 255}};
  sr_mesh_clear(near);
  int n = 0;
  for (int i = 0; i < cr->count; i++) {
    const SrAgent *a = &cr->agents[i];
    float dx = a->pos.x - cam.x, dz = a->pos.z - cam.z, d2 = dx * dx + dz * dz;
    if (d2 > far_m * far_m) continue;
    float walk = a->speed > 0.05f ? a->walk : -1.0f;
    if (d2 <= near_m * near_m || n >= max) {
      sr_build_character_walk(&a->look, a->pos, a->dir, walk, a->umbrella, near);
      continue;
    }
    float *f = inst + n * SR_INST_FLOATS;
    float tall = 1.55f + 0.08f * (float)(a->look.body % 3);
    SrColor o = palette16(a->look.outfit_color, a->look.outfit % 4 != 0), sk = skin[a->look.skin & 7];
    f[0] = a->pos.x;
    f[1] = a->pos.y;
    f[2] = a->pos.z;
    f[3] = (fabsf(a->dir.x) + fabsf(a->dir.z)) > 1e-4f ? atan2f(a->dir.x, a->dir.z) : 0.0f;
    f[4] = walk >= 0 ? sr_walk_swing(walk) : 0.0f;
    f[5] = (0.22f + 0.04f * a->look.body) / 0.22f;
    f[6] = tall;
    f[7] = 0;
    f[8] = o.r / 255.0f;
    f[9] = o.g / 255.0f;
    f[10] = o.b / 255.0f;
    f[11] = 0;
    f[12] = sk.r / 255.0f;
    f[13] = sk.g / 255.0f;
    f[14] = sk.b / 255.0f;
    f[15] = 0;
    n++;
  }
  return n;
}

void sr_build_vehicles(const SrCrowd *cr, SrVec3 cam, float max_dist, SrMesh *out) {
  for (int i = 0; i < cr->vehicle_count; i++) {
    const SrVehicle *v = &cr->vehicles[i];
    float dx = v->pos.x - cam.x, dz = v->pos.z - cam.z;
    if (dx * dx + dz * dz > max_dist * max_dist) continue;
    float lx = fabsf(v->dir.x) > 0.5f ? 2.2f : 0.9f, lz = fabsf(v->dir.x) > 0.5f ? 0.9f : 2.2f;
    SrColor body = sr_corp_color(v->color);
    body = shade(body, 45);
    sr_mesh_box(out, sr_v3(v->pos.x - lx, 0.3f, v->pos.z - lz), sr_v3(v->pos.x + lx, 1.3f, v->pos.z + lz), body,
                SR_LAYER_METAL, 0.5f, 0);
    SrVec3 f = sr_v3_add(v->pos, sr_v3_scale(v->dir, 2.25f));
    SrVec3 b = sr_v3_sub(v->pos, sr_v3_scale(v->dir, 2.25f));
    sr_mesh_box(out, sr_v3(f.x - 0.4f, 0.6f, f.z - 0.4f), sr_v3(f.x + 0.4f, 0.9f, f.z + 0.4f),
                (SrColor){255, 250, 220, 255}, SR_LAYER_WHITE, 0, 255);
    sr_mesh_box(out, sr_v3(b.x - 0.4f, 0.6f, b.z - 0.4f), sr_v3(b.x + 0.4f, 0.9f, b.z + 0.4f),
                v->stopped ? (SrColor){255, 30, 30, 255} : (SrColor){150, 20, 20, 255}, SR_LAYER_WHITE, 0, 255);
  }
}

void sr_build_characters(const SrCrowd *cr, SrVec3 cam, float max_dist, SrMesh *out) {
  sr_mesh_clear(out);
  for (int i = 0; i < cr->count; i++) {
    const SrAgent *a = &cr->agents[i];
    float dx = a->pos.x - cam.x, dz = a->pos.z - cam.z;
    if (dx * dx + dz * dz > max_dist * max_dist) continue;
    sr_build_character_walk(&a->look, a->pos, a->dir, a->speed > 0.05f ? a->walk : -1.0f, a->umbrella, out);
  }
  sr_build_vehicles(cr, cam, max_dist, out);
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
  sr_mesh_clear(out);
  int drops = (int)(600.0f * intensity);
  SrRng r = sr_rng_stream(frame, "rain", 0, 0);
  SrColor col = {170, 190, 230, 160};
  for (int i = 0; i < drops; i++) {
    float x = cam.x + (sr_rng_float(&r) - 0.5f) * 40.0f;
    float z = cam.z + (sr_rng_float(&r) - 0.5f) * 40.0f;
    float y = cam.y + (sr_rng_float(&r) - 0.3f) * 14.0f;
    SrVec3 p = sr_v3(x, y, z);
    if (sr_point_covered(c, p)) continue;
    sr_mesh_quad(out, sr_v3(x, y, z), sr_v3(x + 0.02f, y, z), sr_v3(x + 0.05f, y + 0.6f, z),
                 sr_v3(x + 0.03f, y + 0.6f, z), 0, 0, 1, 1, col, SR_LAYER_WHITE, 90, 0);
  }
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
    sr_mesh_quad(out, sr_v3(x - dx, y - s, z - dz), sr_v3(x + dx, y - s, z + dz), sr_v3(x + dx, y + s, z + dz),
                 sr_v3(x - dx, y + s, z - dz), 0, 1, 1, 0, col, SR_LAYER_ICON_FIRST + SR_POI_CORP_HQ, 255, 0);
    sr_mesh_quad(out, sr_v3(x + dx, y - s, z + dz), sr_v3(x - dx, y - s, z - dz), sr_v3(x - dx, y + s, z - dz),
                 sr_v3(x + dx, y + s, z + dz), 0, 1, 1, 0, col, SR_LAYER_ICON_FIRST + SR_POI_CORP_HQ, 255, 0);
  }
}

/* ---- frame ------------------------------------------------------------------------------------ */

void sr_scene_init(SrSceneCache *s, bool mobile) {
  memset(s, 0, sizeof *s);
  sr_chunk_cache_init(&s->chunks, mobile ? 48 : SR_CHUNK_CACHE);
  s->view_chunks = mobile ? 2 : 3;
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
}

static void add_draw(SrFrame *f, SrMesh *m, SrPass pass, SrVec3 at) {
  if (f->ndraws >= SR_MAX_DRAWS || !m || m->ni == 0) return;
  SrDraw *d = &f->draws[f->ndraws++];
  d->mesh = m;
  d->pass = pass;
  d->nlights = pass == SR_PASS_WORLD ? sr_select_lights(f->lights, f->nlights, at, d->lights) : 0;
}

/* Interior frame: room meshes (rebuilt when doors, devices or lights change), guards, effects. */
static void build_site_frame(SrSceneCache *s, const SrGame *g, SrFrame *f) {
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
  for (int r = 0; r < rooms; r++) add_draw(f, &s->rooms[r], SR_PASS_WORLD, sr_site_room_center(site, r));
  sr_build_site_dynamic(site, &g->deltas, (float)g->steps / 60.0f, &s->site_dyn);
  add_draw(f, &s->site_dyn, SR_PASS_WORLD, g->player.pos);
  sr_build_guards(site, &s->guards);
  if (g->thermal) { /* warm bodies glow through smoke and darkness */
    sr_mesh_thermal(&s->guards, 0);
    add_draw(f, &s->guards, SR_PASS_XRAY, g->player.pos);
  } else {
    add_draw(f, &s->guards, SR_PASS_WORLD, g->player.pos);
  }
}

void sr_scene_build(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f) {
  memset(f, 0, sizeof *f);
  s->frame_no++;
  s->chunks.built_this_frame = 0;
  const SrCity *c = g->city;
  const SrPlayer *p = &g->player;
  int64_t t = sr_game_time(g);
  f->eye = p->pos;
  f->view = sr_m4_fps_view(p->pos, p->yaw, p->pitch);
  f->proj = sr_m4_perspective(70.0f * SR_PI / 180.0f, aspect > 0 ? aspect : 16.0f / 9.0f, 0.1f, 4000.0f);
  SrWeatherState w = sr_weather_at(g->seed, t);
  SrSky sky = sr_sky_at(t, g->pall_removed);
  int district = sr_city_district_at(c, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  const SrPalette *pal = sr_palette(district >= 0 ? c->districts[district].type : SR_D_RESIDENTIAL);
  f->fog[0] = pal->fog.r / 255.0f;
  f->fog[1] = pal->fog.g / 255.0f;
  f->fog[2] = pal->fog.b / 255.0f;
  if (sky.daylight) {
    f->fog[0] = sky.r;
    f->fog[1] = sky.g;
    f->fog[2] = sky.b;
  }
  f->sky[0] = sky.r * 0.5f + f->fog[0] * 0.5f;
  f->sky[1] = sky.g * 0.5f + f->fog[1] * 0.5f;
  f->sky[2] = sky.b * 0.5f + f->fog[2] * 0.5f;
  f->ambient[0] = pal->ambient.r / 255.0f * sky.ambient;
  f->ambient[1] = pal->ambient.g / 255.0f * sky.ambient;
  f->ambient[2] = pal->ambient.b / 255.0f * sky.ambient;
  f->daylight = sky.daylight;
  f->fog_end = SR_MIN(w.visibility, (float)(s->view_chunks + 1) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f);
  f->fog_start = f->fog_end * 0.25f;
  f->wetness = w.wetness;
  f->dither = g->settings.dither;
  f->quantize = g->settings.quantize;
  f->vertex_snap = g->settings.vertex_snap;
  f->affine = g->settings.affine;
  f->crt = g->settings.crt;
  f->bloom = g->settings.bloom;
  if (g->site.active) {
    build_site_frame(s, g, f);
    return;
  }
  if (s->light_grid.city_seed != c->seed || s->light_grid.blocks != c->grid * c->grid || !s->light_grid.lights)
    sr_light_grid_build(&s->light_grid, c);
  f->nlights = sr_light_grid_query(&s->light_grid, c, p->pos, 120.0f, f->lights, SR_MAX_FRAME_LIGHTS);
  /* city chunks around the camera, nearest first, limited build budget */
  int ccx = (int)(p->pos.x * 100.0f) / (SR_BLOCK_CM * SR_CHUNK_BLOCKS);
  int ccy = (int)(p->pos.z * 100.0f) / (SR_BLOCK_CM * SR_CHUNK_BLOCKS);
  int R = s->view_chunks;
  for (int ring = 0; ring <= R; ring++) {
    for (int dy = -ring; dy <= ring; dy++) {
      for (int dx = -ring; dx <= ring; dx++) {
        if (SR_MAX(abs(dx), abs(dy)) != ring) continue;
        SrChunk *ch = sr_chunk_get(&s->chunks, c, &g->deltas, ccx + dx, ccy + dy, 2);
        if (!ch) continue;
        float mx = ((float)(ccx + dx) + 0.5f) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f;
        float mz = ((float)(ccy + dy) + 0.5f) * SR_CHUNK_BLOCKS * SR_BLOCK_CM / 100.0f;
        add_draw(f, &ch->mesh, SR_PASS_WORLD, sr_v3(mx, 2, mz));
      }
    }
  }
  if (g->thermal) { /* thermal vision draws every body as a detailed heat signature */
    sr_build_characters(&g->crowd, p->pos, f->fog_end, &s->characters);
    sr_mesh_thermal(&s->characters, 0);
  } else {
    if (s->inst_body.nv == 0) sr_build_instance_body(&s->inst_body);
    f->inst_count =
        sr_build_crowd_lod(&g->crowd, p->pos, SR_CROWD_NEAR_M, f->fog_end, &s->characters, s->inst, SR_MAX_AGENTS);
    f->inst_mesh = &s->inst_body;
    f->inst = s->inst;
    sr_build_vehicles(&g->crowd, p->pos, f->fog_end, &s->characters); /* vehicles join the near mesh */
  }
  add_draw(f, &s->characters, g->thermal ? SR_PASS_XRAY : SR_PASS_WORLD, p->pos);
  if (w.rain > 0.05f) {
    sr_build_rain(p->pos, w.rain, s->frame_no, c, &s->rain);
    add_draw(f, &s->rain, SR_PASS_WORLD, p->pos);
  }
  if (!s->skyline_built) {
    sr_build_skyline(c, &s->skyline);
    s->skyline_built = true;
  }
  add_draw(f, &s->skyline, SR_PASS_NOFOG, p->pos);
  sr_build_landmarks(c, (float)(g->steps) / 60.0f, &s->landmarks);
  add_draw(f, &s->landmarks, SR_PASS_NOFOG, p->pos);
}

float sr_fog_factor(float z, float start, float end, float emissive) {
  float f = (z - start) / (end - start);
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  return f * (1.0f - 0.7f * emissive);
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
