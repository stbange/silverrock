/* City generation. Integer math only (see sr_world.h). Stages:
 * geography -> districts -> highways -> buildings -> landmarks & places -> metro ->
 * starter neighbourhood -> skyways -> names -> factions -> validation (re-roll on failure). */
#include "core/sr_fixed.h"
#include "core/sr_rng.h"
#include "world/sr_world.h"

#include <stdlib.h>
#include <string.h>

#define LOT_CM (SR_BLOCK_CM - SR_STREET_CM)

/* ---- small helpers ---------------------------------------------------------- */

int sr_city_block_index(const SrCity *c, int bx, int by) {
  if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) return -1;
  return by * c->grid + bx;
}

void sr_city_block_origin(int block, int grid, int32_t *x, int32_t *y) {
  *x = (block % grid) * SR_BLOCK_CM;
  *y = (block / grid) * SR_BLOCK_CM;
}

int sr_city_block_at(const SrCity *c, int32_t x, int32_t y) {
  if (x < 0 || y < 0) return -1;
  return sr_city_block_index(c, x / SR_BLOCK_CM, y / SR_BLOCK_CM);
}

void sr_building_center(const SrBuilding *b, int32_t *x, int32_t *y) {
  *x = (b->x0 + b->x1) / 2;
  *y = (b->y0 + b->y1) / 2;
}

int sr_city_district_at(const SrCity *c, int32_t x, int32_t y) {
  int b = sr_city_block_at(c, x, y);
  return b < 0 ? -1 : c->blocks[b].district;
}

static void block_center(const SrCity *c, int block, int32_t *x, int32_t *y) {
  sr_city_block_origin(block, c->grid, x, y);
  *x += LOT_CM / 2 + SR_STREET_CM;
  *y += LOT_CM / 2 + SR_STREET_CM;
}

const char *sr_district_type_name(SrDistrictType t) {
  static const char *const n[SR_D_COUNT] = {"Corporate Spire", "Civic Center", "Commercial",
                                            "Entertainment", "Residential", "Old Quarter",
                                            "Industrial", "Docks", "Research Campus",
                                            "Undercity", "Barrens"};
  return (unsigned)t < SR_D_COUNT ? n[t] : "?";
}

const char *sr_poi_name(SrPoi p) {
  static const char *const n[SR_POI_COUNT] = {
      "none", "food", "bar", "clinic", "ripperdoc", "gun shop", "electronics", "clothing",
      "fence", "capsule hotel", "matrix booth", "fixer", "police", "grocery", "uv booth",
      "city hall", "corp hq", "corp facility", "gang hideout", "media", "home"};
  return (unsigned)p < SR_POI_COUNT ? n[p] : "?";
}

/* BFS over land blocks from one block; fills dist (block steps), -1 unreachable. */
static void land_bfs(const SrCity *c, int start, int16_t *dist) {
  int n = c->grid * c->grid;
  for (int i = 0; i < n; i++) dist[i] = -1;
  if (start < 0 || c->blocks[start].water) return;
  int16_t *queue = (int16_t *)malloc(sizeof(int16_t) * (size_t)n);
  if (!queue) return; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  int head = 0, tail = 0;
  queue[tail++] = (int16_t)start;
  dist[start] = 0;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  while (head < tail) {
    int b = queue[head++];
    int bx = b % c->grid, by = b / c->grid;
    for (int k = 0; k < 4; k++) {
      int nb = sr_city_block_index(c, bx + dx[k], by + dy[k]);
      if (nb < 0 || c->blocks[nb].water || dist[nb] >= 0) continue;
      dist[nb] = (int16_t)(dist[b] + 1);
      queue[tail++] = (int16_t)nb;
    }
  }
  free(queue);
}

int sr_city_block_steps(const SrCity *c, int a, int b) {
  int16_t dist[SR_MAX_BLOCKS];
  land_bfs(c, a, dist);
  return (b < 0 || b >= c->grid * c->grid) ? -1 : dist[b];
}

int32_t sr_city_walk_dist(const SrCity *c, int ba, int bb) {
  const SrBuilding *a = &c->buildings[ba], *b = &c->buildings[bb];
  int32_t ax, ay, bx, by;
  sr_building_center(a, &ax, &ay);
  sr_building_center(b, &bx, &by);
  if (a->block == b->block) return sr_manhattan_i(ax, ay, bx, by);
  int steps = sr_city_block_steps(c, a->block, b->block);
  if (steps < 0) return -1;
  int32_t direct = sr_manhattan_i(ax, ay, bx, by);
  int32_t via = steps * SR_BLOCK_CM;
  return direct > via ? direct : via;
}

