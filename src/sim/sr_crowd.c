#include "sim/sr_crowd.h"

#include "core/sr_rng.h"
#include "world/sr_dressing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void sr_crowd_init(SrCrowd *cr) {
  memset(cr, 0, sizeof *cr);
  cr->last_slot = -1;
  cr->fresh = true;
  cr->police_outfit = -1;
  cr->police_faction = -1;
}

bool sr_camera_sees(const SrCamera *cam, SrVec3 p) {
  SrVec3 d = sr_v3_sub(p, cam->pos);
  d.y = 0;
  float len = sr_v3_len(d);
  if (len < 0.5f) return true;
  SrVec3 f = cam->forward;
  f.y = 0;
  f = sr_v3_norm(f);
  return sr_v3_dot(sr_v3_scale(d, 1.0f / len), f) >= cam->cos_half_fov;
}

static float dist_xz(SrVec3 a, SrVec3 b) {
  float dx = a.x - b.x, dz = a.z - b.z;
  return sqrtf(dx * dx + dz * dz);
}

int sr_crowd_find(const SrCrowd *cr, uint64_t key) {
  for (int i = 0; i < cr->count; i++)
    if (cr->agents[i].key == key) return i;
  return -1;
}

static bool may_pop(const SrCamera *cam, SrVec3 p) {
  return !(dist_xz(p, cam->pos) < SR_NO_POP_RADIUS && sr_camera_sees(cam, p));
}

static void pick_target(SrAgent *a, const SrCity *c) {
  /* walk to a neighbouring corner of the block's pavement ring, along the clear walking lane
   * between the kerb props and the building band */
  int blk = a->block < 0 ? 0 : a->block;
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float pav[4];
  sr_block_pavements(c, blk, pav);
  float lx0 = (float)(ox + SR_STREET_CM) / 100.0f + sr_walk_lane(pav[0]);
  float lz0 = (float)(oy + SR_STREET_CM) / 100.0f + sr_walk_lane(pav[1]);
  float lx1 = (float)(ox + SR_BLOCK_CM) / 100.0f - sr_walk_lane(pav[2]);
  float lz1 = (float)(oy + SR_BLOCK_CM) / 100.0f - sr_walk_lane(pav[3]);
  float corners[4][2] = {{lx0, lz0}, {lx1, lz0}, {lx1, lz1}, {lx0, lz1}};
  int near = 0;
  float best = 1e9f;
  for (int k = 0; k < 4; k++) {
    float dx = corners[k][0] - a->pos.x, dz = corners[k][1] - a->pos.z;
    if (dx * dx + dz * dz < best) {
      best = dx * dx + dz * dz;
      near = k;
    }
  }
  SrRng r = sr_rng_stream(a->key, "walk", (uint64_t)(a->pos.x * 7 + a->pos.z), 0);
  int k = best > 4.0f ? near : (near + (sr_rng_range(&r, 0, 1) ? 1 : 3)) % 4; /* to the ring, then along it */
  a->target = sr_v3(corners[k][0], 0, corners[k][1]);
}

static void spawn(SrCrowd *cr, const SrCity *c, const SrAmbient *amb, int npc, int block) {
  if (cr->count >= SR_MAX_AGENTS) return;
  SrAgent *a = &cr->agents[cr->count++];
  memset(a, 0, sizeof *a);
  a->key = amb->key;
  a->npc = npc;
  a->look = amb->look;
  a->role = amb->role;
  a->faction = amb->faction;
  a->bravery = amb->bravery;
  a->umbrella = amb->umbrella;
  if (a->role == SR_ROLE_COP && cr->police_outfit >= 0) {
    a->look.outfit_color = (uint8_t)cr->police_outfit;
    a->faction = cr->police_faction;
  }
  a->pos = sr_v3((float)amb->x / 100.0f, 0, (float)amb->y / 100.0f);
  a->speed = 1.2f + (float)(amb->key % 5) * 0.08f;
  a->vel = a->speed; /* spawned out of view, already walking */
  a->state = SR_AG_WALK;
  a->block = block;
  pick_target(a, c);
}

