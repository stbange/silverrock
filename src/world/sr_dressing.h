/* Derived city detail (improve-city-atmosphere): facades, building shapes and roofs, and the street
 * props of every block. Everything is a pure function of the city and its seed, computed on demand
 * with hashes: the generator's random streams, the city data, seeds and saves are untouched. */
#ifndef SR_DRESSING_H
#define SR_DRESSING_H

#include "world/sr_world.h"

/* ---- street props ---------------------------------------------------------------------------- */

typedef enum SrStreetPropKind {
  SR_SP_LAMP,          /* street lamp (kerb band) */
  SR_SP_TRAFFIC_LIGHT, /* at intersections */
  SR_SP_BOLLARD,
  SR_SP_HYDRANT,
  SR_SP_BIN,
  SR_SP_METER,
  SR_SP_TREE,
  SR_SP_PLANTER,
  SR_SP_NEWSBOX,
  SR_SP_CAR, /* parked in the road's parking lane */
  SR_SP_VENDING,
  SR_SP_BENCH,
  SR_SP_AC,
  SR_SP_STALL,
  SR_SP_DUMPSTER,
  SR_SP_CRATES,
  SR_SP_TRASH,
  SR_SP_BOOTH, /* public Matrix or phone booth */
  SR_SP_MANHOLE,
  SR_SP_DRAIN,
  SR_SP_VENT, /* steam vent */
  SR_SP_PUDDLE,
  SR_SP_BARREL, /* burning barrel */
  SR_SP_BARRIER,
  SR_SP_FENCE,
  SR_SP_CONTAINER,
  SR_SP_RUBBLE,
  SR_SP_BURNED_CAR,
  SR_SP_SHRINE,
  SR_SP_FLAG,
  SR_SP_HOLOTREE,
  SR_SP_FIRE_ESCAPE, /* on a side wall: x/z is the wall foot, yaw faces out */
  SR_SP_COUNT
} SrStreetPropKind;

enum {
  SR_SPF_SOLID = 1, /* blocks movement */
  SR_SPF_LIT = 2,   /* carries a light (kept at every detail level) */
  SR_SPF_STEAM = 4, /* steams (more in rain) */
  SR_SPF_ALLEY = 8  /* placed in an alley */
};

typedef enum SrStreetPropBand { SR_BAND_ROAD, SR_BAND_KERB, SR_BAND_BUILDING, SR_BAND_ALLEY } SrStreetPropBand;

typedef struct SrStreetProp {
  uint8_t kind, variant, flags, band;
  float x, z; /* metres */
  float yaw;  /* radians; props face +z rotated by yaw */
} SrStreetProp;

typedef struct SrStreetPropInfo {
  const char *name;
  float w, d, h; /* footprint and height in metres */
  uint8_t flags; /* SR_SPF_* defaults */
} SrStreetPropInfo;

const SrStreetPropInfo *sr_street_prop_info(int kind);

#define SR_MAX_BLOCK_PROPS 160

/* All props of a block in a fixed order (deterministic for the seed); returns the count. */
int sr_dressing_block(const SrCity *c, int block, SrStreetProp *out, int max);
/* Pavement width (metres) between the kerb and the nearest building face along each of the block's
 * four kerbs (0 west, 1 north, 2 east, 3 south); used to keep the walking lane clear. */
void sr_block_pavements(const SrCity *c, int block, float out[4]);
/* Distance in metres from a kerb to the middle of the clear walking lane. */
float sr_walk_lane(float pavement_m);
/* True when a solid prop of the block covers the point (metres). */
bool sr_dressing_blocks(const SrCity *c, float x, float z);
uint64_t sr_dressing_digest(const SrCity *c);

#endif
