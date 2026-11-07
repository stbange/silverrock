/* Street dressing: the props of every block, placed deterministically from the seed in bands along
 * each pavement (kerb band, clear walking lane, building band), in alleys and on the road. */
#include "world/sr_dressing.h"

#include "core/sr_math.h"
#include "core/sr_rng.h"

#include <math.h>
#include <string.h>

static const SrStreetPropInfo k_props[SR_SP_COUNT] = {
    {"lamp", 0.3f, 0.3f, 6.5f, SR_SPF_LIT},
    {"traffic light", 0.3f, 0.3f, 4.2f, SR_SPF_LIT},
    {"bollard", 0.3f, 0.3f, 0.9f, 0},
    {"hydrant", 0.4f, 0.4f, 0.8f, 0},
    {"bin", 0.6f, 0.6f, 1.0f, 0},
    {"meter", 0.25f, 0.25f, 1.3f, 0},
    {"tree", 0.5f, 0.5f, 4.5f, 0},
    {"planter", 1.2f, 1.2f, 0.7f, SR_SPF_SOLID},
    {"news box", 0.6f, 0.5f, 1.1f, 0},
    {"parked car", 1.9f, 4.4f, 1.4f, SR_SPF_SOLID},
    {"vending machine", 1.0f, 0.8f, 2.0f, SR_SPF_SOLID | SR_SPF_LIT},
    {"bench", 1.8f, 0.6f, 0.9f, 0},
    {"air conditioner", 0.9f, 0.6f, 0.7f, 0},
    {"stall", 2.6f, 1.8f, 2.6f, SR_SPF_SOLID | SR_SPF_LIT | SR_SPF_STEAM},
    {"dumpster", 2.0f, 1.2f, 1.4f, SR_SPF_SOLID},
    {"crates", 1.2f, 1.2f, 1.1f, SR_SPF_SOLID},
    {"trash bags", 1.0f, 0.8f, 0.6f, 0},
    {"booth", 1.1f, 1.1f, 2.4f, SR_SPF_SOLID | SR_SPF_LIT},
    {"manhole", 0.9f, 0.9f, 0.02f, 0},
    {"drain", 0.9f, 0.4f, 0.02f, 0},
    {"vent", 0.8f, 0.8f, 0.4f, SR_SPF_STEAM},
    {"puddle", 2.4f, 1.6f, 0.01f, 0},
    {"barrel", 0.7f, 0.7f, 1.35f, SR_SPF_SOLID | SR_SPF_LIT},
    {"barrier", 2.4f, 0.5f, 1.0f, SR_SPF_SOLID},
    {"fence", 4.0f, 0.1f, 2.4f, SR_SPF_SOLID},
    {"container", 2.5f, 6.1f, 2.6f, SR_SPF_SOLID},
    {"rubble", 2.0f, 2.0f, 0.8f, 0},
    {"burned car", 1.9f, 4.4f, 1.3f, SR_SPF_SOLID},
    {"shrine", 1.0f, 0.8f, 1.6f, SR_SPF_LIT},
    {"flag", 0.2f, 0.2f, 7.0f, 0},
    {"holo-tree", 0.8f, 0.8f, 4.0f, SR_SPF_LIT},
    {"fire escape", 3.0f, 1.2f, 0.0f, 0},
};

const SrStreetPropInfo *sr_street_prop_info(int kind) { return &k_props[(unsigned)kind < SR_SP_COUNT ? kind : 0]; }

/* Per district: chance (percent) that a kerb slot and a building slot get a prop, and the weights
 * of the kinds for each band. */
typedef struct Kit {
  int8_t kerb_pct, bldg_pct, car_pct, puddles;
  int8_t kerb[7]; /* hydrant bin meter tree planter newsbox bollard */
  int8_t bldg[7]; /* vending bench stall dumpster crates trash booth */
  int8_t special; /* SrStreetPropKind of the district's signature prop, or -1 */
} Kit;

static const int8_t k_kerb_kinds[7] = {SR_SP_HYDRANT, SR_SP_BIN,     SR_SP_METER,  SR_SP_TREE,
                                       SR_SP_PLANTER, SR_SP_NEWSBOX, SR_SP_BOLLARD};
