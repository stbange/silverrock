#include "game/sr_game.h"

#include "core/sr_log.h"
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "world/sr_nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WALK_SPEED     4.0f
#define SPRINT_SPEED   7.0f
#define CROUCH_SPEED   2.0f
#define PLAYER_RADIUS  0.35f
#define EYE_HEIGHT     1.7f
#define CROUCH_HEIGHT  1.1f
#define INTERACT_RANGE 6.0f

int sr_loop_advance(SrLoop *l, double frame_dt) {
  if (frame_dt < 0) frame_dt = 0;
  l->acc += frame_dt;
  int steps = (int)(l->acc / SR_STEP_DT);
  if (steps > SR_MAX_STEPS_PER_FRAME) {
    steps = SR_MAX_STEPS_PER_FRAME;
    l->acc = 0; /* drop the backlog: no spiral of death */
  } else {
    l->acc -= steps * SR_STEP_DT;
  }
  l->alpha = l->acc / SR_STEP_DT;
  return steps;
}

SrGame *sr_game_create(void) {
  SrGame *g = (SrGame *)calloc(1, sizeof(SrGame));
  if (!g) return NULL; /* GCOVR_EXCL_LINE reason: calloc failure not injectable */
  g->city = (SrCity *)calloc(1, sizeof(SrCity));
  g->pop = (SrPopulation *)calloc(1, sizeof(SrPopulation));
  sr_settings_default(&g->settings);
  sr_clock_init(&g->clock);
  sr_crowd_init(&g->crowd);
  sr_jobs_init(&g->jobs);
  sr_heat_init(&g->heat);
  sr_events_init(&g->events);
  g->crowd_enabled = true;
  g->max_agents = 400;
  g->screen = SR_SCREEN_TITLE;
  g->active_job = -1;
  g->approach_building = -1;
  g->roof_building = -1;
  g->cam.cos_half_fov = 0.5f; /* 120 degree horizontal view */
  return g;
}

void sr_game_destroy(SrGame *g) {
  if (!g) return;
  free(g->city);
  free(g->pop);
  free(g);
}

SrResult sr_game_build_world(SrGame *g, uint64_t seed, const SrCityOptions *opt) {
  SrCityOptions o = {SR_CITY_MEDIUM, 0};
  if (opt) o = *opt;
  if (!g->started || g->seed != seed || g->opt.size != o.size || g->opt.corp_count != o.corp_count ||
      g->city->seed != seed) {
    SrResult r = sr_city_generate(g->city, seed, &o);
    if (r != SR_OK) return r; /* GCOVR_EXCL_LINE reason: generation always succeeds */
    sr_population_generate(g->pop, g->city);
  }
  g->seed = seed;
  g->opt = o;
  return SR_OK;
}

SrResult sr_game_new(SrGame *g, uint64_t seed, const SrCityOptions *opt, SrBackground bg, const char *name) {
  g->started = false;
  SrResult r = sr_game_build_world(g, seed, opt);
  if (r != SR_OK) return r;                /* GCOVR_EXCL_LINE reason: see build_world */
  sr_population_generate(g->pop, g->city); /* fresh notables for a new game */
  sr_clock_init(&g->clock);
  g->clock.rate = g->settings.time_rate > 0 ? g->settings.time_rate : SR_DEFAULT_TIME_RATE;
  sr_crowd_init(&g->crowd);
  sr_jobs_init(&g->jobs);
  sr_heat_init(&g->heat);
  sr_events_init(&g->events);
  memset(&g->deltas, 0, sizeof g->deltas);
  g->difficulty = (SrDifficulty)g->settings.difficulty;
  sr_player_create(&g->player, g->city, name, bg, sr_hash_combine(seed, 0x91A7E5u));
  g->last_minute = sr_clock_seconds(&g->clock) / 60;
  g->steps = 0;
  g->combat_count = 0;
  g->hostiles_near = 0;
  g->pall_removed = false;
  g->started = true;
  g->screen = SR_SCREEN_PLAY;
  g->active_job = -1;
  g->on_ground = true;
  memset(&g->site, 0, sizeof g->site);
  g->approach_building = -1;
  g->buff_camo = g->buff_reflex = g->buff_mask = g->buff_focus = g->buff_pain = 0;
  g->thermal = false;
  g->ability_cd = g->glitch_time = 0;
  g->glitches = 0;
  g->cyber_offline = 0;
  g->cyber_day = 0;
  sr_game_teleport_building(g, g->player.home);
  /* wake up at home, next to the bed */
  if (sr_game_enter(g, g->player.home, SR_ENTRY_FRONT) == SR_OK) {
    int bed = sr_interior_find_prop(&g->site.in, SR_PROP_BED);
    SrVec3 at = sr_site_prop_pos(&g->site, bed);
    SrVec3 c = sr_site_room_center(&g->site, g->site.in.props[bed].room);
    SrVec3 dir = sr_v3_norm(sr_v3(c.x - at.x, 0, c.z - at.z));
    g->player.pos = sr_v3(at.x + dir.x * 1.2f, SR_SITE_FLOOR_Y + EYE_HEIGHT, at.z + dir.z * 1.2f);
    SrVec3 door = sr_site_prop_pos(&g->site, sr_interior_find_prop(&g->site.in, SR_PROP_EXIT));
    g->player.yaw = atan2f(-(door.x - g->player.pos.x), -(door.z - g->player.pos.z)); /* towards the front door */
    sr_game_toast(g, "You wake up at home. Rent is due on Sunday.");
  }
  sr_log(SR_LOG_INFO, "game", "new game in %s (%d districts, %d megacorps)", g->city->name, g->city->district_count,
         g->city->factions.corp_count);
  return SR_OK;
}

