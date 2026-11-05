#include "world/sr_nav.h"

#include "core/sr_fixed.h"

#include <stdlib.h>
#include <string.h>

static int32_t ring_lo(int32_t origin) { return origin + SR_NAV_RING_CM; }
static int32_t ring_hi(int32_t origin) { return origin + SR_BLOCK_CM + 300; } /* 3 m past the lot */

int32_t sr_nav_node_count(const SrCity *c) { return c->grid * c->grid * SR_NAV_PER_BLOCK; }

bool sr_nav_node_valid(const SrCity *c, int32_t node) {
  if (node < 0 || node >= sr_nav_node_count(c)) return false;
  int b = node / SR_NAV_PER_BLOCK, k = node % SR_NAV_PER_BLOCK;
  if (c->blocks[b].water) return false;
  return k < 4 || k - 4 < c->blocks[b].building_count;
}

void sr_nav_node_pos(const SrCity *c, int32_t node, int32_t *x, int32_t *y) {
  int b = node / SR_NAV_PER_BLOCK, k = node % SR_NAV_PER_BLOCK;
  int32_t ox, oy;
  sr_city_block_origin(b, c->grid, &ox, &oy);
  int32_t x0 = ring_lo(ox), y0 = ring_lo(oy), x1 = ring_hi(ox), y1 = ring_hi(oy);
  if (k < 4) {
    *x = (k == 1 || k == 2) ? x1 : x0;
    *y = (k >= 2) ? y1 : y0;
    return;
  }
  const SrBuilding *bd = &c->buildings[c->blocks[b].first_building + (k - 4)];
  int32_t cx = (bd->x0 + bd->x1) / 2;
  *x = cx < x0 ? x0 : cx > x1 ? x1 : cx;
  *y = y1; /* doors open onto the south side of the ring */
}

static int32_t dist(const SrCity *c, int32_t a, int32_t b) {
  int32_t ax, ay, bx, by;
  sr_nav_node_pos(c, a, &ax, &ay);
  sr_nav_node_pos(c, b, &bx, &by);
  return sr_manhattan_i(ax, ay, bx, by);
}

static bool land(const SrCity *c, int bx, int by) {
  int b = sr_city_block_index(c, bx, by);
  return b >= 0 && !c->blocks[b].water;
}

int sr_nav_neighbors(const SrCity *c, int32_t node, int32_t out[8], int32_t cost[8]) {
  if (!sr_nav_node_valid(c, node)) return 0;
  int b = node / SR_NAV_PER_BLOCK, k = node % SR_NAV_PER_BLOCK;
  int bx = b % c->grid, by = b / c->grid;
  int32_t base = b * SR_NAV_PER_BLOCK;
  int n = 0;
#define ADD(target, extra)                                                                                             \
  do {                                                                                                                 \
    out[n] = (target);                                                                                                 \
    cost[n] = dist(c, node, out[n]) + (extra);                                                                         \
    n++;                                                                                                               \
  } while (0)
  if (k >= 4) { /* doors join the south side of the ring */
    ADD(base + 2, 0);
    ADD(base + 3, 0);
    return n;
  }
  ADD(base + (k + 1) % 4, 0); /* along the ring */
  ADD(base + (k + 3) % 4, 0);
  if (k >= 2) /* doors on the south side */
    for (int i = 0; i < c->blocks[b].building_count; i++) ADD(base + 4 + i, 0);
  /* crossings to the neighbouring lots' corners */
  static const int k_east[4] = {-1, 0, 3, -1}, k_west[4] = {1, -1, -1, 2};
  static const int k_south[4] = {-1, -1, 1, 0}, k_north[4] = {3, 2, -1, -1};
  if (k_east[k] >= 0 && land(c, bx + 1, by)) ADD((b + 1) * SR_NAV_PER_BLOCK + k_east[k], SR_NAV_CROSSING);
  if (k_west[k] >= 0 && land(c, bx - 1, by)) ADD((b - 1) * SR_NAV_PER_BLOCK + k_west[k], SR_NAV_CROSSING);
  if (k_south[k] >= 0 && land(c, bx, by + 1)) ADD((b + c->grid) * SR_NAV_PER_BLOCK + k_south[k], SR_NAV_CROSSING);
  if (k_north[k] >= 0 && land(c, bx, by - 1)) ADD((b - c->grid) * SR_NAV_PER_BLOCK + k_north[k], SR_NAV_CROSSING);
#undef ADD
  return n;
}

int32_t sr_nav_door_node(const SrCity *c, int building) {
  const SrBuilding *bd = &c->buildings[building];
  return bd->block * SR_NAV_PER_BLOCK + 4 + (building - c->blocks[bd->block].first_building);
}

