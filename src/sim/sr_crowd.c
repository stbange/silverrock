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

void sr_crowd_update(SrCrowd *cr, const SrCity *c, float dt, float rain) {
  for (int i = 0; i < cr->count; i++) {
    SrAgent *a = &cr->agents[i];
    if (a->timer > 0) {
      a->timer -= dt;
      if (a->timer <= 0 && a->state != SR_AG_SHELTER) {
        a->state = SR_AG_WALK;
        pick_target(a, c);
      }
    }
    if (rain > 0.5f && !a->umbrella && !a->covered && a->state == SR_AG_WALK) {
      a->state = SR_AG_SHELTER;
      a->timer = 5.0f + (float)(a->key % 20); /* time to reach an awning */
    }
    if (a->state == SR_AG_SHELTER && a->timer <= 0) a->covered = true;
    if (rain < 0.2f && a->state == SR_AG_SHELTER) {
      a->state = SR_AG_WALK;
      a->covered = false;
      a->timer = 0;
    }
    float speed = 0.0f;
    SrVec3 goal = a->target;
    switch (a->state) {
      case SR_AG_WALK: speed = a->speed; break;
      case SR_AG_FLEE: speed = 4.0f; break;
      default: speed = 0.0f; break;
    }
    if (speed > 0) {
      SrVec3 d = sr_v3_sub(goal, a->pos);
      d.y = 0;
      float len = sr_v3_len(d);
      if (len < 0.5f) {
        if (a->state == SR_AG_WALK) pick_target(a, c);
      } else {
        a->dir = sr_v3_scale(d, 1.0f / len);
        float step = speed * dt;
        if (step > len) step = len;
        a->pos = sr_v3_add(a->pos, sr_v3_scale(a->dir, step));
        a->walk += step;
      }
    }
  }
}

int sr_crowd_blackout(SrCrowd *cr, const SrCity *c, int district, SrEventLog *log, int64_t t) {
  int reacted = 0;
  for (int i = 0; i < cr->count; i++) {
    SrAgent *a = &cr->agents[i];
    if (sr_city_district_at(c, (int32_t)(a->pos.x * 100.0f), (int32_t)(a->pos.z * 100.0f)) != district) continue;
    /* the brave gawk at the dark towers, the rest hurry for shelter */
    a->state = a->bravery > 160 ? SR_AG_GAWK : SR_AG_SHELTER;
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
        a->state = SR_AG_FLEE;
        SrVec3 away = sr_v3_norm(sr_v3_sub(a->pos, pos));
        if (sr_v3_len(away) < 0.1f) away = sr_v3(1, 0, 0);
        a->target = sr_v3_add(a->pos, sr_v3_scale(away, 40.0f));
        a->timer = 10.0f;
        break;
      }
      case SR_RESP_COVER:
        a->state = SR_AG_COVER;
        a->timer = 8.0f;
        break;
      default:
        a->state = SR_AG_GAWK;
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

bool sr_vehicle_should_stop(const SrVehicle *v, const SrVec3 *peds, int n) {
  SrVec3 perp = sr_v3(-v->dir.z, 0, v->dir.x);
  for (int i = 0; i < n; i++) {
    SrVec3 rel = sr_v3_sub(peds[i], v->pos);
    rel.y = 0;
    float along = sr_v3_dot(rel, v->dir), lateral = sr_v3_dot(rel, perp);
    if (along > 0.5f && along < 9.0f && lateral > -3.0f && lateral < 3.0f) return true;
  }
  return false;
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
    v.color = (uint8_t)sr_rng_range(&r, 0, 23);
    if (!on_road(c, v.pos) || !may_pop(cam, v.pos)) continue;
    cr->vehicles[cr->vehicle_count++] = v;
  }
  /* move: stop for pedestrians on the crossing ahead */
  SrVec3 peds[SR_MAX_AGENTS + 1];
  int np = 0;
  peds[np++] = player;
  for (int i = 0; i < cr->count; i++) peds[np++] = cr->agents[i].pos;
  for (int i = 0; i < cr->vehicle_count; i++) {
    SrVehicle *v = &cr->vehicles[i];
    v->stopped = sr_vehicle_should_stop(v, peds, np);
    if (!v->stopped) v->pos = sr_v3_add(v->pos, sr_v3_scale(v->dir, v->speed * dt));
  }
}
