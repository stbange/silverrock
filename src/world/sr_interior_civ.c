/* Civilian ground floors: every building that is not a mission site or the player's home gets a
 * floor that fits its use, with furniture and the people who work or wait there. Layouts are
 * small grids of 6 m cells; neighbouring cells of one space are open floor (no wall between
 * them), separate rooms are joined by doors. The front entry is always in the middle of the
 * bottom (street, +z) row. Deterministic per building. */
#include "core/sr_rng.h"
#include "world/sr_interior.h"

#include <string.h>

#define C SR_INT_CELL_CM

static int idx(const SrInterior *in, int x, int y) { return y * in->w + x; }

static void prop(SrInterior *in, SrPropType type, int32_t x, int32_t z, int facing, int color, int variant) {
  if (in->prop_count >= SR_INT_MAX_PROPS) return;
  SrProp *p = &in->props[in->prop_count++];
  memset(p, 0, sizeof *p);
  p->type = (uint8_t)type;
  p->room = (uint8_t)sr_interior_room_at(in, x, z);
  p->x = (int16_t)x;
  p->z = (int16_t)z;
  p->facing = (uint8_t)facing;
  p->color = (uint8_t)color;
  p->variant = (uint8_t)variant;
}

static void occ(SrInterior *in, int32_t x, int32_t z, int facing, SrOccPose pose, SrOccRole role) {
  if (in->occ_count >= SR_INT_MAX_OCC) return;
  SrOccupant *o = &in->occ[in->occ_count++];
  o->x = (int16_t)x;
  o->z = (int16_t)z;
  o->facing = (uint8_t)facing;
  o->pose = (uint8_t)pose;
  o->role = (uint8_t)role;
  o->room = (uint8_t)sr_interior_room_at(in, x, z);
}

static void device(SrInterior *in, SrDeviceType type, int room, int dir, bool locked) {
  if (in->device_count >= SR_INT_MAX_DEVICES) return;
  SrDevice *d = &in->devices[in->device_count];
  d->id = in->building * 64u + (uint32_t)in->device_count;
  d->type = type;
  d->room = (uint8_t)room;
  d->door_dir = (uint8_t)dir;
  d->locked = locked;
  d->control_room = -1;
  in->device_count++;
}

/* Joins a room to its east (dir 0) or south (dir 1) neighbour: open floor, or a door. */
static void join(SrInterior *in, int room, int dir, bool open, bool locked) {
  in->doors[room] |= (uint8_t)(1 << dir);
  if (open)
    in->doors[room] |= (uint8_t)(dir == 0 ? SR_INT_OPEN_E : SR_INT_OPEN_S);
  else
    device(in, SR_DEV_DOOR, room, dir, locked);
}

/* Open floor across a row of cells. */
static void open_row(SrInterior *in, int y) {
  for (int x = 0; x + 1 < in->w; x++) join(in, idx(in, x, y), 0, true, false);
}

/* Open floor over the whole grid. */
static void open_all(SrInterior *in) {
  for (int y = 0; y < in->h; y++) {
    open_row(in, y);
    if (y + 1 < in->h)
      for (int x = 0; x < in->w; x++) join(in, idx(in, x, y), 1, true, false);
  }
}

static void rooms(SrInterior *in, SrRoomType t) {
  for (int i = 0; i < in->w * in->h; i++) in->rooms[i] = (uint8_t)t;
}

/* The front entry (middle of the street row) and its exit prop; optionally a service door. */
static void entries(SrInterior *in, bool service) {
  int fx = in->w / 2, front = idx(in, fx, in->h - 1);
  in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_FRONT, (uint8_t)front};
  prop(in, SR_PROP_EXIT, fx * C + 300, in->h * C - 40, 2, 0, 0);
  if (service) {
    int back = idx(in, in->w - 1, 0);
    in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_SERVICE, (uint8_t)back};
    prop(in, SR_PROP_EXIT, in->w * C - 40, 300, 1, 0, 1);
  }
  device(in, SR_DEV_LIGHTS, 0, 0, false);
}