void sr_crowd_refresh(SrCrowd *cr, const SrCity *c, const SrPopulation *pop, const SrCamera *cam, int64_t t,
                      int max_agents) {
  int64_t slot = t / 1800;
  bool new_slot = slot != cr->last_slot;
  cr->last_slot = slot;
  /* despawn agents far away; never in view nearby */
  for (int i = cr->count - 1; i >= 0; i--) {
    SrAgent *a = &cr->agents[i];
    float d = dist_xz(a->pos, cam->pos);
    bool far = d > SR_CROWD_RADIUS + 20.0f;
    bool stale = new_slot && a->npc < 0 && d > SR_CROWD_RADIUS * 0.6f;
    if ((far || stale) && may_pop(cam, a->pos)) { cr->agents[i] = cr->agents[--cr->count]; }
  }
  int bx = (int)(cam->pos.x * 100.0f) / SR_BLOCK_CM, by = (int)(cam->pos.z * 100.0f) / SR_BLOCK_CM;
  int rad = (int)(SR_CROWD_RADIUS * 100.0f) / SR_BLOCK_CM + 1;
  SrAmbient amb[SR_MAX_AMBIENT_PER_BLOCK];
  /* blocks in front of the camera first, so the agent budget goes where people look */
  int order[(2 * 4 + 1) * (2 * 4 + 1)][2];
  float score[(2 * 4 + 1) * (2 * 4 + 1)];
  int no = 0;
  if (rad > 4) rad = 4;
  for (int dy = -rad; dy <= rad; dy++)
    for (int dx = -rad; dx <= rad; dx++) {
      float fx = (float)dx * cam->forward.x + (float)dy * cam->forward.z;
      float s = (float)(dx * dx + dy * dy) - 2.5f * fx;
      int k = no++;
      while (k > 0 && score[k - 1] > s) {
        score[k] = score[k - 1];
        order[k][0] = order[k - 1][0];
        order[k][1] = order[k - 1][1];
        k--;
      }
      score[k] = s;
      order[k][0] = dx;
      order[k][1] = dy;
    }
  for (int oi = 0; oi < no; oi++) {
    {
      int dx = order[oi][0], dy = order[oi][1];
      int b = sr_city_block_index(c, bx + dx, by + dy);
      if (b < 0) continue;
      int n = sr_ambient_generate(c, b, t, amb, SR_MAX_AMBIENT_PER_BLOCK);
      for (int i = 0; i < n && cr->count < max_agents; i++) {
        if (sr_crowd_find(cr, amb[i].key) >= 0) continue;
        SrVec3 p = sr_v3((float)amb[i].x / 100.0f, 0, (float)amb[i].y / 100.0f);
        if (dist_xz(p, cam->pos) > SR_CROWD_RADIUS) continue;
        if (!cr->fresh && !may_pop(cam, p)) continue; /* no pop-in inside the view nearby */
        int npc = pop ? sr_population_find_by_key(pop, amb[i].key) : -1;
        /* curfew: only officers, gangers and one citizen in five stay out */
        int dist = c->blocks[b].district;
        if (dist >= 0 && dist < 16 && (cr->curfew & (1u << dist)) && amb[i].role != SR_ROLE_COP &&
            amb[i].role != SR_ROLE_GANG && amb[i].key % 5 != 0)
          continue;
        spawn(cr, c, &amb[i], npc, b);
      }
    }
  }
  cr->fresh = false;
}

/* ---- activities ------------------------------------------------------------------------- */

/* Props within reach of a standing citizen decide what they do there. */
static const uint8_t k_prop_acts[][4] = {
    {SR_SP_BENCH, SR_SA_SIT, SR_SA_SLEEP, 0},
    {SR_SP_STALL, SR_SA_EAT, SR_SA_QUEUE, SR_SA_BUY},
    {SR_SP_KIOSK, SR_SA_EAT, SR_SA_QUEUE, SR_SA_BUY},
    {SR_SP_VENDING, SR_SA_BUY, 0, 0},
    {SR_SP_VENDROW, SR_SA_BUY, 0, 0},
    {SR_SP_TRAFFIC_LIGHT, SR_SA_WAIT, 0, 0},
    {SR_SP_MARQUEE, SR_SA_QUEUE, SR_SA_DANCE, SR_SA_SMOKE},
    {SR_SP_BARREL, SR_SA_WARM, 0, 0},
    {SR_SP_BOOTH, SR_SA_CALL, 0, 0},
    {SR_SP_HOLOTREE, SR_SA_WATCH, SR_SA_POINT, 0},
    {SR_SP_SHRINE, SR_SA_WINDOW, 0, 0},
};

