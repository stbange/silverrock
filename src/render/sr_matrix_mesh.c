/* The Matrix: glowing wireframes on black. The grid is the city scaled down (district plates,
 * backbones along highways and metro lines, host constructs); inside a host, layers of nodes
 * linked by conduits, and the ICE that guards them. Wireframe vertices carry ground = 2, which
 * the shader draws as glowing quad edges. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <string.h>

static SrColor rgb(int r, int g, int b) { return (SrColor){(uint8_t)r, (uint8_t)g, (uint8_t)b, 255}; }

static void wire_from(SrMesh *m, int v0) {
  for (int v = v0; v < m->nv; v++) {
    m->v[v].ground = 2;
    m->v[v].emissive = 255;
    m->v[v].layer = SR_LAYER_WHITE;
  }
}

void sr_mesh_wire_box(SrMesh *m, SrVec3 lo, SrVec3 hi, SrColor c) {
  int v0 = m->nv;
  sr_mesh_box(m, lo, hi, c, SR_LAYER_WHITE, 0, 255);
  wire_from(m, v0);
}

/* Hollow floor tile: only its edges are drawn (ground = 3). */
static void wire_quad(SrMesh *m, float x0, float z0, float x1, float z1, float y, SrColor c) {
  int v0 = m->nv;
  sr_mesh_quad(m, sr_v3(x0, y, z1), sr_v3(x1, y, z1), sr_v3(x1, y, z0), sr_v3(x0, y, z0), 0, 0, 1, 1, c, SR_LAYER_WHITE,
               255, 0);
  wire_from(m, v0);
  for (int v = v0; v < m->nv; v++) m->v[v].ground = 3;
}

static void wire_diamond(SrMesh *m, SrVec3 p, float r, SrColor c, float spin) {
  int v0 = m->nv;
  SrVec3 o = sr_v3(0, 0, 0);
  sr_mesh_part(m, p, spin, o, 0.785f, sr_v3(-r, -r, -r), sr_v3(r, r, r), c, SR_LAYER_WHITE, 255);
  wire_from(m, v0);
}

/* A beam between two points: a chain of thin wire boxes. */
static void beam(SrMesh *m, SrVec3 a, SrVec3 b, float w, SrColor c) {
  SrVec3 d = sr_v3_sub(b, a);
  float len = sr_v3_len(d);
  int n = (int)(len / 2.0f) + 1;
  for (int i = 0; i < n; i++) {
    SrVec3 p0 = sr_v3_add(a, sr_v3_scale(d, (float)i / (float)n));
    SrVec3 p1 = sr_v3_add(a, sr_v3_scale(d, (float)(i + 1) / (float)n));
    SrVec3 lo = sr_v3(SR_MIN(p0.x, p1.x) - w, SR_MIN(p0.y, p1.y) - w, SR_MIN(p0.z, p1.z) - w);
    SrVec3 hi = sr_v3(SR_MAX(p0.x, p1.x) + w, SR_MAX(p0.y, p1.y) + w, SR_MAX(p0.z, p1.z) + w);
    sr_mesh_wire_box(m, lo, hi, c);
  }
}

static SrColor owner_color(const SrCity *c, int b) {
  int o = c->buildings[b].owner;
  if (o >= 0 && o < c->factions.count) return sr_corp_color(c->factions.f[o].hue);
  return rgb(70, 200, 255);
}

void sr_build_grid(const SrCity *c, SrMesh *m) {
  sr_mesh_clear(m);
  float cell = (float)SR_BLOCK_CM / 100.0f / SR_GRID_SCALE;
  for (int b = 0; b < c->grid * c->grid; b++) {
    if (c->blocks[b].water) continue;
    int32_t ox, oy;
    sr_city_block_origin(b, c->grid, &ox, &oy);
    SrVec3 p = sr_grid_pos(ox, oy, 0);
    SrColor dc = sr_palette(c->districts[c->blocks[b].district].type)->neon[0];
    dc = (SrColor){(uint8_t)(dc.r / 3), (uint8_t)(dc.g / 3), (uint8_t)(dc.b / 3), 255};
    wire_quad(m, p.x + 0.05f, p.z + 0.05f, p.x + cell - 0.05f, p.z + cell - 0.05f, 0, dc);
    if (c->blocks[b].highway) /* backbone along the elevated highway */
      sr_mesh_wire_box(m, sr_v3(p.x, 0.2f, p.z + 0.1f), sr_v3(p.x + cell, 0.35f, p.z + 0.3f), rgb(120, 220, 255));
  }
  for (int e = 0; e < c->metro_edge_count; e++) { /* metro lines as raised backbones */
    int32_t ax, ay, bx, by;
    sr_city_block_origin(c->stations[c->metro_edges[e].a].block, c->grid, &ax, &ay);
    sr_city_block_origin(c->stations[c->metro_edges[e].b].block, c->grid, &bx, &by);
    SrVec3 a = sr_grid_pos(ax + 400, ay + 400, 1.2f), bb = sr_grid_pos(bx + 400, by + 400, 1.2f);
    beam(m, a, bb, 0.06f, rgb(255, 120, 230));
  }
  for (int b = 0; b < c->building_count; b++) {
    if (!sr_building_networked(c, b)) continue;
    const SrBuilding *bd = &c->buildings[b];
    SrVec3 p = sr_grid_building_pos(c, b);
    float hw = (float)(bd->x1 - bd->x0) / 200.0f / SR_GRID_SCALE * 0.7f;
    float hd = (float)(bd->y1 - bd->y0) / 200.0f / SR_GRID_SCALE * 0.7f;
    float h = 0.4f + 0.35f * (float)sr_facility_importance(c, b);
    sr_mesh_wire_box(m, sr_v3(p.x - hw, 0.05f, p.z - hd), sr_v3(p.x + hw, h, p.z + hd), owner_color(c, b));
    if (bd->poi == SR_POI_CORP_HQ) wire_diamond(m, sr_v3(p.x, h + 0.8f, p.z), 0.4f, owner_color(c, b), 0);
  }
}

