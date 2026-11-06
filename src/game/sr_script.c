#include "game/sr_script.h"

#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "game/sr_save.h"
#include "world/sr_interior.h"
#include "world/sr_nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- queries ----------------------------------------------------------------------------- */

static int skill_by_name(const char *n) {
  for (int s = 0; s < SR_SK_COUNT; s++) {
    char norm[32], want[32];
    sr_str_normalize(sr_skill_name((SrSkill)s), norm, sizeof norm);
    sr_str_normalize(n, want, sizeof want);
    if (strcmp(norm, want) == 0) return s;
  }
  return -1;
}

static int max_heat(const SrGame *g) {
  int m = 0;
  for (int j = 0; j < SR_MAX_FACTIONS; j++) m = SR_MAX(m, g->heat.level[j]);
  return m;
}

bool sr_game_query(SrGame *g, const char *path, char *out, size_t cap) {
  const SrPlayer *p = &g->player;
  int64_t t = sr_game_time(g);
#define Q(name, fmt, val)                                                                                              \
  if (strcmp(path, name) == 0) {                                                                                       \
    snprintf(out, cap, fmt, val);                                                                                      \
    return true;                                                                                                       \
  }
  Q("player.cash", "%lld", (long long)p->cred)
  if (strcmp(path, "player.home") == 0) {
    if (p->home < 0)
      snprintf(out, cap, "none");
    else
      snprintf(out, cap, "%d", p->home);
    return true;
  }
  Q("player.home.locked", "%s", p->evicted ? "true" : "false")
  Q("player.home.stash.count", "%d", p->stash.count)
  Q("player.rent.missed", "%d", p->missed)
  Q("player.rent.debt", "%lld", (long long)p->rent_debt)
  Q("player.rent.due", "%lld", (long long)p->rent_due)
  Q("player.rent", "%d", p->rent)
  Q("player.health", "%.2f", (double)p->health)
  Q("player.satiety", "%.2f", (double)p->satiety)
  Q("player.fatigue", "%.2f", (double)p->fatigue)
  Q("player.needs.starving_hours", "%.2f", (double)p->starving_hours)
  Q("player.cred", "%d", p->prog.street_cred)
  Q("player.tier", "%s", sr_cred_tier_name(sr_cred_tier(p->prog.street_cred)))
  Q("player.level", "%d", p->prog.level)
  Q("player.messages", "%d", p->msg_count)
  Q("player.unread", "%d", sr_player_unread(p))
  Q("player.inv.count", "%d", p->inv.count)
  Q("player.x", "%.2f", (double)p->pos.x)
  Q("player.z", "%.2f", (double)p->pos.z)
  Q("player.y", "%.2f", (double)p->pos.y)
  Q("player.yaw", "%.3f", (double)p->yaw)
  Q("player.background", "%s", sr_background_def(p->background)->name)
  Q("player.kills", "%d", p->kills)
  if (sr_str_starts_with(path, "player.skill.")) {
    int s = skill_by_name(path + 13);
    if (s < 0) return false;
    snprintf(out, cap, "%d", p->prog.skill_rank[s]);
    return true;
  }
  Q("heat.police", "%d", g->heat.level[g->city->factions.police])
  Q("heat.max", "%d", max_heat(g))
  Q("clock.time", "%lld", (long long)t)
  Q("clock.day", "%lld", (long long)(t / SR_DAY_S))
  Q("clock.weekday", "%s", sr_weekday_name(sr_time_weekday(t)))
  Q("clock.hour", "%d", sr_time_hour(t))
  Q("clock.minute", "%d", sr_time_minute(t))
  Q("clock.week", "%d", sr_time_week(t))
  Q("city.name", "%s", g->city->name)
  Q("city.districts", "%d", g->city->district_count)
  Q("city.corps", "%d", g->city->factions.corp_count)
  Q("city.buildings", "%d", g->city->building_count)
  Q("city.digest", "%016llx", (unsigned long long)sr_city_digest(g->city))
  Q("city.valid", "%d", sr_city_validate(g->city) == 0)
  Q("crowd.count", "%d", g->crowd.count)
  Q("crowd.visible", "%d", sr_crowd_visible(&g->crowd, &g->cam, 60.0f))
  Q("crowd.popins", "%d", g->crowd.spawned_in_view_near)
  Q("jobs.count", "%d", g->jobs.count)
  Q("jobs.completed", "%d", g->jobs.completed)
  Q("jobs.active", "%d", g->active_job)
  Q("events.count", "%d", g->events.count)
  Q("weather", "%s", sr_weather_name(sr_weather_at(g->seed, t).to))
  Q("sky.daylight", "%s", sr_sky_at(t, g->pall_removed).daylight ? "true" : "false")
  Q("checksum", "%016llx", (unsigned long long)sr_game_checksum(g))
  Q("paused", "%s", g->clock.paused ? "true" : "false")
  Q("audio.muted", "%s", g->audio_muted ? "true" : "false")
  Q("context", "%d", (int)sr_game_context(g).kind)
  Q("site.active", "%s", g->site.active ? "true" : "false")
  Q("site.building", "%d", g->site.active ? g->site.building : -1)
  Q("site.apartment", "%s", g->site.active && g->site.in.apartment ? "true" : "false")
  Q("site.guards", "%d", g->site.guard_count)
  Q("site.guards_up", "%d", sr_site_guards_up(&g->site))
  Q("site.alarm", "%s", g->site.alarm ? "true" : "false")
  Q("site.objective", "%s", g->site.objective_done ? "true" : "false")
  Q("site.camera", "%s", g->site.camera_seen ? "true" : "false")
  Q("site.digest", "%016llx", (unsigned long long)sr_interior_digest(&g->site.in))
  Q("player.roof", "%d", g->roof_building)
  Q("player.outfit", "%d", p->outfit)
  Q("player.stash", "%d", p->stash.count)
  Q("cyber.load", "%d", sr_game_cyber_load(g))
  Q("cyber.capacity", "%d", sr_game_neural_capacity(g))
  Q("cyber.glitches", "%d", g->glitches)
  Q("cyber.thermal", "%s", g->thermal ? "true" : "false")
  Q("approach.building", "%d", g->approach_building)
  if (strcmp(path, "job.target") == 0) {
    const SrJob *j = sr_jobs_get(&g->jobs, g->active_job);
    snprintf(out, cap, "%d", j ? j->target_building : -1);
    return true;
  }
  if (strcmp(path, "screen") == 0) {
    static const char *const n[] = {"title", "play", "pause", "terminal", "dead", "create", "shop"};
    snprintf(out, cap, "%s", n[g->screen]);
    return true;
  }
#undef Q
  return false;
}