static const int8_t k_bldg_kinds[7] = {SR_SP_VENDING, SR_SP_BENCH, SR_SP_STALL, SR_SP_DUMPSTER,
                                       SR_SP_CRATES,  SR_SP_TRASH, SR_SP_BOOTH};

static const Kit k_kits[SR_D_COUNT] = {
    /* spire: clean planters and trees, holo-trees, few cars */
    {30, 25, 25, 2, {2, 2, 1, 5, 6, 1, 4}, {4, 4, 0, 0, 0, 0, 2}, SR_SP_HOLOTREE},
    /* civic: barriers and flags, benches */
    {30, 25, 35, 3, {3, 3, 2, 3, 2, 2, 5}, {2, 5, 0, 1, 0, 1, 2}, SR_SP_FLAG},
    /* commercial: vending rows, stalls, news */
    {35, 45, 45, 4, {3, 4, 3, 1, 1, 4, 1}, {6, 2, 4, 2, 2, 2, 2}, SR_SP_BOOTH},
    /* entertainment: vending, stalls, bins */
    {35, 45, 40, 5, {2, 5, 2, 1, 1, 3, 2}, {6, 1, 5, 2, 2, 3, 2}, SR_SP_VENDING},
    /* residential: bins, trees, trash, AC */
    {32, 35, 50, 5, {3, 5, 2, 4, 1, 2, 1}, {2, 3, 1, 3, 2, 5, 1}, SR_SP_TRASH},
    /* old quarter: stalls with steam, shrines, crates */
    {30, 50, 25, 6, {2, 3, 0, 2, 2, 1, 1}, {2, 1, 7, 1, 4, 3, 1}, SR_SP_SHRINE},
    /* industrial: dumpsters, crates, barriers, fences */
    {28, 35, 30, 6, {4, 3, 0, 0, 0, 0, 4}, {1, 0, 0, 5, 5, 3, 0}, SR_SP_FENCE},
    /* docks: containers, crates, bollards */
    {28, 35, 20, 6, {2, 2, 0, 0, 0, 0, 6}, {1, 0, 0, 3, 6, 2, 0}, SR_SP_CONTAINER},
    /* campus: trees, planters, holo-trees, benches */
    {32, 25, 25, 2, {1, 2, 1, 6, 5, 1, 2}, {3, 5, 0, 0, 0, 0, 1}, SR_SP_HOLOTREE},
    /* undercity: trash, crates, barrels */
    {30, 45, 20, 8, {1, 4, 0, 0, 0, 1, 1}, {1, 0, 3, 4, 4, 7, 1}, SR_SP_BARREL},
    /* barrens: rubble, burned cars, little else */
    {12, 20, 0, 7, {1, 2, 0, 0, 0, 0, 1}, {0, 0, 0, 2, 3, 5, 0}, SR_SP_RUBBLE},
};

static uint64_t hash(const SrCity *c, int block, uint32_t salt, uint32_t i) {
  return sr_mix64(c->seed ^ ((uint64_t)(unsigned)block * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)salt << 32) ^ i);
}

static int pick(const int8_t *w, int n, uint64_t h) {
  int total = 0;
  for (int i = 0; i < n; i++) total += w[i];
  if (total <= 0) return 0;
  int r = (int)(h % (uint64_t)total);
  for (int i = 0; i < n; i++) {
    r -= w[i];
    if (r < 0) return i;
  }
  return n - 1;
}

static void push(SrStreetProp *out, int *n, int max, int kind, int band, float x, float z, float yaw, uint64_t h) {
  if (*n >= max) return;
  SrStreetProp *p = &out[(*n)++];
  p->kind = (uint8_t)kind;
  p->variant = (uint8_t)(h >> 40 & 3u);
  p->flags = k_props[kind].flags;
  p->band = (uint8_t)band;
  p->x = x;
  p->z = z;
  p->yaw = yaw;
}

/* Kerb geometry: the lot spans [lx0, lx1] x [lz0, lz1]; kerb k runs along one side, with `in` the
 * unit vector from the kerb into the lot and `along` the direction of travel. */
