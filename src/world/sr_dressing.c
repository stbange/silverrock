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
    {"vending row", 3.2f, 0.8f, 2.0f, SR_SPF_SOLID | SR_SPF_LIT},
    {"marquee", 3.0f, 0.8f, 3.2f, SR_SPF_LIT},
    {"kiosk", 1.8f, 0.9f, 2.4f, SR_SPF_SOLID | SR_SPF_LIT},
    {"tank", 6.0f, 6.0f, 8.0f, SR_SPF_SOLID},
    {"crane", 10.0f, 4.0f, 34.0f, 0},
    {"glowing tree", 1.0f, 1.0f, 4.5f, SR_SPF_LIT},
    {"shanty", 2.6f, 1.0f, 2.2f, SR_SPF_SOLID | SR_SPF_LIT},
    {"bulb", 0.3f, 0.3f, 0.3f, SR_SPF_LIT},
    {"pillar", 1.4f, 1.4f, 11.0f, SR_SPF_SOLID},
};

static const int8_t k_signature[SR_D_COUNT] = {SR_SP_HOLOTREE, SR_SP_FLAG,   SR_SP_VENDROW, SR_SP_MARQUEE,
                                               SR_SP_KIOSK,    SR_SP_SHRINE, SR_SP_FENCE,   SR_SP_CRANE,
                                               SR_SP_GLOWTREE, SR_SP_SHANTY, SR_SP_RUBBLE};