int64_t sr_game_time(const SrGame *g) { return sr_clock_seconds(&g->clock); }

void sr_game_toast(SrGame *g, const char *msg) {
  sr_strlcpy(g->toast, msg, sizeof g->toast);
  g->toast_time = 4.0f;
}

void sr_game_focus(SrGame *g, bool focused) {
  if (!g->started) return;
  if (!focused) {
    if (g->screen == SR_SCREEN_PLAY || g->screen == SR_SCREEN_TERMINAL) {
      g->screen = SR_SCREEN_PAUSE;
      g->autosave_requests++;
    }
    g->resume_needs_confirm = true;
    g->audio_muted = true;
    g->clock.paused = true;
  }
}

void sr_game_confirm_resume(SrGame *g) {
  g->resume_needs_confirm = false;
  g->audio_muted = false;
  g->clock.paused = false;
  if (g->screen == SR_SCREEN_PAUSE) g->screen = SR_SCREEN_PLAY;
}

/* Called for every in-game minute that passes, live or skipped. */
static void minute_tick(SrGame *g, int64_t t, bool sleeping) {
  sr_player_minute(&g->player, sleeping);
  sr_player_rent_tick(&g->player, t, &g->events);
  bool at_home = g->player.home >= 0 &&
                 sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100)) ==
                     g->city->buildings[g->player.home].block;
  sr_heat_update(&g->heat, t, at_home || sleeping);
  if (t % 3600 == 0) sr_jobs_tick(&g->jobs, &g->player, t, &g->events);
}

static void process_minutes(SrGame *g, bool sleeping) {
  int64_t now_min = sr_clock_seconds(&g->clock) / 60;
  while (g->last_minute < now_min) {
    g->last_minute++;
    minute_tick(g, g->last_minute * 60, sleeping);
  }
}

void sr_game_skip(SrGame *g, int64_t seconds, bool sleeping) {
  if (seconds <= 0) return;
  sr_clock_skip(&g->clock, seconds);
  process_minutes(g, sleeping);
  sr_jobs_tick(&g->jobs, &g->player, sr_game_time(g), &g->events);
}

bool sr_game_blocked(const SrGame *g, float x, float z) {
  int32_t cx = (int32_t)(x * 100.0f), cz = (int32_t)(z * 100.0f);
  int b = sr_city_block_at(g->city, cx, cz);
  if (b < 0) return true; /* outside the city */
  if (g->city->blocks[b].water) return true;
  const SrBlock *blk = &g->city->blocks[b];
  int32_t r = (int32_t)(PLAYER_RADIUS * 100.0f);
  for (int i = 0; i < blk->building_count; i++) {
    const SrBuilding *bd = &g->city->buildings[blk->first_building + i];
    if (cx > bd->x0 - r && cx < bd->x1 + r && cz > bd->y0 - r && cz < bd->y1 + r) return true;
  }
  return false;
}

static SrVec3 front_of(const SrGame *g, int building) {
  const SrBuilding *b = &g->city->buildings[building];
  float x = (float)(b->x0 + b->x1) / 200.0f;
  float z = (float)b->y1 / 100.0f + 1.2f;
  for (int k = 0; k < 40 && sr_game_blocked(g, x, z); k++) z += 0.5f;
  return sr_v3(x, EYE_HEIGHT, z);
}