static int prop_row(int kind) {
  for (int i = 0; i < (int)SR_ARRAY_LEN(k_prop_acts); i++)
    if (k_prop_acts[i][0] == kind) return i;
  return -1;
}

bool sr_activity_fits_prop(int prop_kind, SrStreetAct a) {
  int r = prop_row(prop_kind);
  if (r < 0) return true;
  for (int k = 1; k < 4; k++)
    if (k_prop_acts[r][k] && k_prop_acts[r][k] == a) return true;
  return false;
}

/* one block of street props, cached: stopping citizens look them up often */
static const SrStreetProp *block_props(const SrCity *c, int block, int *n) {
  static SrStreetProp props[SR_MAX_BLOCK_PROPS];
  static const SrCity *cc;
  static uint64_t seed;
  static int cblock = -1, cn;
  *n = 0;
  if (!c) return props; /* GCOVR_EXCL_LINE reason: callers always pass a city */
  if (cc != c || seed != c->seed || cblock != block) {
    cn = sr_dressing_block(c, block, props, SR_MAX_BLOCK_PROPS);
    cc = c;
    seed = c->seed;
    cblock = block;
  }
  *n = cn;
  return props;
}

int sr_activity_prop_near(const SrCity *c, int block, SrVec3 pos, SrVec3 *prop_pos, float *prop_yaw) {
  if (block < 0) return -1;
  int n;
  const SrStreetProp *p = block_props(c, block, &n);
  int best = -1;
  float bd = 3.0f * 3.0f;
  for (int i = 0; i < n; i++) {
    if (prop_row(p[i].kind) < 0) continue;
    float dx = p[i].x - pos.x, dz = p[i].z - pos.z, d = dx * dx + dz * dz;
    if (d <= bd) {
      bd = d;
      best = i;
    }
  }
  if (best < 0) return -1;
  if (prop_pos) *prop_pos = sr_v3(p[best].x, 0, p[best].z);
  if (prop_yaw) *prop_yaw = p[best].yaw;
  return p[best].kind;
}

SrStreetAct sr_activity_pick(const SrCity *c, int block, SrVec3 pos, uint64_t key, int role, SrVec3 *face) {
  SrRng r = sr_rng_stream(key, "activity", (uint64_t)(pos.x * 3.0f) * 131u + (uint64_t)(pos.z * 3.0f), 0);
  SrVec3 pp;
  float yaw = 0;
  int kind = sr_activity_prop_near(c, block, pos, &pp, &yaw);
  SrDistrictType dt = block >= 0 ? c->districts[c->blocks[block].district].type : SR_D_RESIDENTIAL;
  if (kind >= 0) {
    const uint8_t *row = k_prop_acts[prop_row(kind)];
    int opts = 0;
    while (opts < 3 && row[opts + 1]) opts++;
    int k = sr_rng_range(&r, 0, opts - 1);
    SrStreetAct a = (SrStreetAct)row[1 + k];
    /* benches are beds in the Barrens and the Undercity */
    if (kind == SR_SP_BENCH)
      a = (dt == SR_D_BARRENS || dt == SR_D_UNDERCITY || role == SR_ROLE_DRIFTER) && sr_rng_chance(&r, 500)
              ? SR_SA_SLEEP
              : SR_SA_SIT;
    if (face) {
      SrVec3 to = sr_v3_sub(pp, pos);
      to.y = 0;
      *face = kind == SR_SP_BENCH    ? sr_v3(sinf(yaw), 0, cosf(yaw))
              : sr_v3_len(to) > 0.2f ? sr_v3_norm(to)
                                     : sr_v3(0, 0, 1);
    }
    return a;
  }
  /* no prop: by district and role */
  static const uint8_t base[] = {SR_SA_IDLE, SR_SA_IDLE, SR_SA_SMOKE, SR_SA_PHONE, SR_SA_PHONE,
                                 SR_SA_CALL, SR_SA_WAIT, SR_SA_WATCH, SR_SA_LEAN,  SR_SA_WINDOW};
  SrStreetAct a = (SrStreetAct)base[sr_rng_range(&r, 0, (int)SR_ARRAY_LEN(base) - 1)];
  if (dt == SR_D_SPIRE && a == SR_SA_SMOKE) a = SR_SA_PHONE;
  if ((dt == SR_D_DOCKS || dt == SR_D_INDUSTRIAL) && sr_rng_chance(&r, 300)) a = SR_SA_SMOKE;
  if (dt == SR_D_ENTERTAINMENT && sr_rng_chance(&r, 250)) a = SR_SA_DANCE;
  if ((dt == SR_D_BARRENS || role == SR_ROLE_DRIFTER) && sr_rng_chance(&r, 300)) a = SR_SA_BEG;
  if (face) *face = sr_v3(0, 0, 0);
  return a;
}

