/* The generated city: geography, districts, blocks, buildings, places of interest, metro,
 * factions. Everything here is produced by sr_city_generate() from a seed using integer math
 * only, so the same seed gives a bit-identical city on every platform. Units: centimetres. */
#ifndef SR_WORLD_H
#define SR_WORLD_H

#include "core/sr_types.h"

#define SR_GEN_VERSION     1u
#define SR_BLOCK_CM        9600 /* block pitch including the street */
#define SR_STREET_CM       1600 /* street width */
#define SR_MAX_GRID        42
#define SR_MAX_BLOCKS      (SR_MAX_GRID * SR_MAX_GRID)
#define SR_MAX_BUILDINGS   (SR_MAX_BLOCKS * 4)
#define SR_MAX_DISTRICTS   14
#define SR_MAX_STATIONS    24
#define SR_MAX_METRO_EDGES 48
#define SR_MAX_SKYWAYS     160
#define SR_MAX_FACTIONS    32
#define SR_NAME_LEN        40

typedef enum SrDistrictType {
  SR_D_SPIRE,
  SR_D_CIVIC,
  SR_D_COMMERCIAL,
  SR_D_ENTERTAINMENT,
  SR_D_RESIDENTIAL,
  SR_D_OLDQUARTER,
  SR_D_INDUSTRIAL,
  SR_D_DOCKS,
  SR_D_CAMPUS,
  SR_D_UNDERCITY,
  SR_D_BARRENS,
  SR_D_COUNT
} SrDistrictType;

typedef enum SrPoi {
  SR_POI_NONE,
  SR_POI_FOOD,
  SR_POI_BAR,
  SR_POI_CLINIC,
  SR_POI_RIPPERDOC,
  SR_POI_GUNSHOP,
  SR_POI_ELECTRONICS,
  SR_POI_CLOTHING,
  SR_POI_FENCE,
  SR_POI_CAPSULE,
  SR_POI_MATRIX_BOOTH,
  SR_POI_FIXER,
  SR_POI_POLICE,
  SR_POI_GROCERY,
  SR_POI_UV_BOOTH,
  /* special places assigned by generation, not by weight tables */
  SR_POI_CITY_HALL,
  SR_POI_CORP_HQ,
  SR_POI_CORP_FACILITY,
  SR_POI_GANG_HIDEOUT,
  SR_POI_MEDIA,
  SR_POI_HOME,
  SR_POI_COUNT
} SrPoi;
#define SR_POI_FIRST_SPECIAL SR_POI_CITY_HALL

enum SrBuildingFlags {
  SR_BF_ENTERABLE = 1 << 0,
  SR_BF_NETWORKED = 1 << 1,
  SR_BF_MISSION = 1 << 2,
  SR_BF_LANDMARK = 1 << 3,
  SR_BF_ROOF_ACCESS = 1 << 4,
  SR_BF_ELEVATOR = 1 << 5
};

typedef struct SrDistrict {
  SrDistrictType type;
  char name[SR_NAME_LEN];
  int32_t cx, cy; /* centroid in cm */
  int16_t station;
  int16_t block_count;
  int16_t wealth; /* 0..100 */
  bool touches_water;
  bool touches_edge;
} SrDistrict;

typedef struct SrBlock {
  uint8_t water;
  int8_t district;
  uint8_t highway; /* elevated highway overhead: Undercity layer below */
  uint8_t building_count;
  uint16_t first_building;
} SrBlock;

typedef struct SrBuilding {
  uint32_t id;
  int32_t x0, y0, x1, y1; /* footprint */
  int32_t height_cm;
  uint16_t block;
  uint8_t floors;
  uint8_t style;
  uint8_t poi;
  uint8_t flags;
  int8_t owner;          /* faction index or -1 */
  uint8_t silhouette[4]; /* landmark shape parameters: tiers, taper, crown, width */
  char name[SR_NAME_LEN];
} SrBuilding;

typedef struct SrStation {
  int16_t block;
  int8_t district;
} SrStation;

typedef struct SrEdge {
  int16_t a, b;
} SrEdge;

typedef struct SrSkyway {
  uint16_t a, b; /* building indices */
} SrSkyway;

/* ---- factions ---------------------------------------------------------- */

typedef enum SrFactionKind {
  SR_FK_CORP,
  SR_FK_GANG,
  SR_FK_GOV,
  SR_FK_POLICE,
  SR_FK_MEDIA,
  SR_FK_COLLECTIVE,
  SR_FK_UNION,
  SR_FK_CHURCH
} SrFactionKind;

typedef enum SrSector {
  SR_SEC_ARMS,
  SR_SEC_CYBER,
  SR_SEC_BIO,
  SR_SEC_MEDIA,
  SR_SEC_NET,
  SR_SEC_FINANCE,
  SR_SEC_ENERGY,
  SR_SEC_LOGISTICS,
  SR_SEC_CONSTRUCTION,
  SR_SEC_ORBITAL,
  SR_SEC_COUNT
} SrSector;

typedef enum SrDoctrine {
  SR_DOC_SHARK,
  SR_DOC_FORTRESS,
  SR_DOC_SPIDER,
  SR_DOC_PROPHET,
  SR_DOC_VULTURE,
  SR_DOC_COUNT
} SrDoctrine;

typedef struct SrFaction {
  SrFactionKind kind;
  char name[SR_NAME_LEN];
  char blackops[SR_NAME_LEN]; /* corps only */
  uint8_t sector1, sector2, doctrine;
  uint8_t hue; /* 0..23, 15 degree steps */
  uint8_t logo_shape, logo_glyph, logo_frame;
  uint8_t icon;   /* unique per faction */
  uint8_t marker; /* unique member silhouette marker */
  uint8_t theme;  /* gangs */
  int8_t owner;   /* media: owning corp or -1 */
  int32_t hq_building;
  int16_t facilities[8];
  uint8_t facility_count;
  uint16_t turf; /* district bitmask (gangs) */
} SrFaction;