SrVec3 sr_game_outside_pos(const SrGame *g) {
  if (g->site.active && g->site.in.apartment) return g->player.pos; /* reopened on load */
  if (g->site.active) return front_of(g, g->site.building);
  if (g->roof_building >= 0) return front_of(g, g->roof_building);
  return g->player.pos;
}

void sr_game_teleport_building(SrGame *g, int building) {
  /* stand on the sidewalk in front of the building (south side, towards +z) */
  if (g->site.active) sr_site_close(&g->site);
  g->roof_building = -1;
  g->floor_y = 0;
  g->player.pos = front_of(g, building);
  g->player.yaw = 0; /* facing -z: towards the building */
}

static void move_player(SrGame *g, const SrActionState *a, float dt) {
  SrPlayer *p = &g->player;
  p->yaw = sr_wrap_angle(p->yaw - a->look_x);
  p->pitch = sr_clampf(p->pitch - a->look_y, -1.4f, 1.4f);
  bool crouch = a->down[SR_ACT_CROUCH];
  bool sprint = (a->down[SR_ACT_SPRINT] || a->sprint_from_stick) && !crouch;
  float speed = crouch ? CROUCH_SPEED : sprint ? SPRINT_SPEED : WALK_SPEED;
  if (sr_inv_weight(&p->inv) > sr_carry_limit(p->prog.attr[SR_ATTR_BODY])) speed = CROUCH_SPEED;
  if (g->buff_reflex > 0) speed *= 1.35f; /* boosted reflexes: the world seems to slow down */
  float s = sinf(p->yaw), c = cosf(p->yaw);
  float fx = -s, fz = -c; /* forward */
  float rx = c, rz = -s;  /* right */
  float mx = a->move_x, my = a->move_y;
  float len = sqrtf(mx * mx + my * my);
  if (len > 1.0f) {
    mx /= len;
    my /= len;
  }
  float vx = (fx * my + rx * mx) * speed * dt;
  float vz = (fz * my + rz * mx) * speed * dt;
  p->crouched = crouch;
  float x0 = p->pos.x, z0 = p->pos.z;
  if (!sr_game_blocked_here(g, p->pos.x + vx, p->pos.z)) p->pos.x += vx;
  if (!sr_game_blocked_here(g, p->pos.x, p->pos.z + vz)) p->pos.z += vz;
  g->speed = sqrtf((p->pos.x - x0) * (p->pos.x - x0) + (p->pos.z - z0) * (p->pos.z - z0)) / dt;
  /* jumping */
  float eye = g->floor_y + (crouch ? CROUCH_HEIGHT : EYE_HEIGHT);
  if (a->pressed[SR_ACT_JUMP] && g->on_ground) {
    g->vel_y = sr_game_has_cyber(g, 8) ? 7.0f : 4.5f; /* jump boosters */
    g->on_ground = false;
  }
  if (!g->on_ground) {
    g->vel_y -= 9.81f * dt;
    p->pos.y += g->vel_y * dt;
    if (p->pos.y <= eye) {
      p->pos.y = eye;
      g->on_ground = true;
      g->vel_y = 0;
    }
  } else {
    p->pos.y = eye;
  }
  if (len > 0.1f && sprint) sr_progress_use_skill(&p->prog, SR_SK_ATHLETICS, SR_USE_HOSTILE, 0);
}