/* A counter across a cell, facing south (toward the street), with someone behind it. */
static void counter(SrInterior *in, int cx, int cy, int32_t dz, int look, SrOccPose pose, int color) {
  int32_t x = cx * C + 300, z = cy * C + dz;
  prop(in, SR_PROP_COUNTER, x, z, 2, color, look);
  occ(in, x, z - 95, 2, pose, look == SR_COUNTER_DESK && pose == SR_POSE_SIT ? SR_OCC_WORKER : SR_OCC_KEEPER);
}

/* Shelves along the west and east outer walls of a row, facing into the room. */
static void wall_shelves(SrInterior *in, SrRng *r, int y, int every) {
  for (int x = 0; x < in->w; x++) {
    if (x != 0 && x != in->w - 1) continue;
    for (int k = 0; k < 2; k += every) {
      int32_t z = y * C + 150 + k * 300;
      if (x == 0) prop(in, SR_PROP_SHELF, 45, z, 1, sr_rng_range(r, 0, 15), sr_rng_range(r, 0, 3));
      if (x == in->w - 1) prop(in, SR_PROP_SHELF, in->w * C - 45, z, 3, sr_rng_range(r, 0, 15), sr_rng_range(r, 0, 3));
    }
  }
}

/* A table with a seat on each side; someone sits at it now and then. */
static void table(SrInterior *in, SrRng *r, int32_t x, int32_t z, int people) {
  prop(in, SR_PROP_TABLE, x, z, 0, sr_rng_range(r, 0, 15), 0);
  prop(in, SR_PROP_SEAT, x, z - 80, 2, sr_rng_range(r, 0, 15), 0);
  prop(in, SR_PROP_SEAT, x, z + 80, 0, sr_rng_range(r, 0, 15), 0);
  if (people > 0) occ(in, x, z + 80, 0, sr_rng_chance(r, 500) ? SR_POSE_EAT : SR_POSE_SIT, SR_OCC_PATRON);
  if (people > 1) occ(in, x, z - 80, 2, SR_POSE_TALK, SR_OCC_PATRON);
}

static void posters(SrInterior *in, SrRng *r, int n) {
  for (int k = 0; k < n; k++) {
    int x = sr_rng_range(r, 0, in->w - 1);
    prop(in, SR_PROP_POSTER, x * C + (sr_rng_chance(r, 500) ? 140 : 460), 8, 2, sr_rng_range(r, 0, 15), 0);
  }
}

SrInteriorKind sr_interior_kind(const SrCity *c, int building) {
  const SrBuilding *b = &c->buildings[building];
  switch ((SrPoi)b->poi) {
    case SR_POI_GUNSHOP:
    case SR_POI_ELECTRONICS:
    case SR_POI_CLOTHING:
    case SR_POI_RIPPERDOC:
    case SR_POI_FENCE:
    case SR_POI_GROCERY: return SR_FLOOR_SHOP;
    case SR_POI_FOOD: return SR_FLOOR_FOOD;
    case SR_POI_BAR: return SR_FLOOR_BAR;
    case SR_POI_CLINIC: return SR_FLOOR_CLINIC;
    case SR_POI_CAPSULE: return SR_FLOOR_CAPSULE;
    case SR_POI_FIXER: return SR_FLOOR_FIXER;
    case SR_POI_MATRIX_BOOTH:
    case SR_POI_UV_BOOTH: return SR_FLOOR_BOOTH;
    case SR_POI_POLICE:
    case SR_POI_CITY_HALL:
    case SR_POI_MEDIA: return SR_FLOOR_CIVIC;
    case SR_POI_CORP_HQ:
    case SR_POI_CORP_FACILITY: return SR_FLOOR_OFFICE;
    case SR_POI_HOME: return SR_FLOOR_RESIDENTIAL;
    default: break;
  }
  int d = c->blocks[b->block].district;
  SrDistrictType t = d >= 0 ? c->districts[d].type : SR_D_RESIDENTIAL;
  uint64_t h = sr_mix64(c->seed ^ 0x1f7e5ull ^ (uint64_t)building * 0x9e3779b97f4a7c15ull) % 100;
  switch (t) {
    case SR_D_INDUSTRIAL:
    case SR_D_DOCKS: return h < 70 ? SR_FLOOR_WAREHOUSE : SR_FLOOR_RESIDENTIAL;
    case SR_D_SPIRE:
    case SR_D_CIVIC:
    case SR_D_CAMPUS: return h < 70 ? SR_FLOOR_OFFICE : SR_FLOOR_RESIDENTIAL;
    case SR_D_COMMERCIAL:
    case SR_D_ENTERTAINMENT: return h < 45 ? SR_FLOOR_OFFICE : SR_FLOOR_RESIDENTIAL;
    default: return h < 12 ? SR_FLOOR_WAREHOUSE : SR_FLOOR_RESIDENTIAL;
  }
}