SrResult sr_game_set(SrGame *g, const char *path, const char *value) {
  double v = atof(value);
  SrPlayer *p = &g->player;
  if (strcmp(path, "player.cash") == 0)
    p->cred = (int64_t)v;
  else if (strcmp(path, "player.satiety") == 0)
    p->satiety = (float)v;
  else if (strcmp(path, "player.fatigue") == 0)
    p->fatigue = (float)v;
  else if (strcmp(path, "player.health") == 0)
    p->health = (float)v;
  else if (strcmp(path, "player.cred") == 0)
    p->prog.street_cred = (int32_t)v;
  else if (strcmp(path, "player.medevac") == 0)
    p->medevac = v != 0;
  else if (strcmp(path, "clock.time") == 0) {
    sr_clock_set_seconds(&g->clock, (int64_t)v);
    g->last_minute = (int64_t)v / 60;
  } else if (strcmp(path, "hardcore") == 0)
    g->hardcore = v != 0;
  else if (strcmp(path, "hostiles") == 0)
    g->hostiles_near = (int)v;
  else if (strcmp(path, "combat") == 0)
    g->combat_count = (int)v;
  else if (strcmp(path, "pall_removed") == 0)
    g->pall_removed = v != 0;
  else if (strcmp(path, "crowd") == 0)
    g->crowd_enabled = v != 0;
  else if (sr_str_starts_with(path, "player.skill.")) {
    int s = skill_by_name(path + 13);
    if (s < 0) return SR_ERR_NOT_FOUND;
    p->prog.skill_rank[s] = (int8_t)v;
  } else
    return SR_ERR_NOT_FOUND;
  return SR_OK;
}

/* ---- commands ------------------------------------------------------------------------------ */

static int64_t parse_duration(const char *s) {
  char unit = s[strlen(s) - 1];
  double n = atof(s);
  switch (unit) {
    case 'd': return (int64_t)(n * SR_DAY_S);
    case 'h': return (int64_t)(n * 3600);
    case 'm': return (int64_t)(n * 60);
    default: return (int64_t)n;
  }
}