void sr_game_step(SrGame *g, const SrActionState *a) {
  if (!g->started) return;
  if (g->screen == SR_SCREEN_SHOP) {
    if (a) {
      int dir = a->move_y > 0.5f ? -1 : a->move_y < -0.5f ? 1 : 0;
      if (dir != 0 && dir != g->menu_axis && g->menu.count > 0)
        g->menu.sel = (g->menu.sel + dir + g->menu.count) % g->menu.count;
      g->menu_axis = dir;
      if (a->pressed[SR_ACT_INTERACT]) sr_game_shop_buy(g, g->menu.sel);
      if (a->pressed[SR_ACT_PAUSE]) sr_game_shop_close(g);
    }
    return;
  }
  if (g->screen != SR_SCREEN_PLAY) {
    if (a && a->pressed[SR_ACT_PAUSE] && g->screen == SR_SCREEN_PAUSE)
      sr_game_confirm_resume(g);
    else if (a && a->pressed[SR_ACT_PAUSE] && g->screen == SR_SCREEN_TERMINAL)
      g->screen = SR_SCREEN_PLAY;
    else if (a && a->pressed[SR_ACT_INTERACT] && g->screen == SR_SCREEN_TERMINAL && g->site.active &&
             g->site.in.apartment)
      sr_game_housing_open(g); /* the home terminal lists apartments for rent */
    return;
  }
  float dt = (float)SR_STEP_DT;
  g->steps++;
  if (a) {
    if (a->pressed[SR_ACT_PAUSE]) {
      g->screen = SR_SCREEN_PAUSE;
      g->clock.paused = true;
      return;
    }
    if (g->glitch_time <= 0) { /* a glitch freezes the body for a moment */
      move_player(g, a, dt);
      if (a->pressed[SR_ACT_INTERACT]) sr_game_interact(g);
      if (a->pressed[SR_ACT_FIRE]) sr_game_fire(g);
      if (a->pressed[SR_ACT_GADGET]) sr_game_ability(g);
      for (int q = 0; q < 4; q++)
        if (a->pressed[SR_ACT_QUICK1 + q]) sr_game_use_quick(g, q);
    }
    if (a->pressed[SR_ACT_TERMINAL]) g->screen = SR_SCREEN_TERMINAL;
  }
  sr_clock_step(&g->clock);
  process_minutes(g, false);
  int64_t t = sr_game_time(g);
  g->cam.pos = g->player.pos;
  g->cam.forward = sr_dir_from_yaw_pitch(g->player.yaw, 0);
  sr_game_site_step(g, dt);
  if (g->crowd_enabled && !g->site.active) {
    if (g->steps % 30 == 1) sr_crowd_refresh(&g->crowd, g->city, g->pop, &g->cam, t, g->max_agents);
    SrWeatherState w = sr_weather_at(g->seed, t);
    sr_crowd_update(&g->crowd, g->city, dt, w.rain);
    sr_crowd_traffic(&g->crowd, g->city, &g->cam, g->player.pos, dt);
  }
  if (g->toast_time > 0) g->toast_time -= dt;
  if (g->player.health <= 0) sr_game_flatline(g);
  if (g->steps % 15 == 0) g->ctx = sr_game_context(g);
}

uint64_t sr_game_checksum(const SrGame *g) {
  uint64_t h = sr_hash_combine(g->seed, (uint64_t)g->clock.scaled);
  const SrPlayer *p = &g->player;
  float f[8] = {p->pos.x, p->pos.y, p->pos.z, p->yaw, p->pitch, p->health, p->satiety, p->fatigue};
  h = sr_hash_bytes(h, f, sizeof f);
  int64_t i[6] = {p->cred, p->rent_debt, p->rent_due, p->missed, p->evicted, p->prog.street_cred};
  h = sr_hash_bytes(h, i, sizeof i);
  h = sr_hash_bytes(h, p->prog.skill_rank, sizeof p->prog.skill_rank);
  h = sr_hash_bytes(h, p->prog.standing, sizeof p->prog.standing);
  h = sr_hash_combine(h, (uint64_t)p->inv.count * 131u + (uint64_t)p->stash.count);
  for (int k = 0; k < p->inv.count; k++) h = sr_hash_bytes(h, &p->inv.items[k], sizeof(SrItem));
  for (int k = 0; k < p->stash.count; k++) h = sr_hash_bytes(h, &p->stash.items[k], sizeof(SrItem));
  for (int k = 0; k < g->jobs.count; k++) {
    const SrJob *j = &g->jobs.jobs[k];
    int64_t v[6] = {j->id, j->state, j->pay, j->paid, j->executed, j->deadline};
    h = sr_hash_bytes(h, v, sizeof v);
  }
  h = sr_hash_bytes(h, g->heat.level, sizeof g->heat.level);
  h = sr_hash_combine(h, (uint64_t)g->pop->count);
  for (int k = 0; k < g->deltas.count; k++) h = sr_hash_combine(h, g->deltas.id[k] * 7u + g->deltas.value[k]);
  return h;
}

/* ---- interactions ----------------------------------------------------------------------- */

static float dist_to_building(const SrGame *g, int b) {
  const SrBuilding *bd = &g->city->buildings[b];
  float x = g->player.pos.x * 100.0f, z = g->player.pos.z * 100.0f;
  float dx = x < bd->x0 ? bd->x0 - x : x > bd->x1 ? x - bd->x1 : 0;
  float dz = z < bd->y0 ? bd->y0 - z : z > bd->y1 ? z - bd->y1 : 0;
  return sqrtf(dx * dx + dz * dz) / 100.0f;
}

