#include "world/sr_interior.h"

#include "core/sr_rng.h"

#include <string.h>

static int room_index(const SrInterior *in, int x, int y) { return y * in->w + x; }

static void add_prop(SrInterior *in, SrPropType type, int room, int32_t x, int32_t z, int facing, int color,
                     int variant) {
  if (in->prop_count >= SR_INT_MAX_PROPS) return;
  SrProp *p = &in->props[in->prop_count++];
  p->type = (uint8_t)type;
  p->room = (uint8_t)room;
  p->x = (int16_t)x;
  p->z = (int16_t)z;
  p->facing = (uint8_t)facing;
  p->color = (uint8_t)color;
  p->variant = (uint8_t)variant;
}

/* Corner spot k (0..3) of a room, 110 cm from the walls: away from the door openings. */
static void corner(const SrInterior *in, int room, int k, int32_t *x, int32_t *z) {
  int rx = room % in->w, ry = room / in->w;
  *x = rx * SR_INT_CELL_CM + ((k & 1) ? SR_INT_CELL_CM - 110 : 110);
  *z = ry * SR_INT_CELL_CM + ((k & 2) ? SR_INT_CELL_CM - 110 : 110);
}

/* Exit props: one per entry, on the outer wall or ceiling the entry uses. */
static void add_exits(SrInterior *in) {
  for (int e = 0; e < in->entry_count; e++) {
    int room = in->entries[e].room, rx = room % in->w, ry = room / in->w;
    int32_t x0 = rx * SR_INT_CELL_CM, z0 = ry * SR_INT_CELL_CM;
    switch (in->entries[e].type) {
      case SR_ENTRY_FRONT: add_prop(in, SR_PROP_EXIT, room, x0 + 300, z0 + SR_INT_CELL_CM - 40, 2, 0, e); break;
      case SR_ENTRY_SERVICE: add_prop(in, SR_PROP_EXIT, room, x0 + SR_INT_CELL_CM - 40, z0 + 300, 1, 0, e); break;
      case SR_ENTRY_ROOF: add_prop(in, SR_PROP_EXIT, room, x0 + 150, z0 + 150, 0, 0, e); break;
      case SR_ENTRY_VENT: add_prop(in, SR_PROP_EXIT, room, x0 + 450, z0 + 40, 0, 0, e); break;
      default: add_prop(in, SR_PROP_EXIT, room, x0 + 300, z0 + 40, 0, 0, e); break;
    }
  }
}

static void add_device(SrInterior *in, SrDeviceType type, int room, int dir, bool locked) {
  if (in->device_count >= SR_INT_MAX_DEVICES) return;
  SrDevice *d = &in->devices[in->device_count];
  d->id = in->building * 64u + (uint32_t)in->device_count;
  d->type = type;
  d->room = (uint8_t)room;
  d->door_dir = (uint8_t)dir;
  d->locked = locked;
  d->control_room = (int8_t)in->security_room;
  in->device_count++;
}