typedef struct Kerb {
  float x0, z0; /* the start corner on the kerb line */
  float ax, az; /* along */
  float nx, nz; /* into the lot */
  float len, pavement;
} Kerb;

static void block_kerbs(const SrCity *c, int block, Kerb k[4]) {
  int32_t ox, oy;
  sr_city_block_origin(block, c->grid, &ox, &oy);
  float st = (float)SR_STREET_CM / 100.0f, s = (float)SR_BLOCK_CM / 100.0f;
  float lx0 = (float)ox / 100.0f + st, lz0 = (float)oy / 100.0f + st;
  float lx1 = (float)ox / 100.0f + s, lz1 = (float)oy / 100.0f + s;
  float pav[4];
  sr_block_pavements(c, block, pav);
  k[0] = (Kerb){lx0, lz0, 0, 1, 1, 0, lz1 - lz0, pav[0]};  /* west: runs south, lot to the east */
  k[1] = (Kerb){lx0, lz0, 1, 0, 0, 1, lx1 - lx0, pav[1]};  /* north: runs east, lot to the south */
  k[2] = (Kerb){lx1, lz0, 0, 1, -1, 0, lz1 - lz0, pav[2]}; /* east */
  k[3] = (Kerb){lx0, lz1, 1, 0, 0, -1, lx1 - lx0, pav[3]}; /* south */
}

void sr_block_pavements(const SrCity *c, int block, float out[4]) {
  int32_t ox, oy;
  sr_city_block_origin(block, c->grid, &ox, &oy);
  int32_t lx0 = ox + SR_STREET_CM, lz0 = oy + SR_STREET_CM, lx1 = ox + SR_BLOCK_CM, lz1 = oy + SR_BLOCK_CM;
  for (int k = 0; k < 4; k++) out[k] = (float)(SR_BLOCK_CM - SR_STREET_CM) / 100.0f;
  const SrBlock *b = &c->blocks[block];
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    out[0] = SR_MIN(out[0], (float)(bd->x0 - lx0) / 100.0f);
    out[1] = SR_MIN(out[1], (float)(bd->y0 - lz0) / 100.0f);
    out[2] = SR_MIN(out[2], (float)(lx1 - bd->x1) / 100.0f);
    out[3] = SR_MIN(out[3], (float)(lz1 - bd->y1) / 100.0f);
  }
}

#define KERB_BAND 1.2f
#define BLDG_BAND 1.0f

float sr_walk_lane(float pavement) {
  float end = pavement >= KERB_BAND + BLDG_BAND + 1.0f ? pavement - BLDG_BAND : pavement;
  return (KERB_BAND + end) * 0.5f;
}

/* Is the point in front of a door of an enterable building (3 m wide, 3 m deep)? */
static bool at_door(const SrCity *c, const SrBlock *b, float x, float z) {
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    if (bd->poi == SR_POI_NONE && !(bd->flags & SR_BF_ENTERABLE)) continue;
    float cx = (float)(bd->x0 + bd->x1) / 200.0f, fz = (float)bd->y1 / 100.0f;
    if (fabsf(x - cx) < 3.0f && z > fz - 0.5f && z < fz + 3.5f) return true;
  }
  return false;
}

/* Is the point inside (or within 0.3 m of) a building of the block? */
static bool in_building(const SrCity *c, const SrBlock *b, float x, float z, float pad) {
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    if (x > (float)bd->x0 / 100.0f - pad && x < (float)bd->x1 / 100.0f + pad && z > (float)bd->y0 / 100.0f - pad &&
        z < (float)bd->y1 / 100.0f + pad)
      return true;
  }
  return false;
}

