#include "game/sr_site.h"

#include "game/sr_net.h"

#include "core/sr_rng.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CELL       ((float)SR_INT_CELL_CM / 100.0f)
#define GUARD_EYE  1.6f
#define PATROL_SPD 1.4f
#define SEARCH_SPD 2.4f
#define CHASE_SPD  3.2f
#define FIRE_CD    1.1f
#define GUARD_DMG  9.0f

void sr_deltas_set(SrDeltas *d, uint32_t id, uint8_t value) {
  for (int i = 0; i < d->count; i++)
    if (d->id[i] == id) {
      d->value[i] = value;
      return;
    }
  if (d->count < SR_MAX_DELTAS) {
    d->id[d->count] = id;
    d->value[d->count] = value;
    d->count++;
  }
}

int sr_deltas_get(const SrDeltas *d, uint32_t id) {
  for (int i = 0; i < d->count; i++)
    if (d->id[i] == id) return d->value[i];
  return -1;
}

static void delta_or(SrDeltas *d, uint32_t id, uint8_t bits) {
  int v = sr_deltas_get(d, id);
  sr_deltas_set(d, id, (uint8_t)((v < 0 ? 0 : v) | bits));
}

uint32_t sr_site_guard_id(int building, int k) { return 0x80000000u | ((uint32_t)building * 16u + (uint32_t)k); }
uint32_t sr_site_objective_id(int32_t job) { return 0x40000000u | (uint32_t)job; }

/* ---- geometry ------------------------------------------------------------------------------ */

/* Grid row of a room (integer division on purpose). */
static int room_row(const SrSite *s, int room) {
  int row = room / s->in.w;
  return row;
}

int sr_site_room_at(const SrSite *s, float x, float z) {
  float lx = x - s->ox, lz = z - s->oz;
  if (lx < 0 || lz < 0) return -1;
  return sr_interior_room_at(&s->in, (int32_t)(lx * 100.0f), (int32_t)(lz * 100.0f));
}

SrVec3 sr_site_room_center(const SrSite *s, int room) {
  return sr_v3(s->ox + ((float)(room % s->in.w) + 0.5f) * CELL, SR_SITE_FLOOR_Y + GUARD_EYE,
               s->oz + ((float)room_row(s, room) + 0.5f) * CELL);
}

static int door_device(const SrInterior *in, int room, int dir) {
  for (int d = 0; d < in->device_count; d++)
    if (in->devices[d].type == SR_DEV_DOOR && in->devices[d].room == room && in->devices[d].door_dir == dir) return d;
  return -1;
}

bool sr_site_door_open(const SrSite *s, int room, int dir) {
  if (!(s->in.doors[room] & (1 << dir))) return false;
  int d = door_device(&s->in, room, dir);
  return d < 0 || !s->in.devices[d].locked || s->authorised; /* forged credentials open locks */
}

/* Wall test at a local point for a body of radius r. Openings: unlocked doors, and vents when
 * crawling. `sight` treats vents as solid and ignores the body radius. */
static bool wall_at(const SrSite *s, float lx, float lz, float r, bool crouched, bool sight) {
  const SrInterior *in = &s->in;
  float W = (float)in->w * CELL, H = (float)in->h * CELL;
  if (lx < r || lz < r || lx > W - r || lz > H - r) return true;
  float reach = r + SR_SITE_WALL_HALF;
  int cx = (int)(lx / CELL), cz = (int)(lz / CELL);
  /* vertical walls (between columns) */
  for (int k = 1; k < in->w; k++) {
    float X = (float)k * CELL;
    if (fabsf(lx - X) >= reach) continue;
    int room = cz * in->w + (k - 1);
    float along = lz - ((float)cz + 0.5f) * CELL;
    bool open = sr_site_door_open(s, room, 0) && fabsf(along) < SR_SITE_DOOR_HALF - r;
    if (!open && !sight && crouched && (in->vents[room] & 1))
      open = fabsf(along - SR_SITE_VENT_OFFSET) < SR_SITE_VENT_HALF - r * 0.5f;
    if (!open) return true;
  }
  /* horizontal walls (between rows) */
  for (int k = 1; k < in->h; k++) {
    float Z = (float)k * CELL;
    if (fabsf(lz - Z) >= reach) continue;
    int room = (k - 1) * in->w + cx;
    float along = lx - ((float)cx + 0.5f) * CELL;
    bool open = sr_site_door_open(s, room, 1) && fabsf(along) < SR_SITE_DOOR_HALF - r;
    if (!open && !sight && crouched && (in->vents[room] & 2))
      open = fabsf(along - SR_SITE_VENT_OFFSET) < SR_SITE_VENT_HALF - r * 0.5f;
    if (!open) return true;
  }
  return false;
}

bool sr_site_blocked(const SrSite *s, float x, float z, float radius, bool crouched) {
  if (wall_at(s, x - s->ox, z - s->oz, radius, crouched, false)) return true;
  for (int i = 0; i < s->in.prop_count; i++) { /* solid furniture */
    const SrProp *p = &s->in.props[i];
    if (p->type != SR_PROP_BED && p->type != SR_PROP_DESK && p->type != SR_PROP_SHELF && p->type != SR_PROP_SERVER &&
        p->type != SR_PROP_CRATE && p->type != SR_PROP_WARDROBE && p->type != SR_PROP_CONSOLE)
      continue;
    float px = s->ox + (float)p->x / 100.0f, pz = s->oz + (float)p->z / 100.0f;
    if (fabsf(x - px) < 0.5f + radius && fabsf(z - pz) < 0.5f + radius) return true;
  }
  return false;
}