int sr_district_signature(SrDistrictType t) { return k_signature[(unsigned)t < SR_D_COUNT ? t : SR_D_RESIDENTIAL]; }

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
    {35, 45, 45, 4, {3, 4, 3, 1, 1, 4, 1}, {6, 2, 4, 2, 2, 2, 2}, SR_SP_VENDROW},
    /* entertainment: vending, stalls, bins */
    {35, 45, 40, 5, {2, 5, 2, 1, 1, 3, 2}, {6, 1, 5, 2, 2, 3, 2}, SR_SP_MARQUEE},
    /* residential: bins, trees, trash, AC */
    {32, 35, 50, 5, {3, 5, 2, 4, 1, 2, 1}, {2, 3, 1, 3, 2, 5, 1}, SR_SP_KIOSK},
    /* old quarter: stalls with steam, shrines, crates */
    {30, 50, 25, 6, {2, 3, 0, 2, 2, 1, 1}, {2, 1, 7, 1, 4, 3, 1}, SR_SP_SHRINE},
    /* industrial: dumpsters, crates, barriers, fences */
    {28, 35, 30, 6, {4, 3, 0, 0, 0, 0, 4}, {1, 0, 0, 5, 5, 3, 0}, SR_SP_FENCE},
    /* docks: containers, crates, bollards */
    {28, 35, 20, 6, {2, 2, 0, 0, 0, 0, 6}, {1, 0, 0, 3, 6, 2, 0}, SR_SP_CRANE},
    /* campus: trees, planters, holo-trees, benches */
    {32, 25, 25, 2, {1, 2, 1, 6, 5, 1, 2}, {3, 5, 0, 0, 0, 0, 1}, SR_SP_GLOWTREE},
    /* undercity: trash, crates, barrels */
    {30, 45, 20, 8, {1, 4, 0, 0, 0, 1, 1}, {1, 0, 3, 4, 4, 7, 1}, SR_SP_SHANTY},
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
        if (k_props[kind].w > 1.3f || k_props[kind].d > 1.3f || kind == SR_SP_VENDING || kind == SR_SP_BOOTH)
          kind = SR_SP_BOLLARD; /* big props never stand at the kerb */
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
        if (k_props[kind].d > BLDG_BAND + 0.3f && kind != SR_SP_STALL && kind != SR_SP_DUMPSTER)
          kind = SR_SP_CRATES; /* too deep for the building band: set pieces go on open lots */
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
  /* set pieces on open lot space, away from the walking lanes and the doors: the district's
   * signature, containers at the docks, tanks in industry */
  int sig = k_signature[t];
  bool has_sig = false;
  for (int i = 0; i < n; i++) has_sig = has_sig || out[i].kind == sig;
  int pieces = t == SR_D_DOCKS ? 4 : t == SR_D_INDUSTRIAL ? 2 : 1;
  float placed[8][3];
  int np = 0;
  for (int piece = 0; piece < pieces && n < max; piece++) {
    int kind = t == SR_D_DOCKS ? SR_SP_CONTAINER : t == SR_D_INDUSTRIAL ? SR_SP_TANK : sig;
    if (kind == sig && (has_sig || sig == SR_SP_CRANE)) continue;
    const SrStreetPropInfo *in = &k_props[kind];
    float size = SR_MAX(in->w, in->d) * 0.5f + 1.0f;
    for (int tries = 0; tries < 40; tries++) {
      uint64_t h = hash(c, block, 11, (uint32_t)(piece * 64 + tries));
      float x = kerbs[0].x0 + 1.0f + (float)(h % 7800u) / 100.0f,
            z = kerbs[0].z0 + 1.0f + (float)((h >> 16) % 7800u) / 100.0f;
      bool ok = !in_building(c, b, x, z, size + 0.5f) && !at_door(c, b, x, z);
      for (int k = 0; k < 4 && ok; k++) { /* beyond every walking lane */
        float d = (x - kerbs[k].x0) * kerbs[k].nx + (z - kerbs[k].z0) * kerbs[k].nz;
        ok = d > sr_walk_lane(kerbs[k].pavement) + 1.2f + size;
      }
      for (int q = 0; q < np && ok; q++)
        ok = (x - placed[q][0]) * (x - placed[q][0]) + (z - placed[q][1]) * (z - placed[q][1]) >
             (size + placed[q][2]) * (size + placed[q][2]);
      if (!ok) continue;
      push(out, &n, max, kind, SR_BAND_BUILDING, x, z, (float)(h >> 40 & 3u) * SR_PI / 2, h);
      placed[np][0] = x;
      placed[np][1] = z;
      placed[np++][2] = size;
      if (kind == sig) has_sig = true;
      break;
    }
  }
  if (!has_sig && sig == SR_SP_CRANE) { /* the crane straddles the widest pavement (it never blocks) */
    int k = 0;
    for (int q = 1; q < 4; q++)
      if (kerbs[q].pavement > kerbs[k].pavement) k = q;
    float a = kerbs[k].len * 0.5f;
    push(out, &n, max, SR_SP_CRANE, SR_BAND_KERB, kerbs[k].x0 + kerbs[k].ax * a + kerbs[k].nx * 1.0f,
         kerbs[k].z0 + kerbs[k].az * a + kerbs[k].nz * 1.0f, atan2f(kerbs[k].nx, kerbs[k].nz), hash(c, block, 12, 1));
    has_sig = true;
  }
  for (int k = 0; k < 4 && !has_sig && n < max; k++) { /* the first free spot of the building band */
    const Kerb *e = &kerbs[k];
    if (e->pavement < KERB_BAND + BLDG_BAND + 1.0f) continue;
    float off = e->pavement - BLDG_BAND * 0.5f;
    for (int slot = 1; slot * 2.5f < e->len - 3.0f && !has_sig; slot++) {
      float a = (float)slot * 2.5f, x = e->x0 + e->ax * a + e->nx * off, z = e->z0 + e->az * a + e->nz * off;
      float half = k_props[sig].w * 0.5f + 0.3f;
      bool ok = !at_door(c, b, x, z) && !in_building(c, b, x, z, 0.3f) &&
                in_building(c, b, x + e->nx * (BLDG_BAND * 0.5f + 1.0f), z + e->nz * (BLDG_BAND * 0.5f + 1.0f), 0.0f);
      for (int i = 0; i < n && ok; i++) /* clear of the props already there */
        if (out[i].band == SR_BAND_BUILDING)
          ok = (out[i].x - x) * (out[i].x - x) + (out[i].z - z) * (out[i].z - z) > (half + 0.8f) * (half + 0.8f);
      if (!ok) continue;
      push(out, &n, max, sig, SR_BAND_BUILDING, x, z, atan2f(-e->nx, -e->nz), hash(c, block, 12, (uint32_t)slot));
      has_sig = true;
    }
  }
  if (!has_sig && n < max) { /* last resort: at the west kerb (slim, so the walking lane stays clear) */
    push(out, &n, max, sig, SR_BAND_KERB, kerbs[0].x0 + 0.6f, kerbs[0].z0 + 10.0f, atan2f(-kerbs[0].nx, -kerbs[0].nz),
         hash(c, block, 12, 0));
  }
  /* under a highway: pillars along the street's middle, bulbs hanging from the deck, shanties */
  if (b->highway) {
    float st2 = (float)SR_STREET_CM / 200.0f;
    for (int k = 0; k < 4 && n < max; k++) {
      float px = (float)ox / 100.0f + 12.0f + 24.0f * (float)k, pz = (float)oy / 100.0f + st2;
      push(out, &n, max, SR_SP_PILLAR, SR_BAND_ROAD, px, pz, 0, hash(c, block, 13, (uint32_t)k));
      out[n - 1].variant = 0; /* a highway pillar: up to the deck */
      if (t == SR_D_UNDERCITY && (hash(c, block, 14, (uint32_t)k) & 1u))
        push(out, &n, max, SR_SP_SHANTY, SR_BAND_ROAD, px + 2.6f, pz, 0, hash(c, block, 15, (uint32_t)k));
    }
    for (int k = 0; k < 8 && n < max; k++)
      push(out, &n, max, SR_SP_BULB, SR_BAND_ROAD, (float)ox / 100.0f + 6.0f + 12.0f * (float)k,
           (float)oy / 100.0f + st2 + ((k & 1) ? 3.0f : -3.0f), 0, hash(c, block, 16, (uint32_t)k));
  }
  /* pillars of the elevated metro standing in this block's area */
  for (int e = 0; e < c->metro_edge_count && n < max; e++) {
    float pl[64][2];
    int npl = sr_metro_pillars(c, e, pl, 64);
    for (int k = 0; k < npl && n < max; k++)
      if (sr_city_block_at(c, (int32_t)(pl[k][0] * 100.0f), (int32_t)(pl[k][1] * 100.0f)) == block) {
        push(out, &n, max, SR_SP_PILLAR, SR_BAND_ROAD, pl[k][0], pl[k][1], 0,
             hash(c, block, 17, (uint32_t)(e * 64 + k)));
        out[n - 1].variant = 1; /* a metro pillar: up to the track */
      }
  }
  /* burning barrels in the empty lots of the Barrens and the Undercity */
  if ((t == SR_D_BARRENS || t == SR_D_UNDERCITY) && b->building_count < 3) {
    for (int k = 0; k < 2 - (b->building_count > 1); k++) {
      uint64_t h = sr_mix64(c->seed ^ ((uint64_t)block * 31u + (uint64_t)k));
      float x = (float)ox / 100.0f + 22.0f + (float)(h % 50), z = (float)oy / 100.0f + 22.0f + (float)((h >> 16) % 50);
      if (!in_building(c, b, x, z, 2.0f)) push(out, &n, max, SR_SP_BARREL, SR_BAND_BUILDING, x, z, 0, h);
    }
  }
  /* puddles, more in poor districts */
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
  /* shape grammar: everything rises from a ground floor that fills the footprint */
  uint64_t hs = bhash(c, b, 2);
  int r = (int)(hs % 100u);
  float wid = (float)SR_MIN(b->x1 - b->x0, b->y1 - b->y0) / 100.0f;
  f->shape = SR_SHAPE_BOX;
  f->steps = 0;
  switch (t) {
    case SR_D_SPIRE:
    case SR_D_CAMPUS:
      f->shape = (uint8_t)(r < 35   ? SR_SHAPE_PODIUM
                           : r < 70 ? SR_SHAPE_SETBACK
                           : r < 85 ? SR_SHAPE_CHAMFER
                                    : SR_SHAPE_BOX);
      break;
    case SR_D_CIVIC:
    case SR_D_COMMERCIAL:
    case SR_D_ENTERTAINMENT:
      if (hgt > 50.0f)
        f->shape = (uint8_t)(r < 40   ? SR_SHAPE_PODIUM
                             : r < 65 ? SR_SHAPE_SETBACK
                             : r < 80 ? SR_SHAPE_CHAMFER
                                      : SR_SHAPE_BOX);
      else
        f->shape = (uint8_t)(r < 30 ? SR_SHAPE_L : r < 50 ? SR_SHAPE_CHAMFER : SR_SHAPE_BOX);
      break;
    case SR_D_RESIDENTIAL:
      f->shape = (uint8_t)(r < 35                  ? SR_SHAPE_SLAB
                           : r < 60                ? SR_SHAPE_L
                           : hgt > 80.0f && r < 80 ? SR_SHAPE_SETBACK
                                                   : SR_SHAPE_BOX);
      break;
    case SR_D_OLDQUARTER: f->shape = SR_SHAPE_ROWS; break;
    default: f->shape = (uint8_t)(r < 20 && wid > 20.0f ? SR_SHAPE_L : SR_SHAPE_BOX); break;
  }
  if (b->poi == SR_POI_CORP_HQ || b->poi == SR_POI_CITY_HALL) f->shape = SR_SHAPE_BOX; /* landmarks have their own */
  if (f->shape == SR_SHAPE_PODIUM) f->steps = (uint8_t)(2 + (hs >> 8) % 4u);           /* podium floors */
  if (f->shape == SR_SHAPE_SETBACK) f->steps = (uint8_t)(1 + (hs >> 8) % 3u);          /* setbacks */
  if (f->shape == SR_SHAPE_ROWS) f->steps = (uint8_t)(2 + (hs >> 8) % 3u);             /* row houses */
  /* roof kits */
  int rr = (int)((hs >> 16) % 100u);
  switch (t) {
    case SR_D_SPIRE:
      f->roof = (uint8_t)(f->crown == SR_CROWN_NEON ? SR_ROOF_NEON_CROWN
                          : rr < 35 && wid > 18.0f  ? SR_ROOF_HELIPAD
                          : rr < 70                 ? SR_ROOF_ANTENNAS
                                                    : SR_ROOF_HVAC);
      break;
    case SR_D_CAMPUS:
      f->roof = (uint8_t)(rr < 40 && wid > 18.0f ? SR_ROOF_HELIPAD : rr < 75 ? SR_ROOF_HVAC : SR_ROOF_ANTENNAS);
      break;
    case SR_D_CIVIC:
    case SR_D_COMMERCIAL:
    case SR_D_ENTERTAINMENT:
      f->roof = (uint8_t)(f->crown == SR_CROWN_NEON ? SR_ROOF_NEON_CROWN
                          : rr < 35                 ? SR_ROOF_HVAC
                          : rr < 65                 ? SR_ROOF_TANKS
                                                    : SR_ROOF_ANTENNAS);
      break;
    case SR_D_RESIDENTIAL:
      f->roof = (uint8_t)(rr < 40 ? SR_ROOF_TANKS : rr < 75 ? SR_ROOF_DISHES : SR_ROOF_HVAC);
      break;
    case SR_D_OLDQUARTER: f->roof = SR_ROOF_PITCHED; break;
    case SR_D_INDUSTRIAL:
      f->roof = (uint8_t)(rr < 45 ? SR_ROOF_SAWTOOTH : rr < 75 ? SR_ROOF_STACK : SR_ROOF_HVAC);
      break;
    case SR_D_DOCKS: f->roof = (uint8_t)(rr < 55 ? SR_ROOF_CORRUGATED : rr < 75 ? SR_ROOF_STACK : SR_ROOF_FLAT); break;
    case SR_D_UNDERCITY:
      f->roof = (uint8_t)(rr < 40 ? SR_ROOF_CORRUGATED : rr < 70 ? SR_ROOF_DISHES : SR_ROOF_FLAT);
      break;
    default: f->roof = (uint8_t)(rr < 45 ? SR_ROOF_CORRUGATED : rr < 70 ? SR_ROOF_DISHES : SR_ROOF_FLAT); break;
  }
  if (f->roof == SR_ROOF_PITCHED || f->roof == SR_ROOF_SAWTOOTH || f->roof == SR_ROOF_CORRUGATED)
    f->crown = SR_CROWN_NONE; /* sloped roofs need no parapet */
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