SrContext sr_game_context(SrGame *g) {
  if (g->site.active || g->roof_building >= 0) return sr_game_site_context(g);
  SrContext ctx = {SR_CTX_NONE, -1, -1, -1, 1e9f, -1};
  int b0 = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  if (b0 < 0) return ctx;
  int64_t t = sr_game_time(g);
  int bx = b0 % g->city->grid, by = b0 / g->city->grid;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      int b = sr_city_block_index(g->city, bx + dx, by + dy);
      if (b < 0) continue;
      /* metro entrance at the station block centre */
      for (int s = 0; s < g->city->station_count; s++) {
        if (g->city->stations[s].block != b) continue;
        int32_t ox, oy;
        sr_city_block_origin(b, g->city->grid, &ox, &oy);
        float sx = (float)ox / 100.0f + 4.0f, sz = (float)oy / 100.0f + 4.0f;
        float d =
            sqrtf((sx - g->player.pos.x) * (sx - g->player.pos.x) + (sz - g->player.pos.z) * (sz - g->player.pos.z));
        if (d < INTERACT_RANGE && d < ctx.distance) { ctx = (SrContext){SR_CTX_METRO, -1, s, -1, d, -1}; }
      }
      const SrBlock *blk = &g->city->blocks[b];
      for (int i = 0; i < blk->building_count; i++) {
        int bi = blk->first_building + i;
        float d = dist_to_building(g, bi);
        if (d > INTERACT_RANGE || d >= ctx.distance) continue;
        SrContextKind k = SR_CTX_NONE;
        int job = -1;
        for (int j = 0; j < g->jobs.count; j++) {
          SrJob *jb = &g->jobs.jobs[j];
          if (jb->state != SR_JS_ACCEPTED) continue;
          if (!jb->executed && jb->target_building == bi) {
            k = SR_CTX_JOB_TARGET;
            job = jb->id;
          } else if (jb->executed && jb->client_building == bi) {
            k = SR_CTX_JOB_REPORT;
            job = jb->id;
          }
        }
        if (k == SR_CTX_NONE) {
          switch (g->city->buildings[bi].poi) {
            case SR_POI_FOOD:
            case SR_POI_BAR:
            case SR_POI_GROCERY: k = SR_CTX_FOOD; break;
            case SR_POI_FIXER: k = SR_CTX_FIXER; break;
            case SR_POI_HOME: k = bi == g->player.home ? SR_CTX_HOME : SR_CTX_NONE; break;
            case SR_POI_CAPSULE: k = SR_CTX_CAPSULE; break;
            case SR_POI_CLINIC: k = SR_CTX_CLINIC; break;
            case SR_POI_GUNSHOP:
            case SR_POI_ELECTRONICS:
            case SR_POI_CLOTHING:
            case SR_POI_RIPPERDOC:
            case SR_POI_FENCE: k = SR_CTX_SHOP; break;
            default: break;
          }
          if (bi == g->player.home) k = SR_CTX_HOME;
          if (k != SR_CTX_NONE && k != SR_CTX_HOME && !sr_poi_open(g->city, bi, t)) k = SR_CTX_CLOSED;
          if (k == SR_CTX_NONE && (g->city->buildings[bi].flags & SR_BF_MISSION)) k = SR_CTX_ENTER;
        }
        if (k != SR_CTX_NONE) ctx = (SrContext){k, bi, -1, job, d, -1};
      }
    }
  }
  return ctx;
}

SrResult sr_game_eat_at(SrGame *g, int building) {
  SrItem stock[16];
  int n = sr_vendor_stock(g->city, building, sr_game_time(g) / SR_DAY_S, stock, 16);
  for (int i = 0; i < n; i++) {
    if (stock[i].kind != SR_IK_FOOD) continue;
    SrResult r = sr_player_buy(&g->player, g->city, building, &stock[i], sr_game_time(g));
    if (r == SR_OK) {
      char msg[160];
      int staff = sr_population_staff_of(g->pop, building);
      const char *greet = "";
      if (staff >= 0) { /* regulars are remembered */
        greet = sr_npc_greeting(&g->pop->npcs[staff]);
        sr_npc_remember(&g->pop->npcs[staff], 5);
      }
      snprintf(msg, sizeof msg, "%s You eat %s.", greet, sr_food_def(stock[i].type)->name);
      sr_game_toast(g, msg);
    } else {
      sr_game_toast(g, r == SR_ERR_DENIED ? "Can't buy that right now." : "Inventory full.");
    }
    return r;
  }
  return SR_ERR_NOT_FOUND;
}