int32_t sr_nav_nearest_node(const SrCity *c, int32_t x, int32_t y) {
  int b0 = sr_city_block_at(c, x, y);
  if (b0 < 0 || c->blocks[b0].water) return -1;
  int32_t best = -1, bd = INT32_MAX;
  for (int dy = -1; dy <= 1; dy++) /* rings reach 3 m into the neighbours' street strips */
    for (int dx = -1; dx <= 1; dx++) {
      int b = sr_city_block_index(c, b0 % c->grid + dx, b0 / c->grid + dy);
      if (b < 0 || c->blocks[b].water) continue;
      for (int k = 0; k < 4 + c->blocks[b].building_count; k++) {
        int32_t nx, ny, node = b * SR_NAV_PER_BLOCK + k;
        sr_nav_node_pos(c, node, &nx, &ny);
        int32_t d = sr_manhattan_i(x, y, nx, ny);
        if (d < bd) {
          bd = d;
          best = node;
        }
      }
    }
  return best;
}

bool sr_nav_connected(const SrCity *c) {
  int32_t n = sr_nav_node_count(c);
  uint8_t *seen = (uint8_t *)calloc((size_t)n, 1);
  int32_t *queue = (int32_t *)malloc(sizeof(int32_t) * (size_t)n);
  bool ok = seen && queue;
  if (ok) {
    int head = 0, tail = 0;
    int32_t start = sr_nav_door_node(c, c->home_building);
    queue[tail++] = start;
    seen[start] = 1;
    while (head < tail) {
      int32_t nb[8], cost[8];
      int k = sr_nav_neighbors(c, queue[head++], nb, cost);
      for (int i = 0; i < k; i++)
        if (!seen[nb[i]]) {
          seen[nb[i]] = 1;
          queue[tail++] = nb[i];
        }
    }
    for (int32_t v = 0; v < n && ok; v++) ok = !sr_nav_node_valid(c, v) || seen[v];
  }
  free(seen);
  free(queue);
  return ok;
}

static int chunks_per_side(const SrCity *c) { return (c->grid + SR_NAV_CHUNK - 1) / SR_NAV_CHUNK; }

int sr_nav_chunk_of(const SrCity *c, int32_t node) {
  int b = node / SR_NAV_PER_BLOCK;
  return (b / c->grid) / SR_NAV_CHUNK * chunks_per_side(c) + (b % c->grid) / SR_NAV_CHUNK;
}

void sr_nav_chunk_build(const SrCity *c, int cx, int cy, SrNavChunk *out) {
  memset(out, 0, sizeof *out);
  out->cx = cx;
  out->cy = cy;
  int chunk = cy * chunks_per_side(c) + cx;
  for (int by = cy * SR_NAV_CHUNK; by < (cy + 1) * SR_NAV_CHUNK && by < c->grid; by++)
    for (int bx = cx * SR_NAV_CHUNK; bx < (cx + 1) * SR_NAV_CHUNK && bx < c->grid; bx++)
      for (int k = 0; k < SR_NAV_PER_BLOCK; k++) {
        int32_t node = (by * c->grid + bx) * SR_NAV_PER_BLOCK + k;
        if (sr_nav_node_valid(c, node)) out->nodes[out->node_count++] = node;
      }
  for (int i = 0; i < out->node_count; i++) {
    int32_t nb[8], cost[8];
    int n = sr_nav_neighbors(c, out->nodes[i], nb, cost);
    for (int j = 0; j < n; j++) {
      if (sr_nav_chunk_of(c, nb[j]) != chunk) {
        out->portal_count++;
      } else if (nb[j] > out->nodes[i]) { /* each internal edge once */
        out->ea[out->edge_count] = out->nodes[i];
        out->eb[out->edge_count] = nb[j];
        out->cost[out->edge_count] = cost[j];
        out->edge_count++;
      }
    }
  }
}

/* Chunk corridor: BFS over chunks that share a crossing, then widened by one chunk. */
static int corridor(const SrCity *c, int from_chunk, int to_chunk, uint8_t *allowed) {
  int cs = chunks_per_side(c), n = cs * cs;
  int *prev = (int *)malloc(sizeof(int) * (size_t)n * 2);
  if (!prev) return 0; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  int *queue = prev + n;
  for (int i = 0; i < n; i++) prev[i] = -1;
  int head = 0, tail = 0;
  queue[tail++] = from_chunk;
  prev[from_chunk] = from_chunk;
  while (head < tail && prev[to_chunk] < 0) {
    int ch = queue[head++];
    int cx = ch % cs, cy = ch / cs;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + dx[k], ny = cy + dy[k];
      if (nx < 0 || ny < 0 || nx >= cs || ny >= cs || prev[ny * cs + nx] >= 0) continue;
      prev[ny * cs + nx] = ch;
      queue[tail++] = ny * cs + nx;
    }
  }
  int count = 0;
  memset(allowed, 0, (size_t)n);
  if (prev[to_chunk] >= 0) {
    for (int ch = to_chunk;; ch = prev[ch]) {
      int cx = ch % cs, cy = ch / cs;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          int nx = cx + dx, ny = cy + dy;
          if (nx < 0 || ny < 0 || nx >= cs || ny >= cs || allowed[ny * cs + nx]) continue;
          allowed[ny * cs + nx] = 1;
          count++;
        }
      if (ch == from_chunk) break;
    }
  }
  free(prev);
  return count;
}