static int action_by_name(const char *n) {
  for (int a = 0; a < SR_ACT_COUNT; a++)
    if (strcmp(n, sr_action_name((SrAction)a)) == 0) return a;
  return -1;
}

static int poi_by_name(const char *n) {
  for (int p = 0; p < SR_POI_COUNT; p++) {
    char a[32], b[32];
    sr_str_normalize(sr_poi_name((SrPoi)p), a, sizeof a);
    sr_str_normalize(n, b, sizeof b);
    if (strcmp(a, b) == 0) return p;
  }
  return -1;
}

static int bg_by_name(const char *n) {
  static const char *const names[SR_BG_COUNT] = {"street_kid", "wageslave", "military", "netrunner", "medic"};
  for (int i = 0; i < SR_BG_COUNT; i++)
    if (strcmp(n, names[i]) == 0) return i;
  return -1;
}

static int name_index(const char *n, const char *(*name_of)(int), int count) {
  char want[48];
  sr_str_normalize(n, want, sizeof want);
  for (int i = 0; i < count; i++) {
    char have[48];
    sr_str_normalize(name_of(i), have, sizeof have);
    if (strcmp(have, want) == 0) return i;
  }
  return -1;
}

static const char *gadget_name(int i) { return sr_gadget_def(i)->name; }
static const char *cyber_name(int i) { return sr_cyber_def(i)->name; }

static int prop_by_name(const char *n) {
  static const char *const names[] = {"exit", "bed", "stash", "terminal", "wardrobe", "objective", "console"};
  for (int i = 0; i < (int)SR_ARRAY_LEN(names); i++)
    if (strcmp(n, names[i]) == 0) return i;
  return -1;
}

static int entry_by_name(const char *n) {
  static const char *const names[] = {"front", "service", "vent", "roof", "window"};
  for (int i = 0; i < (int)SR_ARRAY_LEN(names); i++)
    if (strcmp(n, names[i]) == 0) return i;
  return -1;
}

/* Stands the player in front of a prop inside the current site, facing it. */
static SrResult face_prop(SrGame *g, int type) {
  int pr = sr_interior_find_prop(&g->site.in, (SrPropType)type);
  if (!g->site.active || pr < 0) return SR_ERR_NOT_FOUND;
  SrVec3 at = sr_site_prop_pos(&g->site, pr);
  SrVec3 c = sr_site_room_center(&g->site, g->site.in.props[pr].room);
  SrVec3 dir = sr_v3_norm(sr_v3(c.x - at.x, 0, c.z - at.z));
  g->player.pos = sr_v3(at.x + dir.x, SR_SITE_FLOOR_Y + 1.7f, at.z + dir.z);
  g->player.yaw = atan2f(dir.x, dir.z);
  return SR_OK;
}

typedef struct ScriptState {
  SrGame *g;
  uint64_t seed;
  bool held[SR_ACT_COUNT];
  float move_x, move_y;
  float look_x;
  SrMemStore mem;
  SrStorage store;
} ScriptState;

static void run_steps(ScriptState *st, int n, int press_action) {
  for (int i = 0; i < n; i++) {
    SrActionState a;
    memset(&a, 0, sizeof a);
    for (int k = 0; k < SR_ACT_COUNT; k++) a.down[k] = st->held[k];
    if (press_action >= 0 && i == 0) {
      a.down[press_action] = true;
      a.pressed[press_action] = true;
    }
    a.move_x = st->move_x;
    a.move_y = st->move_y;
    a.look_x = st->look_x;
    sr_game_step(st->g, &a);
  }
}