/* ---- shapes ------------------------------------------------------------------------------------- */

static int vol(SrVolume *out, int n, float x0, float z0, float x1, float z1, float y0, float y1, uint8_t flags) {
  if (n >= SR_MAX_VOLUMES || y1 <= y0 + 0.01f || x1 <= x0 || z1 <= z0) return n;
  out[n] = (SrVolume){x0, z0, x1, z1, y0, y1, flags};
  return n + 1;
}

int sr_building_volumes(const SrCity *c, int building, const SrFacade *f, SrVolume *out) {
  const SrBuilding *b = &c->buildings[building];
  float x0 = (float)b->x0 / 100.0f, z0 = (float)b->y0 / 100.0f, x1 = (float)b->x1 / 100.0f, z1 = (float)b->y1 / 100.0f;
  float h = (float)b->height_cm / 100.0f, w = x1 - x0, d = z1 - z0, base = 4.5f;
  uint64_t hs = bhash(c, b, 3);
  if (h <= base + 1.5f || f->shape == SR_SHAPE_BOX)
    return vol(out, 0, x0, z0, x1, z1, 0.0f, h, SR_VOL_BASE | SR_VOL_ROOF | SR_VOL_CROWN);
  int n = vol(out, 0, x0, z0, x1, z1, 0.0f, base, SR_VOL_BASE | SR_VOL_CROWN);
  switch (f->shape) {
    case SR_SHAPE_PODIUM: {
      float top = SR_MIN(h - 6.0f, base + (float)f->steps * f->floor_h);
      n = vol(out, n, x0, z0, x1, z1, base, top, SR_VOL_CROWN);
      float i = 0.1f + 0.15f * (float)(hs % 100u) / 100.0f;
      n = vol(out, n, x0 + w * i, z0 + d * i, x1 - w * i, z1 - d * i, top, h, SR_VOL_ROOF | SR_VOL_CROWN);
      break;
    }
    case SR_SHAPE_SETBACK: {
      int tiers = f->steps + 1;
      float y = base, lx0 = x0, lz0 = z0, lx1 = x1, lz1 = z1;
      for (int t = 0; t < tiers; t++) {
        float frac = t == 0 ? 0.5f : 0.5f / (float)(tiers - 1);
        float top = t == tiers - 1 ? h : y + (h - base) * frac;
        n = vol(out, n, lx0, lz0, lx1, lz1, y, top, (uint8_t)(SR_VOL_CROWN | (t == tiers - 1 ? SR_VOL_ROOF : 0)));
        lx0 += w * 0.08f;
        lx1 -= w * 0.08f;
        lz0 += d * 0.08f;
        lz1 -= d * 0.08f;
        y = top;
      }
      break;
    }
    case SR_SHAPE_CHAMFER: {
      float k = SR_MIN(w, d) * 0.15f;
      n = vol(out, n, x0 + k, z0, x1 - k, z1, base, h, SR_VOL_ROOF | SR_VOL_CROWN);
      n = vol(out, n, x0, z0 + k, x1, z1 - k, base, h, 0);
      break;
    }
    case SR_SHAPE_L: {
      n = vol(out, n, x0, z1 - d * 0.55f, x1, z1, base, h, SR_VOL_ROOF | SR_VOL_CROWN);
      n = vol(out, n, x0, z0, x0 + w * 0.5f, z1 - d * 0.55f, base, SR_MIN(h, SR_MAX(base + 3.0f, h * 0.7f)),
              SR_VOL_CROWN);
      break;
    }
    case SR_SHAPE_SLAB:
      n = vol(out, n, x0, z0 + d * 0.2f, x1, z1 - d * 0.25f, base, h, SR_VOL_ROOF | SR_VOL_CROWN);
      break;
    case SR_SHAPE_ROWS: {
      int houses = SR_MAX(2, f->steps);
      for (int i = 0; i < houses; i++) {
        float hx0 = x0 + w * (float)i / (float)houses, hx1 = x0 + w * (float)(i + 1) / (float)houses;
        float hh = SR_MIN(h, SR_MAX(base + 2.5f, h * (0.6f + 0.4f * (float)((hs >> (i * 7)) % 100u) / 100.0f)));
        n = vol(out, n, hx0, z0, hx1, z1, base, hh, SR_VOL_PITCHED);
      }
      break;
    }
    default: n = vol(out, n, x0, z0, x1, z1, base, h, SR_VOL_ROOF | SR_VOL_CROWN); break;
  }
  return n;
}