const char *sr_interior_kind_name(SrInteriorKind k) {
  static const char *const names[SR_FLOOR_COUNT] = {"site",   "home",      "shop",  "food",  "bar",
                                                    "clinic", "capsule",   "fixer", "booth", "residential",
                                                    "office", "warehouse", "civic"};
  return (unsigned)k < SR_FLOOR_COUNT ? names[k] : "?";
}

static void shop(SrInterior *in, SrRng *r, const SrBuilding *b) {
  rooms(in, SR_ROOM_SHOP);
  open_all(in);
  int cx = in->w / 2;
  counter(in, cx, 0, 280, SR_COUNTER_SHOP, SR_POSE_STAND, sr_rng_range(r, 0, 15));
  wall_shelves(in, r, 0, 1);
  wall_shelves(in, r, in->h - 1, 1);
  for (int x = 0; x < in->w; x++) /* stock on the back wall, behind the counter */
    for (int k = 0; k < 2; k++)
      prop(in, SR_PROP_SHELF, x * C + 160 + k * 280, 30, 2, sr_rng_range(r, 0, 15), sr_rng_range(r, 0, 3));
  if (in->w >= 3) /* a middle gondola of shelves in wide shops */
    for (int x = 0; x < in->w; x++)
      if (x != in->w / 2) prop(in, SR_PROP_SHELF, x * C + 300, (in->h - 1) * C + 220, 0, sr_rng_range(r, 0, 15), 1);
  prop(in, SR_PROP_PLANT, cx * C + 520, (in->h - 1) * C + 500, 0, 0, 1);
  posters(in, r, 2);
  int browsing = sr_rng_range(r, 0, 2) + (b->poi == SR_POI_GROCERY);
  for (int k = 0; k < browsing; k++) {
    bool west = k % 2 == 0;
    occ(in, west ? 150 : in->w * C - 150, (in->h - 1) * C + 150 + 250 * (k / 2), west ? 3 : 1,
        sr_rng_chance(r, 300) ? SR_POSE_PHONE : SR_POSE_STAND, SR_OCC_PATRON);
  }
}

static void food(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_DINER);
  open_all(in);
  int cx = in->w / 2;
  counter(in, cx, 0, 260, SR_COUNTER_BAR, SR_POSE_STAND, sr_rng_range(r, 0, 15));
  for (int k = -1; k <= 1; k++) { /* stools along the counter */
    int32_t x = cx * C + 300 + k * 80, z = 260 + 85;
    prop(in, SR_PROP_SEAT, x, z, 0, 0, 1);
    if (sr_rng_chance(r, 450)) occ(in, x, z, 0, SR_POSE_SIT, SR_OCC_PATRON);
  }
  int fx = in->w / 2; /* keep the line from the door to the counter clear */
  for (int x = 0; x < in->w; x++)
    if (x != fx) table(in, r, x * C + 300, (in->h - 1) * C + 200, sr_rng_range(r, 0, 2));
  if (in->w == 2) table(in, r, 300, (in->h - 1) * C + 440, sr_rng_range(r, 0, 1));
  prop(in, SR_PROP_LAMP, 60, 60, 0, sr_rng_range(r, 0, 15), 0);
  posters(in, r, 2);
}