static void set_state(SrAgent *a, SrAgentState st) {
  if (a->state != st) a->state_time = 0;
  a->state = st;
  if (st != SR_AG_IDLE && st != SR_AG_SHELTER && st != SR_AG_GAWK && st != SR_AG_COVER) {
    a->activity = SR_SA_NONE;
    a->group = 0;
    a->group_n = a->group_i = 0;
  }
}

/* Activities free of a prop: a standing citizen nearby may become a talking partner. */
static bool free_act(int act) {
  return act == SR_SA_IDLE || act == SR_SA_SMOKE || act == SR_SA_PHONE || act == SR_SA_CALL || act == SR_SA_TALK ||
         act == SR_SA_WAIT || act == SR_SA_WATCH || act == SR_SA_LEAN || act == SR_SA_WINDOW;
}

/* Stop for a while; what to do is chosen once standing still (sr_crowd_update). */
static void stop_here(SrAgent *a) {
  set_state(a, SR_AG_IDLE);
  a->activity = SR_SA_NONE;
  a->timer = 6.0f + (float)(sr_mix64(a->key ^ (uint64_t)(a->walk * 10.0f)) % 35u);
}

static void start_activity(SrCrowd *cr, const SrCity *c, SrAgent *a, int self) {
  SrVec3 face;
  SrStreetAct act = sr_activity_pick(c, a->block, a->pos, a->key, a->role, &face);
  a->activity = (uint8_t)act;
  if (sr_v3_len(face) > 0.1f) a->face = face;
  if (sr_activity_prop_near(c, a->block, a->pos, NULL, NULL) >= 0 || (a->key % 10u) >= 6u) return;
  /* away from props, a citizen standing free within 2.5 m may become a partner: groups of two or three */
  for (int i = 0; i < cr->count; i++) {
    SrAgent *o = &cr->agents[i];
    if (i == self || o->state != SR_AG_IDLE || !free_act(o->activity)) continue;
    float dx = o->pos.x - a->pos.x, dz = o->pos.z - a->pos.z;
    if (dx * dx + dz * dz > 2.5f * 2.5f) continue;
    uint64_t lead = o->group ? o->group : o->key;
    int n = o->group ? o->group_n : 1;
    if (n >= 3) return;
    if (!o->group) {
      o->group = lead;
      o->group_n = 1;
      o->group_i = 0;
      o->activity = SR_SA_TALK;
    }
    a->group = lead;
    a->group_i = (uint8_t)n;
    a->activity = SR_SA_TALK;
    a->timer = SR_MAX(o->timer, 4.0f);
    SrVec3 mid = sr_v3(0, 0, 0);
    for (int k = 0; k < cr->count; k++)
      if (cr->agents[k].group == lead) {
        cr->agents[k].group_n = (uint8_t)(n + 1);
        mid = sr_v3_add(mid, cr->agents[k].pos);
      }
    mid = sr_v3_scale(mid, 1.0f / (float)(n + 1));
    for (int k = 0; k < cr->count; k++) { /* everyone in the group faces its middle */
      SrAgent *m = &cr->agents[k];
      if (m->group != lead) continue;
      SrVec3 to = sr_v3_sub(mid, m->pos);
      to.y = 0;
      if (sr_v3_len(to) > 0.05f) m->face = m->dir = sr_v3_norm(to);
    }
    return;
  }
}

