/* Matrix hosts and the global grid. A host is generated from its building's interior: every
 * device becomes a device-controller node, nodes are layered by security zone (gateway, public,
 * restricted, core), and data stores, a security monitor, records, a power node and the admin
 * core are added by building type. Security 1..10 comes from the owner's Matrix defence and the
 * facility's importance. Everything is a pure function of (seed, building), so a host has the
 * same layout on every visit. The grid is the city scaled down: district plates, backbones along
 * the elevated highways and metro lines, host constructs at their buildings' scaled positions. */
#ifndef SR_HOST_H
#define SR_HOST_H

#include "core/sr_math.h"
#include "world/sr_interior.h"

#define SR_HOST_MAX_NODES 80
#define SR_GRID_SCALE     40.0f /* metres of city per metre of grid */
#define SR_HOST_LAYER_H   9.0f  /* height between host layers */

typedef enum SrNodeType {
  SR_NODE_GATEWAY,
  SR_NODE_JUNCTION,
  SR_NODE_DEVICE,  /* controls one interior device */
  SR_NODE_DATA,    /* data store with paydata */
  SR_NODE_MONITOR, /* security monitor: alarms, cameras overview, logs */
  SR_NODE_RECORDS, /* warrants, credentials, accounts */
  SR_NODE_POWER,   /* building power; district grid in power hosts */
  SR_NODE_CORE,    /* admin core */
  SR_NODE_COUNT
} SrNodeType;

typedef enum SrPaydata {
  SR_PD_RESEARCH,
  SR_PD_FINANCE,
  SR_PD_PERSONNEL,
  SR_PD_BLACKMAIL,
  SR_PD_SECURITY, /* the site's security layout: guards, cameras */
  SR_PD_COUNT
} SrPaydata;

typedef struct SrNode {
  uint8_t type;
  uint8_t layer;
  int8_t device; /* interior device index for device nodes, else -1 */
  uint8_t data_kind;
  int16_t parent;     /* conduit towards the gateway, -1 for the gateway */
  uint16_t data_size; /* megabytes */
  int32_t data_value; /* cred */
  float x, y, z;      /* host-local position */
} SrNode;

typedef struct SrHost {
  uint32_t building;
  int security; /* 1..10 */
  int node_count;
  SrNode nodes[SR_HOST_MAX_NODES];
  int gateway, monitor, core, records, power;
  bool district_grid; /* the power node can black out the whole district */
} SrHost;

/* Matrix defence of a faction (1..10); -1 (no owner) is weak. */
int sr_faction_netsec(const SrCity *c, int faction);
/* Importance of a facility (1..10): HQs and civic buildings high, shops low. */
int sr_facility_importance(const SrCity *c, int building);
int sr_host_security_for(int netsec, int importance);
bool sr_building_networked(const SrCity *c, int building);
/* Generates the host of a networked building from its interior (which must match the building). */
void sr_host_generate(const SrCity *c, int building, const SrInterior *in, SrHost *out);
uint64_t sr_host_digest(const SrHost *h);
/* Node controlling an interior device, or -1. */
int sr_host_device_node(const SrHost *h, int device);
const char *sr_node_type_name(SrNodeType t);
const char *sr_paydata_name(SrPaydata p);

/* The global grid. */
SrVec3 sr_grid_pos(int32_t x_cm, int32_t z_cm, float height_m);
SrVec3 sr_grid_building_pos(const SrCity *c, int building);

#endif