/* ---- billboards ---------------------------------------------------------------------------------- */

int sr_billboard_slots(const SrCity *c, int block, SrBillboardSlot *out, int max) {
  if (block < 0 || block >= c->grid * c->grid || c->blocks[block].water) return 0;
  const SrBlock *b = &c->blocks[block];
  SrDistrictType t = c->districts[b->district].type;
  static const int8_t k_roof_pct[SR_D_COUNT] = {35, 25, 60, 70, 35, 30, 25, 20, 20, 30, 10};
  int n = 0;
  for (int i = 0; i < b->building_count && n < max; i++) {
    int bi = b->first_building + i;
    const SrBuilding *bd = &c->buildings[bi];
    if (bd->poi == SR_POI_CORP_HQ || bd->poi == SR_POI_CITY_HALL) continue;
    SrFacade f;
    sr_facade_for(c, bi, &f);
    uint64_t h = bhash(c, bd, 9);
    float x0 = (float)bd->x0 / 100.0f, x1 = (float)bd->x1 / 100.0f, z1 = (float)bd->y1 / 100.0f;
    float hgt = (float)bd->height_cm / 100.0f;
    bool flat = f.roof == SR_ROOF_FLAT || f.roof == SR_ROOF_HVAC || f.roof == SR_ROOF_TANKS || f.roof == SR_ROOF_DISHES;
    if (flat && hgt > 10.0f && hgt < 90.0f && f.shape == SR_SHAPE_BOX && (int)(h % 100u) < k_roof_pct[t]) {
      float w = SR_MIN(14.0f, (x1 - x0) * 0.7f);
      out[n++] = (SrBillboardSlot){(x0 + x1) * 0.5f, hgt + 1.5f,
                                   z1 - 1.0f,        w,
                                   w * 0.45f,        0.0f,
                                   SR_SLOT_ROOF,     (uint8_t)((h >> 8) % 100u < 30u),
                                   (int16_t)bi,      (uint32_t)(h >> 16)};
    }
    /* blank side walls facing a gap: a big painted advert above the alley */
    if ((f.blank & 1u) && n < max && hgt > 16.0f && ((h >> 24) & 1u))
      out[n++] = (SrBillboardSlot){x0 - 0.05f,
                                   8.0f,
                                   (float)(bd->y0 + bd->y1) / 200.0f,
                                   8.0f,
                                   5.0f,
                                   -SR_PI / 2,
                                   SR_SLOT_WALL,
                                   0,
                                   (int16_t)bi,
                                   (uint32_t)(h >> 28)};
    if ((f.blank & 2u) && n < max && hgt > 16.0f && ((h >> 25) & 1u))
      out[n++] = (SrBillboardSlot){x1 + 0.05f,
                                   8.0f,
                                   (float)(bd->y0 + bd->y1) / 200.0f,
                                   8.0f,
                                   5.0f,
                                   SR_PI / 2,
                                   SR_SLOT_WALL,
                                   0,
                                   (int16_t)bi,
                                   (uint32_t)(h >> 30)};
  }
  if (b->highway && n < max) { /* over the highway deck, facing the street below */
    int32_t ox, oy;
    sr_city_block_origin(block, c->grid, &ox, &oy);
    uint64_t h = sr_mix64(c->seed ^ ((uint64_t)block * 0xA24BAED4963EE407ull));
    out[n++] = (SrBillboardSlot){(float)ox / 100.0f + 48.0f,
                                 12.2f,
                                 (float)oy / 100.0f + (float)SR_STREET_CM / 100.0f - 2.0f,
                                 12.0f,
                                 4.0f,
                                 0.0f,
                                 SR_SLOT_HIGHWAY,
                                 0,
                                 -1,
                                 (uint32_t)(h >> 20)};
  }
  return n;
}

