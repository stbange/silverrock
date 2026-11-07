/* Derived city detail (improve-city-atmosphere): facades, building shapes and roofs, and the street
 * props of every block. Everything is a pure function of the city and its seed, computed on demand
 * with hashes: the generator's random streams, the city data, seeds and saves are untouched. */
#ifndef SR_DRESSING_H
#define SR_DRESSING_H

#include "world/sr_world.h"

/* ---- facades ------------------------------------------------------------------------------------ */

enum {
  SR_WIN_OFFICE,
  SR_WIN_BALCONY,
  SR_WIN_HIVE,
  SR_WIN_CURTAIN,
  SR_WIN_BRICK,
  SR_WIN_SHUTTERED,
  SR_WIN_BOARDED,
  SR_WIN_CLERESTORY,
  SR_WIN_COUNT
};
enum { SR_BASE_SHOPFRONT, SR_BASE_SHUTTER, SR_BASE_LOBBY, SR_BASE_DOCK, SR_BASE_STALL, SR_BASE_POSTERS, SR_BASE_COUNT };
enum { SR_CROWN_PARAPET, SR_CROWN_CORNICE, SR_CROWN_NEON, SR_CROWN_NONE, SR_CROWN_COUNT };
/* Building uses for window lighting (match the renderer's SR_USE_*). */
enum { SR_FUSE_OFFICE, SR_FUSE_HOME, SR_FUSE_INDUSTRIAL, SR_FUSE_VACANT };
/* Shapes, all inside the ground footprint. */
enum {
  SR_SHAPE_BOX,
  SR_SHAPE_PODIUM,
  SR_SHAPE_SETBACK,
  SR_SHAPE_CHAMFER,
  SR_SHAPE_L,
  SR_SHAPE_SLAB,
  SR_SHAPE_ROWS,
  SR_SHAPE_COUNT
};
/* Roof kits. */
enum {
  SR_ROOF_FLAT,
  SR_ROOF_TANKS,
  SR_ROOF_HVAC,
  SR_ROOF_ANTENNAS,
  SR_ROOF_HELIPAD,
  SR_ROOF_NEON_CROWN,
  SR_ROOF_PITCHED,
  SR_ROOF_SAWTOOTH,
  SR_ROOF_CORRUGATED,
  SR_ROOF_DISHES,
  SR_ROOF_STACK,
  SR_ROOF_COUNT
};

typedef struct SrFacade {
  uint8_t winmod;   /* SR_WIN_* window module of the shaft */
  uint8_t base;     /* SR_BASE_* ground floor */
  uint8_t crown;    /* SR_CROWN_* */
  uint8_t use;      /* SR_FUSE_* */
  uint8_t shape;    /* SR_SHAPE_* */
  uint8_t roof;     /* SR_ROOF_* */
  uint8_t blank;    /* bit per face (0 -x, 1 +x, 2 -z, 3 +z): blank wall toward a close neighbour */
  uint8_t seed;     /* 0..63, varies the lit windows */
  uint8_t tint[3];  /* wall tint */
  uint8_t steps;    /* setbacks or podium floors */
  float floor_h;    /* metres per floor (3.2..4.2) */
  float uv_u, uv_v; /* texture offset, so neighbours' windows never line up */
} SrFacade;

/* The facade of a building: a pure function of the seed, the building, its district and its
 * neighbours. Buildings on one block never share both window module and ground floor. */
void sr_facade_for(const SrCity *c, int building, SrFacade *out);
uint64_t sr_facade_digest(const SrCity *c);

/* The boxes a building is made of: the ground floor fills the footprint, the shape rises above it.
 * Every volume lies inside the footprint. */
enum { SR_VOL_BASE = 1, SR_VOL_ROOF = 2, SR_VOL_CROWN = 4, SR_VOL_PITCHED = 8 };
typedef struct SrVolume {
  float x0, z0, x1, z1, y0, y1;
  uint8_t flags;
} SrVolume;
#define SR_MAX_VOLUMES 8
int sr_building_volumes(const SrCity *c, int building, const SrFacade *f, SrVolume *out);

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

/* ---- billboards ---------------------------------------------------------------------------------- */

typedef enum SrSlotKind { SR_SLOT_ROOF, SR_SLOT_WALL, SR_SLOT_HIGHWAY } SrSlotKind;
typedef struct SrBillboardSlot {
  float x, y, z; /* centre of the panel's bottom edge, metres */
  float w, h;    /* panel size */
  float yaw;     /* the panel faces +z turned by yaw */
  uint8_t kind;  /* SrSlotKind */
  uint8_t empty; /* 1 when no corp has bought it by default */
  int16_t building;
  uint32_t seed;
} SrBillboardSlot;
#define SR_MAX_BLOCK_SLOTS 8
/* Billboard slots of a block: on low flat roofs, on blank side walls, and over highways. */
int sr_billboard_slots(const SrCity *c, int block, SrBillboardSlot *out, int max);

#endif