void sr_interior_generate(const SrCity *c, int building, SrInterior *in) {
  memset(in, 0, sizeof *in);
  const SrBuilding *b = &c->buildings[building];
  in->building = (uint32_t)building;
  SrRng r = sr_rng_stream(c->seed, "interior", (uint64_t)building, 0);
  int32_t fw = b->x1 - b->x0, fh = b->y1 - b->y0;
  in->w = SR_CLAMP(fw / 1200, 3, SR_INT_MAX_W);
  in->h = SR_CLAMP(fh / 1200, 3, SR_INT_MAX_H);
  int n = in->w * in->h;
  bool mission = (b->flags & SR_BF_MISSION) != 0;
  /* Spanning tree of doors (randomised depth-first carve), then extra loops. */
  bool seen[SR_INT_MAX_ROOMS] = {false};
  int stack[SR_INT_MAX_ROOMS], sp = 0;
  stack[sp++] = 0;
  seen[0] = true;
  while (sp > 0) {
    int cur = stack[sp - 1];
    int cx = cur % in->w, cy = cur / in->w;
    int opts[4], no = 0;
    if (cx + 1 < in->w && !seen[cur + 1]) opts[no++] = 0;
    if (cx > 0 && !seen[cur - 1]) opts[no++] = 1;
    if (cy + 1 < in->h && !seen[cur + in->w]) opts[no++] = 2;
    if (cy > 0 && !seen[cur - in->w]) opts[no++] = 3;
    if (no == 0) {
      sp--;
      continue;
    }
    int o = opts[sr_rng_range(&r, 0, no - 1)];
    int nxt;
    switch (o) {
      case 0:
        in->doors[cur] |= 1;
        nxt = cur + 1;
        break;
      case 1:
        in->doors[cur - 1] |= 1;
        nxt = cur - 1;
        break;
      case 2:
        in->doors[cur] |= 2;
        nxt = cur + in->w;
        break;
      default:
        in->doors[cur - in->w] |= 2;
        nxt = cur - in->w;
        break;
    }
    seen[nxt] = true;
    stack[sp++] = nxt;
  }
  for (int i = 0; i < n; i++) {
    int x = i % in->w, y = i / in->w;
    if (x + 1 < in->w && sr_rng_chance(&r, 200)) in->doors[i] |= 1;
    if (y + 1 < in->h && sr_rng_chance(&r, 200)) in->doors[i] |= 2;
    if (x + 1 < in->w && sr_rng_chance(&r, 350)) in->vents[i] |= 1;
    if (y + 1 < in->h && sr_rng_chance(&r, 350)) in->vents[i] |= 2;
    in->rooms[i] = (uint8_t)(sr_rng_chance(&r, 500) ? SR_ROOM_OFFICE : SR_ROOM_HALL);
  }
  in->rooms[room_index(in, 0, in->h - 1)] = SR_ROOM_LOBBY;
  in->target_room = room_index(in, in->w - 1, 0);
  in->rooms[in->target_room] = SR_ROOM_TARGET;
  in->security_room = -1;
  if (mission) {
    in->security_room = room_index(in, in->w / 2, in->h / 2);
    if (in->security_room == in->target_room) in->security_room = 0; /* GCOVR_EXCL_LINE reason: w,h >= 3 */
    in->rooms[in->security_room] = SR_ROOM_SECURITY;
    /* vent shaft from the roof room to the target guarantees a second route */
    for (int x = 0; x < in->w - 1; x++) in->vents[room_index(in, x, 0)] |= 1;
  }
  /* entries */
  in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_FRONT, (uint8_t)room_index(in, 0, in->h - 1)};
  in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_SERVICE, (uint8_t)room_index(in, in->w - 1, in->h - 1)};
  if (mission) {
    in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_ROOF, (uint8_t)room_index(in, 0, 0)};
    in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_VENT, (uint8_t)room_index(in, 0, 0)};
    if (sr_rng_chance(&r, 500)) in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_WINDOW, (uint8_t)(in->w - 1)};
  }
  /* devices: doors on every connection, locked doors guard the target, cameras, lights */
  for (int i = 0; i < n; i++) {
    for (int dir = 0; dir < 2; dir++) {
      if (!(in->doors[i] & (1 << dir))) continue;
      int other = dir == 0 ? i + 1 : i + in->w;
      bool locked = mission && (other == in->target_room || i == in->target_room || other == in->security_room ||
                                i == in->security_room);
      add_device(in, SR_DEV_DOOR, i, dir, locked);
    }
  }
  if (mission) {
    int cams = sr_rng_range(&r, 2, 5);
    for (int k = 0; k < cams; k++) add_device(in, SR_DEV_CAMERA, sr_rng_range(&r, 0, n - 1), 0, false);
    add_device(in, SR_DEV_CAMERA, in->target_room, 0, false);
    if (sr_rng_chance(&r, 400)) add_device(in, SR_DEV_TURRET, in->target_room, 0, false);
    add_device(in, SR_DEV_ALARM, in->security_room, 0, false);
    in->guards = sr_rng_range(&r, 2, 4) + (b->poi == SR_POI_CORP_HQ ? 4 : 0);
  }
  add_device(in, SR_DEV_LIGHTS, 0, 0, false);
  if (b->flags & SR_BF_ELEVATOR) add_device(in, SR_DEV_ELEVATOR, in->entries[0].room, 0, false);
  /* props from their own stream, so the room layout above stays as it was */
  SrRng pr = sr_rng_stream(c->seed, "interior-props", (uint64_t)building, 0);
  add_exits(in);
  add_prop(in, SR_PROP_OBJECTIVE, in->target_room, in->target_room % in->w * SR_INT_CELL_CM + 300,
           in->target_room / in->w * SR_INT_CELL_CM + 300, 2, 0, 0);
  if (in->security_room >= 0) {
    int32_t x, z;
    corner(in, in->security_room, 1, &x, &z);
    add_prop(in, SR_PROP_CONSOLE, in->security_room, x, z, 3, 0, 0);
  }
  for (int i = 0; i < n; i++) {
    int32_t x, z;
    int k = sr_rng_range(&pr, 0, 3);
    corner(in, i, k, &x, &z);
    int col = sr_rng_range(&pr, 0, 15);
    switch (in->rooms[i]) {
      case SR_ROOM_OFFICE: add_prop(in, SR_PROP_DESK, i, x, z, k, col, sr_rng_range(&pr, 0, 3)); break;
      case SR_ROOM_LOBBY: add_prop(in, SR_PROP_PLANT, i, x, z, k, col, 0); break;
      case SR_ROOM_TARGET: add_prop(in, SR_PROP_SERVER, i, x, z, k, col, 0); break;
      case SR_ROOM_HALL:
        if (sr_rng_chance(&pr, 500)) add_prop(in, SR_PROP_CRATE, i, x, z, k, col, sr_rng_range(&pr, 0, 3));
        break;
      default: break;
    }
  }
}