/* ---- vertical layers ---------------------------------------------------------------------------- */

static int tube_between(const SrCity *c, int i, int j, float y, SrSkywayTube *t) {
  const SrBuilding *a = &c->buildings[i], *b = &c->buildings[j];
  int32_t zo0 = SR_MAX(a->y0, b->y0), zo1 = SR_MIN(a->y1, b->y1), xo0 = SR_MAX(a->x0, b->x0),
          xo1 = SR_MIN(a->x1, b->x1);
  if (zo1 - zo0 >= 600 && (a->x1 <= b->x0 || b->x1 <= a->x0)) { /* facing along x */
    const SrBuilding *w = a->x1 <= b->x0 ? a : b, *e = w == a ? b : a;
    float zm = (float)(zo0 + zo1) / 200.0f;
    *t = (SrSkywayTube){(float)w->x1 / 100.0f, zm - 2.0f, (float)e->x0 / 100.0f, zm + 2.0f, y, (int16_t)i, (int16_t)j};
    return 1;
  }
  if (xo1 - xo0 >= 600 && (a->y1 <= b->y0 || b->y1 <= a->y0)) { /* facing along z */
    const SrBuilding *nn = a->y1 <= b->y0 ? a : b, *s = nn == a ? b : a;
    float xm = (float)(xo0 + xo1) / 200.0f;
    *t = (SrSkywayTube){xm - 2.0f, (float)nn->y1 / 100.0f, xm + 2.0f, (float)s->y0 / 100.0f, y, (int16_t)i, (int16_t)j};
    return 1;
  }
  return 0;
}