int sr_city_metro_hops(const SrCity *c, int sa, int sb) {
  int16_t dist[SR_MAX_STATIONS];
  int16_t queue[SR_MAX_STATIONS];
  for (int i = 0; i < c->station_count; i++) dist[i] = -1;
  if (sa < 0 || sb < 0 || sa >= c->station_count || sb >= c->station_count) return -1;
  int head = 0, tail = 0;
  queue[tail++] = (int16_t)sa;
  dist[sa] = 0;
  while (head < tail) {
    int s = queue[head++];
    for (int e = 0; e < c->metro_edge_count; e++) {
      int o = -1;
      if (c->metro_edges[e].a == s) o = c->metro_edges[e].b;
      if (c->metro_edges[e].b == s) o = c->metro_edges[e].a;
      if (o < 0 || dist[o] >= 0) continue;
      dist[o] = (int16_t)(dist[s] + 1);
      queue[tail++] = (int16_t)o;
    }
  }
  return dist[sb];
}

int sr_city_nearest_station(const SrCity *c, int block) {
  int16_t dist[SR_MAX_BLOCKS];
  land_bfs(c, block, dist);
  int best = -1, best_d = 1 << 30;
  for (int s = 0; s < c->station_count; s++) {
    int d = dist[c->stations[s].block];
    if (d >= 0 && d < best_d) {
      best_d = d;
      best = s;
    }
  }
  return best;
}

