#include "world/sr_interior.h"

#include "core/sr_rng.h"

#include <string.h>

static int room_index(const SrInterior *in, int x, int y) { return y * in->w + x; }

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
    case 0: in->doors[cur] |= 1; nxt = cur + 1; break;
    case 1: in->doors[cur - 1] |= 1; nxt = cur - 1; break;
    case 2: in->doors[cur] |= 2; nxt = cur + in->w; break;
    default: in->doors[cur - in->w] |= 2; nxt = cur - in->w; break;
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
  in->entries[in->entry_count++] =
      (SrEntry){SR_ENTRY_SERVICE, (uint8_t)room_index(in, in->w - 1, in->h - 1)};
  if (mission) {
    in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_ROOF, (uint8_t)room_index(in, 0, 0)};
    in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_VENT, (uint8_t)room_index(in, 0, 0)};
    if (sr_rng_chance(&r, 500))
      in->entries[in->entry_count++] = (SrEntry){SR_ENTRY_WINDOW, (uint8_t)(in->w - 1)};
  }
  /* devices: doors on every connection, locked doors guard the target, cameras, lights */
  for (int i = 0; i < n; i++) {
    for (int dir = 0; dir < 2; dir++) {
      if (!(in->doors[i] & (1 << dir))) continue;
      int other = dir == 0 ? i + 1 : i + in->w;
      bool locked = mission && (other == in->target_room || i == in->target_room ||
                                other == in->security_room || i == in->security_room);
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

bool sr_interior_reachable(const SrInterior *in, int from, int to, bool use_vents,
                           bool through_locked) {
  bool seen[SR_INT_MAX_ROOMS] = {false};
  int queue[SR_INT_MAX_ROOMS], head = 0, tail = 0;
  queue[tail++] = from;
  seen[from] = true;
  while (head < tail) {
    int cur = queue[head++];
    if (cur == to) return true;
    int cx = cur % in->w, cy = cur / in->w;
    int nb[4] = {cx + 1 < in->w ? cur + 1 : -1, cx > 0 ? cur - 1 : -1,
                 cy + 1 < in->h ? cur + in->w : -1, cy > 0 ? cur - in->w : -1};
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
    bool needs = dev->type == SR_DEV_CAMERA || (dev->type == SR_DEV_DOOR && dev->locked) ||
                 dev->type == SR_DEV_TURRET;
    if (needs && (dev->control_room < 0 || in->rooms[dev->control_room] != SR_ROOM_SECURITY))
      return false;
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
  return h;
}