int sr_skyway_tubes(const SrCity *c, SrSkywayTube *out, int max) {
  int n = 0;
  for (int k = 0; k < c->skyway_count && n < max; k++) { /* the generator's skyways */
    int a = c->skyways[k].a, b = c->skyways[k].b;
    float y = SR_MAX(40.0f, (float)SR_MIN(c->buildings[a].height_cm, c->buildings[b].height_cm) / 100.0f * 0.5f);
    n += tube_between(c, a, b, y, &out[n]);
  }
  for (int i = 0; i < c->building_count && n < max; i++) {
    const SrBuilding *a = &c->buildings[i];
    if (a->height_cm < 10000 || c->districts[c->blocks[a->block].district].type != SR_D_SPIRE) continue;
    for (int j = i + 1; j < c->building_count && n < max; j++) {
      const SrBuilding *b = &c->buildings[j];
      if (b->height_cm < 10000 || c->districts[c->blocks[b->block].district].type != SR_D_SPIRE) continue;
      float ax = (float)(a->x0 + a->x1) / 200.0f, az = (float)(a->y0 + a->y1) / 200.0f;
      float bx = (float)(b->x0 + b->x1) / 200.0f, bz = (float)(b->y0 + b->y1) / 200.0f;
      float d = sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz));
      if (d < 40.0f || d > 90.0f) continue;
      float y = SR_MAX(60.0f, SR_MIN(150.0f, (float)SR_MIN(a->height_cm, b->height_cm) / 100.0f * 0.45f));
      n += tube_between(c, i, j, y, &out[n]);
    }
  }
  return n;
}

