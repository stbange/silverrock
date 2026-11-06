#include "world/sr_host.h"

#include "core/sr_rng.h"

#include <math.h>
#include <string.h>

int sr_faction_netsec(const SrCity *c, int f) {
  if (f < 0 || f >= c->factions.count) return 2;
  const SrFaction *fa = &c->factions.f[f];
  int v;
  switch (fa->kind) {
    case SR_FK_CORP: v = 5; break;
    case SR_FK_POLICE:
    case SR_FK_GOV: v = 6; break;
    case SR_FK_MEDIA: v = 4; break;
    case SR_FK_GANG: v = 2; break;
    default: v = 3; break;
  }
  if (fa->kind == SR_FK_CORP) {
    if (fa->sector1 == SR_SEC_NET || fa->sector2 == SR_SEC_NET) v += 3;
    if (fa->sector1 == SR_SEC_CYBER || fa->sector1 == SR_SEC_FINANCE) v += 2;
    if (fa->doctrine == SR_DOC_FORTRESS) v += 1;
    if (fa->doctrine == SR_DOC_VULTURE) v -= 1;
  }
  return SR_CLAMP(v, 1, 10);
}

int sr_facility_importance(const SrCity *c, int b) {
  const SrBuilding *bd = &c->buildings[b];
  switch (bd->poi) {
    case SR_POI_CORP_HQ: return 10;
    case SR_POI_CITY_HALL: return 9;
    case SR_POI_POLICE: return 8;
    case SR_POI_CORP_FACILITY: return 6;
    case SR_POI_MEDIA: return 6;
    case SR_POI_GANG_HIDEOUT: return 4;
    case SR_POI_CLINIC:
    case SR_POI_RIPPERDOC: return 3;
    case SR_POI_HOME: return 1;
    default: return 2;
  }
}

int sr_host_security_for(int netsec, int importance) {
  int s = (netsec * 6 + importance * 4 + 5) / 10;
  return SR_CLAMP(s, 1, 10);
}

bool sr_building_networked(const SrCity *c, int b) {
  return b >= 0 && b < c->building_count && (c->buildings[b].flags & SR_BF_NETWORKED);
}

static int add_node(SrHost *h, SrNodeType type, int layer, int parent) {
  if (h->node_count >= SR_HOST_MAX_NODES) return -1;
  SrNode *n = &h->nodes[h->node_count];
  memset(n, 0, sizeof *n);
  n->type = (uint8_t)type;
  n->layer = (uint8_t)layer;
  n->parent = (int16_t)parent;
  n->device = -1;
  return h->node_count++;
}

/* Security zone of an interior room: 1 public, 2 restricted, 3 core. */
static int zone_of(const SrInterior *in, int room) {
  if (in->apartment) return 1;
  SrRoomType t = (SrRoomType)in->rooms[room];
  if (t == SR_ROOM_SECURITY || t == SR_ROOM_SERVER || t == SR_ROOM_TARGET) return 3;
  if (t == SR_ROOM_LOBBY || t == SR_ROOM_BAR) return 1;
  return 2;
}