SrResult sr_game_sleep(SrGame *g, int hours, SrSleepPlace place) {
  if (sr_player_sleep_allowed(&g->player, g->hostiles_near) != SR_OK) {
    sr_game_toast(g, "You can't sleep with hostiles nearby.");
    return SR_ERR_DENIED;
  }
  if (place == SR_SLEEP_HOME && !sr_player_can_use_home(&g->player)) {
    sr_game_toast(g, "You're evicted. Pay the landlord first.");
    return SR_ERR_DENIED;
  }
  if (place == SR_SLEEP_CAPSULE) {
    int32_t price = sr_capsule_price(g->city, g->ctx.building >= 0 ? g->ctx.building : g->player.home);
    if (g->player.cred < price) return SR_ERR_DENIED;
    sr_player_tx(&g->player, sr_game_time(g), -price, "capsule night");
  }
  sr_game_skip(g, (int64_t)hours * 3600, true);
  if (hours >= 6) g->player.fatigue = 0;
  int b = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  SrDistrictType dt = b >= 0 ? g->city->districts[g->city->blocks[b].district].type : SR_D_RESIDENTIAL;
  sr_player_wake(&g->player, place, g->seed, sr_game_time(g), dt);
  g->autosave_requests++;
  sr_game_toast(g, "You wake up. Still night.");
  return SR_OK;
}

SrResult sr_game_metro(SrGame *g, int to) {
  int b = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  int from = b >= 0 ? sr_city_nearest_station(g->city, b) : -1;
  if (from < 0 || to < 0 || to >= g->city->station_count) return SR_ERR_INVALID;
  SrTrip trip = sr_metro_trip(g->city, from, to, sr_game_time(g));
  if (!trip.possible) return SR_ERR_INVALID; /* GCOVR_EXCL_LINE reason: metro is connected */
  if (g->player.cred < trip.fare) return SR_ERR_DENIED;
  sr_player_tx(&g->player, sr_game_time(g), -trip.fare, "metro fare");
  sr_game_skip(g, trip.seconds, false);
  int32_t ox, oy;
  sr_city_block_origin(g->city->stations[to].block, g->city->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 4.0f, EYE_HEIGHT, (float)oy / 100.0f + 4.0f);
  return SR_OK;
}

static int player_building_near(SrGame *g) {
  int b = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  if (b < 0) return g->player.home;
  const SrBlock *blk = &g->city->blocks[b];
  return blk->building_count ? blk->first_building : g->player.home;
}

SrResult sr_game_cab(SrGame *g, int to) {
  SrTrip trip = sr_cab_trip(g->city, player_building_near(g), to);
  if (g->player.cred < trip.fare) return SR_ERR_DENIED;
  sr_player_tx(&g->player, sr_game_time(g), -trip.fare, "cab");
  sr_game_skip(g, trip.seconds, false);
  sr_game_teleport_building(g, to);
  return SR_OK;
}

SrResult sr_game_walk_to(SrGame *g, int to) {
  int from = player_building_near(g);
  int32_t d = from == to ? 0 : sr_nav_walk_cm(g->city, from, to); /* sidewalks and crossings */
  if (d < 0) return SR_ERR_INVALID;
  sr_game_skip(g, (int64_t)(d / 140) + 30, false); /* 1.4 m/s */
  sr_game_teleport_building(g, to);
  return SR_OK;
}

int sr_game_fixer(SrGame *g, int fixer, int out_ids[4]) {
  return sr_jobs_fixer_offers(&g->jobs, g->city, fixer, sr_game_time(g), g->player.prog.street_cred, out_ids);
}

SrResult sr_game_accept(SrGame *g, int32_t id) {
  SrJob *j = sr_jobs_get(&g->jobs, id);
  if (!j) return SR_ERR_NOT_FOUND;
  SrResult r = sr_job_accept(j, &g->player, sr_game_time(g));
  if (r == SR_OK) g->active_job = id;
  return r;
}

SrMissionResult sr_game_execute(SrGame *g, int32_t id, SrApproach a) {
  SrMissionResult res;
  memset(&res, 0, sizeof res);
  SrJob *j = sr_jobs_get(&g->jobs, id);
  if (!j) return res;
  res = sr_job_execute(g->city, j, &g->player, a, sr_hash_combine(g->seed, g->steps + (uint64_t)sr_game_time(g)));
  sr_game_skip(g, res.seconds, false);
  if (res.alarm || res.kills) {
    SrCrime cr;
    memset(&cr, 0, sizeof cr);
    cr.jurisdiction = (int8_t)sr_heat_jurisdiction(g->city, j->target_building);
    cr.severity = (uint8_t)(res.kills ? 5 : 2);
    cr.witnessed = res.witnessed;
    cr.camera = res.camera;
    cr.evidence_left = res.kills > 0;
    cr.look = sr_appearance_key(&g->player.look);
    cr.time = sr_game_time(g);
    sr_heat_crime(&g->heat, &cr);
    if (res.witnessed || res.camera) sr_progress_standing(&g->player.prog, j->target_faction, -15, true);
  }
  if (j->type == SR_JT_DELIVER && res.success) sr_game_report(g, id); /* handing over completes */
  return res;
}