static void bar(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_DINER);
  open_all(in);
  int col = sr_rng_range(r, 0, 15);
  for (int x = 0; x < in->w; x++) { /* one long bar along the back */
    prop(in, SR_PROP_COUNTER, x * C + 300, 260, 2, col, SR_COUNTER_BAR);
    for (int k = -1; k <= 1; k += 2) {
      int32_t sx = x * C + 300 + k * 140, sz = 260 + 85;
      prop(in, SR_PROP_SEAT, sx, sz, 0, 0, 1);
      if (sr_rng_chance(r, 400)) occ(in, sx, sz, 0, sr_rng_chance(r, 500) ? SR_POSE_SIT : SR_POSE_TALK, SR_OCC_PATRON);
    }
  }
  occ(in, (in->w / 2) * C + 300, 165, 2, SR_POSE_STAND, SR_OCC_KEEPER);
  for (int x = 0; x < in->w; x++)
    if (x != in->w / 2) table(in, r, x * C + 300, (in->h - 1) * C + 260, sr_rng_range(r, 0, 2));
  occ(in, 120, (in->h - 1) * C + 120, 1, SR_POSE_LEAN, SR_OCC_PATRON); /* against the wall by the door */
  for (int k = 0; k < 3; k++) prop(in, SR_PROP_LAMP, k * 260 + 80, (in->h - 1) * C + 540, 0, col + k, 0);
  posters(in, r, 3);
}

static void clinic(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_LOBBY);
  open_row(in, in->h - 1);
  int cx = in->w / 2;
  for (int x = 0; x < in->w; x++) {
    in->rooms[idx(in, x, 0)] = SR_ROOM_WARD;
    if (x != cx)
      join(in, idx(in, x, 0), 1, false, false); /* a door from the waiting room */
    else
      join(in, idx(in, x, 0), 0, false, false); /* the middle ward opens into the next one */
    prop(in, SR_PROP_BED, x * C + 150, 220, 0, 6, 0);
    if (sr_rng_chance(r, 500)) occ(in, x * C + 330, 220, 3, SR_POSE_STAND, SR_OCC_WORKER); /* a nurse */
  }
  counter(in, cx, in->h - 1, 170, SR_COUNTER_DESK, SR_POSE_STAND, 6);
  int y = (in->h - 1) * C;
  for (int k = 0; k < 4; k++) { /* waiting chairs along the walls */
    int32_t sx = k < 2 ? 70 : in->w * C - 70, sz = y + 220 + (k % 2) * 160;
    prop(in, SR_PROP_SEAT, sx, sz, k < 2 ? 1 : 3, 6, 0);
    if (sr_rng_chance(r, 450)) occ(in, sx, sz, k < 2 ? 1 : 3, SR_POSE_SIT, SR_OCC_PATRON);
  }
  prop(in, SR_PROP_PLANT, cx * C + 540, y + 520, 0, 0, 0);
}

static void capsule(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_CORRIDOR);
  open_all(in);
  int y = (in->h - 1) * C;
  in->rooms[idx(in, in->w / 2, in->h - 1)] = SR_ROOM_LOBBY;
  counter(in, in->w / 2, in->h - 1, 180, SR_COUNTER_DESK, SR_POSE_STAND, 1);
  for (int row = 0; row < in->h - 1; row++) /* pods along both long walls */
    for (int x = 0; x < in->w; x++) {
      prop(in, SR_PROP_POD, x * C + 150, row * C + 70, 2, x + row, 0);
      prop(in, SR_PROP_POD, x * C + 450, row * C + 70, 2, x + row + 1, 0);
    }
  prop(in, SR_PROP_POD, 70, y + 300, 1, 3, 0);
  prop(in, SR_PROP_POD, in->w * C - 70, y + 300, 3, 4, 0);
  if (sr_rng_chance(r, 600)) occ(in, in->w * C / 2 - 200, y + 380, 0, SR_POSE_PHONE, SR_OCC_PATRON);
}