static bool smoke_at(const SrSite *s, float x, float z) {
  for (int i = 0; i < s->effect_count; i++) {
    const SrSiteEffect *e = &s->effects[i];
    if (!e->active || e->type != SR_FX_SMOKE) continue;
    float dx = x - e->pos.x, dz = z - e->pos.z;
    if (dx * dx + dz * dz < e->radius * e->radius) return true;
  }
  return false;
}

bool sr_site_los(const SrSite *s, SrVec3 a, SrVec3 b) {
  float dx = b.x - a.x, dz = b.z - a.z;
  float len = sqrtf(dx * dx + dz * dz);
  int steps = (int)(len / 0.2f) + 1;
  for (int i = 1; i < steps; i++) {
    float t = (float)i / (float)steps;
    float x = a.x + dx * t, z = a.z + dz * t;
    if (wall_at(s, x - s->ox, z - s->oz, 0.0f, false, true)) return false;
    if (smoke_at(s, x, z)) return false;
  }
  return true;
}

float sr_site_light(const SrSite *s, int room) {
  if (s->lights_off) return 0.12f;
  if (room >= 0 && s->in.rooms[room] == SR_ROOM_STORAGE) return 0.45f;
  return 0.75f;
}

bool sr_site_restricted(const SrSite *s, int room, bool has_fake_id) {
  if (s->in.apartment || room < 0) return false;
  if (s->hostile || s->alarm) return true;
  if (s->authorised) return false; /* forged credentials, until suspicion rises */
  SrRoomType t = (SrRoomType)s->in.rooms[room];
  if (t == SR_ROOM_LOBBY) return false;
  if (t == SR_ROOM_TARGET || t == SR_ROOM_SECURITY || t == SR_ROOM_SERVER) return true;
  return !has_fake_id && s->in.guards > 0;
}

/* ---- props, devices ------------------------------------------------------------------------ */

SrVec3 sr_site_prop_pos(const SrSite *s, int prop) {
  const SrProp *p = &s->in.props[prop];
  return sr_v3(s->ox + (float)p->x / 100.0f, SR_SITE_FLOOR_Y, s->oz + (float)p->z / 100.0f);
}

static float ahead_score(SrVec3 eye, float yaw, SrVec3 at, float range) {
  float dx = at.x - eye.x, dz = at.z - eye.z;
  float d = sqrtf(dx * dx + dz * dz);
  if (d > range) return -1;
  if (d < 0.6f) return 2.0f - d;
  SrVec3 f = sr_dir_from_yaw_pitch(yaw, 0);
  float dot = (f.x * dx + f.z * dz) / d;
  return dot < 0.6f ? -1 : dot + (range - d) / range;
}

int sr_site_prop_near(const SrSite *s, SrVec3 eye, float yaw, float range) {
  int best = -1;
  float bs = 0;
  for (int i = 0; i < s->in.prop_count; i++) {
    SrPropType t = (SrPropType)s->in.props[i].type;
    if (t != SR_PROP_EXIT && t != SR_PROP_BED && t != SR_PROP_STASH && t != SR_PROP_TERMINAL && t != SR_PROP_WARDROBE &&
        t != SR_PROP_OBJECTIVE && t != SR_PROP_CONSOLE)
      continue;
    float sc = ahead_score(eye, yaw, sr_site_prop_pos(s, i), range);
    if (sc > bs) {
      bs = sc;
      best = i;
    }
  }
  return best;
}

static SrVec3 door_pos(const SrSite *s, int room, int dir) {
  float x = s->ox + ((float)(room % s->in.w) + (dir == 0 ? 1.0f : 0.5f)) * CELL;
  float z = s->oz + ((float)room_row(s, room) + (dir == 1 ? 1.0f : 0.5f)) * CELL;
  return sr_v3(x, SR_SITE_FLOOR_Y + GUARD_EYE, z);
}

int sr_site_locked_door_ahead(const SrSite *s, SrVec3 eye, float yaw, float range) {
  int best = -1;
  float bs = 0;
  for (int d = 0; d < s->in.device_count; d++) {
    const SrDevice *dv = &s->in.devices[d];
    if (dv->type != SR_DEV_DOOR || !dv->locked) continue;
    float sc = ahead_score(eye, yaw, door_pos(s, dv->room, dv->door_dir), range);
    if (sc > bs) {
      bs = sc;
      best = d;
    }
  }
  return best;
}

void sr_site_unlock(SrSite *s, SrDeltas *d, int dev) {
  s->in.devices[dev].locked = false;
  delta_or(d, s->in.devices[dev].id, SR_DV_UNLOCKED);
}

bool sr_site_device_disabled(const SrSite *s, const SrDeltas *d, int dev) {
  int v = sr_deltas_get(d, s->in.devices[dev].id);
  return v > 0 && (v & SR_DV_DISABLED);
}

static void disable_device(SrSite *s, SrDeltas *d, int dev) {
  delta_or(d, s->in.devices[dev].id, SR_DV_DISABLED);
  if (s->in.devices[dev].type == SR_DEV_LIGHTS) s->lights_off = true;
}

void sr_site_console(SrSite *s, SrDeltas *d) {
  for (int i = 0; i < s->in.device_count; i++) {
    SrDeviceType t = s->in.devices[i].type;
    if (t == SR_DEV_CAMERA || t == SR_DEV_TURRET || t == SR_DEV_ALARM) disable_device(s, d, i);
    if (t == SR_DEV_DOOR && s->in.devices[i].locked) sr_site_unlock(s, d, i);
  }
  s->alarm = false;
  s->cam_detect = 0;
}

