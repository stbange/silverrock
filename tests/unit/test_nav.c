/* Pedestrian navigation graphs and hierarchical path search. */
#include "core/sr_rng.h"
#include "sr_test.h"
#include "world/sr_nav.h"

#include <stdlib.h>

/* Every valid node reachable from a start node (graph BFS). */
static int reachable(const SrCity *c, int32_t start, uint8_t *seen) {
  int32_t n = sr_nav_node_count(c);
  memset(seen, 0, (size_t)n);
  int32_t *queue = (int32_t *)malloc(sizeof(int32_t) * (size_t)n);
  int head = 0, tail = 0, count = 0;
  queue[tail++] = start;
  seen[start] = 1;
  while (head < tail) {
    int32_t cur = queue[head++];
    count++;
    int32_t nb[8], cost[8];
    int k = sr_nav_neighbors(c, cur, nb, cost);
    for (int i = 0; i < k; i++) {
      if (seen[nb[i]]) continue;
      seen[nb[i]] = 1;
      queue[tail++] = nb[i];
    }
  }
  free(queue);
  return count;
}

static void test_nav_connectivity(void) {
  /* every sidewalk node and every door is reachable from home; the seed sweep checks 50+ seeds */
  SrCity *c = (SrCity *)malloc(sizeof(SrCity));
  uint8_t *seen = (uint8_t *)malloc((size_t)SR_MAX_BLOCKS * SR_NAV_PER_BLOCK);
  int bad = 0;
  for (int seed = 0; seed < 4; seed++) {
    sr_city_generate(c, 9000u + (uint64_t)seed, NULL);
    int32_t home = sr_nav_door_node(c, c->home_building);
    SR_CHECK(sr_nav_node_valid(c, home));
    int got = reachable(c, home, seen);
    int valid = 0;
    for (int32_t v = 0; v < sr_nav_node_count(c); v++) {
      if (!sr_nav_node_valid(c, v)) continue;
      valid++;
      if (!seen[v]) bad++;
    }
    SR_CHECK_EQ(got, valid);
    SR_CHECK(sr_nav_connected(c));
    /* edges are symmetric with equal costs */
    for (int32_t v = 0; v < sr_nav_node_count(c); v += 37) {
      int32_t nb[8], cost[8];
      int k = sr_nav_neighbors(c, v, nb, cost);
      for (int i = 0; i < k; i++) {
        int32_t back[8], bc[8];
        int m = sr_nav_neighbors(c, nb[i], back, bc);
        bool found = false;
        for (int j = 0; j < m; j++) found = found || (back[j] == v && bc[j] == cost[i]);
        SR_CHECK(found);
      }
    }
  }
  SR_CHECK_EQ(bad, 0);
  free(seen);
  free(c);
}