int32_t sr_game_report(SrGame *g, int32_t id) {
  SrJob *j = sr_jobs_get(&g->jobs, id);
  if (!j) return 0;
  int32_t paid = sr_job_complete(j, &g->player, sr_game_time(g), &g->events, g->city);
  if (j->state == SR_JS_COMPLETED) {
    g->jobs.completed++;
    g->autosave_requests++;
    if (g->active_job == id) g->active_job = -1;
  }
  return paid;
}

bool sr_game_use_quick(SrGame *g, int slot) {
  SrPlayer *p = &g->player;
  if (slot < 0 || slot > 3 || p->quick[slot] == 255) return false;
  return sr_game_use_gadget(g, p->quick[slot]);
}

void sr_game_fire(SrGame *g) {
  SrPlayer *p = &g->player;
  if (p->equipped < 0 || p->equipped >= p->inv.count) return;
  const SrItem *w = &p->inv.items[p->equipped];
  if (w->kind != SR_IK_WEAPON || sr_archetype(w->type)->category == SR_WC_MELEE) return;
  int ammo = sr_inv_find(&p->inv, SR_IK_AMMO, -1);
  if (ammo < 0) {
    sr_game_toast(g, "Click. Out of ammo.");
    return;
  }
  SrWeaponStats s = sr_weapon_stats(g->seed, w);
  sr_inv_remove_at(&p->inv, ammo, 1);
  if (p->equipped > ammo && sr_inv_find(&p->inv, SR_IK_AMMO, -1) < 0) p->equipped--;
  if (g->site.active) {
    if (sr_game_has_cyber(g, 4) || g->buff_focus > 0) s.spread *= 0.5f; /* smartlink, focus */
    sr_site_shoot(&g->site, &g->deltas, p->pos, p->yaw, p->pitch, &s);
    return;
  }
  float radius = s.noise * 1.5f;
  int reacted = sr_crowd_gunfire(&g->crowd, g->city, p->pos, radius, &g->events, sr_game_time(g));
  const SrWorldEvent *ev = sr_events_recent(&g->events, 0);
  bool reported = ev && ev->type == SR_EV_CRIME_REPORT && ev->time == sr_game_time(g);
  if (reacted > 0 && reported) {
    SrCrime cr;
    memset(&cr, 0, sizeof cr);
    cr.jurisdiction = (int8_t)g->city->factions.police;
    cr.severity = 2;
    cr.witnessed = true;
    cr.look = sr_appearance_key(&p->look);
    cr.time = sr_game_time(g);
    sr_heat_crime(&g->heat, &cr);
  }
}

void sr_game_flatline(SrGame *g) {
  SrPlayer *p = &g->player;
  if (sr_game_has_cyber(g, 13) && !(g->cyber_offline & 0x80000000u)) { /* Second Heart, once a day */
    g->cyber_offline |= 0x80000000u;
    p->health = p->max_health * 0.4f;
    sr_game_toast(g, "Your second heart kicks in.");
    return;
  }
  if (g->site.active) sr_game_leave(g);
  SrFlatline f = sr_flatline(p->medevac, p->cred, g->seed, sr_game_time(g), g->hardcore);
  if (f.delete_save) {
    g->screen = SR_SCREEN_DEAD;
    return;
  }
  if (f.wake_in_clinic) {
    int32_t d;
    int clinic = sr_city_find_poi_near(g->city, SR_POI_CLINIC, player_building_near(g), &d);
    sr_player_tx(p, sr_game_time(g), -f.bill, "MedEvac treatment");
    if (clinic >= 0) sr_game_teleport_building(g, clinic);
    sr_game_toast(g, "MedEvac picked you up. The bill is on its way.");
  } else {
    if (f.cash_lost) sr_player_tx(p, sr_game_time(g), -f.cash_lost, "robbed while down");
    sr_game_toast(g, "You wake up in an alley. Your wallet feels lighter.");
  }
  sr_game_skip(g, 6 * 3600, true);
  p->health = p->max_health * 0.5f;
}