SrVec3 sr_site_camera_pos(const SrSite *s, int dev) {
  int room = s->in.devices[dev].room;
  float x = s->ox + (float)(room % s->in.w) * CELL + 0.3f + 0.2f * (float)(dev % 3);
  float z = s->oz + (float)room_row(s, room) * CELL + 0.3f;
  return sr_v3(x, SR_SITE_FLOOR_Y + 2.7f, z);
}

/* ---- opening ------------------------------------------------------------------------------- */

static int bfs_next(const SrInterior *in, int from, int to) {
  if (from == to) return to;
  int prev[SR_INT_MAX_ROOMS];
  for (int i = 0; i < SR_INT_MAX_ROOMS; i++) prev[i] = -1;
  int queue[SR_INT_MAX_ROOMS], head = 0, tail = 0;
  queue[tail++] = from;
  prev[from] = from;
  while (head < tail) {
    int cur = queue[head++];
    if (cur == to) break;
    int cx = cur % in->w, cy = cur / in->w;
    int nb[4] = {cx + 1 < in->w ? cur + 1 : -1, cx > 0 ? cur - 1 : -1, cy + 1 < in->h ? cur + in->w : -1,
                 cy > 0 ? cur - in->w : -1};
    for (int k = 0; k < 4; k++) {
      int o = nb[k];
      if (o < 0 || prev[o] >= 0) continue;
      int lo = cur < o ? cur : o, dir = abs(o - cur) == 1 ? 0 : 1;
      if (!(in->doors[lo] & (1 << dir))) continue; /* guards carry keycards: locks do not matter */
      prev[o] = cur;
      queue[tail++] = o;
    }
  }
  if (prev[to] < 0) return from; /* GCOVR_EXCL_LINE reason: interiors are connected */
  int step = to;
  while (prev[step] != from) step = prev[step];
  return step;
}

void sr_site_open(SrSite *s, const SrInterior *in, int building, float ox, float oz, const SrDeltas *d, int entry,
                  int32_t job) {
  memset(s, 0, sizeof *s);
  s->active = true;
  s->in = *in;
  s->building = building;
  s->ox = ox;
  s->oz = oz;
  s->entry = entry;
  s->job = job;
  for (int i = 0; i < s->in.device_count; i++) {
    int v = sr_deltas_get(d, s->in.devices[i].id);
    if (v <= 0) continue;
    if (v & SR_DV_UNLOCKED) s->in.devices[i].locked = false;
    if ((v & SR_DV_DISABLED) && s->in.devices[i].type == SR_DEV_LIGHTS) s->lights_off = true;
  }
  if (job >= 0 && sr_deltas_get(d, sr_site_objective_id(job)) > 0) s->objective_done = true;
  SrRng r = sr_rng_stream(sr_interior_digest(in), "guards", (uint64_t)building, 0);
  int n = in->w * in->h;
  int entry_room = in->entry_count ? in->entries[0].room : 0;
  for (int k = 0; k < in->guards && s->guard_count < SR_SITE_MAX_GUARDS; k++) {
    SrGuard *g = &s->guards[s->guard_count];
    memset(g, 0, sizeof *g);
    g->id = sr_site_guard_id(building, k);
    int room = k == 0 && in->security_room >= 0 ? in->security_room : sr_rng_range(&r, 0, n - 1);
    if (room == entry_room) room = in->target_room;
    g->route[0] = (uint8_t)room;
    g->route_n = 1 + sr_rng_range(&r, 1, 3);
    for (int i = 1; i < g->route_n; i++) g->route[i] = (uint8_t)sr_rng_range(&r, 0, n - 1);
    SrVec3 c = sr_site_room_center(s, room);
    c.x += (float)sr_rng_range(&r, -100, 100) / 100.0f;
    c.z += (float)sr_rng_range(&r, -100, 100) / 100.0f;
    sr_enemy_init(&g->e, c, -1, 100.0f);
    g->e.yaw = (float)sr_rng_range(&r, 0, 628) / 100.0f;
    g->decoy = -1;
    int v = sr_deltas_get(d, g->id);
    if (v == SR_GV_DEAD) g->e.dead = true;
    if (v == SR_GV_KO) g->e.ko = true;
    g->down_time = -1.0f;          /* down since an earlier visit: lies still */
    if (v > 0) g->reported = true; /* found and dealt with after the last visit */
    s->guard_count++;
  }
}

void sr_site_close(SrSite *s) { s->active = false; }

int sr_site_entry_of_type(const SrSite *s, SrEntryType t) {
  for (int e = 0; e < s->in.entry_count; e++)
    if (s->in.entries[e].type == t) return e;
  return -1;
}

SrVec3 sr_site_entry_pos(const SrSite *s, int entry, float *yaw) {
  for (int i = 0; i < s->in.prop_count; i++) {
    const SrProp *p = &s->in.props[i];
    if (p->type != SR_PROP_EXIT || p->variant != entry) continue;
    SrVec3 at = sr_site_prop_pos(s, i);
    static const float k_dx[4] = {0, -1, 0, 1}, k_dz[4] = {1, 0, -1, 0}; /* inward, per facing */
    static const float k_yaw[4] = {SR_PI, SR_PI / 2, 0, -SR_PI / 2};
    float step = s->in.entries[entry].type == SR_ENTRY_ROOF ? 0.8f : 1.1f;
    if (yaw) *yaw = k_yaw[p->facing & 3];
    return sr_v3(at.x + k_dx[p->facing & 3] * step, SR_SITE_FLOOR_Y + 1.7f, at.z + k_dz[p->facing & 3] * step);
  }
  if (yaw) *yaw = 0;                /* GCOVR_EXCL_LINE reason: every entry has an exit prop */
  return sr_site_room_center(s, 0); /* GCOVR_EXCL_LINE reason: as above */
}