static SrResult command(ScriptState *st, char *line, char *out, size_t cap) {
  char *tok[8];
  int n = sr_str_tokenize(line, tok, 8);
  out[0] = '\0';
  if (n == 0) return SR_OK;
  SrGame *g = st->g;
  const char *cmd = tok[0];
  if (strcmp(cmd, "seed") == 0 && n >= 2) {
    int64_t v;
    if (sr_parse_i64(tok[1], &v))
      st->seed = (uint64_t)v;
    else if (!sr_seed_from_code(tok[1], &st->seed))
      return SR_ERR_INVALID;
    return SR_OK;
  }
  if (strcmp(cmd, "newgame") == 0) {
    int bg = SR_BG_STREET_KID;
    SrCityOptions opt = {SR_CITY_MEDIUM, 0};
    for (int i = 1; i < n; i++) {
      if (sr_str_starts_with(tok[i], "background=")) {
        const char *b = tok[i] + 11;
        bg = strcmp(b, "any") == 0 ? (int)(st->seed % SR_BG_COUNT) : bg_by_name(b);
        if (bg < 0) return SR_ERR_INVALID;
      } else if (sr_str_starts_with(tok[i], "corps=")) {
        opt.corp_count = atoi(tok[i] + 6);
      }
    }
    return sr_game_new(g, st->seed, &opt, (SrBackground)bg, "Test Runner");
  }
  if (strcmp(cmd, "get") == 0 && n >= 2) { /* queries work on every screen */
    return sr_game_query(g, tok[1], out, cap) ? SR_OK : SR_ERR_NOT_FOUND;
  }
  if (!g->started) return SR_ERR_INVALID;
  if (strcmp(cmd, "advance") == 0 && n >= 2) {
    sr_game_skip(g, parse_duration(tok[1]), false);
    return SR_OK;
  }
  if (strcmp(cmd, "steps") == 0 && n >= 2) {
    run_steps(st, atoi(tok[1]), -1);
    return SR_OK;
  }
  if ((strcmp(cmd, "press") == 0 || strcmp(cmd, "release") == 0 || strcmp(cmd, "hold") == 0) && n >= 2) {
    int a = action_by_name(tok[1]);
    if (a < 0) return SR_ERR_INVALID;
    if (cmd[0] == 'p')
      run_steps(st, 1, a);
    else if (cmd[0] == 'r')
      st->held[a] = false;
    else {
      st->held[a] = true;
      run_steps(st, n >= 3 ? atoi(tok[2]) : 1, -1);
      st->held[a] = false;
    }
    return SR_OK;
  }
  if (strcmp(cmd, "move") == 0 && n >= 4) {
    st->move_x = (float)atof(tok[1]);
    st->move_y = (float)atof(tok[2]);
    run_steps(st, atoi(tok[3]), -1);
    st->move_x = st->move_y = 0;
    return SR_OK;
  }
  if (strcmp(cmd, "look") == 0 && n >= 3) {
    st->look_x = (float)atof(tok[1]);
    run_steps(st, atoi(tok[2]), -1);
    st->look_x = 0;
    return SR_OK;
  }
  if (strcmp(cmd, "set") == 0 && n >= 3) {
    const char *val = (n >= 4 && strcmp(tok[2], "=") == 0) ? tok[3] : tok[2];
    return sr_game_set(g, tok[1], val);
  }
  if (strcmp(cmd, "teleport") == 0 && n >= 2) {
    if (strcmp(tok[1], "home") == 0)
      sr_game_teleport_building(g, g->player.home);
    else if (strcmp(tok[1], "building") == 0 && n >= 3)
      sr_game_teleport_building(g, atoi(tok[2]));
    else {
      int poi = poi_by_name(tok[1]);
      if (poi < 0) return SR_ERR_INVALID;
      int32_t d;
      int b = sr_city_find_poi_near(g->city, (SrPoi)poi, g->player.home, &d);
      if (b < 0) return SR_ERR_NOT_FOUND;
      sr_game_teleport_building(g, b);
    }
    return SR_OK;
  }
  if (strcmp(cmd, "interact") == 0) return sr_game_interact(g);
  if (strcmp(cmd, "fire") == 0) {
    sr_game_fire(g);
    return SR_OK;
  }
  if (strcmp(cmd, "eat") == 0) {
    int32_t d;
    int b = sr_city_find_poi_near(g->city, SR_POI_FOOD, g->player.home, &d);
    return sr_game_eat_at(g, b);
  }
  if (strcmp(cmd, "sleep") == 0 && n >= 2) {
    SrSleepPlace place = SR_SLEEP_HOME;
    if (n >= 3 && strcmp(tok[2], "capsule") == 0) place = SR_SLEEP_CAPSULE;
    if (n >= 3 && strcmp(tok[2], "street") == 0) place = SR_SLEEP_STREET;
    return sr_game_sleep(g, atoi(tok[1]), place);
  }
  if (strcmp(cmd, "focus") == 0 && n >= 2) {
    if (atoi(tok[1]))
      sr_game_confirm_resume(g);
    else
      sr_game_focus(g, false);
    return SR_OK;
  }
  if (strcmp(cmd, "save") == 0 && n >= 2) return sr_save_to(g, &st->store, tok[1], 0);
  if (strcmp(cmd, "load") == 0 && n >= 2) return sr_load_from(g, &st->store, tok[1]);
  if (strcmp(cmd, "paydebt") == 0) return sr_player_pay_debt(&g->player, sr_game_time(g));
  if (strcmp(cmd, "autoplay") == 0) {
    SrStrategy s = SR_STRAT_GIG;
    int weeks = 1;
    for (int i = 1; i < n; i++) {
      if (sr_str_starts_with(tok[i], "strategy=")) s = sr_strategy_parse(tok[i] + 9);
      if (sr_str_starts_with(tok[i], "weeks=")) weeks = atoi(tok[i] + 6);
    }
    SrAutoplayStats stats = sr_autoplay(g, s, weeks * 7, s == SR_STRAT_GIG ? 0 : 3);
    snprintf(out, cap, "days=%d jobs=%d failed=%d missed_rent=%d earned=%lld", stats.days, stats.jobs_done,
             stats.jobs_failed, stats.missed_rent, (long long)stats.earned);
    return stats.soft_lock ? SR_ERR_INVALID : SR_OK;
  }
  if (strcmp(cmd, "job") == 0 && n >= 2) {
    static int32_t first_offer = -1; /* "first": the first job of the latest fixer offers */
    if (strcmp(tok[1], "offers") == 0) {
      int ids[4];
      int fixer = g->city->factions.fixer_building[g->city->blocks[g->city->buildings[g->player.home].block].district];
      int k = sr_game_fixer(g, fixer, ids);
      first_offer = k > 0 ? ids[0] : -1;
      snprintf(out, cap, "%d", k);
      return SR_OK;
    }
    int32_t id = n >= 3 ? (strcmp(tok[2], "first") == 0 ? first_offer : atoi(tok[2])) : -1;
    if (strcmp(tok[1], "accept") == 0 && n >= 3) return sr_game_accept(g, id);
    if (strcmp(tok[1], "execute") == 0 && n >= 4) {
      SrMissionResult r = sr_game_execute(g, id, (SrApproach)atoi(tok[3]));
      snprintf(out, cap, "%d", r.success);
      return SR_OK;
    }
    if (strcmp(tok[1], "report") == 0 && n >= 3) {
      snprintf(out, cap, "%d", sr_game_report(g, id));
      return SR_OK;
    }
    if (strcmp(tok[1], "frame") == 0 && n >= 4) return sr_game_frame(g, id, atoi(tok[3]));
  }
  if (strcmp(cmd, "enter") == 0 && n >= 2) {
    int b = strcmp(tok[1], "home") == 0 ? g->player.home : atoi(tok[1]);
    if (strcmp(tok[1], "target") == 0) {
      const SrJob *j = sr_jobs_get(&g->jobs, g->active_job);
      b = j ? j->target_building : -1;
    }
    int via = n >= 3 ? entry_by_name(tok[2]) : SR_ENTRY_FRONT;
    if (via < 0) return SR_ERR_INVALID;
    return sr_game_enter(g, b, (SrEntryType)via);
  }
  if (strcmp(cmd, "leave") == 0) {
    if (!sr_game_inside(g)) return SR_ERR_INVALID;
    sr_game_leave(g);
    return SR_OK;
  }
  if (strcmp(cmd, "face") == 0 && n >= 2) {
    int t = prop_by_name(tok[1]);
    return t < 0 ? SR_ERR_INVALID : face_prop(g, t);
  }
  if (strcmp(cmd, "give") == 0 && n >= 3) {
    bool cyber = strcmp(tok[1], "cyber") == 0;
    int idx = cyber ? name_index(tok[2], cyber_name, sr_cyber_count()) : name_index(tok[2], gadget_name, SR_GD_COUNT);
    if (idx < 0) return SR_ERR_INVALID;
    SrItem it = sr_item_simple(cyber ? SR_IK_CYBER : SR_IK_GADGET, idx, n >= 4 ? atoi(tok[3]) : 1);
    return sr_inv_add(&g->player.inv, it) ? SR_OK : SR_ERR_NOMEM;
  }
  if (strcmp(cmd, "install") == 0 && n >= 2) {
    int idx = name_index(tok[1], cyber_name, sr_cyber_count());
    if (idx < 0 || !sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_CYBER, idx, 1))) return SR_ERR_INVALID;
    return sr_game_install_cyber(g, sr_inv_find(&g->player.inv, SR_IK_CYBER, idx)) ? SR_OK : SR_ERR_DENIED;
  }
  if (strcmp(cmd, "use") == 0 && n >= 2) {
    int idx = name_index(tok[1], gadget_name, SR_GD_COUNT);
    if (idx < 0) return SR_ERR_INVALID;
    return sr_game_use_gadget(g, idx) ? SR_OK : SR_ERR_DENIED;
  }
  if (strcmp(cmd, "ability") == 0) return sr_game_ability(g) ? SR_OK : SR_ERR_DENIED;
  if (strcmp(cmd, "grapple") == 0) return sr_game_grapple(g);
  if (strcmp(cmd, "echo") == 0) return SR_OK;
  return SR_ERR_INVALID;
}