typedef struct SrCouncillor {
  char name[SR_NAME_LEN];
  int8_t lean;   /* -2 reform .. +2 law & order */
  int8_t patron; /* corp faction index or -1 */
} SrCouncillor;

typedef struct SrFactions {
  int count;
  SrFaction f[SR_MAX_FACTIONS];
  int corp_count; /* corps occupy indices [0, corp_count) */
  int gang_first, gang_count;
  int gov, police, collective, union_, church;
  int media_first, media_count;
  int8_t rel[SR_MAX_FACTIONS][SR_MAX_FACTIONS]; /* -100..100 */
  char mayor[SR_NAME_LEN];
  int8_t mayor_patron;
  int council_count;
  SrCouncillor council[9];
  int precinct_count;
  int32_t precinct_building[SR_MAX_DISTRICTS];
  int8_t precinct_of_district[SR_MAX_DISTRICTS];
  int32_t fixer_building[SR_MAX_DISTRICTS];
} SrFactions;

/* ---- city ---------------------------------------------------------------- */

typedef enum SrCitySize { SR_CITY_MEDIUM = 0, SR_CITY_LARGE = 1 } SrCitySize;

typedef struct SrCityOptions {
  SrCitySize size;
  int corp_count; /* 0 = random 5..10 */
} SrCityOptions;

typedef struct SrCity {
  uint64_t seed;
  uint32_t gen_version;
  SrCityOptions opt;
  char name[SR_NAME_LEN];
  int grid;       /* grid x grid blocks */
  int coast_side; /* 0 west, 1 east, 2 north, 3 south, 4 inland */
  SrBlock blocks[SR_MAX_BLOCKS];
  int district_count;
  SrDistrict districts[SR_MAX_DISTRICTS];
  int building_count;
  SrBuilding buildings[SR_MAX_BUILDINGS];
  int station_count;
  SrStation stations[SR_MAX_STATIONS];
  int metro_edge_count;
  SrEdge metro_edges[SR_MAX_METRO_EDGES];
  int skyway_count;
  SrSkyway skyways[SR_MAX_SKYWAYS];
  int32_t home_building;
  int32_t landlord_building;
  SrFactions factions;
  int rerolls;
  int force_invariant_failures; /* test hook: fail validation this many times */
} SrCity;

/* Allocates and generates a city. Returns NULL only on allocation failure. */
SrCity *sr_city_create(uint64_t seed, const SrCityOptions *opt);
void sr_city_destroy(SrCity *c);
/* Generates into an existing allocation. */
SrResult sr_city_generate(SrCity *c, uint64_t seed, const SrCityOptions *opt);
/* Returns 0 if valid, or a non-zero code naming the first violated invariant. */
int sr_city_validate(const SrCity *c);
const char *sr_city_invariant_name(int code);
uint64_t sr_city_digest(const SrCity *c);

/* Geometry helpers */
int sr_city_block_index(const SrCity *c, int bx, int by);
void sr_city_block_origin(int block, int grid, int32_t *x, int32_t *y);
int sr_city_block_at(const SrCity *c, int32_t x, int32_t y); /* -1 outside */
void sr_building_center(const SrBuilding *b, int32_t *x, int32_t *y);
int sr_city_district_at(const SrCity *c, int32_t x, int32_t y);
/* Walking distance between two buildings over land blocks (cm); -1 if unreachable. */
int32_t sr_city_walk_dist(const SrCity *c, int building_a, int building_b);
/* Block steps between two blocks over land; -1 unreachable. */
int sr_city_block_steps(const SrCity *c, int block_a, int block_b);
/* Metro hops between stations; -1 if unconnected. */
int sr_city_metro_hops(const SrCity *c, int station_a, int station_b);
int sr_city_nearest_station(const SrCity *c, int block);
int sr_city_find_poi_near(const SrCity *c, SrPoi poi, int from_building, int32_t *dist_out);
const char *sr_district_type_name(SrDistrictType t);
const char *sr_poi_name(SrPoi p);

/* Factions (called by city generation) */
void sr_factions_generate(SrCity *c, uint64_t seed);
int sr_factions_validate(const SrCity *c);
const char *sr_sector_name(SrSector s);
const char *sr_doctrine_name(SrDoctrine d);

/* Names */
void sr_name_person(uint64_t seed, uint64_t id, char *out, size_t cap);
void sr_name_corp(uint64_t seed, uint64_t id, char *out, size_t cap);
void sr_name_gang(uint64_t seed, uint64_t id, char *out, size_t cap);
void sr_name_district(uint64_t seed, uint64_t id, SrDistrictType t, char *out, size_t cap);
void sr_name_city(uint64_t seed, char *out, size_t cap);
void sr_name_business(uint64_t seed, uint64_t id, SrPoi poi, char *out, size_t cap);
void sr_name_blackops(uint64_t seed, uint64_t id, char *out, size_t cap);
bool sr_name_blocked(const char *name);

/* Seed codes: 13 base32 symbols carry the 64-bit seed, 2 symbols carry a checksum. */
#define SR_SEED_CODE_LEN 18 /* "XXXXX-XXXXX-XXXXX" + NUL */
void sr_seed_to_code(uint64_t seed, char out[SR_SEED_CODE_LEN]);
bool sr_seed_from_code(const char *code, uint64_t *seed_out);

#endif