int sr_city_find_poi_near(const SrCity *c, SrPoi poi, int from, int32_t *dist_out) {
  int best = -1;
  int32_t best_d = INT32_MAX;
  int16_t dist[SR_MAX_BLOCKS];
  land_bfs(c, c->buildings[from].block, dist);
  int32_t fx, fy;
  sr_building_center(&c->buildings[from], &fx, &fy);
  for (int i = 0; i < c->building_count; i++) {
    const SrBuilding *b = &c->buildings[i];
    if (b->poi != poi || dist[b->block] < 0) continue;
    int32_t bx, by;
    sr_building_center(b, &bx, &by);
    int32_t d = sr_manhattan_i(fx, fy, bx, by);
    int32_t via = dist[b->block] * SR_BLOCK_CM;
    if (via > d) d = via;
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  if (dist_out) *dist_out = best_d;
  return best;
}

/* ---- stage 1: geography ------------------------------------------------------- */

static void gen_geography(SrCity *c, SrRng *r) {
  int g = c->grid;
  c->coast_side = sr_rng_chance(r, 150) ? 4 : sr_rng_range(r, 0, 3);
  int depth[SR_MAX_GRID];
  int base = sr_rng_range(r, 2, 4);
  int cur = base;
  for (int i = 0; i < g; i++) {
    cur += sr_rng_range(r, -1, 1);
    cur = SR_CLAMP(cur, 1, 6);
    depth[i] = cur;
  }
  for (int by = 0; by < g; by++) {
    for (int bx = 0; bx < g; bx++) {
      int idx = by * g + bx;
      int water = 0;
      switch (c->coast_side) {
      case 0: water = bx < depth[by]; break;
      case 1: water = bx >= g - depth[by]; break;
      case 2: water = by < depth[bx]; break;
      case 3: water = by >= g - depth[bx]; break;
      default: water = 0; break;
      }
      c->blocks[idx].water = (uint8_t)water;
      c->blocks[idx].district = -1;
    }
  }
}

/* ---- stage 2: districts ---------------------------------------------------------- */

static int count_land(const SrCity *c) {
  int n = 0;
  for (int i = 0; i < c->grid * c->grid; i++) n += !c->blocks[i].water;
  return n;
}

static void assign_blocks(SrCity *c, const int32_t *sx, const int32_t *sy, int n) {
  for (int i = 0; i < c->grid * c->grid; i++) {
    if (c->blocks[i].water) continue;
    int32_t bx, by;
    block_center(c, i, &bx, &by);
    int best = 0;
    int64_t bd = INT64_MAX;
    for (int k = 0; k < n; k++) {
      int64_t d = sr_dist2_i(bx, by, sx[k], sy[k]);
      if (d < bd) {
        bd = d;
        best = k;
      }
    }
    c->blocks[i].district = (int8_t)best;
  }
}

static void gen_districts(SrCity *c, SrRng *r) {
  int g = c->grid;
  int n = sr_rng_range(r, 8, 14);
  int32_t sx[SR_MAX_DISTRICTS], sy[SR_MAX_DISTRICTS];
  int land = count_land(c);
  int32_t min_d = (int32_t)sr_isqrt64((uint64_t)land / (uint64_t)n) * SR_BLOCK_CM / 2;
  int placed = 0;
  for (int attempt = 0; placed < n && attempt < 4000; attempt++) {
    int b = sr_rng_range(r, 0, g * g - 1);
    if (c->blocks[b].water) continue;
    int32_t x, y;
    block_center(c, b, &x, &y);
    bool ok = true;
    for (int k = 0; k < placed && attempt < 3000; k++) {
      if (sr_dist_i(x, y, sx[k], sy[k]) < min_d) ok = false;
    }
    if (!ok) continue;
    sx[placed] = x;
    sy[placed] = y;
    placed++;
  }
  n = placed;
  /* Lloyd relaxation in integer arithmetic. */
  for (int iter = 0; iter < 3; iter++) {
    assign_blocks(c, sx, sy, n);
    int64_t ax[SR_MAX_DISTRICTS] = {0}, ay[SR_MAX_DISTRICTS] = {0}, cnt[SR_MAX_DISTRICTS] = {0};
    for (int i = 0; i < g * g; i++) {
      if (c->blocks[i].water) continue;
      int32_t x, y;
      block_center(c, i, &x, &y);
      int d = c->blocks[i].district;
      ax[d] += x;
      ay[d] += y;
      cnt[d]++;
    }
    for (int k = 0; k < n; k++) {
      if (cnt[k]) {
        sx[k] = (int32_t)(ax[k] / cnt[k]);
        sy[k] = (int32_t)(ay[k] / cnt[k]);
      }
    }
  }
  assign_blocks(c, sx, sy, n);
  c->district_count = n;
  for (int k = 0; k < n; k++) {
    SrDistrict *d = &c->districts[k];
    memset(d, 0, sizeof *d);
    d->cx = sx[k];
    d->cy = sy[k];
    d->station = -1;
    d->type = SR_D_COUNT;
  }
  for (int i = 0; i < g * g; i++) {
    if (c->blocks[i].water) continue;
    SrDistrict *d = &c->districts[c->blocks[i].district];
    d->block_count++;
    int bx = i % g, by = i / g;
    if (bx == 0 || by == 0 || bx == g - 1 || by == g - 1) d->touches_edge = true;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nb = sr_city_block_index(c, bx + dx[k], by + dy[k]);
      if (nb >= 0 && c->blocks[nb].water) d->touches_water = true;
    }
  }

  /* Type assignment with constraints. */
  int32_t cx = g * SR_BLOCK_CM / 2, cy = cx;
  int order[SR_MAX_DISTRICTS];
  for (int k = 0; k < n; k++) order[k] = k;
  /* sort by distance to centre (insertion sort, deterministic) */
  for (int i = 1; i < n; i++) {
    int v = order[i], j = i - 1;
    int64_t dv = sr_dist2_i(sx[v], sy[v], cx, cy);
    while (j >= 0 && sr_dist2_i(sx[order[j]], sy[order[j]], cx, cy) > dv) {
      order[j + 1] = order[j];
      j--;
    }
    order[j + 1] = v;
  }
  c->districts[order[0]].type = SR_D_SPIRE;
  c->districts[order[1]].type = SR_D_CIVIC;
  int docks = -1;
  for (int i = n - 1; i >= 2; i--) {
    int k = order[i];
    if (c->districts[k].touches_water) {
      docks = k;
      break;
    }
  }
  if (docks >= 0) c->districts[docks].type = SR_D_DOCKS;
  /* Industrial next to the docks (or a random outer district). */
  int ind = -1;
  int64_t best = INT64_MAX;
  for (int i = 2; i < n; i++) {
    int k = order[i];
    if (c->districts[k].type != SR_D_COUNT) continue;
    int64_t d = docks >= 0 ? sr_dist2_i(sx[k], sy[k], sx[docks], sy[docks]) : -(int64_t)i;
    if (d < best) {
      best = d;
      ind = k;
    }
  }
  c->districts[ind].type = SR_D_INDUSTRIAL;
  /* Campus at the edge (farthest from centre) when there is room. */
  if (n >= 10) {
    for (int i = n - 1; i >= 2; i--) {
      if (c->districts[order[i]].type == SR_D_COUNT) {
        c->districts[order[i]].type = SR_D_CAMPUS;
        break;
      }
    }
  }
  /* Required types in shuffled order, then free choice. */
  static const SrDistrictType required[] = {SR_D_RESIDENTIAL, SR_D_COMMERCIAL,
                                            SR_D_ENTERTAINMENT, SR_D_UNDERCITY};
  int free_list[SR_MAX_DISTRICTS], nf = 0;
  for (int i = 2; i < n; i++)
    if (c->districts[order[i]].type == SR_D_COUNT) free_list[nf++] = order[i];
  for (int i = nf - 1; i > 0; i--) {
    int j = sr_rng_range(r, 0, i);
    int t = free_list[i];
    free_list[i] = free_list[j];
    free_list[j] = t;
  }
  int fi = 0;
  for (size_t q = 0; q < SR_ARRAY_LEN(required) && fi < nf; q++)
    c->districts[free_list[fi++]].type = required[q];
  static const SrDistrictType extra[] = {SR_D_RESIDENTIAL, SR_D_COMMERCIAL, SR_D_OLDQUARTER,
                                         SR_D_INDUSTRIAL, SR_D_BARRENS, SR_D_RESIDENTIAL};
  for (; fi < nf; fi++) {
    SrDistrict *d = &c->districts[free_list[fi]];
    SrDistrictType t = extra[sr_rng_range(r, 0, (int32_t)SR_ARRAY_LEN(extra) - 1)];
    if (t == SR_D_BARRENS && !d->touches_edge) t = SR_D_RESIDENTIAL;
    d->type = t;
  }
  static const int16_t wealth[SR_D_COUNT] = {95, 70, 55, 45, 35, 40, 25, 20, 80, 8, 5};
  for (int k = 0; k < n; k++) c->districts[k].wealth = wealth[c->districts[k].type];
}