static void test_nav_paths(void) {
  SrCity *c = sr_city_create(4711, NULL);
  SrNavPath *flat = (SrNavPath *)malloc(sizeof(SrNavPath)), *hier = (SrNavPath *)malloc(sizeof(SrNavPath));
  SrRng r = sr_rng_stream(1, "nav-test", 0, 0);
  int compared = 0;
  long flat_work = 0, hier_work = 0;
  for (int i = 0; i < 40; i++) {
    int a = sr_rng_range(&r, 0, c->building_count - 1), b = sr_rng_range(&r, 0, c->building_count - 1);
    int32_t na = sr_nav_door_node(c, a), nb = sr_nav_door_node(c, b);
    SR_CHECK(sr_nav_path(c, na, nb, false, flat));
    SR_CHECK(sr_nav_path(c, na, nb, true, hier));
    SR_CHECK_EQ(flat->node[0], na);
    SR_CHECK_EQ(flat->node[flat->count - 1], nb);
    SR_CHECK_EQ(hier->node[hier->count - 1], nb);
    /* the corridor search finds the same or a slightly longer route with less work */
    SR_CHECK(hier->length_cm >= flat->length_cm);
    SR_CHECK(hier->length_cm <= flat->length_cm + flat->length_cm / 4 + 2000);
    flat_work += flat->expanded;
    hier_work += hier->expanded;
    compared++;
    /* consecutive nodes are neighbours */
    for (int k = 1; k < hier->count; k++) {
      int32_t nbs[8], cost[8];
      int n = sr_nav_neighbors(c, hier->node[k - 1], nbs, cost);
      bool ok = false;
      for (int j = 0; j < n; j++) ok = ok || nbs[j] == hier->node[k];
      SR_CHECK(ok);
    }
    SR_CHECK(sr_nav_walk_cm(c, a, b) == hier->length_cm);
  }
  printf("  nav search work: flat %ld nodes, hierarchical %ld nodes over %d paths\n", flat_work, hier_work, compared);
  SR_CHECK(hier_work <= flat_work);
  /* degenerate cases */
  SR_CHECK(!sr_nav_path(c, -1, 0, true, flat));
  SR_CHECK(!sr_nav_path(c, 0, sr_nav_node_count(c), false, flat));
  SR_CHECK_EQ(sr_nav_neighbors(c, -5, NULL, NULL), 0);
  int32_t d = sr_nav_door_node(c, 0);
  SR_CHECK(sr_nav_path(c, d, d, true, flat));
  SR_CHECK_EQ(flat->length_cm, 0);
  /* nearest node: corners and doors of the block the point is in */
  int32_t x, y;
  sr_nav_node_pos(c, d, &x, &y);
  SR_CHECK_EQ(sr_nav_nearest_node(c, x, y), d);
  SR_CHECK_EQ(sr_nav_nearest_node(c, -100000, -100000), -1);
  int water = -1;
  for (int b = 0; b < c->grid * c->grid && water < 0; b++)
    if (c->blocks[b].water) water = b;
  if (water >= 0) {
    int32_t ox, oy;
    sr_city_block_origin(water, c->grid, &ox, &oy);
    SR_CHECK_EQ(sr_nav_nearest_node(c, ox + 100, oy + 100), -1);
    SR_CHECK(!sr_nav_node_valid(c, water * SR_NAV_PER_BLOCK));
  }
  /* corners lie on the sidewalk, outside every building */
  for (int b = 0; b < c->grid * c->grid; b += 7) {
    if (c->blocks[b].water) continue;
    for (int k = 0; k < 4; k++) {
      sr_nav_node_pos(c, b * SR_NAV_PER_BLOCK + k, &x, &y);
      const SrBlock *blk = &c->blocks[b];
      for (int i = 0; i < blk->building_count; i++) {
        const SrBuilding *bd = &c->buildings[blk->first_building + i];
        SR_CHECK(!(x > bd->x0 && x < bd->x1 && y > bd->y0 && y < bd->y1));
      }
    }
  }
  free(flat);
  free(hier);
  sr_city_destroy(c);
}

static void test_nav_chunks(void) {
  SrCity *c = sr_city_create(4712, NULL);
  SrNavChunk *ch = (SrNavChunk *)malloc(sizeof(SrNavChunk));
  int cs = (c->grid + SR_NAV_CHUNK - 1) / SR_NAV_CHUNK, total = 0, portals = 0;
  for (int cy = 0; cy < cs; cy++)
    for (int cx = 0; cx < cs; cx++) {
      sr_nav_chunk_build(c, cx, cy, ch);
      total += ch->node_count;
      portals += ch->portal_count;
      for (int i = 0; i < ch->node_count; i++) SR_CHECK_EQ(sr_nav_chunk_of(c, ch->nodes[i]), cy * cs + cx);
      for (int e = 0; e < ch->edge_count; e++) SR_CHECK(ch->cost[e] > 0);
    }
  int valid = 0;
  for (int32_t v = 0; v < sr_nav_node_count(c); v++) valid += sr_nav_node_valid(c, v);
  SR_CHECK_EQ(total, valid); /* chunks partition the graph */
  SR_CHECK(portals > 0);
  free(ch);
  sr_city_destroy(c);
}

void suite_nav(void) {
  SR_RUN(test_nav_connectivity);
  SR_RUN(test_nav_paths);
  SR_RUN(test_nav_chunks);
}