void sr_host_generate(const SrCity *c, int building, const SrInterior *in, SrHost *h) {
  memset(h, 0, sizeof *h);
  const SrBuilding *bd = &c->buildings[building];
  h->building = (uint32_t)building;
  h->security = sr_host_security_for(sr_faction_netsec(c, bd->owner), sr_facility_importance(c, building));
  h->monitor = h->core = h->records = h->power = -1;
  SrRng r = sr_rng_stream(c->seed, "host", (uint64_t)building, 0);
  h->gateway = add_node(h, SR_NODE_GATEWAY, 0, -1);
  int junction[4];
  junction[0] = h->gateway;
  for (int l = 1; l <= 3; l++) junction[l] = add_node(h, SR_NODE_JUNCTION, l, junction[l - 1]);
  /* one controller per device, in the layer of its room's security zone */
  for (int d = 0; d < in->device_count; d++) {
    int layer = in->devices[d].type == SR_DEV_ALARM ? 3 : zone_of(in, in->devices[d].room);
    int n = add_node(h, SR_NODE_DEVICE, layer, junction[layer]);
    if (n >= 0) h->nodes[n].device = (int8_t)d;
  }
  /* data stores: always the site's security data, more with security */
  int stores = 1 + h->security / 3;
  for (int k = 0; k < stores; k++) {
    int n = add_node(h, SR_NODE_DATA, k == 0 ? 3 : 2, junction[k == 0 ? 3 : 2]);
    if (n < 0) break;
    SrNode *nd = &h->nodes[n];
    if (k == 0) {
      nd->data_kind = SR_PD_SECURITY;
    } else if (bd->poi == SR_POI_CITY_HALL || bd->poi == SR_POI_MEDIA) {
      nd->data_kind = (uint8_t)(sr_rng_chance(&r, 500) ? SR_PD_BLACKMAIL : SR_PD_PERSONNEL);
    } else if (bd->poi == SR_POI_CORP_HQ) {
      nd->data_kind = (uint8_t)(sr_rng_chance(&r, 500) ? SR_PD_FINANCE : SR_PD_RESEARCH);
    } else {
      nd->data_kind = (uint8_t)sr_rng_range(&r, SR_PD_RESEARCH, SR_PD_BLACKMAIL);
    }
    nd->data_size = (uint16_t)(20 + sr_rng_range(&r, 0, 40) * (1 + h->security / 2));
    nd->data_value = 60 * h->security + sr_rng_range(&r, 0, 80 * h->security);
  }
  h->monitor = add_node(h, SR_NODE_MONITOR, 3, junction[3]);
  bool records = bd->poi == SR_POI_CITY_HALL || bd->poi == SR_POI_POLICE ||
                 (bd->owner >= 0 && bd->owner < c->factions.count && c->factions.f[bd->owner].kind == SR_FK_CORP &&
                  c->factions.f[bd->owner].sector1 == SR_SEC_FINANCE);
  if (records) h->records = add_node(h, SR_NODE_RECORDS, 3, junction[3]);
  h->power = add_node(h, SR_NODE_POWER, 2, junction[2]);
  h->district_grid = bd->poi == SR_POI_CITY_HALL ||
                     (bd->owner >= 0 && bd->owner < c->factions.count && c->factions.f[bd->owner].kind == SR_FK_CORP &&
                      c->factions.f[bd->owner].sector1 == SR_SEC_ENERGY && bd->poi == SR_POI_CORP_FACILITY);
  h->core = add_node(h, SR_NODE_CORE, 3, junction[3]);
  /* layout: one ring per layer, stacked upwards; positions are deterministic */
  int per_layer[4] = {0}, index[SR_HOST_MAX_NODES];
  for (int i = 0; i < h->node_count; i++) index[i] = per_layer[h->nodes[i].layer]++;
  for (int i = 0; i < h->node_count; i++) {
    SrNode *n = &h->nodes[i];
    float y = (float)n->layer * SR_HOST_LAYER_H;
    if (n->type == SR_NODE_JUNCTION || n->type == SR_NODE_GATEWAY) {
      n->x = 0;
      n->z = 0;
      n->y = y;
      continue;
    }
    int count = per_layer[n->layer] - 1; /* the junction sits in the middle */
    float radius = 7.0f + 1.6f * (float)count;
    float a = 2.0f * SR_PI * (float)(index[i] - 1) / (float)(count > 0 ? count : 1);
    n->x = cosf(a) * radius;
    n->z = sinf(a) * radius;
    n->y = y + (float)((index[i] % 3) - 1) * 0.8f;
  }
}

uint64_t sr_host_digest(const SrHost *h) {
  uint64_t d = sr_hash_combine((uint64_t)h->building, (uint64_t)h->security);
  for (int i = 0; i < h->node_count; i++) {
    const SrNode *n = &h->nodes[i];
    uint64_t v = (uint64_t)n->type | (uint64_t)n->layer << 8 | (uint64_t)(uint8_t)n->device << 16 |
                 (uint64_t)n->data_kind << 24 | (uint64_t)n->data_size << 32 | (uint64_t)(uint16_t)n->parent << 48;
    d = sr_hash_combine(d, v);
  }
  return d;
}

int sr_host_device_node(const SrHost *h, int device) {
  for (int i = 0; i < h->node_count; i++)
    if (h->nodes[i].type == SR_NODE_DEVICE && h->nodes[i].device == device) return i;
  return -1;
}

const char *sr_node_type_name(SrNodeType t) {
  static const char *const n[SR_NODE_COUNT] = {"gateway",          "junction", "device", "data store",
                                               "security monitor", "records",  "power",  "admin core"};
  return (unsigned)t < SR_NODE_COUNT ? n[t] : "?";
}

const char *sr_paydata_name(SrPaydata p) {
  static const char *const n[SR_PD_COUNT] = {"research", "finance", "personnel", "blackmail", "security codes"};
  return (unsigned)p < SR_PD_COUNT ? n[p] : "?";
}

SrVec3 sr_grid_pos(int32_t x_cm, int32_t z_cm, float height_m) {
  return sr_v3((float)x_cm / 100.0f / SR_GRID_SCALE, height_m, (float)z_cm / 100.0f / SR_GRID_SCALE);
}

SrVec3 sr_grid_building_pos(const SrCity *c, int b) {
  const SrBuilding *bd = &c->buildings[b];
  return sr_grid_pos((bd->x0 + bd->x1) / 2, (bd->y0 + bd->y1) / 2, 0.4f);
}