/* ---- stage 3: highways ------------------------------------------------------------ */

static void gen_highways(SrCity *c) {
  for (int k = 0; k < c->district_count; k++) {
    if (c->districts[k].type != SR_D_UNDERCITY) continue;
    int hb = sr_city_block_at(c, c->districts[k].cx, c->districts[k].cy);
    int hx = hb % c->grid, hy = hb / c->grid;
    for (int i = 0; i < c->grid; i++) {
      int a = sr_city_block_index(c, i, hy), b = sr_city_block_index(c, hx, i);
      if (!c->blocks[a].water) c->blocks[a].highway = 1;
      if (!c->blocks[b].water) c->blocks[b].highway = 1;
    }
  }
}

/* ---- stage 4: buildings -------------------------------------------------------------- */

typedef struct Style {
  int8_t lots_min, lots_max;
  int16_t h_min, h_max; /* metres */
} Style;

static const Style k_styles[SR_D_COUNT] = {
    {1, 2, 120, 320}, /* spire */
    {1, 2, 30, 80},   /* civic */
    {2, 4, 25, 90},   /* commercial */
    {2, 4, 15, 60},   /* entertainment */
    {1, 2, 50, 140},  /* residential */
    {4, 4, 8, 24},    /* old quarter */
    {1, 2, 10, 30},   /* industrial */
    {1, 2, 8, 20},    /* docks */
    {1, 1, 15, 40},   /* campus */
    {2, 4, 6, 20},    /* undercity */
    {0, 2, 4, 12},    /* barrens */
};

static void gen_buildings(SrCity *c, SrRng *r) {
  c->building_count = 0;
  for (int i = 0; i < c->grid * c->grid; i++) {
    SrBlock *blk = &c->blocks[i];
    blk->first_building = (uint16_t)c->building_count;
    blk->building_count = 0;
    if (blk->water) continue;
    SrDistrictType t = c->districts[blk->district].type;
    const Style *st = &k_styles[t];
    int lots = sr_rng_range(r, st->lots_min, st->lots_max);
    if (lots == 3) lots = 4;
    int32_t ox, oy;
    sr_city_block_origin(i, c->grid, &ox, &oy);
    ox += SR_STREET_CM;
    oy += SR_STREET_CM;
    bool split_x = sr_rng_chance(r, 500);
    for (int l = 0; l < lots; l++) {
      int32_t x0 = ox, y0 = oy, x1 = ox + LOT_CM, y1 = oy + LOT_CM;
      if (lots == 2) {
        if (split_x) {
          x0 = ox + l * (LOT_CM / 2);
          x1 = x0 + LOT_CM / 2;
        } else {
          y0 = oy + l * (LOT_CM / 2);
          y1 = y0 + LOT_CM / 2;
        }
      } else if (lots == 4) {
        x0 = ox + (l % 2) * (LOT_CM / 2);
        y0 = oy + (l / 2) * (LOT_CM / 2);
        x1 = x0 + LOT_CM / 2;
        y1 = y0 + LOT_CM / 2;
      }
      int32_t inset = sr_rng_range(r, 200, 600);
      SrBuilding *b = &c->buildings[c->building_count];
      memset(b, 0, sizeof *b);
      b->id = (uint32_t)c->building_count;
      b->x0 = x0 + inset;
      b->y0 = y0 + inset;
      b->x1 = x1 - inset;
      b->y1 = y1 - inset;
      b->height_cm = sr_rng_range(r, st->h_min, st->h_max) * 100;
      b->floors = (uint8_t)SR_MIN(255, SR_MAX(1, b->height_cm / 350));
      b->block = (uint16_t)i;
      b->style = (uint8_t)sr_rng_range(r, 0, 7);
      b->owner = -1;
      b->flags = SR_BF_ROOF_ACCESS;
      if (b->height_cm > 2000) b->flags |= SR_BF_ELEVATOR;
      c->building_count++;
      blk->building_count++;
    }
  }
}

/* ---- stage 5: places of interest ------------------------------------------------------ */