/* Binary heap of (f, node). */
typedef struct Heap {
  int64_t *f;
  int32_t *node;
  int n;
} Heap;

static void heap_push(Heap *h, int64_t f, int32_t node) {
  int i = h->n++;
  while (i > 0) {
    int p = (i - 1) / 2;
    if (h->f[p] < f || (h->f[p] == f && h->node[p] <= node)) break;
    h->f[i] = h->f[p];
    h->node[i] = h->node[p];
    i = p;
  }
  h->f[i] = f;
  h->node[i] = node;
}

static int32_t heap_pop(Heap *h) {
  int32_t top = h->node[0];
  int64_t f = h->f[--h->n];
  int32_t node = h->node[h->n];
  int i = 0;
  for (;;) {
    int l = 2 * i + 1, r = l + 1, m = i;
    int64_t mf = f;
    int32_t mn = node;
    if (l < h->n && (h->f[l] < mf || (h->f[l] == mf && h->node[l] < mn))) m = l, mf = h->f[l], mn = h->node[l];
    if (r < h->n && (h->f[r] < mf || (h->f[r] == mf && h->node[r] < mn))) m = r;
    if (m == i) break;
    h->f[i] = h->f[m];
    h->node[i] = h->node[m];
    i = m;
  }
  h->f[i] = f;
  h->node[i] = node;
  return top;
}

static bool astar(const SrCity *c, int32_t from, int32_t to, const uint8_t *allowed, SrNavPath *out) {
  int32_t n = sr_nav_node_count(c);
  int32_t *g = (int32_t *)malloc(sizeof(int32_t) * (size_t)n * 2);
  int64_t *hf = (int64_t *)malloc(sizeof(int64_t) * (size_t)n * 8);
  int32_t *hn = (int32_t *)malloc(sizeof(int32_t) * (size_t)n * 8);
  uint8_t *closed = (uint8_t *)calloc((size_t)n, 1);
  bool found = false;
  if (!g || !hf || !hn || !closed) goto done; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  int32_t *prev = g + n;
  for (int32_t i = 0; i < n; i++) g[i] = INT32_MAX, prev[i] = -1;
  Heap h = {hf, hn, 0};
  g[from] = 0;
  heap_push(&h, dist(c, from, to), from);
  while (h.n > 0) {
    int32_t cur = heap_pop(&h);
    if (closed[cur]) continue;
    closed[cur] = 1;
    out->expanded++;
    if (cur == to) {
      found = true;
      break;
    }
    int32_t nb[8], cost[8];
    int k = sr_nav_neighbors(c, cur, nb, cost);
    for (int i = 0; i < k; i++) {
      if (closed[nb[i]] || (allowed && !allowed[sr_nav_chunk_of(c, nb[i])])) continue;
      int32_t ng = g[cur] + cost[i];
      if (ng >= g[nb[i]] || h.n >= n * 8) continue;
      g[nb[i]] = ng;
      prev[nb[i]] = cur;
      heap_push(&h, (int64_t)ng + dist(c, nb[i], to), nb[i]);
    }
  }
  if (found) {
    out->length_cm = g[to];
    int count = 0;
    for (int32_t v = to; v >= 0; v = prev[v]) count++;
    out->count = count < SR_NAV_MAX_PATH ? count : SR_NAV_MAX_PATH;
    int i = count - 1;
    for (int32_t v = to; v >= 0; v = prev[v], i--)
      if (i < SR_NAV_MAX_PATH) out->node[i] = v;
  }
done:
  free(g);
  free(hf);
  free(hn);
  free(closed);
  return found;
}

bool sr_nav_path(const SrCity *c, int32_t from, int32_t to, bool hierarchical, SrNavPath *out) {
  memset(out, 0, sizeof *out);
  if (!sr_nav_node_valid(c, from) || !sr_nav_node_valid(c, to)) return false;
  if (hierarchical) {
    int cs = chunks_per_side(c);
    uint8_t *allowed = (uint8_t *)malloc((size_t)cs * (size_t)cs);
    if (!allowed) return false; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
    out->corridor = corridor(c, sr_nav_chunk_of(c, from), sr_nav_chunk_of(c, to), allowed);
    bool ok = astar(c, from, to, allowed, out);
    free(allowed);
    if (ok) return true;
    int corr = out->corridor; /* blocked inside the corridor (water): search the whole graph */
    memset(out, 0, sizeof *out);
    out->corridor = corr;
  }
  return astar(c, from, to, NULL, out);
}

int32_t sr_nav_walk_cm(const SrCity *c, int a, int b) {
  SrNavPath *p = (SrNavPath *)malloc(sizeof(SrNavPath));
  if (!p) return -1; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  int32_t len = sr_nav_path(c, sr_nav_door_node(c, a), sr_nav_door_node(c, b), true, p) ? p->length_cm : -1;
  free(p);
  return len;
}
