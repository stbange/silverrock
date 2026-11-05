/* Pedestrian navigation. Every land block has a sidewalk ring around its lot with four corner
 * nodes and one door node per building; crossings link corners across the streets. The graph
 * is defined per chunk (SR_NAV_CHUNK x SR_NAV_CHUNK blocks) and paths are found hierarchically:
 * first a corridor of chunks, then A* over the nodes inside that corridor. Integer centimetres,
 * so paths are identical on every platform. */
#ifndef SR_NAV_H
#define SR_NAV_H

#include "world/sr_world.h"

#define SR_NAV_PER_BLOCK   8 /* 4 corners + up to 4 doors */
#define SR_NAV_CHUNK       4 /* blocks per chunk side (same as render chunks) */
#define SR_NAV_MAX_PATH    1024
#define SR_NAV_MAX_CHUNK_N (SR_NAV_CHUNK * SR_NAV_CHUNK * SR_NAV_PER_BLOCK)
#define SR_NAV_RING_CM     1300 /* ring inset from the block origin (3 m into the street strip) */
#define SR_NAV_CROSSING    300  /* extra cost of crossing a street */

typedef struct SrNavPath {
  int count;
  int32_t node[SR_NAV_MAX_PATH];
  int32_t length_cm;
  int expanded; /* nodes taken off the open list (search effort) */
  int corridor; /* chunks in the corridor, 0 for a flat search */
} SrNavPath;

typedef struct SrNavChunk {
  int cx, cy;
  int node_count;
  int32_t nodes[SR_NAV_MAX_CHUNK_N];
  int edge_count; /* edges with both ends in the chunk */
  int32_t ea[SR_NAV_MAX_CHUNK_N * 4], eb[SR_NAV_MAX_CHUNK_N * 4], cost[SR_NAV_MAX_CHUNK_N * 4];
  int portal_count; /* edges leaving the chunk */
} SrNavChunk;

int32_t sr_nav_node_count(const SrCity *c);
bool sr_nav_node_valid(const SrCity *c, int32_t node);
void sr_nav_node_pos(const SrCity *c, int32_t node, int32_t *x, int32_t *y);
/* Neighbours of a node with edge costs; returns how many (at most 8). */
int sr_nav_neighbors(const SrCity *c, int32_t node, int32_t out[8], int32_t cost[8]);
int32_t sr_nav_door_node(const SrCity *c, int building);
/* Nearest node of the block containing (x, y), -1 off the land. */
int32_t sr_nav_nearest_node(const SrCity *c, int32_t x, int32_t y);
int sr_nav_chunk_of(const SrCity *c, int32_t node);
/* Connectivity invariant: every sidewalk node and door is reachable from the home door. */
bool sr_nav_connected(const SrCity *c);
/* Materialises one chunk's graph (nodes, internal edges, portal count). */
void sr_nav_chunk_build(const SrCity *c, int cx, int cy, SrNavChunk *out);
/* Shortest path; hierarchical limits the search to a corridor of chunks. */
bool sr_nav_path(const SrCity *c, int32_t from, int32_t to, bool hierarchical, SrNavPath *out);
/* Walking distance between two buildings' doors in cm, -1 if unreachable. */
int32_t sr_nav_walk_cm(const SrCity *c, int building_a, int building_b);

#endif