/* Per-district weights for places of interest (index = SrPoi up to UV_BOOTH). */
static const int32_t k_poi_weights[SR_D_COUNT][SR_POI_UV_BOOTH + 1] = {
    /* none food bar clin rip gun elec clo fence caps booth fixer pol groc uv */
    {60, 6, 3, 3, 1, 1, 3, 4, 0, 1, 2, 0, 0, 2, 3},     /* spire */
    {60, 6, 2, 3, 0, 0, 2, 2, 0, 1, 2, 1, 4, 3, 2},     /* civic */
    {35, 10, 5, 3, 2, 3, 6, 6, 2, 3, 3, 1, 0, 6, 3},    /* commercial */
    {35, 10, 14, 2, 3, 2, 3, 4, 3, 4, 3, 2, 0, 3, 2},   /* entertainment */
    {55, 8, 3, 3, 2, 1, 2, 2, 1, 2, 2, 1, 0, 6, 3},     /* residential */
    {40, 10, 6, 3, 3, 2, 2, 3, 3, 2, 2, 3, 0, 4, 2},    /* old quarter */
    {60, 5, 3, 2, 3, 2, 3, 1, 3, 2, 2, 1, 0, 2, 1},     /* industrial */
    {60, 5, 4, 1, 2, 2, 2, 1, 4, 2, 2, 2, 0, 2, 1},     /* docks */
    {70, 6, 1, 4, 0, 0, 3, 1, 0, 1, 2, 0, 0, 2, 2},     /* campus */
    {40, 8, 6, 2, 6, 4, 3, 2, 6, 4, 3, 3, 0, 3, 1},     /* undercity */
    {60, 4, 2, 1, 4, 3, 1, 1, 5, 1, 1, 2, 0, 1, 0},     /* barrens */
};

static int district_of_building(const SrCity *c, int b) {
  return c->blocks[c->buildings[b].block].district;
}

static bool is_free_building(const SrCity *c, int b) {
  return c->buildings[b].poi == SR_POI_NONE && !(c->buildings[b].flags & SR_BF_LANDMARK);
}

static void gen_landmarks(SrCity *c, int corp_count) {
  /* City Hall: largest building of the Civic Center. HQ candidates: tallest Spire buildings
   * (HQ assignment happens in factions; here we only flag City Hall). */
  int hall = -1;
  int64_t best = -1;
  for (int i = 0; i < c->building_count; i++) {
    if (c->districts[district_of_building(c, i)].type != SR_D_CIVIC) continue;
    const SrBuilding *b = &c->buildings[i];
    int64_t area = (int64_t)(b->x1 - b->x0) * (b->y1 - b->y0);
    if (area > best) {
      best = area;
      hall = i;
    }
  }
  c->buildings[hall].poi = SR_POI_CITY_HALL;
  c->buildings[hall].flags |= SR_BF_LANDMARK | SR_BF_ENTERABLE | SR_BF_NETWORKED;
  (void)corp_count;
}

static void gen_pois(SrCity *c, SrRng *r) {
  for (int i = 0; i < c->building_count; i++) {
    if (!is_free_building(c, i)) continue;
    SrDistrictType t = c->districts[district_of_building(c, i)].type;
    int poi = sr_rng_weighted(r, k_poi_weights[t], SR_POI_UV_BOOTH + 1);
    c->buildings[i].poi = (uint8_t)poi;
  }
  /* Every district needs food; every place type must exist somewhere. */
  for (int k = 0; k < c->district_count; k++) {
    bool has_food = false;
    int candidate = -1;
    for (int i = 0; i < c->building_count; i++) {
      if (district_of_building(c, i) != k) continue;
      if (c->buildings[i].poi == SR_POI_FOOD) has_food = true;
      if (candidate < 0 && is_free_building(c, i)) candidate = i;
    }
    if (!has_food && candidate >= 0) c->buildings[candidate].poi = SR_POI_FOOD;
  }
  for (int p = SR_POI_FOOD; p <= SR_POI_UV_BOOTH; p++) {
    bool found = false;
    for (int i = 0; i < c->building_count && !found; i++) found = c->buildings[i].poi == p;
    for (int tries = 0; !found && tries < 10000; tries++) {
      int i = sr_rng_range(r, 0, c->building_count - 1);
      if (is_free_building(c, i)) {
        c->buildings[i].poi = (uint8_t)p;
        found = true;
      }
    }
  }
}

/* ---- stage 6: metro ----------------------------------------------------------------------- */

static void add_metro_edge(SrCity *c, int a, int b) {
  if (c->metro_edge_count < SR_MAX_METRO_EDGES) {
    c->metro_edges[c->metro_edge_count].a = (int16_t)a;
    c->metro_edges[c->metro_edge_count].b = (int16_t)b;
    c->metro_edge_count++;
  }
}