int sr_dressing_block(const SrCity *c, int block, SrStreetProp *out, int max) {
  int n = 0;
  if (block < 0 || block >= c->grid * c->grid) return 0;
  const SrBlock *b = &c->blocks[block];
  if (b->water) return 0;
  SrDistrictType t = c->districts[b->district].type;
  const Kit *kit = &k_kits[t];
  Kerb kerbs[4];
  block_kerbs(c, block, kerbs);
  int32_t ox, oy;
  sr_city_block_origin(block, c->grid, &ox, &oy);
  float st = (float)SR_STREET_CM / 100.0f;
  for (int k = 0; k < 4; k++) {
    const Kerb *e = &kerbs[k];
    float face = atan2f(-e->nx, -e->nz); /* props face the street */
    /* lamps every 24 m in the kerb band */
    for (int li = 0; 4.0f + 24.0f * (float)li < e->len; li++) {
      float a = 4.0f + 24.0f * (float)li;
      float x = e->x0 + e->ax * a + e->nx * 0.5f, z = e->z0 + e->az * a + e->nz * 0.5f;
      push(out, &n, max, SR_SP_LAMP, SR_BAND_KERB, x, z, face, hash(c, block, 1, (uint32_t)(k * 8 + li)));
    }
    /* traffic light at the corner where the kerb starts */
    push(out, &n, max, SR_SP_TRAFFIC_LIGHT, SR_BAND_KERB, e->x0 + e->nx * 0.6f + e->ax * 1.2f,
         e->z0 + e->nz * 0.6f + e->az * 1.2f, face, hash(c, block, 2, (uint32_t)k));
    /* kerb band slots every 4 m (lamp slots and corners skipped) */
    for (int slot = 2; slot * 4.0f < e->len - 4.0f; slot++) {
      float a = (float)slot * 4.0f;
      if (fmodf(a - 4.0f, 24.0f) < 1.0f) continue;
      uint64_t h = hash(c, block, 3, (uint32_t)(k * 64 + slot));
      float x = e->x0 + e->ax * a + e->nx * 0.6f, z = e->z0 + e->az * a + e->nz * 0.6f;
      if ((int)(h % 100u) < kit->kerb_pct) {
        int kind = (h >> 20) % 9u == 0 && kit->special >= 0 ? kit->special : k_kerb_kinds[pick(kit->kerb, 7, h >> 8)];
        if (kind == SR_SP_CONTAINER || kind == SR_SP_STALL || kind == SR_SP_VENDING || kind == SR_SP_BOOTH ||
            kind == SR_SP_FENCE)
          kind = SR_SP_BOLLARD; /* big signature props stand in the building band, not at the kerb */
        if (!at_door(c, b, x, z)) push(out, &n, max, kind, SR_BAND_KERB, x, z, face, h);
      }
      /* parked cars in the road's parking lane */
      if ((int)((h >> 32) % 100u) < kit->car_pct && slot % 2 == 0 && slot * 4.0f + 6.0f < e->len) {
        float cx = e->x0 + e->ax * (a + 2.0f) - e->nx * 1.3f, cz = e->z0 + e->az * (a + 2.0f) - e->nz * 1.3f;
        int kind = t == SR_D_BARRENS ? SR_SP_BURNED_CAR : SR_SP_CAR;
        push(out, &n, max, kind, SR_BAND_ROAD, cx, cz, atan2f(e->ax, e->az), h >> 3);
      }
      /* drains along the kerb in the road, every 20 m */
      if (slot % 5 == 0)
        push(out, &n, max, SR_SP_DRAIN, SR_BAND_ROAD, e->x0 + e->ax * a - e->nx * 0.35f,
             e->z0 + e->az * a - e->nz * 0.35f, atan2f(e->ax, e->az), h >> 5);
    }
    /* building band: 1 m in front of the nearest face, only where the pavement is wide enough */
    if (e->pavement >= KERB_BAND + BLDG_BAND + 1.0f) {
      float off = e->pavement - BLDG_BAND * 0.5f;
      for (int slot = 1; slot * 5.0f < e->len - 3.0f; slot++) {
        float a = (float)slot * 5.0f;
        uint64_t h = hash(c, block, 4, (uint32_t)(k * 64 + slot));
        if ((int)(h % 100u) >= kit->bldg_pct) continue;
        float x = e->x0 + e->ax * a + e->nx * off, z = e->z0 + e->az * a + e->nz * off;
        if (at_door(c, b, x, z) || in_building(c, b, x, z, 0.3f)) continue;
        /* only in front of a wall: something must stand within 1.6 m behind the slot */
        float bx = x + e->nx * (BLDG_BAND * 0.5f + 1.0f), bz = z + e->nz * (BLDG_BAND * 0.5f + 1.0f);
        if (!in_building(c, b, bx, bz, 0.0f)) continue;
        int kind = (h >> 20) % 7u == 0 && kit->special >= 0 ? kit->special : k_bldg_kinds[pick(kit->bldg, 7, h >> 8)];
        if (kind == SR_SP_CONTAINER && e->pavement < 7.0f) kind = SR_SP_CRATES;
        push(out, &n, max, kind, SR_BAND_BUILDING, x, z, face, h);
      }
    }
  }
  /* alleys: gaps of 4 m or more between neighbouring buildings */
  for (int i = 0; i < b->building_count; i++) {
    for (int j = 0; j < b->building_count; j++) {
      const SrBuilding *p = &c->buildings[b->first_building + i], *q = &c->buildings[b->first_building + j];
      float gap = (float)(q->x0 - p->x1) / 100.0f; /* gap along x between p (west) and q (east) */
      bool xgap = gap >= 4.0f && q->y0 < p->y1 && p->y0 < q->y1;
      float gapz = (float)(q->y0 - p->y1) / 100.0f;
      bool zgap = gapz >= 4.0f && q->x0 < p->x1 && p->x0 < q->x1;
      if (!xgap && !zgap) continue;
      float lo, hi, mid;
      if (xgap) {
        mid = (float)(p->x1 + q->x0) / 200.0f;
        lo = (float)SR_MAX(p->y0, q->y0) / 100.0f;
        hi = (float)SR_MIN(p->y1, q->y1) / 100.0f;
      } else {
        mid = (float)(p->y1 + q->y0) / 200.0f;
        lo = (float)SR_MAX(p->x0, q->x0) / 100.0f;
        hi = (float)SR_MIN(p->x1, q->x1) / 100.0f;
      }
      float width = xgap ? gap : gapz, side = width * 0.5f - 0.7f;
      int clutter = (t == SR_D_UNDERCITY || t == SR_D_OLDQUARTER) ? 6 : 4;
      for (int s = 0; s < clutter; s++) {
        uint64_t h = hash(c, block, 5, (uint32_t)(i * 97 + j * 13 + s));
        float a = lo + 2.0f + (float)(h % 1000u) / 1000.0f * SR_MAX(0.0f, hi - lo - 4.0f);
        float o = (s % 2 ? side : -side);
        static const int8_t kinds[6] = {SR_SP_DUMPSTER, SR_SP_TRASH, SR_SP_CRATES,
                                        SR_SP_VENT,     SR_SP_TRASH, SR_SP_PUDDLE};
        int kind = kinds[s % 6];
        if (kind == SR_SP_PUDDLE) o = 0; /* puddles lie in the middle of the alley */
        float x = xgap ? mid + o : a, z = xgap ? a : mid + o;
        push(out, &n, max, kind, SR_BAND_ALLEY, x, z, xgap ? (o > 0 ? -SR_PI / 2 : SR_PI / 2) : (o > 0 ? SR_PI : 0), h);
        out[n - 1].flags |= SR_SPF_ALLEY;
      }
      /* a weak light over the alley, and fire escapes on both walls */
      float la = (lo + hi) * 0.5f;
      push(out, &n, max, SR_SP_LAMP, SR_BAND_ALLEY, xgap ? mid + side : la, xgap ? la : mid + side, 0,
           hash(c, block, 6, (uint32_t)(i * 7 + j)));
      out[n - 1].variant = 3; /* wall lamp */
      out[n - 1].flags |= SR_SPF_ALLEY;
      push(out, &n, max, SR_SP_FIRE_ESCAPE, SR_BAND_ALLEY, xgap ? (float)p->x1 / 100.0f : la - 6.0f,
           xgap ? la - 6.0f : (float)p->y1 / 100.0f, xgap ? SR_PI / 2 : 0, hash(c, block, 7, (uint32_t)(i * 7 + j)));
      out[n - 1].flags |= SR_SPF_ALLEY;
      push(out, &n, max, SR_SP_FIRE_ESCAPE, SR_BAND_ALLEY, xgap ? (float)q->x0 / 100.0f : la + 6.0f,
           xgap ? la + 6.0f : (float)q->y0 / 100.0f, xgap ? -SR_PI / 2 : SR_PI,
           hash(c, block, 8, (uint32_t)(i * 7 + j)));
      out[n - 1].flags |= SR_SPF_ALLEY;
    }
  }
  /* the road: manholes on both of the block's street strips, some of them steaming */
  float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f;
  for (int s = 0; s < 2; s++) {
    uint64_t h = hash(c, block, 9, (uint32_t)s);
    float start = st + 8.0f + (float)(h % 2000u) / 100.0f, pitch = 30.0f + (float)(h >> 16 & 15u);
    for (int mi = 0; start + pitch * (float)mi < (float)SR_BLOCK_CM / 100.0f - 4.0f; mi++) {
      float a = start + pitch * (float)mi;
      float lane = (h >> 24 & 1u) ? st * 0.28f : st * 0.72f;
      float x = s == 0 ? x0 + lane : x0 + a, z = s == 0 ? z0 + a : z0 + lane;
      push(out, &n, max, SR_SP_MANHOLE, SR_BAND_ROAD, x, z, 0, h ^ (uint64_t)(a * 13.0f));
      if (((h ^ (uint64_t)a) >> 7) % 3u == 0) out[n - 1].flags |= SR_SPF_STEAM;
    }
  }
  /* puddles, more in poor districts; barrels in empty lots of the Barrens and Undercity */
  for (int i = 0; i < kit->puddles; i++) {
    uint64_t h = hash(c, block, 10, (uint32_t)i);
    float x = x0 + (float)(h % 9600u) / 100.0f, z = z0 + (float)((h >> 16) % 9600u) / 100.0f;
    if (in_building(c, b, x, z, 0.5f)) continue;
    push(out, &n, max, SR_SP_PUDDLE, SR_BAND_ROAD, x, z, (float)(h >> 32 & 255u) / 40.0f, h);
  }
  return n;
}