void sr_interior_apartment(const SrCity *c, int building, int tier, SrInterior *in) {
  static const int8_t k_w[5] = {1, 2, 2, 3, 3}, k_h[5] = {1, 1, 2, 2, 3};
  memset(in, 0, sizeof *in);
  tier = SR_CLAMP(tier, 0, 4);
  in->building = (uint32_t)building;
  in->apartment = true;
  in->tier = (uint8_t)tier;
  in->w = k_w[tier];
  in->h = k_h[tier];
  in->target_room = -1;
  in->security_room = -1;
  int n = in->w * in->h;
  for (int i = 0; i < n; i++) {
    in->rooms[i] = SR_ROOM_HOME;
    if (i % in->w + 1 < in->w) in->doors[i] |= 1; /* open plan: every neighbour connects */
    if (i / in->w + 1 < in->h) in->doors[i] |= 2;
  }
  for (int i = 0; i < n; i++)
    for (int dir = 0; dir < 2; dir++)
      if (in->doors[i] & (1 << dir)) add_device(in, SR_DEV_DOOR, i, dir, false);
  add_device(in, SR_DEV_LIGHTS, 0, 0, false);
  int front = room_index(in, 0, in->h - 1);
  in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_FRONT, (uint8_t)front};
  add_exits(in);
  /* amenities: the bed in the far room, the rest spread over the corners */
  SrRng r = sr_rng_stream(c->seed, "apartment", (uint64_t)building, (uint64_t)tier);
  int far = room_index(in, in->w - 1, 0);
  bool used[SR_INT_MAX_ROOMS][4];
  memset(used, 0, sizeof used);
  used[front][2] = used[front][3] = true; /* keep the front door clear */
  static const SrPropType amen[4] = {SR_PROP_BED, SR_PROP_STASH, SR_PROP_TERMINAL, SR_PROP_WARDROBE};
  for (int a = 0; a < 4; a++) {
    int room = a == 0 ? far : sr_rng_range(&r, 0, n - 1);
    int k = sr_rng_range(&r, 0, 3);
    for (int tries = 0; tries < 16 && used[room][k]; tries++) {
      k = (k + 1) & 3;
      if (tries % 4 == 3) room = (room + 1) % n;
    }
    used[room][k] = true;
    int32_t x, z;
    corner(in, room, k, &x, &z);
    add_prop(in, amen[a], room, x, z, k, sr_rng_range(&r, 0, 15), sr_rng_range(&r, 0, 3));
  }
  /* procedural decoration: posters on walls, plants, lamps, rugs, shelves */
  int deco = 3 + tier * 2 + sr_rng_range(&r, 0, 3);
  for (int d = 0; d < deco; d++) {
    int room = sr_rng_range(&r, 0, n - 1);
    int kind = sr_rng_range(&r, 0, 4);
    int col = sr_rng_range(&r, 0, 15), var = sr_rng_range(&r, 0, 3);
    int32_t x0 = room % in->w * SR_INT_CELL_CM, z0 = room / in->w * SR_INT_CELL_CM;
    if (kind == 0) { /* poster on the north or west wall, clear of door openings */
      bool north = sr_rng_chance(&r, 500);
      int32_t along = sr_rng_chance(&r, 500) ? 120 : SR_INT_CELL_CM - 120;
      add_prop(in, SR_PROP_POSTER, room, north ? x0 + along : x0 + 8, north ? z0 + 8 : z0 + along, north ? 2 : 1, col,
               var);
    } else if (kind == 4) {
      add_prop(in, SR_PROP_RUG, room, x0 + 300, z0 + 300, 0, col, var);
    } else {
      int k = sr_rng_range(&r, 0, 3);
      if (used[room][k]) continue;
      used[room][k] = true;
      int32_t x, z;
      corner(in, room, k, &x, &z);
      static const SrPropType kinds[3] = {SR_PROP_PLANT, SR_PROP_LAMP, SR_PROP_SHELF};
      add_prop(in, kinds[kind - 1], room, x, z, k, col, var);
    }
  }
}