static void gen_metro(SrCity *c) {
  c->station_count = 0;
  c->metro_edge_count = 0;
  for (int k = 0; k < c->district_count; k++) {
    int best = -1;
    int64_t bd = INT64_MAX;
    for (int i = 0; i < c->grid * c->grid; i++) {
      if (c->blocks[i].water || c->blocks[i].district != k) continue;
      int32_t x, y;
      block_center(c, i, &x, &y);
      int64_t d = sr_dist2_i(x, y, c->districts[k].cx, c->districts[k].cy);
      if (d < bd) {
        bd = d;
        best = i;
      }
    }
    c->stations[c->station_count].block = (int16_t)best;
    c->stations[c->station_count].district = (int8_t)k;
    c->districts[k].station = (int16_t)c->station_count;
    c->station_count++;
  }
  /* Prim's minimum spanning tree on Manhattan distance between stations. */
  bool in_tree[SR_MAX_STATIONS] = {false};
  in_tree[0] = true;
  for (int added = 1; added < c->station_count; added++) {
    int ba = -1, bb = -1;
    int32_t bd = INT32_MAX;
    for (int a = 0; a < c->station_count; a++) {
      if (!in_tree[a]) continue;
      for (int b = 0; b < c->station_count; b++) {
        if (in_tree[b]) continue;
        int32_t ax, ay, bx, by;
        block_center(c, c->stations[a].block, &ax, &ay);
        block_center(c, c->stations[b].block, &bx, &by);
        int32_t d = sr_manhattan_i(ax, ay, bx, by);
        if (d < bd) {
          bd = d;
          ba = a;
          bb = b;
        }
      }
    }
    in_tree[bb] = true;
    add_metro_edge(c, ba, bb);
  }
  /* One loop line closing the network for redundancy. */
  if (c->station_count >= 6) add_metro_edge(c, 0, c->station_count - 1);
}

/* ---- stage 7: starter neighbourhood ------------------------------------------------------ */

static int place_poi_near(SrCity *c, int home, SrPoi poi, int32_t max_dist) {
  int32_t d;
  if (sr_city_find_poi_near(c, poi, home, &d) >= 0 && d <= max_dist) return 1;
  /* Convert the closest free building within range. */
  int best = -1;
  int32_t bd = INT32_MAX;
  for (int i = 0; i < c->building_count; i++) {
    if (i == home || !is_free_building(c, i)) continue;
    int32_t wd = sr_city_walk_dist(c, home, i);
    if (wd >= 0 && wd <= max_dist && wd < bd) {
      bd = wd;
      best = i;
    }
  }
  if (best < 0) return 0;
  c->buildings[best].poi = (uint8_t)poi;
  return 1;
}

static int gen_starter(SrCity *c) {
  int res = -1;
  for (int k = 0; k < c->district_count; k++)
    if (c->districts[k].type == SR_D_RESIDENTIAL) {
      res = k;
      break;
    }
  int home = -1;
  int64_t bd = INT64_MAX;
  for (int i = 0; i < c->building_count; i++) {
    if (district_of_building(c, i) != res || !is_free_building(c, i)) continue;
    /* need at least one other building in reach for services */
    int32_t x, y;
    sr_building_center(&c->buildings[i], &x, &y);
    int64_t d = sr_dist2_i(x, y, c->districts[res].cx, c->districts[res].cy);
    if (d < bd) {
      bd = d;
      home = i;
    }
  }
  if (home < 0) return 0;
  c->home_building = home;
  c->buildings[home].poi = SR_POI_HOME;
  c->buildings[home].flags |= SR_BF_ENTERABLE | SR_BF_NETWORKED;
  int ok = place_poi_near(c, home, SR_POI_FOOD, 15000);
  ok &= place_poi_near(c, home, SR_POI_FIXER, 15000);
  ok &= place_poi_near(c, home, SR_POI_CLINIC, 15000);
  /* Landlord office: any free building nearby (re-uses the generic "none" type). */
  c->landlord_building = home;
  /* Metro within 300 m: otherwise add a station next to home, linked to the nearest. */
  int hb = c->buildings[home].block;
  int near = sr_city_nearest_station(c, hb);
  int steps = sr_city_block_steps(c, hb, c->stations[near].block);
  if (steps * SR_BLOCK_CM > 30000 && c->station_count < SR_MAX_STATIONS) {
    c->stations[c->station_count].block = (int16_t)hb;
    c->stations[c->station_count].district = (int8_t)res;
    add_metro_edge(c, near, c->station_count);
    c->station_count++;
  }
  return ok;
}

/* ---- stage 8: skyways ---------------------------------------------------------------------- */

static int tallest_in_block(const SrCity *c, int block) {
  const SrBlock *b = &c->blocks[block];
  int best = -1;
  for (int i = 0; i < b->building_count; i++) {
    int idx = b->first_building + i;
    if (best < 0 || c->buildings[idx].height_cm > c->buildings[best].height_cm) best = idx;
  }
  return best;
}