bool sr_dressing_blocks(const SrCity *c, float x, float z) {
  int blk = sr_city_block_at(c, (int32_t)(x * 100.0f), (int32_t)(z * 100.0f));
  if (blk < 0) return false;
  SrStreetProp props[SR_MAX_BLOCK_PROPS];
  int n = sr_dressing_block(c, blk, props, SR_MAX_BLOCK_PROPS);
  for (int i = 0; i < n; i++) {
    if (!(props[i].flags & SR_SPF_SOLID)) continue;
    const SrStreetPropInfo *in = &k_props[props[i].kind];
    float dx = x - props[i].x, dz = z - props[i].z, cy = cosf(props[i].yaw), sy = sinf(props[i].yaw);
    float lx = dx * cy - dz * sy, lz = dx * sy + dz * cy; /* into the prop's frame */
    if (fabsf(lx) <= in->w * 0.5f && fabsf(lz) <= in->d * 0.5f) return true;
  }
  return false;
}

uint64_t sr_dressing_digest(const SrCity *c) {
  uint64_t h = 0;
  SrStreetProp props[SR_MAX_BLOCK_PROPS];
  for (int blk = 0; blk < c->grid * c->grid; blk++) {
    int n = sr_dressing_block(c, blk, props, SR_MAX_BLOCK_PROPS);
    for (int i = 0; i < n; i++) {
      int32_t q[4] = {props[i].kind | props[i].variant << 8 | props[i].flags << 16, (int32_t)(props[i].x * 100.0f),
                      (int32_t)(props[i].z * 100.0f), (int32_t)(props[i].yaw * 1000.0f)};
      h = sr_hash_bytes(h, q, sizeof q);
    }
  }
  return h;
}