SrResult sr_game_command(SrGame *g, const char *line, char *out, size_t cap) {
  ScriptState st;
  memset(&st, 0, sizeof st);
  st.g = g;
  st.seed = g->seed;
  sr_memstore_init(&st.mem, &st.store);
  char buf[256];
  sr_strlcpy(buf, line, sizeof buf);
  SrResult r = command(&st, buf, out, cap);
  sr_memstore_free(&st.mem);
  return r;
}

/* ---- autoplayer ------------------------------------------------------------------------------ */

SrStrategy sr_strategy_parse(const char *s) {
  if (strcmp(s, "ghost") == 0) return SR_STRAT_GHOST;
  if (strcmp(s, "face") == 0) return SR_STRAT_FACE;
  if (strcmp(s, "hammer") == 0) return SR_STRAT_HAMMER;
  if (strcmp(s, "netrunner") == 0) return SR_STRAT_NETRUNNER;
  if (strcmp(s, "live") == 0) return SR_STRAT_LIVE;
  return SR_STRAT_GIG;
}

static int here(SrGame *g) {
  int b = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  if (b < 0 || g->city->blocks[b].building_count == 0) return g->player.home;
  return g->city->blocks[b].first_building;
}

static void travel(SrGame *g, int to) {
  int32_t d = sr_city_walk_dist(g->city, here(g), to);
  SrTrip cab = sr_cab_trip(g->city, here(g), to);
  if (d > 120000 && g->player.cred > cab.fare * 4 + g->player.rent)
    sr_game_cab(g, to);
  else
    sr_game_walk_to(g, to);
}