static void gen_skyways(SrCity *c, SrRng *r) {
  c->skyway_count = 0;
  for (int i = 0; i < c->grid * c->grid && c->skyway_count < SR_MAX_SKYWAYS; i++) {
    if (c->blocks[i].water) continue;
    SrDistrictType t = c->districts[c->blocks[i].district].type;
    if (t != SR_D_SPIRE && t != SR_D_COMMERCIAL) continue;
    int nb = sr_city_block_index(c, i % c->grid + 1, i / c->grid);
    if (nb < 0 || c->blocks[nb].water) continue;
    int a = tallest_in_block(c, i), b = tallest_in_block(c, nb);
    if (a < 0 || b < 0) continue;
    if (c->buildings[a].height_cm < 6000 || c->buildings[b].height_cm < 6000) continue;
    if (!sr_rng_chance(r, 400)) continue;
    c->skyways[c->skyway_count].a = (uint16_t)a;
    c->skyways[c->skyway_count].b = (uint16_t)b;
    c->buildings[a].flags |= SR_BF_ELEVATOR;
    c->buildings[b].flags |= SR_BF_ELEVATOR;
    c->skyway_count++;
  }
}

/* ---- stage 9: names and flags -------------------------------------------------------------- */

static void gen_names(SrCity *c) {
  sr_name_city(c->seed, c->name, sizeof c->name);
  for (int k = 0; k < c->district_count; k++) {
    for (uint64_t salt = 0;; salt++) {
      sr_name_district(c->seed, (uint64_t)k * 1000u + salt, c->districts[k].type,
                       c->districts[k].name, sizeof c->districts[k].name);
      bool dup = false;
      for (int j = 0; j < k; j++) dup |= strcmp(c->districts[j].name, c->districts[k].name) == 0;
      if (!dup) break;
    }
  }
  for (int i = 0; i < c->building_count; i++) {
    SrBuilding *b = &c->buildings[i];
    if (b->poi != SR_POI_NONE && b->poi < SR_POI_FIRST_SPECIAL) {
      sr_name_business(c->seed, (uint64_t)i, (SrPoi)b->poi, b->name, sizeof b->name);
      b->flags |= SR_BF_ENTERABLE | SR_BF_NETWORKED;
    }
  }
}

/* ---- validation ---------------------------------------------------------------------------- */

enum {
  INV_OK = 0, INV_DISTRICT_COUNT, INV_DISTRICT_TYPES, INV_DOCKS, INV_CONNECTED, INV_METRO,
  INV_STARTER, INV_SERVICES, INV_LANDMARKS, INV_NAMES, INV_FACTIONS, INV_FORCED, INV_COUNT
};

const char *sr_city_invariant_name(int code) {
  static const char *const n[INV_COUNT] = {"ok", "district count", "district types", "docks",
                                           "connectivity", "metro", "starter neighbourhood",
                                           "services", "landmarks", "names", "factions",
                                           "forced failure"};
  return code >= 0 && code < INV_COUNT ? n[code] : "?";
}

int sr_city_validate(const SrCity *c) {
  if (c->district_count < 8 || c->district_count > 14) return INV_DISTRICT_COUNT;
  int counts[SR_D_COUNT] = {0};
  for (int k = 0; k < c->district_count; k++) {
    if (c->districts[k].type >= SR_D_COUNT || c->districts[k].block_count == 0)
      return INV_DISTRICT_TYPES;
    counts[c->districts[k].type]++;
  }
  if (counts[SR_D_SPIRE] != 1 || counts[SR_D_CIVIC] != 1 || !counts[SR_D_RESIDENTIAL] ||
      !counts[SR_D_COMMERCIAL] || !counts[SR_D_INDUSTRIAL] || !counts[SR_D_ENTERTAINMENT] ||
      !counts[SR_D_UNDERCITY])
    return INV_DISTRICT_TYPES;
  for (int k = 0; k < c->district_count; k++)
    if (c->districts[k].type == SR_D_DOCKS && !c->districts[k].touches_water) return INV_DOCKS;
  int16_t dist[SR_MAX_BLOCKS];
  land_bfs(c, c->buildings[c->home_building].block, dist);
  for (int i = 0; i < c->grid * c->grid; i++)
    if (!c->blocks[i].water && dist[i] < 0) return INV_CONNECTED;
  for (int k = 0; k < c->district_count; k++)
    if (c->districts[k].station < 0) return INV_METRO;
  for (int s = 1; s < c->station_count; s++)
    if (sr_city_metro_hops(c, 0, s) < 0) return INV_METRO;
  int32_t d;
  if (c->buildings[c->home_building].poi != SR_POI_HOME) return INV_STARTER;
  static const SrPoi near[] = {SR_POI_FOOD, SR_POI_FIXER, SR_POI_CLINIC};
  for (size_t q = 0; q < SR_ARRAY_LEN(near); q++) {
    if (sr_city_find_poi_near(c, near[q], c->home_building, &d) < 0 || d > 15000)
      return INV_STARTER;
  }
  int ns = sr_city_nearest_station(c, c->buildings[c->home_building].block);
  if (ns < 0 || sr_city_block_steps(c, c->buildings[c->home_building].block,
                                    c->stations[ns].block) * SR_BLOCK_CM > 30000)
    return INV_STARTER;
  bool has[SR_POI_COUNT] = {false};
  bool food[SR_MAX_DISTRICTS] = {false};
  for (int i = 0; i < c->building_count; i++) {
    has[c->buildings[i].poi] = true;
    if (c->buildings[i].poi == SR_POI_FOOD) food[district_of_building(c, i)] = true;
  }
  for (int p = SR_POI_FOOD; p <= SR_POI_UV_BOOTH; p++)
    if (!has[p]) return INV_SERVICES;
  for (int k = 0; k < c->district_count; k++)
    if (!food[k]) return INV_SERVICES;
  if (!has[SR_POI_CITY_HALL]) return INV_LANDMARKS;
  for (int k = 0; k < c->district_count; k++)
    for (int j = 0; j < k; j++)
      if (strcmp(c->districts[k].name, c->districts[j].name) == 0) return INV_NAMES;
  if (sr_factions_validate(c) != 0) return INV_FACTIONS;
  if (c->force_invariant_failures > 0) return INV_FORCED;
  return INV_OK;
}