void sr_metro_route(const SrCity *c, int edge, float pts[3][2]) {
  const SrEdge *e = &c->metro_edges[edge];
  int32_t ax, ay, bx, by;
  sr_city_block_origin(c->stations[e->a].block, c->grid, &ax, &ay);
  sr_city_block_origin(c->stations[e->b].block, c->grid, &bx, &by);
  float st = (float)SR_STREET_CM / 200.0f; /* the middle of the street strips */
  pts[0][0] = (float)ax / 100.0f + st;
  pts[0][1] = (float)ay / 100.0f + st;
  pts[1][0] = (float)bx / 100.0f + st;
  pts[1][1] = pts[0][1];
  pts[2][0] = pts[1][0];
  pts[2][1] = (float)by / 100.0f + st;
}

int sr_metro_pillars(const SrCity *c, int edge, float out[][2], int max) {
  float p[3][2];
  sr_metro_route(c, edge, p);
  int n = 0;
  for (int s = 0; s < 2; s++) {
    float ax = p[s][0], az = p[s][1], bx = p[s + 1][0], bz = p[s + 1][1];
    float len = fabsf(ax - bx) + fabsf(az - bz);
    int pillars = (int)(len / 32.0f);
    for (int k = 1; k <= pillars && n < max; k++) {
      float t = (float)k / (float)(pillars + 1);
      out[n][0] = ax + (bx - ax) * t;
      out[n++][1] = az + (bz - az) * t;
    }
  }
  return n;
}
