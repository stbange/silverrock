/* Interiors generated on approach from typed room graphs. Mission-capable interiors offer at
 * least two distinct entry routes and a complete security chain (every camera and lock is
 * linked to the security room). */
#ifndef SR_INTERIOR_H
#define SR_INTERIOR_H

#include "world/sr_world.h"

#define SR_INT_MAX_W 6
#define SR_INT_MAX_H 5
#define SR_INT_MAX_ROOMS (SR_INT_MAX_W * SR_INT_MAX_H)
#define SR_INT_MAX_ENTRIES 6
#define SR_INT_MAX_DEVICES 48

typedef enum SrRoomType {
  SR_ROOM_HALL, SR_ROOM_OFFICE, SR_ROOM_STORAGE, SR_ROOM_SECURITY, SR_ROOM_SERVER,
  SR_ROOM_LAB, SR_ROOM_LOBBY, SR_ROOM_BAR, SR_ROOM_TARGET
} SrRoomType;

typedef enum SrEntryType {
  SR_ENTRY_FRONT, SR_ENTRY_SERVICE, SR_ENTRY_VENT, SR_ENTRY_ROOF, SR_ENTRY_WINDOW
} SrEntryType;

typedef enum SrDeviceType {
  SR_DEV_DOOR, SR_DEV_CAMERA, SR_DEV_TURRET, SR_DEV_ALARM, SR_DEV_LIGHTS, SR_DEV_ELEVATOR
} SrDeviceType;

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
  uint8_t rooms[SR_INT_MAX_ROOMS];  /* SrRoomType */
  uint8_t doors[SR_INT_MAX_ROOMS];  /* bit0: door east, bit1: door south */
  uint8_t vents[SR_INT_MAX_ROOMS];  /* bit0: vent east, bit1: vent south */
  int entry_count;
  SrEntry entries[SR_INT_MAX_ENTRIES];
  int device_count;
  SrDevice devices[SR_INT_MAX_DEVICES];
  int target_room;
  int security_room;
  int guards;
} SrInterior;

void sr_interior_generate(const SrCity *c, int building, SrInterior *out);
/* Rooms reachable from an entry, optionally allowing vents and locked doors. */
bool sr_interior_reachable(const SrInterior *in, int from_room, int to_room, bool use_vents,
                           bool through_locked);
/* Number of entries that reach the target using distinct openings. */
int sr_interior_route_count(const SrInterior *in);
bool sr_interior_security_chain_ok(const SrInterior *in);
uint64_t sr_interior_digest(const SrInterior *in);

#endif