/* ---- facades ------------------------------------------------------------------------------------ */

/* Window module weights per district type. */
static const int8_t k_win_weights[SR_D_COUNT][SR_WIN_COUNT] = {
    /* office balcony hive curtain brick shuttered boarded clerestory */
    {3, 0, 0, 6, 0, 0, 0, 0}, /* spire */
    {3, 0, 0, 1, 3, 0, 0, 0}, /* civic */
    {3, 2, 1, 1, 1, 2, 0, 0}, /* commercial */
    {1, 2, 2, 0, 1, 2, 0, 0}, /* entertainment */
    {0, 4, 2, 0, 1, 2, 0, 0}, /* residential */
    {0, 1, 0, 0, 4, 3, 1, 0}, /* old quarter */
    {1, 0, 0, 0, 1, 0, 2, 4}, /* industrial */
    {0, 0, 0, 0, 0, 1, 3, 3}, /* docks */
    {3, 0, 0, 3, 0, 0, 0, 0}, /* campus */
    {0, 0, 2, 0, 0, 3, 3, 0}, /* undercity */
    {0, 0, 0, 0, 0, 1, 6, 0}, /* barrens */
};

/* Ground-floor weights per district type (for buildings that are not places). */
static const int8_t k_base_weights[SR_D_COUNT][SR_BASE_COUNT] = {
    /* shopfront shutter lobby dock stall posters */
    {2, 0, 6, 0, 0, 1}, /* spire */
    {1, 1, 5, 0, 0, 2}, /* civic */
    {6, 2, 1, 0, 2, 1}, /* commercial */
    {5, 3, 1, 0, 2, 2}, /* entertainment */
    {2, 3, 4, 0, 1, 2}, /* residential */
    {5, 2, 0, 0, 5, 1}, /* old quarter */
    {0, 2, 0, 6, 0, 2}, /* industrial */
    {0, 2, 0, 6, 0, 2}, /* docks */
    {1, 0, 6, 0, 0, 1}, /* campus */
    {2, 5, 0, 1, 3, 3}, /* undercity */
    {0, 4, 0, 1, 1, 5}, /* barrens */
};