void sr_crowd_update(SrCrowd *cr, const SrCity *c, float dt, float rain) {
  for (int i = 0; i < cr->count; i++) {
    SrAgent *a = &cr->agents[i];
    a->state_time += dt;
    if (a->timer > 0) {
      a->timer -= dt;
      if (a->timer <= 0 && a->state != SR_AG_SHELTER) {
        set_state(a, SR_AG_WALK);
        a->pending = 0;
        pick_target(a, c);
      }
    }
    if (rain > 0.5f && !a->umbrella && !a->covered && (a->state == SR_AG_WALK || a->state == SR_AG_IDLE)) {
      set_state(a, SR_AG_SHELTER);
      a->timer = 5.0f + (float)(a->key % 20); /* time to reach an awning */
      static const uint8_t k_shelter[] = {SR_SA_IDLE, SR_SA_PHONE, SR_SA_SMOKE, SR_SA_CALL, SR_SA_WAIT};
      a->activity = k_shelter[a->key % SR_ARRAY_LEN(k_shelter)];
    }
    if (a->state == SR_AG_SHELTER && a->timer <= 0 && !a->covered) {
      a->covered = true;
      a->state_time = 0; /* shaking off the water starts now */
    }
    if (rain < 0.2f && a->state == SR_AG_SHELTER) {
      set_state(a, SR_AG_WALK);
      a->covered = false;
      a->timer = 0;
    }
    float speed = 0.0f;
    SrVec3 goal = a->target;
    switch (a->state) {
      case SR_AG_WALK:
        /* without an umbrella, rain hurries the step */
        speed = a->speed * (rain >= 0.3f && !a->umbrella ? 1.25f : 1.0f);
        break;
      case SR_AG_FLEE: speed = SR_FLEE_SPEED; break;
      default: speed = 0.0f; break;
    }
    if (a->state == SR_AG_GAWK && a->activity == SR_SA_NONE) {
      static const uint8_t k_gawk[] = {SR_SA_FILM, SR_SA_POINT, SR_SA_CALL, SR_SA_IDLE};
      a->activity = k_gawk[a->key % SR_ARRAY_LEN(k_gawk)];
    }
    if (a->state == SR_AG_COVER) a->activity = SR_SA_COWER;
    /* speed ramps up and down instead of jumping */
    if (a->vel < speed)
      a->vel = SR_MIN(speed, a->vel + SR_AGENT_ACCEL * dt);
    else
      a->vel = SR_MAX(speed, a->vel - SR_AGENT_BRAKE * dt);
    if (speed > 0) {
      SrVec3 d = sr_v3_sub(goal, a->pos);
      d.y = 0;
      float len = sr_v3_len(d);
      if (len < 0.5f) {
        if (a->state == SR_AG_WALK) {
          /* at a corner some stop for a while: an activity of the place, or a chat */
          uint64_t h = sr_mix64(a->key ^ (uint64_t)(a->walk * 4.0f));
          if (a->pending || h % 4u == 0) {
            a->pending = 0;
            stop_here(a);
          } else {
            pick_target(a, c);
          }
        }
        continue;
      }
      a->dir = sr_v3_scale(d, 1.0f / len);
    }
    if (a->vel > 0) { /* moving on, or coasting to a stop */
      float step = a->vel * dt;
      SrVec3 d = sr_v3_sub(goal, a->pos);
      d.y = 0;
      float len = sr_v3_len(d);
      if (speed > 0 && step > len) step = len;
      if (speed <= 0) step = SR_MIN(step, 0.4f);
      a->pos = sr_v3_add(a->pos, sr_v3_scale(a->dir, step));
      a->walk += step;
    } else if (a->state == SR_AG_IDLE) {
      if (a->activity == SR_SA_NONE) start_activity(cr, c, a, i); /* at rest: decide what to do here */
      if (sr_v3_len(a->face) > 0.1f) a->dir = a->face;            /* face the prop, the partner or the street */
    }
  }
}