static void fixer(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_DINER);
  open_row(in, in->h - 1);
  open_row(in, 0);
  for (int x = 0; x < in->w; x++) in->rooms[idx(in, x, 0)] = SR_ROOM_BACK;
  join(in, idx(in, in->w / 2, 0), 1, false, false);
  /* the fixer's table in the back, the fixer behind it facing the door */
  int32_t tx = (in->w / 2) * C + 300 + (in->w == 2 ? -380 : -420), tz = 260; /* beside the door, not in it */
  prop(in, SR_PROP_COUNTER, tx, tz, 2, sr_rng_range(r, 0, 15), SR_COUNTER_TABLE);
  occ(in, tx, tz - 95, 2, SR_POSE_SIT, SR_OCC_KEEPER);
  prop(in, SR_PROP_SEAT, tx, tz - 95, 2, 0, 0);
  prop(in, SR_PROP_SEAT, tx, tz + 90, 0, 0, 0);
  occ(in, tx + 220, 120, 2, SR_POSE_STAND, SR_OCC_OFFICER); /* muscle */
  prop(in, SR_PROP_LAMP, tx - 220, 120, 0, 2, 0);
  prop(in, SR_PROP_CRATE, 80, 80, 0, 0, 1);
  /* the front room: a few tables, people waiting their turn */
  for (int x = 0; x < in->w; x++)
    if (x != in->w / 2) table(in, r, x * C + 300, (in->h - 1) * C + 250, sr_rng_range(r, 0, 1));
  occ(in, in->w * C - 120, (in->h - 1) * C + 100, 3, SR_POSE_LEAN, SR_OCC_PATRON);
  posters(in, r, 1);
}

static void booths(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_SHOP);
  open_all(in);
  for (int x = 0; x < in->w; x++)
    for (int k = 0; k < 2; k++) {
      int32_t bx = x * C + 150 + k * 300;
      prop(in, SR_PROP_BOOTH, bx, 90, 2, sr_rng_range(r, 0, 15), 0);
      if (sr_rng_chance(r, 350)) occ(in, bx, 110, 0, SR_POSE_SIT, SR_OCC_PATRON);
    }
  counter(in, 0, in->h - 1, 200, SR_COUNTER_DESK, SR_POSE_STAND, 1);
  posters(in, r, 1);
}

static void residential(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_CORRIDOR);
  int ly = in->h - 1, fx = in->w / 2;
  for (int x = 0; x < in->w; x++) in->rooms[idx(in, x, ly)] = SR_ROOM_LOBBY;
  open_row(in, ly);
  for (int y = 0; y < ly; y++) open_row(in, y);
  for (int y = 0; y + 1 < ly; y++)
    for (int x = 0; x < in->w; x++) join(in, idx(in, x, y), 1, true, false);
  join(in, idx(in, fx, ly - 1), 1, false, false); /* the door from the lobby to the corridor */
  /* lobby: mailboxes on the west wall, the lift on the east wall, a plant */
  int32_t y0 = ly * C;
  for (int k = 0; k < 3; k++) prop(in, SR_PROP_MAILBOX, 30, y0 + 160 + k * 110, 1, 0, k);
  prop(in, SR_PROP_LIFT, in->w * C - 40, y0 + 300, 3, 0, 0);
  prop(in, SR_PROP_PLANT, fx * C + 540, y0 + 520, 0, 0, sr_rng_range(r, 0, 3));
  occ(in, 110, y0 + 270, 3, SR_POSE_STAND, SR_OCC_RESIDENT); /* at the mailboxes */
  if (sr_rng_chance(r, 600)) occ(in, in->w * C - 150, y0 + 300, 1, SR_POSE_PHONE, SR_OCC_RESIDENT); /* waiting */
  /* corridor: flat doors on the north wall and the outer walls */
  int flat = 1 + sr_rng_range(r, 0, 3) * 10;
  for (int x = 0; x < in->w; x++)
    for (int k = 0; k < 2; k++) prop(in, SR_PROP_FLAT_DOOR, x * C + 150 + k * 300, 12, 2, (x + k) & 7, flat++);
  for (int y = 0; y < ly; y++) {
    prop(in, SR_PROP_FLAT_DOOR, 12, y * C + 300, 1, y, flat++);
    prop(in, SR_PROP_FLAT_DOOR, in->w * C - 12, y * C + 300, 3, y + 1, flat++);
  }
  if (sr_rng_chance(r, 500)) occ(in, (in->w - 1) * C + 200, 200, 0, SR_POSE_PHONE, SR_OCC_RESIDENT);
  for (int k = 0; k < 2; k++) prop(in, SR_PROP_LAMP, k * (in->w * C - 120) + 60, 60, 0, 6, 0);
}