static uint64_t bhash(const SrCity *c, const SrBuilding *b, uint32_t salt) {
  return sr_mix64(c->seed ^ ((uint64_t)b->id * 0xD6E8FEB86659FD93ull) ^ ((uint64_t)salt << 40));
}

static int base_for_poi(int poi) {
  switch (poi) {
    case SR_POI_CITY_HALL:
    case SR_POI_CORP_HQ:
    case SR_POI_CORP_FACILITY:
    case SR_POI_POLICE:
    case SR_POI_MEDIA:
    case SR_POI_HOME:
    case SR_POI_CLINIC:
    case SR_POI_CAPSULE: return SR_BASE_LOBBY;
    case SR_POI_GANG_HIDEOUT: return SR_BASE_SHUTTER;
    case SR_POI_FOOD: return SR_BASE_STALL;
    default: return SR_BASE_SHOPFRONT;
  }
}

static int use_for(int winmod, SrDistrictType t, uint64_t h) {
  switch (winmod) {
    case SR_WIN_OFFICE:
    case SR_WIN_CURTAIN:
      return (t == SR_D_RESIDENTIAL || t == SR_D_ENTERTAINMENT) && (h & 1u) ? SR_FUSE_HOME : SR_FUSE_OFFICE;
    case SR_WIN_CLERESTORY: return SR_FUSE_INDUSTRIAL;
    case SR_WIN_BOARDED: return (h % 4u) == 0 ? SR_FUSE_HOME : SR_FUSE_VACANT;
    default: return SR_FUSE_HOME;
  }
}

/* Faces with a neighbour closer than 6 m become blank walls. */
static uint8_t blank_faces(const SrCity *c, const SrBuilding *b) {
  const SrBlock *k = &c->blocks[b->block];
  uint8_t bits = 0;
  for (int i = 0; i < k->building_count; i++) {
    const SrBuilding *o = &c->buildings[k->first_building + i];
    if (o == b) continue;
    bool zover = o->y0 < b->y1 && b->y0 < o->y1, xover = o->x0 < b->x1 && b->x0 < o->x1;
    if (zover && b->x0 - o->x1 >= 0 && b->x0 - o->x1 < 600) bits |= 1u;
    if (zover && o->x0 - b->x1 >= 0 && o->x0 - b->x1 < 600) bits |= 2u;
    if (xover && b->y0 - o->y1 >= 0 && b->y0 - o->y1 < 600) bits |= 4u;
    if (xover && o->y0 - b->y1 >= 0 && o->y0 - b->y1 < 600) bits |= 8u;
  }
  return bits;
}