/* ---- guards -------------------------------------------------------------------------------- */

static bool guard_up(const SrGuard *g) { return !g->e.dead && !g->e.ko && !g->e.surrendered; }

int sr_site_guards_up(const SrSite *s) {
  int n = 0;
  for (int i = 0; i < s->guard_count; i++) n += guard_up(&s->guards[i]);
  return n;
}

int sr_site_guard_ahead(const SrSite *s, SrVec3 eye, float yaw, float pitch, float range, float cone_cos) {
  SrVec3 f = sr_dir_from_yaw_pitch(yaw, pitch);
  int best = -1;
  float bd = 0;
  for (int i = 0; i < s->guard_count; i++) {
    const SrGuard *g = &s->guards[i];
    if (g->e.dead || g->e.ko) continue;
    SrVec3 aim = sr_v3(g->e.pos.x, g->e.pos.y - 0.4f, g->e.pos.z); /* chest */
    SrVec3 d = sr_v3_sub(aim, eye);
    float dist = sr_v3_len(d);
    if (dist > range || dist < 0.01f) continue;
    float dot = sr_v3_dot(sr_v3_scale(d, 1.0f / dist), f);
    if (dot < cone_cos || !sr_site_los(s, eye, aim)) continue;
    if (dot > bd) {
      bd = dot;
      best = i;
    }
  }
  return best;
}

static void guard_down(SrSite *s, SrDeltas *d, SrGuard *g, bool lethal) {
  g->down_time = s->time;
  if (lethal) {
    g->e.dead = true;
    s->kills++;
  } else {
    g->e.ko = true;
    s->knockouts++;
  }
  sr_deltas_set(d, g->id, lethal ? SR_GV_DEAD : SR_GV_KO);
}

bool sr_site_takedown(SrSite *s, SrDeltas *d, SrVec3 eye, float yaw, bool lethal) {
  for (int i = 0; i < s->guard_count; i++) {
    SrGuard *g = &s->guards[i];
    SrVec3 at = sr_v3(eye.x, g->e.pos.y, eye.z);
    if (ahead_score(eye, yaw, g->e.pos, 2.0f) < 0) continue;
    float noise = 0;
    if (!sr_takedown(&g->e, at, lethal, &noise)) continue;
    g->e.dead = g->e.ko = false; /* booked by guard_down */
    g->hit_dir = atan2f(eye.x - g->e.pos.x, eye.z - g->e.pos.z);
    guard_down(s, d, g, lethal);
    sr_site_noise(s, g->e.pos, noise);
    return true;
  }
  return false;
}

void sr_site_noise(SrSite *s, SrVec3 pos, float radius) {
  for (int i = 0; i < s->guard_count; i++) sr_hear_noise(&s->guards[i].e, pos, radius);
}

bool sr_site_shoot(SrSite *s, SrDeltas *d, SrVec3 eye, float yaw, float pitch, const SrWeaponStats *w) {
  float cone = cosf(0.035f + w->spread * 0.01f);
  sr_site_noise(s, eye, w->noise * 1.5f);
  int gi = sr_site_guard_ahead(s, eye, yaw, pitch, w->range, cone);
  if (gi < 0) return false;
  SrGuard *g = &s->guards[gi];
  SrVec3 to = sr_v3_norm(sr_v3_sub(g->e.pos, eye));
  bool head = sr_v3_dot(to, sr_dir_from_yaw_pitch(yaw, pitch)) > 0.9995f;
  float before = g->e.health;
  g->hit_time = s->time;
  g->hit_dir = atan2f(eye.x - g->e.pos.x, eye.z - g->e.pos.z);
  bool hit = sr_enemy_hit(&g->e, w, head ? SR_ZONE_HEAD : SR_ZONE_TORSO);
  g->hit_kind = head ? 3 : (before - g->e.health) >= 0.35f * g->e.max_health ? 2 : 1;
  if (hit) {
    bool lethal = g->e.dead;
    g->e.dead = g->e.ko = false;
    guard_down(s, d, g, lethal);
  } else {
    g->e.investigate = eye;
    s->hostile = true;
  }
  return true;
}

/* ---- effects and gadgets ------------------------------------------------------------------- */

int sr_site_add_effect(SrSite *s, SrEffectType t, SrVec3 pos, float radius, float time) {
  int slot = -1;
  for (int i = 0; i < s->effect_count; i++)
    if (!s->effects[i].active) slot = slot < 0 ? i : slot;
  if (slot < 0) {
    if (s->effect_count >= SR_SITE_MAX_EFFECTS) return -1;
    slot = s->effect_count++;
  }
  SrSiteEffect *e = &s->effects[slot];
  memset(e, 0, sizeof *e);
  e->type = (uint8_t)t;
  e->active = true;
  e->pos = pos;
  e->radius = radius;
  e->time = time;
  return slot;
}

static bool effect_active(const SrSite *s, SrEffectType t) {
  for (int i = 0; i < s->effect_count; i++)
    if (s->effects[i].active && s->effects[i].type == t) return true;
  return false;
}