static void office(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_OFFICE);
  int ly = in->h - 1, fx = in->w / 2;
  for (int x = 0; x < in->w; x++) in->rooms[idx(in, x, ly)] = SR_ROOM_LOBBY;
  open_row(in, ly);
  for (int y = 0; y < ly; y++) open_row(in, y);
  for (int y = 0; y + 1 < ly; y++)
    for (int x = 0; x < in->w; x++) join(in, idx(in, x, y), 1, true, false);
  join(in, idx(in, 0, ly - 1), 1, false, false); /* the way in past reception, at the side */
  counter(in, fx, ly, 200, SR_COUNTER_DESK, SR_POSE_STAND, 6);
  for (int k = 0; k < 2; k++) {
    int32_t sx = 80 + k * (in->w * C - 160), sz = ly * C + 300;
    prop(in, SR_PROP_SEAT, sx, sz, k ? 3 : 1, 6, 0);
    if (sr_rng_chance(r, 300)) occ(in, sx, sz, k ? 3 : 1, SR_POSE_SIT, SR_OCC_PATRON);
  }
  prop(in, SR_PROP_PLANT, 80, ly * C + 520, 0, 0, 2);
  prop(in, SR_PROP_PLANT, in->w * C - 80, ly * C + 520, 0, 0, 3);
  /* open-plan desks, a few people working late */
  for (int y = 0; y < ly; y++)
    for (int x = 0; x < in->w; x++)
      for (int k = 0; k < 2; k++) {
        int32_t dx = x * C + 170 + k * 260, dz = y * C + 260;
        prop(in, SR_PROP_DESK, dx, dz, 2, sr_rng_range(r, 0, 15), sr_rng_range(r, 0, 3));
        if (sr_rng_chance(r, 160)) occ(in, dx, dz - 70, 2, SR_POSE_SIT, SR_OCC_WORKER);
      }
}

static void warehouse(SrInterior *in, SrRng *r) {
  rooms(in, SR_ROOM_WAREHOUSE);
  open_all(in);
  for (int y = 0; y < in->h; y++)
    for (int x = 0; x < in->w; x++) {
      if (y == in->h - 1 && x == in->w / 2) continue; /* keep the door clear */
      int n = sr_rng_range(r, 1, 3);
      for (int k = 0; k < n; k++)
        prop(in, k == 0 && sr_rng_chance(r, 400) ? SR_PROP_SHELF : SR_PROP_CRATE, x * C + 150 + (k % 2) * 300,
             y * C + 150 + (k / 2) * 300, sr_rng_range(r, 0, 3), 0, sr_rng_range(r, 0, 3));
    }
  occ(in, (in->w / 2) * C + 520, (in->h - 1) * C + 200, 3, SR_POSE_STAND, SR_OCC_WORKER);
  if (sr_rng_chance(r, 500)) occ(in, 300, 450, 1, SR_POSE_PHONE, SR_OCC_WORKER);
}