static void facade_raw(const SrCity *c, const SrBuilding *b, SrFacade *f, int bump) {
  SrDistrictType t = c->districts[c->blocks[b->block].district].type;
  uint64_t h = bhash(c, b, 1);
  memset(f, 0, sizeof *f);
  int w = pick(k_win_weights[t], SR_WIN_COUNT, h);
  for (int k = 0; k < bump; k++) { /* next allowed module, so neighbours differ */
    do { w = (w + 1) % SR_WIN_COUNT; } while (k_win_weights[t][w] == 0);
  }
  f->winmod = (uint8_t)w;
  f->base = (uint8_t)(b->poi != SR_POI_NONE ? base_for_poi(b->poi) : pick(k_base_weights[t], SR_BASE_COUNT, h >> 12));
  f->use = (uint8_t)use_for(w, t, h >> 20);
  f->seed = (uint8_t)(h >> 26 & 63u);
  float hgt = (float)b->height_cm / 100.0f;
  f->crown = (uint8_t)(hgt > 120.0f && (h >> 32) % 3u == 0    ? SR_CROWN_NEON
                       : w == SR_WIN_BRICK                    ? SR_CROWN_CORNICE
                       : t == SR_D_BARRENS || t == SR_D_DOCKS ? SR_CROWN_NONE
                                                              : SR_CROWN_PARAPET);
  f->floor_h = 3.2f + (float)(h >> 36 & 7u) * 0.14f;
  f->uv_u = (float)(h >> 40 & 15u) * 0.25f;
  f->uv_v = (float)(h >> 44 & 3u) * 0.25f;
  /* tint: a light base, warmed or cooled by the district, varied per building */
  static const uint8_t k_tint[SR_D_COUNT][3] = {{150, 165, 185}, {165, 165, 172}, {170, 160, 165}, {165, 150, 170},
                                                {150, 168, 168}, {180, 160, 140}, {170, 160, 145}, {150, 160, 172},
                                                {160, 175, 165}, {130, 145, 130}, {150, 140, 130}};
  for (int k = 0; k < 3; k++) f->tint[k] = (uint8_t)(k_tint[t][k] - 14 + (int)(h >> (48 + k * 5) & 31u));
  f->blank = blank_faces(c, b);
  f->shape = SR_SHAPE_BOX; /* shapes and roofs: sr_facade_for */
  f->roof = SR_ROOF_FLAT;
}

void sr_facade_for(const SrCity *c, int building, SrFacade *out) {
  const SrBuilding *b = &c->buildings[building];
  const SrBlock *k = &c->blocks[b->block];
  /* buildings earlier on the block come first: re-roll the window module until no earlier
   * building shares both module and ground floor */
  int bump = 0;
  for (; bump < SR_WIN_COUNT; bump++) {
    facade_raw(c, b, out, bump);
    bool clash = false;
    for (int i = 0; i < k->building_count && !clash; i++) {
      int o = k->first_building + i;
      if (o >= building) break;
      SrFacade f;
      sr_facade_for(c, o, &f);
      clash = f.winmod == out->winmod && f.base == out->base;
    }
    if (!clash) break;
  }
  if (bump == SR_WIN_COUNT) facade_raw(c, b, out, 0); /* a district with one module: the ground floor differs */
  if (bump == SR_WIN_COUNT) out->base = (uint8_t)((out->base + 1 + (b->id % 3u)) % SR_BASE_COUNT);
  out->shape = SR_SHAPE_BOX;
  out->roof = SR_ROOF_FLAT;
}

uint64_t sr_facade_digest(const SrCity *c) {
  uint64_t h = 0;
  for (int i = 0; i < c->building_count; i++) {
    SrFacade f;
    sr_facade_for(c, i, &f);
    uint8_t q[16] = {f.winmod,
                     f.base,
                     f.crown,
                     f.use,
                     f.shape,
                     f.roof,
                     f.blank,
                     f.seed,
                     f.tint[0],
                     f.tint[1],
                     f.tint[2],
                     f.steps,
                     (uint8_t)(f.floor_h * 50.0f),
                     (uint8_t)(f.uv_u * 4.0f),
                     (uint8_t)(f.uv_v * 4.0f),
                     0};
    h = sr_hash_bytes(h, q, sizeof q);
  }
  return h;
}