SrVec3 sr_site_throw_point(const SrSite *s, SrVec3 eye, float yaw, float max_dist) {
  SrVec3 f = sr_dir_from_yaw_pitch(yaw, 0);
  SrVec3 at = eye;
  for (int i = 1; (float)i * 0.25f <= max_dist; i++) {
    SrVec3 p = sr_v3_add(eye, sr_v3_scale(f, (float)i * 0.25f));
    if (wall_at(s, p.x - s->ox, p.z - s->oz, 0.2f, false, true)) break;
    at = p;
  }
  at.y = SR_SITE_FLOOR_Y + 0.2f;
  return at;
}

static float flat_dist(SrVec3 a, SrVec3 b) {
  float dx = a.x - b.x, dz = a.z - b.z;
  return sqrtf(dx * dx + dz * dz);
}

bool sr_site_gadget(SrSite *s, SrSiteCtx *ctx, int gadget, SrVec3 eye, float yaw) {
  const SrGadgetDef *def = sr_gadget_def(gadget);
  SrVec3 at = sr_site_throw_point(s, eye, yaw, 12.0f);
  SrVec3 at_eye = sr_v3(at.x, SR_SITE_FLOOR_Y + GUARD_EYE, at.z);
  switch (gadget) {
    case SR_GD_FRAG:
      for (int i = 0; i < s->guard_count; i++) {
        SrGuard *g = &s->guards[i];
        float dist = flat_dist(g->e.pos, at);
        if (!guard_up(g) || dist > def->radius || !sr_site_los(s, at_eye, g->e.pos)) continue;
        g->e.health -= def->damage * (1.0f - dist / def->radius * 0.5f);
        if (g->e.health <= 0) guard_down(s, ctx->deltas, g, true);
      }
      if (flat_dist(eye, at) < def->radius) ctx->player_damage += def->damage * 0.5f;
      sr_site_noise(s, at, 40.0f);
      s->hostile = true;
      return true;
    case SR_GD_FLASH:
      for (int i = 0; i < s->guard_count; i++) {
        SrGuard *g = &s->guards[i];
        if (guard_up(g) && flat_dist(g->e.pos, at) < def->radius && sr_site_los(s, at_eye, g->e.pos))
          g->stun = def->duration;
      }
      sr_site_noise(s, at, 20.0f);
      return true;
    case SR_GD_SLEEPGAS:
      for (int i = 0; i < s->guard_count; i++) {
        SrGuard *g = &s->guards[i];
        if (guard_up(g) && flat_dist(g->e.pos, at) < def->radius * 0.5f) guard_down(s, ctx->deltas, g, false);
      }
      if (flat_dist(eye, at) < def->radius * 0.5f && !ctx->toxin_filter) ctx->player_damage += 10.0f;
      return true;
    case SR_GD_EMP:
      for (int i = 0; i < s->in.device_count; i++) {
        SrDeviceType t = s->in.devices[i].type;
        if (t != SR_DEV_CAMERA && t != SR_DEV_TURRET && t != SR_DEV_LIGHTS) continue;
        SrVec3 dp = t == SR_DEV_LIGHTS ? at : sr_site_camera_pos(s, i);
        if (flat_dist(dp, at) <= def->radius) disable_device(s, ctx->deltas, i);
      }
      return true;
    case SR_GD_SMOKE: return sr_site_add_effect(s, SR_FX_SMOKE, at, 3.5f, def->duration) >= 0;
    case SR_GD_LURE: sr_site_noise(s, at, def->radius); return true;
    case SR_GD_DECOY: {
      SrVec3 p = sr_site_throw_point(s, eye, yaw, 3.0f);
      p.y = SR_SITE_FLOOR_Y + GUARD_EYE;
      return sr_site_add_effect(s, SR_FX_DECOY, p, 0.5f, def->duration) >= 0;
    }
    case SR_GD_JAMMER: return sr_site_add_effect(s, SR_FX_JAMMER, eye, def->radius, def->duration) >= 0;
    case SR_GD_TRIPMINE:
      return sr_site_add_effect(s, SR_FX_TRIPMINE, sr_site_throw_point(s, eye, yaw, 1.5f), 1.5f, 1e9f) >= 0;
    case SR_GD_TURRET:
      return sr_site_add_effect(s, SR_FX_TURRET, sr_site_throw_point(s, eye, yaw, 1.5f), def->radius, def->duration) >=
             0;
    case SR_GD_DATASPIKE: return sr_site_add_effect(s, SR_FX_LOOP, at, def->radius, def->duration) >= 0;
    case SR_GD_STICKYCAM:
    case SR_GD_DRONE:
    case SR_GD_SPIDER: {
      int n = 0;
      float reach = gadget == SR_GD_STICKYCAM ? 15.0f : def->radius;
      for (int i = 0; i < s->guard_count; i++)
        if (flat_dist(s->guards[i].e.pos, eye) < reach) {
          s->guards[i].tagged = true;
          n++;
        }
      return n > 0 || gadget != SR_GD_STICKYCAM;
    }
    case SR_GD_BREACH:
    case SR_GD_MULTITOOL:
    case SR_GD_LANCE: {
      int d = sr_site_locked_door_ahead(s, eye, yaw, 2.5f);
      if (d < 0) return false;
      sr_site_unlock(s, ctx->deltas, d);
      if (gadget == SR_GD_BREACH) {
        sr_site_noise(s, eye, 30.0f);
        s->hostile = true;
      }
      return true;
    }
    default: return false;
  }
}

/* ---- update -------------------------------------------------------------------------------- */

static void move_towards(SrGuard *g, SrVec3 target, float speed, float dt) {
  float dx = target.x - g->e.pos.x, dz = target.z - g->e.pos.z;
  float d = sqrtf(dx * dx + dz * dz);
  if (d < 0.05f) return;
  float step = SR_MIN(d, speed * dt);
  g->e.pos.x += dx / d * step;
  g->e.pos.z += dz / d * step;
  g->e.yaw = atan2f(-dx, -dz);
  g->walk += step;
}