static bool approach_for(SrStrategy s, const SrJob *j, SrApproach *out) {
  static const SrApproach pref[] = {SR_AP_FACE, SR_AP_GHOST, SR_AP_RUNNER, SR_AP_HAMMER};
  SrApproach want = SR_AP_FACE;
  switch (s) {
    case SR_STRAT_GHOST: want = SR_AP_GHOST; break;
    case SR_STRAT_FACE: want = SR_AP_FACE; break;
    case SR_STRAT_HAMMER: want = SR_AP_HAMMER; break;
    case SR_STRAT_NETRUNNER: want = SR_AP_RUNNER; break;
    default:
      for (int i = 0; i < 4; i++)
        if (j->approaches & (1u << pref[i])) {
          *out = pref[i];
          return true;
        }
      return false;
  }
  *out = want;
  if (j->approaches & (1u << want)) return true;
  if (j->tier == 0) { /* specialists still take street gigs to pay the rent */
    for (int i = 0; i < 4; i++)
      if (j->approaches & (1u << pref[i])) {
        *out = pref[i];
        return true;
      }
  }
  return false;
}

static bool find_open_food(SrGame *g, int *out) {
  int best = -1;
  int32_t bd = INT32_MAX;
  int from = here(g);
  int64_t t = sr_game_time(g);
  for (int i = 0; i < g->city->building_count; i++) {
    int poi = g->city->buildings[i].poi;
    if ((poi != SR_POI_FOOD && poi != SR_POI_BAR && poi != SR_POI_GROCERY) || !sr_poi_open(g->city, i, t)) continue;
    int32_t ax, ay, bx, by;
    sr_building_center(&g->city->buildings[from], &ax, &ay);
    sr_building_center(&g->city->buildings[i], &bx, &by);
    int32_t d = abs(ax - bx) + abs(ay - by);
    if (d < bd) {
      bd = d;
      best = i;
    }
  }
  *out = best;
  return best >= 0;
}