static SrColor node_color(const SrGame *g, int i) {
  const SrNode *n = &g->dive.host.nodes[i];
  if (sr_game_node_locked(g, i)) return rgb(255, 60, 60);
  switch ((SrNodeType)n->type) {
    case SR_NODE_GATEWAY: return rgb(80, 255, 140);
    case SR_NODE_DATA: return rgb(255, 210, 60);
    case SR_NODE_DEVICE: return rgb(80, 200, 255);
    case SR_NODE_RECORDS: return rgb(200, 120, 255);
    case SR_NODE_POWER: return rgb(255, 150, 40);
    case SR_NODE_CORE: return rgb(255, 255, 255);
    default: return rgb(120, 140, 200);
  }
}

static SrColor ice_color(SrIceType t) {
  static const SrColor k[SR_ICE_COUNT] = {{255, 230, 80, 255}, {255, 60, 60, 255}, {170, 80, 255, 255},
                                          {60, 255, 255, 255}, {255, 40, 40, 255}, {120, 0, 30, 255},
                                          {255, 255, 255, 255}};
  return k[t];
}

void sr_build_host(const SrGame *g, float t, SrMesh *m) {
  sr_mesh_clear(m);
  const SrDive *d = &g->dive;
  const SrHost *h = &d->host;
  for (int l = 0; l <= 3; l++) { /* layer platforms: tiled, so the wireframe reads as a floor */
    int n = 6 + 2 * l;
    float tile = 3.0f, r = tile * (float)n / 2.0f, y = (float)l * SR_HOST_LAYER_H - 0.5f;
    SrColor c = l == 3 ? rgb(60, 30, 90) : rgb(20, 50, 110);
    for (int i = 0; i < n; i++)
      for (int k = 0; k < n; k++) {
        float x0 = -r + tile * (float)i, z0 = -r + tile * (float)k;
        if (l > 0 && i > 0 && k > 0 && i < n - 1 && k < n - 1 && (i + k) % 3 == 0) continue; /* gaps to look through */
        wire_quad(m, x0, z0, x0 + tile, z0 + tile, y, c);
      }
  }
  for (int i = 0; i < h->node_count; i++) {
    const SrNode *n = &h->nodes[i];
    SrVec3 p = sr_v3(n->x, n->y + 1.5f, n->z);
    SrColor c = node_color(g, i);
    if (n->parent >= 0) {
      const SrNode *pa = &h->nodes[n->parent];
      SrVec3 q = sr_v3(pa->x, pa->y + 1.5f, pa->z);
      beam(m, p, q, 0.05f, rgb(c.r / 3, c.g / 3, c.b / 3));
      float k = fmodf(t * 0.5f + (float)i * 0.13f, 1.0f); /* a data packet running along the conduit */
      SrVec3 pk = sr_v3_add(q, sr_v3_scale(sr_v3_sub(p, q), k));
      sr_mesh_wire_box(m, sr_v3(pk.x - 0.12f, pk.y - 0.12f, pk.z - 0.12f),
                       sr_v3(pk.x + 0.12f, pk.y + 0.12f, pk.z + 0.12f), c);
    }
    switch ((SrNodeType)n->type) {
      case SR_NODE_GATEWAY:
        sr_mesh_wire_box(m, sr_v3(p.x - 1.6f, p.y - 1.6f, p.z - 0.3f), sr_v3(p.x + 1.6f, p.y + 1.6f, p.z + 0.3f), c);
        break;
      case SR_NODE_JUNCTION: wire_diamond(m, p, 0.9f, c, t * 0.4f); break;
      case SR_NODE_DATA:
        sr_mesh_wire_box(m, sr_v3(p.x - 1.0f, p.y - 1.4f, p.z - 1.0f), sr_v3(p.x + 1.0f, p.y + 1.4f, p.z + 1.0f), c);
        break;
      case SR_NODE_CORE: wire_diamond(m, p, 1.4f, c, t * 0.8f); break;
      default:
        sr_mesh_wire_box(m, sr_v3(p.x - 0.8f, p.y - 0.8f, p.z - 0.8f), sr_v3(p.x + 0.8f, p.y + 0.8f, p.z + 0.8f), c);
        break;
    }
  }
  for (int k = 0; k < d->ice_count; k++) {
    const SrIce *ice = &d->ice[k];
    if (!ice->active) continue;
    SrVec3 p = ice->e.pos;
    SrColor c = ice_color((SrIceType)ice->type);
    if (ice->type == SR_ICE_BARRIER) {
      sr_mesh_wire_box(m, sr_v3(p.x - 2.0f, p.y - 2.0f, p.z - 2.0f), sr_v3(p.x + 2.0f, p.y + 2.0f, p.z + 2.0f), c);
    } else {
      float r = ice->type == SR_ICE_BLACK || ice->type == SR_ICE_KILLER ? 0.8f : 0.55f;
      wire_diamond(m, p, r, c, t * 2.0f + (float)k);
      if (ice->type == SR_ICE_WATCHER) /* the eye */
        sr_mesh_wire_box(m, sr_v3(p.x - 0.15f, p.y - 0.15f, p.z - 0.15f), sr_v3(p.x + 0.15f, p.y + 0.15f, p.z + 0.15f),
                         rgb(255, 255, 255));
    }
  }
}