/* Walks a guard towards a world point through door midpoints. */
static void navigate(SrSite *s, SrGuard *g, SrVec3 target, float speed, float dt) {
  int from = sr_site_room_at(s, g->e.pos.x, g->e.pos.z), to = sr_site_room_at(s, target.x, target.z);
  if (from < 0 || to < 0 || from == to) {
    move_towards(g, target, speed, dt);
    return;
  }
  int next = bfs_next(&s->in, from, to);
  int lo = from < next ? from : next, dir = abs(next - from) == 1 ? 0 : 1;
  SrVec3 door = door_pos(s, lo, dir);
  if (flat_dist(door, g->e.pos) < 0.3f) /* step through into the next room */
    move_towards(g, sr_site_room_center(s, next), speed, dt);
  else
    move_towards(g, door, speed, dt);
}

static bool alarm_device_live(const SrSite *s, const SrDeltas *d) {
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_ALARM && !sr_site_device_disabled(s, d, i)) return true;
  return false;
}

static void raise_alarm(SrSite *s, SrSiteCtx *ctx, SrVec3 where) {
  s->hostile = true;
  if (effect_active(s, SR_FX_JAMMER) || !alarm_device_live(s, ctx->deltas)) return;
  if (!s->alarm) {
    s->alarm = true;
    s->reinforce_at = s->time + 45.0f; /* the security company sends more people */
    for (int i = 0; i < s->guard_count; i++) {
      SrGuard *g = &s->guards[i];
      g->e.heightened = true;
      sr_hear_noise(&g->e, where, 1e6f);
    }
  }
}

static void update_guard(SrSite *s, SrSiteCtx *ctx, SrGuard *g, float dt) {
  if (g->e.dead || g->e.ko) return;
  if (g->stun > 0) {
    g->stun -= dt;
    return;
  }
  int proom = sr_site_room_at(s, ctx->player.x, ctx->player.z);
  /* bodies */
  for (int i = 0; i < s->guard_count; i++) {
    SrGuard *o = &s->guards[i];
    if (o == g || o->reported || !(o->e.dead || o->e.ko)) continue;
    if (sr_in_view(&g->e, o->e.pos, 12.0f) && sr_site_los(s, g->e.pos, o->e.pos)) {
      sr_body_discovered(&g->e, o->e.pos);
      o->reported = true;
      s->hostile = true;
    }
  }
  /* decoys: the guard treats a decoy in view as the player until it gets close */
  if (g->decoy < 0) {
    for (int i = 0; i < s->effect_count; i++) {
      SrSiteEffect *e = &s->effects[i];
      if (e->active && e->type == SR_FX_DECOY && sr_in_view(&g->e, e->pos, SR_SITE_GUARD_RANGE) &&
          sr_site_los(s, g->e.pos, e->pos)) {
        g->decoy = (int8_t)i;
        g->e.alert = SR_ALERT_COMBAT;
        g->e.awareness = 1.0f;
        g->e.investigate = e->pos;
      }
    }
  }
  if (g->decoy >= 0) {
    SrSiteEffect *e = &s->effects[g->decoy];
    if (!e->active || flat_dist(e->pos, g->e.pos) < SR_SITE_DECOY_REVEAL) {
      e->active = false; /* revealed */
      g->decoy = -1;
      g->e.alert = SR_ALERT_SEARCHING;
      g->e.search_timer = 10.0f;
      return;
    }
    g->fire_cd -= dt;
    if (g->fire_cd <= 0) {
      g->fire_cd = FIRE_CD;
      s->shots_fired++;
      g->shot_time = s->time;
      g->shot_tracer = s->shots_fired % 3 == 0;
    }
    navigate(s, g, e->pos, CHASE_SPD, dt);
    return;
  }
  /* perception of the player (only where the player is not allowed, or once hostile) */
  SrPerceptInput in;
  memset(&in, 0, sizeof in);
  in.player_pos = sr_v3(1e6f, 0, 1e6f);
  bool visible = sr_site_los(s, g->e.pos, ctx->player);
  if (visible && sr_site_restricted(s, proom, ctx->has_fake_id) && !(ctx->masked && !s->hostile))
    in.player_pos = ctx->player;
  in.light = sr_site_light(s, proom);
  in.speed = ctx->player_speed;
  in.crouched = ctx->crouched;
  in.camo = ctx->camo;
  in.visibility = SR_SITE_GUARD_RANGE;
  in.difficulty = ctx->difficulty;
  SrAlert before = g->e.alert;
  SrAlert a = sr_perceive(&g->e, &in, dt);
  bool sees_player = in.player_pos.x < 1e5f && sr_in_view(&g->e, ctx->player, SR_SITE_GUARD_RANGE);
  if (sees_player) s->witnessed = s->witnessed || a == SR_ALERT_COMBAT;
  if (a == SR_ALERT_COMBAT && before != SR_ALERT_COMBAT) raise_alarm(s, ctx, ctx->player);
  if (a == SR_ALERT_COMBAT && sr_enemy_tactics(&g->e, effect_active(s, SR_FX_JAMMER), s->kills + s->knockouts))
    raise_alarm(s, ctx, ctx->player);
  switch (g->e.alert) {
    case SR_ALERT_COMBAT: {
      float d = flat_dist(g->e.pos, ctx->player);
      if (d > 6.0f || !visible) navigate(s, g, ctx->player, CHASE_SPD, dt);
      g->e.yaw = atan2f(-(ctx->player.x - g->e.pos.x), -(ctx->player.z - g->e.pos.z));
      g->fire_cd -= dt;
      if (visible && g->fire_cd <= 0) {
        g->fire_cd = FIRE_CD;
        s->shots_fired++;
        g->shot_time = s->time;
        g->shot_tracer = s->shots_fired % 3 == 0;
        uint64_t key = sr_hash_combine(ctx->key, g->id * 977u + (uint64_t)s->shots_fired);
        if (sr_enemy_fire(&g->e, d, ctx->difficulty, key))
          ctx->player_damage += GUARD_DMG * sr_difficulty(ctx->difficulty)->enemy_damage;
        sr_site_noise(s, g->e.pos, 25.0f);
      }
      break;
    }
    case SR_ALERT_SEARCHING:
    case SR_ALERT_SUSPICIOUS: navigate(s, g, g->e.investigate, SEARCH_SPD, dt); break;
    default: {
      SrVec3 goal = sr_site_room_center(s, g->route[g->route_i]);
      if (g->pause > 0) { /* looking around at the waypoint */
        g->pause -= dt;
        break;
      }
      if (flat_dist(goal, g->e.pos) < 0.3f) {
        g->route_i = (g->route_i + 1) % g->route_n;
        g->pause = SR_GUARD_PAUSE_MIN + (float)(g->id % 3u);
        break;
      }
      navigate(s, g, goal, PATROL_SPD, dt);
      break;
    }
  }
}