SrAutoplayStats sr_autoplay(SrGame *g, SrStrategy s, int days, int max_tier) {
  SrAutoplayStats st;
  memset(&st, 0, sizeof st);
  g->crowd_enabled = false;
  int64_t start_cash = g->player.cred;
  int missed_before = 0;
  int64_t end = sr_game_time(g) + (int64_t)days * SR_DAY_S;
  int iterations = 0;
  int64_t last_sleep = sr_game_time(g);
  while (sr_game_time(g) < end) {
    if (++iterations > days * 80) {
      st.soft_lock = true;
      break;
    }
    SrPlayer *p = &g->player;
    int64_t t = sr_game_time(g);
    if (p->missed > missed_before) st.missed_rent++;
    missed_before = p->missed;
    if (p->satiety < 45.0f) {
      int food;
      if (find_open_food(g, &food)) {
        travel(g, food);
        if (sr_game_eat_at(g, food) == SR_OK)
          st.meals++;
        else
          sr_game_skip(g, 1800, false);
      } else {
        sr_game_skip(g, 1800, false);
      }
      continue;
    }
    if (p->fatigue > 70.0f || t - last_sleep > 20 * 3600) {
      if (!p->evicted) {
        travel(g, p->home);
        sr_game_sleep(g, 8, SR_SLEEP_HOME);
      } else if (p->rent_debt > 0 && p->cred >= p->rent_debt) {
        sr_player_pay_debt(p, t);
        continue;
      } else {
        sr_game_sleep(g, 8, SR_SLEEP_STREET);
      }
      st.sleeps++;
      last_sleep = sr_game_time(g);
      continue;
    }
    if (s == SR_STRAT_LIVE) { /* just living: income is injected by the caller */
      sr_game_skip(g, 3600, false);
      continue;
    }
    SrJob *job = g->active_job >= 0 ? sr_jobs_get(&g->jobs, g->active_job) : NULL;
    if (job && job->state != SR_JS_ACCEPTED) {
      if (job->state == SR_JS_FAILED) st.jobs_failed++;
      g->active_job = -1;
      job = NULL;
    }
    if (!job) {
      int district = g->city->blocks[g->city->buildings[p->home].block].district;
      int fixer = g->city->factions.fixer_building[district];
      travel(g, fixer);
      int ids[4];
      int k = sr_game_fixer(g, fixer, ids);
      bool took = false;
      for (int i = 0; i < k && !took; i++) {
        SrJob *j = sr_jobs_get(&g->jobs, ids[i]);
        SrApproach a;
        if (!j || j->state != SR_JS_OFFERED || j->tier > max_tier || !approach_for(s, j, &a)) continue;
        took = sr_game_accept(g, ids[i]) == SR_OK;
      }
      if (!took) sr_game_skip(g, 2 * 3600, false);
      continue;
    }
    if (!job->executed) {
      SrApproach a = SR_AP_HAMMER;
      approach_for(s, job, &a);
      travel(g, job->target_building);
      int32_t id = job->id;
      SrMissionResult r = sr_game_execute(g, id, a);
      job = sr_jobs_get(&g->jobs, id);
      if (!r.success && job && job->state == SR_JS_ACCEPTED) {
        sr_game_skip(g, 3600, false); /* regroup, try again later */
      }
      if (g->player.health < g->player.max_health * 0.4f) {
        int med = sr_inv_find(&g->player.inv, SR_IK_GADGET, SR_GD_MEDKIT);
        if (med >= 0) {
          sr_inv_remove_at(&g->player.inv, med, 1);
          g->player.health = SR_MIN(g->player.max_health, g->player.health + 40);
        }
      }
      continue;
    }
    if (job->state == SR_JS_ACCEPTED) {
      travel(g, job->client_building);
      int32_t id = job->id;
      int32_t paid = sr_game_report(g, id);
      job = sr_jobs_get(&g->jobs, id);
      if (job && job->state == SR_JS_COMPLETED) {
        st.jobs_done++;
        st.earned += paid;
      } else if (job && job->state == SR_JS_FAILED) {
        st.jobs_failed++;
      }
    }
  }
  st.days = days;
  st.starving_hours = g->player.starving_hours;
  st.spent = start_cash + st.earned - g->player.cred;
  return st;
}

/* ---- scenario runner ------------------------------------------------------------------------ */