SrResult sr_game_interact(SrGame *g) {
  SrContext c = sr_game_context(g);
  g->ctx = c;
  if (c.kind >= SR_CTX_EXIT && c.kind != SR_CTX_ENTER) return sr_game_site_interact(g, &c);
  switch (c.kind) {
    case SR_CTX_FOOD: return sr_game_eat_at(g, c.building);
    case SR_CTX_SHOP: sr_game_shop_open(g, c.building); return SR_OK;
    case SR_CTX_ENTER: return sr_game_enter(g, c.building, g->player.crouched ? SR_ENTRY_SERVICE : SR_ENTRY_FRONT);
    case SR_CTX_HOME:
      if (g->player.evicted) {
        if (sr_player_pay_debt(&g->player, sr_game_time(g)) == SR_OK) {
          sr_game_toast(g, "Debt paid. The door unlocks.");
          return SR_OK;
        }
        sr_game_toast(g, "Locked. You owe the landlord.");
        return SR_ERR_DENIED;
      }
      return sr_game_enter(g, c.building, SR_ENTRY_FRONT);
    case SR_CTX_CAPSULE: return sr_game_sleep(g, 8, SR_SLEEP_CAPSULE);
    case SR_CTX_FIXER: {
      int ids[4];
      int n = sr_game_fixer(g, c.building, ids);
      for (int i = 0; i < n; i++) {
        SrJob *j = sr_jobs_get(&g->jobs, ids[i]);
        if (j && j->state == SR_JS_OFFERED && sr_game_accept(g, ids[i]) == SR_OK) {
          char msg[128];
          snprintf(msg, sizeof msg, "Job accepted: %s", j->title);
          sr_game_toast(g, msg);
          return SR_OK;
        }
      }
      sr_game_toast(g, "No work for you today, choom.");
      return SR_ERR_NOT_FOUND;
    }
    case SR_CTX_JOB_TARGET: {
      SrJob *j = sr_jobs_get(&g->jobs, c.job);
      if (sr_job_is_site((SrJobType)j->type)) /* site jobs are played inside */
        return sr_game_enter(g, c.building, g->player.crouched ? SR_ENTRY_SERVICE : SR_ENTRY_FRONT);
      SrApproach best = SR_AP_HAMMER;
      for (int a = 0; a < SR_AP_COUNT; a++)
        if (j->approaches & (1u << a)) {
          best = (SrApproach)a;
          break;
        }
      SrMissionResult r = sr_game_execute(g, c.job, best);
      sr_game_toast(g, r.success ? "Objective complete. Report back." : "That went sideways.");
      return r.success ? SR_OK : SR_ERR_DENIED;
    }
    case SR_CTX_JOB_REPORT: {
      int32_t paid = sr_game_report(g, c.job);
      char msg[64];
      snprintf(msg, sizeof msg, "Paid %d cred.", paid);
      sr_game_toast(g, msg);
      return SR_OK;
    }
    case SR_CTX_METRO: {
      int to = (c.station + 1) % g->city->station_count;
      SrResult r = sr_game_metro(g, to);
      if (r == SR_OK) sr_game_toast(g, "You ride the metro.");
      return r;
    }
    case SR_CTX_CLOSED: sr_game_toast(g, "Closed. Come back later."); return SR_ERR_DENIED;
    default: return SR_ERR_NOT_FOUND;
  }
}

/* ---- character creation ----------------------------------------------------------------- */

void sr_game_begin_create(SrGame *g, uint64_t seed) {
  g->create_seed = seed;
  g->create_bg = SR_BG_STREET_KID;
  g->create_look = sr_hash_combine(seed, 0x91A7E5u);
  g->screen = SR_SCREEN_CREATE;
}

void sr_game_create_select(SrGame *g, int bg) {
  if (bg >= 0 && bg < SR_BG_COUNT) g->create_bg = bg;
}

void sr_game_create_randomize(SrGame *g) { g->create_look = sr_mix64(g->create_look + 0x9E37u); }

SrAppearance sr_game_create_preview(const SrGame *g) {
  return sr_appearance_generate(g->create_look, SR_ROLE_WORKER, 0);
}

SrResult sr_game_create_confirm(SrGame *g) {
  SrResult r = sr_game_new(g, g->create_seed, NULL, (SrBackground)g->create_bg, "Runner");
  if (r == SR_OK) g->player.look = sr_game_create_preview(g);
  return r;
}