static bool device_bit(const SrSite *s, const SrDeltas *d, int dev, int bit) {
  int v = sr_deltas_get(d, s->in.devices[dev].id);
  return v > 0 && (v & bit);
}

/* A camera switched off (not looped) shows up on the monitors: a guard goes to look at it. */
static void camera_checks(SrSite *s, SrSiteCtx *ctx, float dt) {
  for (int i = 0; i < s->in.device_count; i++) {
    const SrDevice *dv = &s->in.devices[i];
    if (dv->type != SR_DEV_CAMERA || s->checked[i] || !device_bit(s, ctx->deltas, i, SR_DV_DISABLED)) continue;
    s->check_t[i] += dt;
    if (s->check_t[i] < SR_CAMERA_CHECK_S) continue;
    s->checked[i] = true;
    int best = -1;
    float bd = 1e9f;
    SrVec3 cp = sr_site_camera_pos(s, i);
    for (int k = 0; k < s->guard_count; k++) {
      const SrGuard *g = &s->guards[k];
      if (!guard_up(g)) continue;
      float d = sr_v3_len(sr_v3_sub(g->e.pos, cp));
      if (d < bd) bd = d, best = k;
    }
    if (best >= 0) {
      SrGuard *g = &s->guards[best];
      g->e.investigate = sr_v3(cp.x, SR_SITE_FLOOR_Y + 1.6f, cp.z + 0.6f);
      if (g->e.alert == SR_ALERT_UNAWARE) g->e.alert = SR_ALERT_SUSPICIOUS;
      g->e.search_timer = 30.0f;
      g->e.awareness = SR_MAX(g->e.awareness, 0.3f);
    }
  }
}

void sr_site_false_alarm(SrSite *s, int room) {
  SrVec3 c = sr_site_room_center(s, room);
  for (int i = 0; i < s->guard_count; i++) {
    SrGuard *g = &s->guards[i];
    if (!guard_up(g) || g->e.alert == SR_ALERT_COMBAT) continue;
    g->e.investigate = c;
    g->e.alert = SR_ALERT_SEARCHING;
    g->e.awareness = SR_MAX(g->e.awareness, 0.3f);
    g->e.search_timer = 40.0f;
  }
}

static void update_cameras(SrSite *s, SrSiteCtx *ctx, float dt) {
  camera_checks(s, ctx, dt);
  if (effect_active(s, SR_FX_LOOP)) return;
  int proom = sr_site_room_at(s, ctx->player.x, ctx->player.z);
  bool seen = false;
  for (int i = 0; i < s->in.device_count; i++) {
    const SrDevice *dv = &s->in.devices[i];
    if (dv->type != SR_DEV_CAMERA || dv->room != proom || sr_site_device_disabled(s, ctx->deltas, i)) continue;
    if (device_bit(s, ctx->deltas, i, SR_DV_LOOPED)) continue; /* looped: shows an empty corridor */
    if (!sr_site_restricted(s, proom, ctx->has_fake_id) || ctx->camo) continue;
    SrVec3 cp = sr_site_camera_pos(s, i);
    if (!sr_site_los(s, cp, ctx->player)) continue;
    /* sweeping cone towards the room centre */
    SrVec3 c = sr_site_room_center(s, dv->room);
    float base = atan2f(c.x - cp.x, c.z - cp.z) + sinf(s->time * 0.6f + (float)i) * 0.6f;
    float to = atan2f(ctx->player.x - cp.x, ctx->player.z - cp.z);
    if (fabsf(sr_wrap_angle(to - base)) < 0.9f) seen = true;
  }
  if (seen) {
    s->cam_detect += dt / SR_SITE_CAMERA_TIME * (ctx->crouched ? 0.6f : 1.0f);
    if (s->cam_detect >= 1.0f) {
      s->cam_detect = 1.0f;
      s->camera_seen = true;
      raise_alarm(s, ctx, ctx->player);
    }
  } else {
    s->cam_detect = SR_MAX(0.0f, s->cam_detect - dt * 0.3f);
  }
  /* turrets fire once the alarm is up; with switched friend-or-foe they fire at the guards */
  s->turret_cd -= dt;
  for (int i = 0; i < s->in.device_count; i++) {
    const SrDevice *dv = &s->in.devices[i];
    if (dv->type != SR_DEV_TURRET || sr_site_device_disabled(s, ctx->deltas, i)) continue;
    SrVec3 tp = sr_site_camera_pos(s, i);
    if (device_bit(s, ctx->deltas, i, SR_DV_FOF)) {
      for (int k = 0; k < s->guard_count && s->turret_cd <= 0; k++) {
        SrGuard *g = &s->guards[k];
        if (!guard_up(g) || sr_site_room_at(s, g->e.pos.x, g->e.pos.z) != dv->room || !sr_site_los(s, tp, g->e.pos))
          continue;
        s->turret_cd = 0.6f;
        s->shots_fired++;
        g->e.health -= 20.0f;
        if (g->e.health <= 0) guard_down(s, ctx->deltas, g, true);
      }
      continue;
    }
    if (!s->alarm || dv->room != proom) continue;
    if (s->turret_cd > 0 || !sr_site_los(s, tp, ctx->player)) continue;
    s->turret_cd = 0.6f;
    s->shots_fired++;
    ctx->player_damage += 6.0f * sr_difficulty(ctx->difficulty)->enemy_damage;
  }
}