int sr_crowd_blackout(SrCrowd *cr, const SrCity *c, int district, SrEventLog *log, int64_t t) {
  int reacted = 0;
  for (int i = 0; i < cr->count; i++) {
    SrAgent *a = &cr->agents[i];
    if (sr_city_district_at(c, (int32_t)(a->pos.x * 100.0f), (int32_t)(a->pos.z * 100.0f)) != district) continue;
    /* the brave gawk at the dark towers, the rest hurry for shelter */
    set_state(a, a->bravery > 160 ? SR_AG_GAWK : SR_AG_SHELTER);
    a->timer = 8.0f + (float)(a->key % 10);
    reacted++;
  }
  SrWorldEvent e;
  memset(&e, 0, sizeof e);
  e.type = SR_EV_BLACKOUT;
  e.time = t;
  e.district = (int8_t)district;
  e.building = -1;
  e.victim_faction = e.blamed_faction = -1;
  e.significant = true;
  snprintf(e.text, sizeof e.text, "Blackout in %s", c->districts[district].name);
  sr_events_add(log, &e);
  return reacted;
}

int sr_crowd_gunfire(SrCrowd *cr, const SrCity *c, SrVec3 pos, float radius, SrEventLog *log, int64_t t) {
  int reacted = 0;
  bool reported = false;
  int block = sr_city_block_at(c, (int32_t)(pos.x * 100.0f), (int32_t)(pos.z * 100.0f));
  SrDistrictType dt = block >= 0 ? c->districts[c->blocks[block].district].type : SR_D_RESIDENTIAL;
  for (int i = 0; i < cr->count; i++) {
    SrAgent *a = &cr->agents[i];
    if (dist_xz(a->pos, pos) > radius) continue;
    SrResponse r = sr_react_gunfire(a->bravery, dt, a->key);
    reacted++;
    switch (r) {
      case SR_RESP_FLEE: {
        set_state(a, SR_AG_FLEE);
        SrVec3 away = sr_v3_norm(sr_v3_sub(a->pos, pos));
        if (sr_v3_len(away) < 0.1f) away = sr_v3(1, 0, 0);
        a->target = sr_v3_add(a->pos, sr_v3_scale(away, 40.0f));
        a->timer = 10.0f;
        break;
      }
      case SR_RESP_COVER:
        set_state(a, SR_AG_COVER);
        a->timer = 8.0f;
        break;
      default:
        set_state(a, SR_AG_GAWK);
        a->timer = 6.0f;
        a->dir = sr_v3_norm(sr_v3_sub(pos, a->pos));
        break;
    }
    if (!reported && sr_witness_reports(dt, a->bravery, a->key)) {
      reported = true;
      cr->reports++;
      if (log) {
        SrWorldEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.type = SR_EV_CRIME_REPORT;
        ev.time = t;
        ev.district = block >= 0 ? c->blocks[block].district : -1;
        ev.building = -1;
        ev.victim_faction = -1;
        ev.blamed_faction = -2;
        ev.significant = false;
        memcpy(ev.text, "shots fired, suspect described", 31);
        sr_events_add(log, &ev);
      }
    }
  }
  return reacted;
}

float sr_crowd_exposed_share(const SrCrowd *cr) {
  if (cr->count == 0) return 0.0f;
  int exposed = 0;
  for (int i = 0; i < cr->count; i++) exposed += !cr->agents[i].umbrella && !cr->agents[i].covered;
  return (float)exposed / (float)cr->count;
}

int sr_crowd_visible(const SrCrowd *cr, const SrCamera *cam, float max_dist) {
  int n = 0;
  for (int i = 0; i < cr->count; i++)
    if (dist_xz(cr->agents[i].pos, cam->pos) <= max_dist && sr_camera_sees(cam, cr->agents[i].pos)) n++;
  return n;
}

/* ---- traffic ------------------------------------------------------------------------------ */

bool sr_vehicle_should_stop_within(const SrVehicle *v, const SrVec3 *peds, int n, float range) {
  SrVec3 perp = sr_v3(-v->dir.z, 0, v->dir.x);
  for (int i = 0; i < n; i++) {
    SrVec3 rel = sr_v3_sub(peds[i], v->pos);
    rel.y = 0;
    float along = sr_v3_dot(rel, v->dir), lateral = sr_v3_dot(rel, perp);
    if (along > 0.5f && along < range && lateral > -3.0f && lateral < 3.0f) return true;
  }
  return false;
}