static bool compare(const char *actual, const char *op, const char *expected) {
  char *e1, *e2;
  double a = strtod(actual, &e1), b = strtod(expected, &e2);
  bool numeric = *actual && *expected && *e1 == '\0' && *e2 == '\0';
  if (numeric) {
    if (strcmp(op, "==") == 0) return fabs(a - b) < 1e-6;
    if (strcmp(op, "!=") == 0) return fabs(a - b) >= 1e-6;
    if (strcmp(op, "<") == 0) return a < b;
    if (strcmp(op, "<=") == 0) return a <= b;
    if (strcmp(op, ">") == 0) return a > b;
    if (strcmp(op, ">=") == 0) return a >= b;
    return false;
  }
  if (strcmp(op, "==") == 0) return strcmp(actual, expected) == 0;
  if (strcmp(op, "!=") == 0) return strcmp(actual, expected) != 0;
  return false;
}

static int run_once(const char *text, const char *name, uint64_t seed, bool seeded, char *err, size_t cap) {
  ScriptState st;
  memset(&st, 0, sizeof st);
  st.g = sr_game_create();
  st.seed = seed;
  sr_memstore_init(&st.mem, &st.store);
  int lineno = 0, rc = 0;
  const char *p = text;
  while (*p && rc == 0) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    char line[256];
    if (len >= sizeof line) len = sizeof line - 1;
    memcpy(line, p, len);
    line[len] = '\0';
    p = nl ? nl + 1 : p + len;
    lineno++;
    char *hash = strchr(line, '#');
    if (hash) *hash = '\0';
    sr_str_trim(line);
    if (!line[0]) continue;
    if (sr_str_starts_with(line, "seed") && strstr(line, "random(") && seeded) continue;
    if (sr_str_starts_with(line, "expect ")) {
      char *tok[5];
      int n = sr_str_tokenize(line, tok, 5);
      char actual[128];
      if (n < 4 || !sr_game_query(st.g, tok[1], actual, sizeof actual)) {
        snprintf(err, cap, "%s:%d: cannot evaluate '%s'", name, lineno, n > 1 ? tok[1] : "");
        rc = 1;
      } else if (!compare(actual, tok[2], tok[3])) {
        snprintf(err, cap, "%s:%d: expected %s %s %s, actual %s (seed %llu)", name, lineno, tok[1], tok[2], tok[3],
                 actual, (unsigned long long)st.seed);
        rc = 1;
      }
      continue;
    }
    char out[160];
    SrResult r = command(&st, line, out, sizeof out);
    if (r != SR_OK) {
      snprintf(err, cap, "%s:%d: command failed (%s): %s %s", name, lineno, sr_result_name(r), line, out);
      rc = 1;
    }
  }
  sr_memstore_free(&st.mem);
  sr_game_destroy(st.g);
  return rc;
}

int sr_script_run(const char *text, const char *name, char *err, size_t cap) {
  const char *rnd = strstr(text, "random(");
  if (rnd) {
    int n = atoi(rnd + 7);
    int failures = 0;
    char first[256] = "";
    for (int i = 0; i < n; i++) {
      char e[256];
      if (run_once(text, name, 0xC0FFEEull + (uint64_t)i * 7919u, true, e, sizeof e) != 0) {
        if (!failures) sr_strlcpy(first, e, sizeof first);
        failures++;
      }
    }
    /* sweeps allow up to 5 % failing seeds where the spec says "at least 95 %" */
    int allowed = strstr(text, "allow_failures=5%") ? n / 20 : 0;
    if (failures > allowed) {
      snprintf(err, cap, "%d/%d seeds failed; first: %s", failures, n, first);
      return 1;
    }
    return 0;
  }
  return run_once(text, name, 0x5EEDC0DEull, false, err, cap);
}

int sr_sweep_seeds(uint64_t first, int n, char *err, size_t cap) {
  int failures = 0;
  SrCity *c = (SrCity *)calloc(1, sizeof(SrCity));
  for (int i = 0; i < n; i++) {
    uint64_t seed = first + (uint64_t)i;
    sr_city_generate(c, seed, NULL);
    int inv = sr_city_validate(c);
    bool routes = true;
    for (int b = 0; b < c->building_count && routes; b++) {
      if (!(c->buildings[b].flags & SR_BF_MISSION)) continue;
      SrInterior in;
      sr_interior_generate(c, b, &in);
      routes = sr_interior_route_count(&in) >= 2 && sr_interior_security_chain_ok(&in);
    }
    bool nav = sr_nav_connected(c);
    if (inv != 0 || !routes || !nav) {
      if (!failures)
        snprintf(err, cap, "seed %llu: %s", (unsigned long long)seed,
                 inv       ? sr_city_invariant_name(inv)
                 : !routes ? "mission routes"
                           : "navigation connectivity");
      failures++;
    }
  }
  free(c);
  return failures;
}