static void update_effects(SrSite *s, SrSiteCtx *ctx, float dt) {
  for (int i = 0; i < s->effect_count; i++) {
    SrSiteEffect *e = &s->effects[i];
    if (!e->active) continue;
    e->time -= dt;
    if (e->time <= 0) {
      e->active = false;
      continue;
    }
    e->cooldown -= dt;
    if (e->type == SR_FX_TRIPMINE) {
      for (int k = 0; k < s->guard_count; k++) {
        SrGuard *g = &s->guards[k];
        if (!guard_up(g) || flat_dist(g->e.pos, e->pos) > e->radius) continue;
        guard_down(s, ctx->deltas, g, true);
        e->active = false;
        sr_site_noise(s, e->pos, 30.0f);
        break;
      }
    } else if (e->type == SR_FX_TURRET && e->cooldown <= 0) {
      SrVec3 tp = sr_v3(e->pos.x, SR_SITE_FLOOR_Y + 1.0f, e->pos.z);
      for (int k = 0; k < s->guard_count; k++) {
        SrGuard *g = &s->guards[k];
        if (!guard_up(g) || g->e.alert != SR_ALERT_COMBAT || flat_dist(g->e.pos, tp) > e->radius) continue;
        if (!sr_site_los(s, tp, g->e.pos)) continue;
        g->e.health -= 15.0f;
        if (g->e.health <= 0) guard_down(s, ctx->deltas, g, true);
        e->cooldown = 0.8f;
        break;
      }
    }
  }
}

void sr_site_update(SrSite *s, SrSiteCtx *ctx, float dt) {
  if (!s->active) return;
  s->time += dt;
  ctx->player_damage = 0;
  update_effects(s, ctx, dt);
  s->awareness = 0;
  for (int i = 0; i < s->guard_count; i++) {
    SrGuard *g = &s->guards[i];
    float walk0 = g->walk;
    bool was_ko = g->e.ko;
    update_guard(s, ctx, g, dt);
    g->speed = dt > 0 ? (g->walk - walk0) / dt : 0.0f;
    if (was_ko && !g->e.ko && !g->e.dead) g->wake_time = s->time;
    if (guard_up(g)) s->awareness = SR_MAX(s->awareness, SR_MIN(g->e.awareness, 1.0f));
  }
  update_cameras(s, ctx, dt);
  s->damage += ctx->player_damage;
  /* reinforcements called by the alarm walk in through the front */
  if (s->reinforce_at > 0 && s->time >= s->reinforce_at) {
    s->reinforce_at = 0;
    if (s->guard_count < SR_SITE_MAX_GUARDS) {
      SrGuard *g = &s->guards[s->guard_count];
      memset(g, 0, sizeof *g);
      g->id = sr_site_guard_id((int)s->building, 12 + s->guard_count % 4);
      sr_enemy_init(&g->e, sr_site_room_center(s, s->in.entries[0].room), -1, 100.0f);
      g->route[0] = s->in.entries[0].room;
      g->route_n = 1;
      g->decoy = -1;
      g->e.alert = SR_ALERT_SEARCHING;
      g->e.investigate = ctx->player;
      g->e.search_timer = 60.0f;
      s->guard_count++;
    }
  }
  /* the elevator travels to the floor its controller asks for */
  for (int i = 0; i < s->in.device_count; i++) {
    if (s->in.devices[i].type != SR_DEV_ELEVATOR) continue;
    int target = sr_deltas_get(ctx->deltas, sr_net_elevator_id(s->in.devices[i].id));
    s->elevator_target = target < 0 ? 0 : target;
    float d = (float)s->elevator_target - s->elevator_floor;
    float step = SR_MIN(fabsf(d), 0.5f * dt);
    s->elevator_floor += d > 0 ? step : -step;
  }
}

SrMissionResult sr_site_result(const SrSite *s, int64_t now) {
  SrMissionResult r;
  memset(&r, 0, sizeof r);
  r.success = s->objective_done;
  r.alarm = s->alarm;
  r.kills = s->kills;
  r.knockouts = s->knockouts;
  r.damage = s->damage;
  r.witnessed = s->witnessed;
  r.camera = s->camera_seen;
  r.seconds = now - s->entered_at;
  return r;
}