bool sr_vehicle_should_stop(const SrVehicle *v, const SrVec3 *peds, int n) {
  return sr_vehicle_should_stop_within(v, peds, n, 9.0f);
}

static float lane_offset(float sign) { return sign > 0 ? 4.0f : 12.0f; }

int sr_vehicle_turn_at(const SrVehicle *v, int k) {
  uint64_t h = sr_mix64(v->key * 31u + (uint64_t)(int64_t)k * 977u + (fabsf(v->dir.x) > 0.5f ? 7u : 0u));
  int r = (int)(h % 10u);
  return r < 3 ? 1 : r < 5 ? -1 : 0;
}

/* d1 for a turn: right (+1) or left (-1) of the heading (x right, z down on the map) */
static SrVec3 turn_dir(SrVec3 d0, int turn) { return turn > 0 ? sr_v3(-d0.z, 0, d0.x) : sr_v3(d0.z, 0, -d0.x); }

void sr_vehicle_drive(SrVehicle *v, const SrCity *c, const SrVec3 *peds, int np, float dt) {
  float B = (float)SR_BLOCK_CM / 100.0f;
  float brake_dist = v->vel * v->vel / (2.0f * SR_CAR_BRAKE);
  v->stopped = sr_vehicle_should_stop_within(v, peds, np, 9.0f + brake_dist);
  float want = v->stopped ? 0.0f : v->speed * (v->turning ? 0.6f : 1.0f);
  float prev = v->vel;
  if (v->vel < want)
    v->vel = SR_MIN(want, v->vel + SR_CAR_ACCEL * dt);
  else
    v->vel = SR_MAX(want, v->vel - SR_CAR_BRAKE * dt);
  v->accel = dt > 0 ? (v->vel - prev) / dt : 0.0f;
  float step = v->vel * dt;
  v->odo += step;
  if (v->turning) { /* along the quarter circle into the cross lane */
    v->turn_a += step / v->turn_r;
    if (v->turn_a >= SR_PI * 0.5f) {
      v->pos = sr_v3_add(v->turn_c, sr_v3_scale(v->turn_d0, v->turn_r));
      v->dir = v->turn_d1;
      v->turning = false;
      v->indicator = 0;
    } else {
      float a = v->turn_a;
      v->pos = sr_v3_add(v->turn_c, sr_v3_add(sr_v3_scale(v->turn_d1, -v->turn_r * cosf(a)),
                                              sr_v3_scale(v->turn_d0, v->turn_r * sinf(a))));
      v->dir = sr_v3_norm(sr_v3_add(sr_v3_scale(v->turn_d1, sinf(a)), sr_v3_scale(v->turn_d0, cosf(a))));
    }
    return;
  }
  /* the next junction ahead and whether this car turns there */
  bool xaxis = fabsf(v->dir.x) > 0.5f;
  float s0 = xaxis ? v->dir.x : v->dir.z, u = xaxis ? v->pos.x : v->pos.z;
  int k = (int)floorf(u / B) + (s0 > 0 ? 0 : 1);
  int turn = 0;
  float start = 0;
  for (int pass = 0; pass < 3; pass++, k += s0 > 0 ? 1 : -1) {
    int t = sr_vehicle_turn_at(v, k);
    if (!t) continue;
    SrVec3 d1 = turn_dir(v->dir, t);
    float s1 = xaxis ? d1.z : d1.x, r = t > 0 ? 6.0f : 8.0f;
    float line = (float)k * B + lane_offset(s1);
    float st = line - s0 * r;
    if ((st - u) * s0 < -0.01f) continue; /* already past it */
    turn = t;
    start = st;
    break;
  }
  float dist = turn ? (start - u) * s0 : 1e9f;
  v->indicator = (int8_t)(turn && dist < SR_MAX(v->vel * 2.0f, 8.0f) ? turn : 0);
  if (turn && dist <= step) {
    SrVec3 d1 = turn_dir(v->dir, turn);
    float r = turn > 0 ? 6.0f : 8.0f;
    SrVec3 p0 = v->pos;
    if (xaxis)
      p0.x = start;
    else
      p0.z = start;
    SrVec3 c0 = sr_v3_add(p0, sr_v3_scale(d1, r));
    SrVec3 end = sr_v3_add(c0, sr_v3_scale(v->dir, r));
    int blk = sr_city_block_at(c, (int32_t)(end.x * 100.0f), (int32_t)(end.z * 100.0f));
    SrVec3 far = sr_v3_add(end, sr_v3_scale(d1, 20.0f));
    int blk2 = sr_city_block_at(c, (int32_t)(far.x * 100.0f), (int32_t)(far.z * 100.0f));
    if (blk >= 0 && blk2 >= 0 && !c->blocks[blk].water && !c->blocks[blk2].water) {
      v->turning = true;
      v->turn_r = r;
      v->turn_a = (step - dist) / r;
      v->turn_c = c0;
      v->turn_d0 = v->dir;
      v->turn_d1 = d1;
      v->pos = p0;
      return;
    }
  }
  v->pos = sr_v3_add(v->pos, sr_v3_scale(v->dir, step));
}