/* ---- driver ------------------------------------------------------------------------------------ */

SrResult sr_city_generate(SrCity *c, uint64_t seed, const SrCityOptions *opt) {
  int forced = c->force_invariant_failures;
  memset(c, 0, sizeof *c);
  c->seed = seed;
  c->gen_version = SR_GEN_VERSION;
  if (opt) c->opt = *opt;
  c->grid = c->opt.size == SR_CITY_LARGE ? 42 : 32;
  for (int attempt = 0; attempt < 32; attempt++) {
    c->force_invariant_failures = forced;
    c->rerolls = attempt;
    SrRng r = sr_rng_stream(seed, "city", (uint64_t)attempt, 0);
    gen_geography(c, &r);
    gen_districts(c, &r);
    gen_highways(c);
    gen_buildings(c, &r);
    gen_landmarks(c, c->opt.corp_count);
    gen_pois(c, &r);
    gen_metro(c);
    int ok = gen_starter(c);
    gen_skyways(c, &r);
    gen_names(c);
    sr_factions_generate(c, sr_hash_combine(seed, (uint64_t)attempt));
    if (ok && sr_city_validate(c) == 0) return SR_OK;
    if (forced > 0) forced--;
  }
  return SR_ERR_INVALID; /* GCOVR_EXCL_LINE reason: no seed found that fails 32 times */
}

SrCity *sr_city_create(uint64_t seed, const SrCityOptions *opt) {
  SrCity *c = (SrCity *)calloc(1, sizeof(SrCity));
  if (!c) return NULL; /* GCOVR_EXCL_LINE reason: calloc failure not injectable */
  if (sr_city_generate(c, seed, opt) != SR_OK) { /* GCOVR_EXCL_START reason: see above */
    free(c);
    return NULL;
  } /* GCOVR_EXCL_STOP */
  return c;
}

void sr_city_destroy(SrCity *c) { free(c); }

uint64_t sr_city_digest(const SrCity *c) {
  uint64_t h = sr_hash_combine(c->seed, c->gen_version);
  h = sr_hash_bytes(h, c->name, strlen(c->name));
  for (int i = 0; i < c->grid * c->grid; i++) {
    const SrBlock *b = &c->blocks[i];
    uint8_t v[4] = {b->water, (uint8_t)b->district, b->highway, b->building_count};
    h = sr_hash_bytes(h, v, 4);
  }
  for (int k = 0; k < c->district_count; k++) {
    const SrDistrict *d = &c->districts[k];
    int32_t v[4] = {(int32_t)d->type, d->cx, d->cy, d->station};
    h = sr_hash_bytes(h, v, sizeof v);
    h = sr_hash_bytes(h, d->name, strlen(d->name));
  }
  for (int i = 0; i < c->building_count; i++) {
    const SrBuilding *b = &c->buildings[i];
    int32_t v[9] = {b->x0, b->y0, b->x1, b->y1, b->height_cm, b->poi, b->flags, b->owner,
                    b->style};
    h = sr_hash_bytes(h, v, sizeof v);
  }
  for (int s = 0; s < c->station_count; s++) h = sr_hash_combine(h, (uint64_t)c->stations[s].block);
  for (int f = 0; f < c->factions.count; f++) {
    const SrFaction *fa = &c->factions.f[f];
    h = sr_hash_bytes(h, fa->name, strlen(fa->name));
    h = sr_hash_combine(h, (uint64_t)fa->hq_building * 31u + fa->hue);
  }
  return h;
}