int sr_interior_find_prop(const SrInterior *in, SrPropType type) {
  for (int i = 0; i < in->prop_count; i++)
    if (in->props[i].type == type) return i;
  return -1;
}

int sr_interior_room_at(const SrInterior *in, int32_t x, int32_t z) {
  if (x < 0 || z < 0) return -1;
  int rx = x / SR_INT_CELL_CM, ry = z / SR_INT_CELL_CM;
  if (rx >= in->w || ry >= in->h) return -1;
  return ry * in->w + rx;
}

static bool door_between(const SrInterior *in, int a, int b, bool through_locked) {
  int lo = a < b ? a : b, hi = a < b ? b : a;
  int dir = hi - lo == 1 ? 0 : 1;
  if (!(in->doors[lo] & (1 << dir))) return false;
  if (through_locked) return true;
  for (int d = 0; d < in->device_count; d++) {
    const SrDevice *dev = &in->devices[d];
    if (dev->type == SR_DEV_DOOR && dev->room == lo && dev->door_dir == dir) return !dev->locked;
  }
  return true; /* GCOVR_EXCL_LINE reason: every door connection has a device */
}

static bool vent_between(const SrInterior *in, int a, int b) {
  int lo = a < b ? a : b, hi = a < b ? b : a;
  int dir = hi - lo == 1 ? 0 : 1;
  return (in->vents[lo] & (1 << dir)) != 0;
}

bool sr_interior_reachable(const SrInterior *in, int from, int to, bool use_vents, bool through_locked) {
  bool seen[SR_INT_MAX_ROOMS] = {false};
  int queue[SR_INT_MAX_ROOMS], head = 0, tail = 0;
  queue[tail++] = from;
  seen[from] = true;
  while (head < tail) {
    int cur = queue[head++];
    if (cur == to) return true;
    int cx = cur % in->w, cy = cur / in->w;
    int nb[4] = {cx + 1 < in->w ? cur + 1 : -1, cx > 0 ? cur - 1 : -1, cy + 1 < in->h ? cur + in->w : -1,
                 cy > 0 ? cur - in->w : -1};
    for (int k = 0; k < 4; k++) {
      int o = nb[k];
      if (o < 0 || seen[o]) continue;
      if (door_between(in, cur, o, through_locked) || (use_vents && vent_between(in, cur, o))) {
        seen[o] = true;
        queue[tail++] = o;
      }
    }
  }
  return false;
}

int sr_interior_route_count(const SrInterior *in) {
  /* Count entries with distinct openings: vent and roof routes may crawl vents, doors may be
   * opened by keycard or the Matrix (through_locked). */
  int count = 0;
  for (int e = 0; e < in->entry_count; e++) {
    bool vents = in->entries[e].type == SR_ENTRY_VENT || in->entries[e].type == SR_ENTRY_ROOF;
    if (sr_interior_reachable(in, in->entries[e].room, in->target_room, vents, true)) count++;
  }
  return count;
}

bool sr_interior_security_chain_ok(const SrInterior *in) {
  for (int d = 0; d < in->device_count; d++) {
    const SrDevice *dev = &in->devices[d];
    bool needs = dev->type == SR_DEV_CAMERA || (dev->type == SR_DEV_DOOR && dev->locked) || dev->type == SR_DEV_TURRET;
    if (needs && (dev->control_room < 0 || in->rooms[dev->control_room] != SR_ROOM_SECURITY)) return false;
  }
  return true;
}

uint64_t sr_interior_digest(const SrInterior *in) {
  uint64_t h = sr_hash_bytes(0, in->rooms, sizeof in->rooms);
  h = sr_hash_bytes(h, in->doors, sizeof in->doors);
  h = sr_hash_bytes(h, in->vents, sizeof in->vents);
  for (int d = 0; d < in->device_count; d++) {
    uint32_t v[3] = {in->devices[d].id, (uint32_t)in->devices[d].type, in->devices[d].locked};
    h = sr_hash_bytes(h, v, sizeof v);
  }
  for (int p = 0; p < in->prop_count; p++) h = sr_hash_bytes(h, &in->props[p], sizeof(SrProp));
  return h;
}