static bool on_road(const SrCity *c, SrVec3 p) {
  int blk = sr_city_block_at(c, (int32_t)(p.x * 100.0f), (int32_t)(p.z * 100.0f));
  return blk >= 0 && !c->blocks[blk].water;
}

void sr_crowd_traffic(SrCrowd *cr, const SrCity *c, const SrCamera *cam, SrVec3 player, float dt) {
  /* despawn far or off-road vehicles (never inside the view nearby) */
  for (int i = cr->vehicle_count - 1; i >= 0; i--) {
    SrVehicle *v = &cr->vehicles[i];
    bool gone = dist_xz(v->pos, cam->pos) > SR_CROWD_RADIUS + 30.0f || !on_road(c, v->pos);
    if (gone && may_pop(cam, v->pos)) cr->vehicles[i] = cr->vehicles[--cr->vehicle_count];
  }
  /* spawn on lanes: along the north street strip (x lanes) or the west strip (z lanes) */
  for (int tries = 0; tries < 8 && cr->vehicle_count < SR_MAX_VEHICLES; tries++) {
    SrRng r = sr_rng_stream(c->seed, "traffic", cr->vehicle_serial++, 0);
    float block = (float)SR_BLOCK_CM / 100.0f;
    int bx = (int)(cam->pos.x / block) + sr_rng_range(&r, -1, 1);
    int bz = (int)(cam->pos.z / block) + sr_rng_range(&r, -1, 1);
    bool xlane = sr_rng_chance(&r, 500);
    float sign = sr_rng_chance(&r, 500) ? 1.0f : -1.0f;
    float offset = sign > 0 ? 4.0f : 12.0f;
    float t = sr_rng_float(&r) * block;
    SrVehicle v;
    memset(&v, 0, sizeof v);
    v.key = sr_rng_u32(&r);
    v.dir = xlane ? sr_v3(sign, 0, 0) : sr_v3(0, 0, sign);
    v.pos = xlane ? sr_v3((float)bx * block + t, 0, (float)bz * block + offset)
                  : sr_v3((float)bx * block + offset, 0, (float)bz * block + t);
    v.speed = 8.0f + sr_rng_float(&r) * 6.0f;
    v.vel = v.speed; /* spawned out of view, already driving */
    v.color = (uint8_t)sr_rng_range(&r, 0, 23);
    if (!on_road(c, v.pos) || !may_pop(cam, v.pos)) continue;
    cr->vehicles[cr->vehicle_count++] = v;
  }
  /* move: brake for pedestrians ahead, accelerate away, turn at junctions */
  SrVec3 peds[SR_MAX_AGENTS + 1];
  int np = 0;
  peds[np++] = player;
  for (int i = 0; i < cr->count; i++) peds[np++] = cr->agents[i].pos;
  for (int i = 0; i < cr->vehicle_count; i++) sr_vehicle_drive(&cr->vehicles[i], c, peds, np, dt);
}
