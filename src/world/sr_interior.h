/* Interiors generated on approach from typed room graphs. Mission-capable interiors offer at
 * least two distinct entry routes and a complete security chain (every camera and lock is
 * linked to the security room). Every other building has a civilian ground floor that fits
 * its use: a shop floor, a diner, a bar, a clinic, capsules, a fixer's back room, booths, a
 * residential lobby and corridor, offices, a warehouse or a civic front desk, with the people
 * who work or wait there. */
#ifndef SR_INTERIOR_H
#define SR_INTERIOR_H

#include "world/sr_world.h"

#define SR_INT_MAX_W       6
#define SR_INT_MAX_H       5
#define SR_INT_MAX_ROOMS   (SR_INT_MAX_W * SR_INT_MAX_H)
#define SR_INT_MAX_ENTRIES 6
#define SR_INT_MAX_DEVICES 48
#define SR_INT_MAX_PROPS   64
#define SR_INT_MAX_OCC     10
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
  SR_ROOM_HOME,
  /* civilian floors */
  SR_ROOM_SHOP,
  SR_ROOM_DINER,
  SR_ROOM_WARD,
  SR_ROOM_CORRIDOR,
  SR_ROOM_WAREHOUSE,
  SR_ROOM_BACK
} SrRoomType;

/* What a building's ground floor is. */
typedef enum SrInteriorKind {
  SR_FLOOR_SITE,        /* mission layout (corporate sites, hideouts, job targets) */
  SR_FLOOR_HOME,        /* the player's apartment */
  SR_FLOOR_SHOP,        /* counter and shelves: gunshop, electronics, clothing, ripperdoc, fence, grocery */
  SR_FLOOR_FOOD,        /* noodle bar or diner: counter, stools, tables */
  SR_FLOOR_BAR,         /* long bar, stools, booths */
  SR_FLOOR_CLINIC,      /* waiting room, reception, beds */
  SR_FLOOR_CAPSULE,     /* reception and rows of sleeping pods */
  SR_FLOOR_FIXER,       /* front room and the fixer's table in the back */
  SR_FLOOR_BOOTH,       /* Matrix or UV booths and an attendant */
  SR_FLOOR_RESIDENTIAL, /* lobby, mailboxes, lift, a corridor of flat doors */
  SR_FLOOR_OFFICE,      /* reception and open-plan desks */
  SR_FLOOR_WAREHOUSE,   /* an open hall of crates and racks */
  SR_FLOOR_CIVIC,       /* police, city hall, media: a lobby with a front desk */
  SR_FLOOR_COUNT
} SrInteriorKind;

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
  /* civilian floors */
  SR_PROP_COUNTER, /* where the place is served: buy, eat, treat, sleep, jobs, talk (variant: look) */
  SR_PROP_TABLE,
  SR_PROP_SEAT,      /* stool or chair */
  SR_PROP_FLAT_DOOR, /* a resident's front door: private (variant: flat number) */
  SR_PROP_MAILBOX,
  SR_PROP_LIFT,  /* the lift to the upper floors: residents only */
  SR_PROP_POD,   /* a sleeping capsule */
  SR_PROP_BOOTH, /* a Matrix or UV booth */
  SR_PROP_COUNT
} SrPropType;

/* Counter looks (SrProp.variant of SR_PROP_COUNTER). */
enum { SR_COUNTER_SHOP, SR_COUNTER_BAR, SR_COUNTER_DESK, SR_COUNTER_TABLE };

/* People on a civilian floor: who they are and what they do. */
typedef enum SrOccRole { SR_OCC_KEEPER, SR_OCC_PATRON, SR_OCC_RESIDENT, SR_OCC_WORKER, SR_OCC_OFFICER } SrOccRole;
typedef enum SrOccPose { SR_POSE_STAND, SR_POSE_SIT, SR_POSE_LEAN, SR_POSE_EAT, SR_POSE_TALK, SR_POSE_PHONE } SrOccPose;
typedef struct SrOccupant {
  int16_t x, z;   /* local cm */
  uint8_t facing; /* 0 north, 1 east, 2 south, 3 west */
  uint8_t pose;   /* SrOccPose */
  uint8_t role;   /* SrOccRole */
  uint8_t room;
} SrOccupant;

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
  uint8_t kind; /* SrInteriorKind */
  int prop_count;
  SrProp props[SR_INT_MAX_PROPS];
  int occ_count;
  SrOccupant occ[SR_INT_MAX_OCC];
} SrInterior;

/* Doors bits: 1 door east, 2 door south; 4 and 8 mark the connection as open floor (no wall). */
#define SR_INT_OPEN_E 4
#define SR_INT_OPEN_S 8

void sr_interior_generate(const SrCity *c, int building, SrInterior *out);
/* What a building's ground floor is when it is not a mission site or the player's home. */
SrInteriorKind sr_interior_kind(const SrCity *c, int building);
/* The civilian ground floor of a building (kind from sr_interior_kind). */
void sr_interior_civilian(const SrCity *c, int building, SrInterior *out);
const char *sr_interior_kind_name(SrInteriorKind k);
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