static void civic(SrInterior *in, SrRng *r, const SrBuilding *b) {
  rooms(in, SR_ROOM_LOBBY);
  open_row(in, in->h - 1);
  for (int x = 0; x < in->w; x++) {
    in->rooms[idx(in, x, 0)] = SR_ROOM_OFFICE;
    if (x + 1 < in->w) join(in, idx(in, x, 0), 0, true, false);
  }
  join(in, idx(in, 0, 0), 1, false, true); /* staff only, at the side */
  bool police = b->poi == SR_POI_POLICE;
  int32_t tx = (in->w / 2) * C + 300, tz = (in->h - 1) * C + 170;
  prop(in, SR_PROP_COUNTER, tx, tz, 2, police ? 1 : 6, SR_COUNTER_DESK);
  occ(in, tx, tz - 95, 2, SR_POSE_STAND, police ? SR_OCC_OFFICER : SR_OCC_KEEPER);
  for (int k = 0; k < 4; k++) {
    int32_t sx = k < 2 ? 70 : in->w * C - 70, sz = (in->h - 1) * C + 260 + (k % 2) * 160;
    prop(in, SR_PROP_SEAT, sx, sz, k < 2 ? 1 : 3, 0, 0);
    if (sr_rng_chance(r, 400)) occ(in, sx, sz, k < 2 ? 1 : 3, SR_POSE_SIT, SR_OCC_PATRON);
  }
  for (int x = 0; x < in->w; x++) prop(in, SR_PROP_DESK, x * C + 300, 260, 2, 0, 0);
  posters(in, r, 2);
}

void sr_interior_civilian(const SrCity *c, int building, SrInterior *in) {
  memset(in, 0, sizeof *in);
  const SrBuilding *b = &c->buildings[building];
  SrInteriorKind kind = sr_interior_kind(c, building);
  in->building = (uint32_t)building;
  in->kind = (uint8_t)kind;
  in->target_room = -1;
  in->security_room = -1;
  SrRng r = sr_rng_stream(c->seed, "civilian", (uint64_t)building, 0);
  int32_t fw = b->x1 - b->x0, fh = b->y1 - b->y0;
  int wide = SR_CLAMP(fw / 1200, 2, SR_INT_MAX_W), deep = SR_CLAMP(fh / 1200, 2, SR_INT_MAX_H);
  switch (kind) {
    case SR_FLOOR_SHOP:
    case SR_FLOOR_FOOD:
    case SR_FLOOR_BOOTH:
    case SR_FLOOR_FIXER:
      in->w = SR_CLAMP(wide, 2, 3);
      in->h = 2;
      break;
    case SR_FLOOR_BAR:
    case SR_FLOOR_CLINIC:
    case SR_FLOOR_CIVIC:
      in->w = 3;
      in->h = 2;
      break;
    case SR_FLOOR_CAPSULE:
      in->w = 3;
      in->h = SR_CLAMP(deep, 2, 3);
      break;
    default:
      in->w = SR_CLAMP(wide, 3, 5);
      in->h = SR_CLAMP(deep, 2, 3);
      break;
  }
  bool service = kind != SR_FLOOR_RESIDENTIAL && kind != SR_FLOOR_BOOTH;
  switch (kind) {
    case SR_FLOOR_SHOP: shop(in, &r, b); break;
    case SR_FLOOR_FOOD: food(in, &r); break;
    case SR_FLOOR_BAR: bar(in, &r); break;
    case SR_FLOOR_CLINIC: clinic(in, &r); break;
    case SR_FLOOR_CAPSULE: capsule(in, &r); break;
    case SR_FLOOR_FIXER: fixer(in, &r); break;
    case SR_FLOOR_BOOTH: booths(in, &r); break;
    case SR_FLOOR_OFFICE: office(in, &r); break;
    case SR_FLOOR_WAREHOUSE: warehouse(in, &r); break;
    case SR_FLOOR_CIVIC: civic(in, &r, b); break;
    default: residential(in, &r); break;
  }
  entries(in, service);
  if (b->flags & SR_BF_ELEVATOR) device(in, SR_DEV_ELEVATOR, in->entries[0].room, 0, false);
}
