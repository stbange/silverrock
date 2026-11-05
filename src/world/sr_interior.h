/* Interiors generated on approach from typed room graphs. Mission-capable interiors offer at
 * least two distinct entry routes and a complete security chain (every camera and lock is
 * linked to the security room). */
#ifndef SR_INTERIOR_H
#define SR_INTERIOR_H

#include "world/sr_world.h"

#define SR_INT_MAX_W       6
#define SR_INT_MAX_H       5
#define SR_INT_MAX_ROOMS   (SR_INT_MAX_W * SR_INT_MAX_H)
#define SR_INT_MAX_ENTRIES 6
#define SR_INT_MAX_DEVICES 48
#define SR_INT_MAX_PROPS   40
#define SR_INT_CELL_CM     600 /* one room cell is 6 m x 6 m */

typedef enum SrRoomType {
  SR_ROOM_HALL,
  SR_ROOM_OFFICE,
  SR_ROOM_STORAGE,
  SR_ROOM_SECURITY,
  SR_ROOM_SERVER,
  SR_ROOM_LAB,
  SR_ROOM_LOBBY,
  SR_ROOM_BAR,
  SR_ROOM_TARGET,
  SR_ROOM_HOME
} SrRoomType;

typedef enum SrEntryType {
  SR_ENTRY_FRONT,
  SR_ENTRY_SERVICE,
  SR_ENTRY_VENT,
  SR_ENTRY_ROOF,
  SR_ENTRY_WINDOW
} SrEntryType;

typedef enum SrDeviceType {
  SR_DEV_DOOR,
  SR_DEV_CAMERA,
  SR_DEV_TURRET,
  SR_DEV_ALARM,
  SR_DEV_LIGHTS,
  SR_DEV_ELEVATOR
} SrDeviceType;

/* Furniture and interactive objects. Positions are local centimetres from the interior's
 * north-west corner (x east, z south); room (x, y) covers [x*600, x*600+600) x [y*600, y*600+600). */
typedef enum SrPropType {
  SR_PROP_EXIT,      /* leave through the entry it belongs to */
  SR_PROP_BED,       /* sleep */
  SR_PROP_STASH,     /* store and take items */
  SR_PROP_TERMINAL,  /* messages, news, jobs */
  SR_PROP_WARDROBE,  /* change outfit */
  SR_PROP_OBJECTIVE, /* the job's objective */
  SR_PROP_CONSOLE,   /* security console: cameras, locks, alarm */
  SR_PROP_DESK,
  SR_PROP_SHELF,
  SR_PROP_PLANT,
  SR_PROP_POSTER,
  SR_PROP_LAMP,
  SR_PROP_CRATE,
  SR_PROP_RUG,
  SR_PROP_SERVER,
  SR_PROP_COUNT
} SrPropType;

typedef struct SrProp {
  uint8_t type;
  uint8_t room;
  uint8_t color;   /* palette index for decoration */
  uint8_t variant; /* decoration variant, or the entry index of an exit */
  int16_t x, z;    /* local cm */
  uint8_t facing;  /* 0 north, 1 east, 2 south, 3 west */
  uint8_t pad;
} SrProp;

typedef struct SrEntry {
  SrEntryType type;
  uint8_t room;
} SrEntry;

typedef struct SrDevice {
  uint32_t id; /* stable: building id * 64 + index */
  SrDeviceType type;
  uint8_t room;
  uint8_t door_dir; /* doors: 0 east, 1 south */
  bool locked;
  int8_t control_room; /* room index of the controlling security room, -1 none */
} SrDevice;

typedef struct SrInterior {
  uint32_t building;
  int w, h;
  uint8_t rooms[SR_INT_MAX_ROOMS]; /* SrRoomType */
  uint8_t doors[SR_INT_MAX_ROOMS]; /* bit0: door east, bit1: door south */
  uint8_t vents[SR_INT_MAX_ROOMS]; /* bit0: vent east, bit1: vent south */
  int entry_count;
  SrEntry entries[SR_INT_MAX_ENTRIES];
  int device_count;
  SrDevice devices[SR_INT_MAX_DEVICES];
  int target_room;   /* -1 for apartments */
  int security_room; /* -1 when there is none */
  int guards;
  bool apartment;
  uint8_t tier; /* apartments: SrHousing tier */
  int prop_count;
  SrProp props[SR_INT_MAX_PROPS];
} SrInterior;

void sr_interior_generate(const SrCity *c, int building, SrInterior *out);
/* The player's apartment for a housing tier (0 coffin .. 4 penthouse) with procedural decoration. */
void sr_interior_apartment(const SrCity *c, int building, int tier, SrInterior *out);
/* Index of the first prop of a type, or -1. */
int sr_interior_find_prop(const SrInterior *in, SrPropType type);
/* Room index containing a local position in cm, or -1 outside. */
int sr_interior_room_at(const SrInterior *in, int32_t x_cm, int32_t z_cm);
/* Rooms reachable from an entry, optionally allowing vents and locked doors. */
bool sr_interior_reachable(const SrInterior *in, int from_room, int to_room, bool use_vents, bool through_locked);
/* Number of entries that reach the target using distinct openings. */
int sr_interior_route_count(const SrInterior *in);
bool sr_interior_security_chain_ok(const SrInterior *in);
uint64_t sr_interior_digest(const SrInterior *in);

#endif
