/* Guidance and navigation (see sr_guide.h). */
#include "game/sr_guide.h"

#include "game/sr_game.h"
#include "world/sr_nav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float dist_xz(SrVec3 a, SrVec3 b) {
  float dx = a.x - b.x, dz = a.z - b.z;
  return sqrtf(dx * dx + dz * dz);
}

static SrVec3 node_pos(const SrCity *c, int32_t node) {
  int32_t x, y;
  sr_nav_node_pos(c, node, &x, &y);
  return sr_v3((float)x / 100.0f, 0, (float)y / 100.0f);
}

/* ---- destinations ------------------------------------------------------------------------ */

SrVec3 sr_station_entrance(const SrGame *g, int station) {
  int32_t ox, oy;
  sr_city_block_origin(g->city->stations[station].block, g->city->grid, &ox, &oy);
  return sr_v3((float)ox / 100.0f + 4.0f, 0, (float)oy / 100.0f + 4.0f);
}

static void set_dest(SrGame *g, SrDestKind kind, int32_t building, SrVec3 target, const char *label) {
  SrRoute *r = &g->route;
  r->kind = (uint8_t)kind;
  r->building = building;
  r->target = target;
  r->automatic = false;
  r->job = -1;
  r->n = 0;
  r->length = 0;
  snprintf(r->label, sizeof r->label, "%s", label);
  sr_route_plan(g);
}

static void building_label(const SrGame *g, int b, char *out, size_t cap) {
  const SrBuilding *bd = &g->city->buildings[b];
  if (b == g->player.home)
    snprintf(out, cap, "HOME");
  else if (bd->name[0])
    snprintf(out, cap, "%s", bd->name);
  else if (bd->poi != SR_POI_NONE)
    snprintf(out, cap, "%s", sr_poi_name((SrPoi)bd->poi));
  else
    snprintf(out, cap, "A BUILDING");
}

void sr_route_set_building(SrGame *g, int building) {
  if (building < 0 || building >= g->city->building_count) return;
  int32_t door = sr_nav_door_node(g->city, building);
  int32_t cx, cy;
  sr_building_center(&g->city->buildings[building], &cx, &cy);
  SrVec3 at = door >= 0 ? node_pos(g->city, door) : sr_v3((float)cx / 100.0f, 0, (float)cy / 100.0f);
  char label[48];
  building_label(g, building, label, sizeof label);
  set_dest(g, SR_DEST_BUILDING, building, at, label);
}

void sr_route_set_point(SrGame *g, float x, float z) { set_dest(g, SR_DEST_POINT, -1, sr_v3(x, 0, z), "MARKED POINT"); }

void sr_route_set_station(SrGame *g, int station) {
  if (station < 0 || station >= g->city->station_count) return;
  char label[48];
  snprintf(label, sizeof label, "METRO: %s", g->city->districts[g->city->stations[station].district].name);
  set_dest(g, SR_DEST_STATION, station, sr_station_entrance(g, station), label);
}

void sr_route_clear(SrGame *g) {
  if (g->route.automatic) g->route.declined_job = g->route.job;
  int32_t keep = g->route.declined_job;
  memset(&g->route, 0, sizeof g->route);
  g->route.job = -1;
  g->route.building = -1;
  g->route.declined_job = keep;
  g->route.done_building = -2;
}

bool sr_route_plan(SrGame *g) {
  SrRoute *r = &g->route;
  r->n = 0;
  r->length = 0;
  r->planned = g->steps;
  if (r->kind == SR_DEST_NONE) return false;
  SrVec3 at = g->player.pos;
  at.y = 0;
  if (g->site.active) { /* inside: a straight line until the player is out */
    r->pts[0] = at;
    r->pts[1] = r->target;
    r->n = 2;
    r->length = dist_xz(at, r->target);
    return true;
  }
  const SrCity *c = g->city;
  int32_t from = sr_nav_nearest_node(c, (int32_t)(at.x * 100.0f), (int32_t)(at.z * 100.0f));
  int32_t to = r->kind == SR_DEST_BUILDING
                   ? sr_nav_door_node(c, r->building)
                   : sr_nav_nearest_node(c, (int32_t)(r->target.x * 100.0f), (int32_t)(r->target.z * 100.0f));
  static SrNavPath path; /* 4 KB: not on the web's small stack */
  r->pts[r->n++] = at;
  if (from >= 0 && to >= 0 && sr_nav_path(c, from, to, true, &path)) {
    for (int i = 0; i < path.count && r->n < SR_ROUTE_MAX - 1; i++) {
      SrVec3 p = node_pos(c, path.node[i]);
      if (r->n >= 2) { /* drop points on a straight line */
        SrVec3 a = r->pts[r->n - 2], b = r->pts[r->n - 1];
        float cross = (b.x - a.x) * (p.z - b.z) - (b.z - a.z) * (p.x - b.x);
        if (fabsf(cross) < 0.5f) r->n--;
      }
      if (dist_xz(p, r->pts[r->n - 1]) > 0.5f) r->pts[r->n++] = p;
    }
  }
  if (dist_xz(r->target, r->pts[r->n - 1]) > 0.5f) r->pts[r->n++] = r->target;
  for (int i = 1; i < r->n; i++) r->length += dist_xz(r->pts[i - 1], r->pts[i]);
  return r->n >= 2;
}

float sr_route_distance(const SrGame *g) {
  if (g->route.kind == SR_DEST_NONE) return 0;
  if (g->route.n < 2) return dist_xz(g->player.pos, g->route.target);
  return g->route.length;
}

void sr_route_instruction(const SrGame *g, char *out, int cap) {
  const SrRoute *r = &g->route;
  out[0] = '\0';
  if (r->kind == SR_DEST_NONE || r->n < 2) return;
  if (r->length < 25.0f) {
    snprintf(out, (size_t)cap, "DESTINATION AHEAD");
    return;
  }
  float along = 0;
  for (int i = 1; i + 1 < r->n; i++) {
    along += dist_xz(r->pts[i - 1], r->pts[i]);
    float ax = r->pts[i].x - r->pts[i - 1].x, az = r->pts[i].z - r->pts[i - 1].z;
    float bx = r->pts[i + 1].x - r->pts[i].x, bz = r->pts[i + 1].z - r->pts[i].z;
    float la = sqrtf(ax * ax + az * az), lb = sqrtf(bx * bx + bz * bz);
    if (la < 0.1f || lb < 0.1f) continue;
    if (i == 1 && la < 8.0f) continue; /* the step from a door onto the walking line */
    float cross = (ax * bz - az * bx) / (la * lb), dot = (ax * bx + az * bz) / (la * lb);
    if (dot > 0.85f) continue; /* nearly straight: not a turn */
    int m = (int)(along / 10.0f + 0.5f) * 10;
    if (m <= 10)
      snprintf(out, (size_t)cap, "TURN %s NOW", cross > 0 ? "RIGHT" : "LEFT");
    else
      snprintf(out, (size_t)cap, "TURN %s IN %d M", cross > 0 ? "RIGHT" : "LEFT", m);
    return;
  }
  snprintf(out, (size_t)cap, "CONTINUE %d M", (int)(r->length / 10.0f + 0.5f) * 10);
}

/* ---- tips -------------------------------------------------------------------------------- */

static const char *const k_tips[SR_TIP_COUNT][2] = {
    [SR_TIP_WELCOME] =
        {"THE STREET. WALK WITH WASD, LOOK WITH THE MOUSE. PRESS M FOR THE CITY MAP.",
         "THE STREET. MOVE WITH THE LEFT THUMB, LOOK WITH THE RIGHT. THE MAP BUTTON OPENS THE CITY MAP."},
    [SR_TIP_MAP] = {"ON THE MAP, PICK ANY PLACE WITH E: A ROUTE LIGHTS UP ON THE PAVEMENT.",
                    "ON THE MAP, TAP ANY PLACE: A ROUTE LIGHTS UP ON THE PAVEMENT."},
    [SR_TIP_ROUTE] = {"FOLLOW THE GLOWING ARROWS. THE DIAMOND ON THE COMPASS MARKS YOUR DESTINATION.",
                      "FOLLOW THE GLOWING ARROWS. THE DIAMOND ON THE COMPASS MARKS YOUR DESTINATION."},
    [SR_TIP_INTERACT] = {"PRESS E TO ENTER, TALK, BUY OR USE WHAT IS IN FRONT OF YOU.",
                         "THE HAND BUTTON ENTERS, TALKS, BUYS OR USES WHAT IS IN FRONT OF YOU."},
    [SR_TIP_HUNGER] = {"YOU ARE HUNGRY. FOOD STALLS AND DINERS SHOW ON THE MAP UNDER FOOD (R CYCLES THE FILTER).",
                       "YOU ARE HUNGRY. FOOD STALLS AND DINERS SHOW ON THE MAP UNDER FOOD."},
    [SR_TIP_FATIGUE] = {"YOU ARE TIRED. SLEEP AT HOME OR IN A CAPSULE HOTEL.",
                        "YOU ARE TIRED. SLEEP AT HOME OR IN A CAPSULE HOTEL."},
    [SR_TIP_RENT] = {"RENT IS DUE SOON. TAKE A JOB FROM A FIXER OR THE TERMINAL (T).",
                     "RENT IS DUE SOON. TAKE A JOB FROM A FIXER OR THE TERMINAL."},
    [SR_TIP_JOBS] = {"NEED MONEY? FIXERS OFFER JOBS. THEY ARE ON THE MAP UNDER WORK.",
                     "NEED MONEY? FIXERS OFFER JOBS. THEY ARE ON THE MAP UNDER WORK."},
    [SR_TIP_JOB_ROUTE] = {"THE ROUTE NOW FOLLOWS YOUR ACTIVE JOB. CLEAR IT ON THE MAP WITH Q.",
                          "THE ROUTE NOW FOLLOWS YOUR ACTIVE JOB. CLEAR IT ON THE MAP."},
    [SR_TIP_HEAT] = {"SOMEONE CALLED IT IN: YOUR HEAT ROSE. LIE LOW, CHANGE CLOTHES OR AVOID PATROLS.",
                     "SOMEONE CALLED IT IN: YOUR HEAT ROSE. LIE LOW, CHANGE CLOTHES OR AVOID PATROLS."},
    [SR_TIP_SITE] = {"INSIDE A SITE: CROUCH (C) TO MOVE QUIETLY, KEEP TO THE DARK, WATCH THE GUARDS.",
                     "INSIDE A SITE: CROUCH TO MOVE QUIETLY, KEEP TO THE DARK, WATCH THE GUARDS."},
    [SR_TIP_COMBAT] = {"YOU ARE SPOTTED. BREAK THE LINE OF SIGHT OR FIGHT FROM COVER.",
                       "YOU ARE SPOTTED. BREAK THE LINE OF SIGHT OR FIGHT FROM COVER."},
    [SR_TIP_HURT] = {"BADLY HURT. A MEDKIT HELPS; A CLINIC PATCHES YOU UP FOR CRED.",
                     "BADLY HURT. A MEDKIT HELPS; A CLINIC PATCHES YOU UP FOR CRED."},
    [SR_TIP_MATRIX] = {"THE MATRIX: FLY WITH WASD, SPACE AND C. E WORKS A NODE. ICE HUNTS YOU.",
                       "THE MATRIX: FLY WITH THE STICK, THE UP AND DOWN BUTTONS. TAP A NODE. ICE HUNTS YOU."},
    [SR_TIP_METRO] = {"A LONG WAY? THE METRO IS FASTER: STATIONS ARE ON THE MAP UNDER TRAVEL.",
                      "A LONG WAY? THE METRO IS FASTER: STATIONS ARE ON THE MAP UNDER TRAVEL."},
    [SR_TIP_TERMINAL] = {"YOUR TERMINAL (T) HAS JOBS, NEWS, THE MARKET AND THE CORPORATE BOARD.",
                         "YOUR TERMINAL HAS JOBS, NEWS, THE MARKET AND THE CORPORATE BOARD."},
    [SR_TIP_HELP] = {"HELP AND THESE TIPS: PAUSE (ESC), THEN H. TIPS CAN BE TURNED OFF THERE.",
                     "HELP AND THESE TIPS: PAUSE, THEN HELP. TIPS CAN BE TURNED OFF THERE."},
};

static const char *const k_tip_names[SR_TIP_COUNT] = {"welcome", "map",    "route",     "interact", "hunger", "fatigue",
                                                      "rent",    "jobs",   "job_route", "heat",     "site",   "combat",
                                                      "hurt",    "matrix", "metro",     "terminal", "help"};

const char *sr_guide_tip_name(SrTip tip) { return (unsigned)tip < SR_TIP_COUNT ? k_tip_names[tip] : "?"; }

void sr_guide_init(SrGuide *gd) {
  memset(gd, 0, sizeof *gd);
  gd->tip = -1;
}

void sr_guide_new_game(SrGame *g) {
  memset(&g->route, 0, sizeof g->route);
  g->route.job = g->route.building = g->route.declined_job = -1;
  g->route.done_building = -2;
  sr_guide_init(&g->guide);
  g->map.span = 600.0f;
  g->map.filter = SR_MAPF_ALL;
  g->help_page = 0;
}

static void save_seen(SrGame *g) {
  snprintf(g->settings.tips_seen, sizeof g->settings.tips_seen, "%llx", (unsigned long long)g->guide.seen);
}

bool sr_guide_tip(SrGame *g, SrTip tip) {
  SrGuide *gd = &g->guide;
  if (!g->settings.guidance || (unsigned)tip >= SR_TIP_COUNT) return false;
  gd->seen |= strtoull(g->settings.tips_seen, NULL, 16);
  if (gd->seen & (1ull << tip)) return false;
  for (int i = 0; i < gd->nq; i++)
    if (gd->queue[i] == (int)tip) return false;
  if (gd->tip == (int)tip || gd->nq >= (int)(sizeof gd->queue / sizeof gd->queue[0])) return false;
  gd->queue[gd->nq++] = (int)tip;
  return true;
}

void sr_guide_reset(SrGame *g) {
  g->guide.seen = 0;
  g->guide.nq = 0;
  g->guide.tip = -1;
  g->settings.tips_seen[0] = '\0';
}

const char *sr_guide_tip_text(const SrGame *g, bool touch, float *alpha) {
  const SrGuide *gd = &g->guide;
  if (gd->tip < 0 || !g->settings.guidance) return NULL;
  double t = sr_game_fx_now(g) - gd->tip_t;
  if (alpha) *alpha = (float)fmin(1.0, fmin(t / 0.3, (SR_TIP_SECONDS - t) / 0.6));
  return k_tips[gd->tip][touch ? 1 : 0];
}

static void tips_step(SrGame *g) {
  SrGuide *gd = &g->guide;
  double now = sr_game_fx_now(g);
  if (gd->tip >= 0 && now - gd->tip_t >= SR_TIP_SECONDS) gd->tip = -1;
  if (!g->settings.guidance) {
    gd->tip = -1;
    gd->nq = 0;
    return;
  }
  if (g->steps % 30 == 0) { /* what is happening that a tip could help with */
    const SrPlayer *p = &g->player;
    int64_t t = sr_game_time(g);
    bool street = !g->site.active && !g->dive.active;
    int heat = 0;
    for (int k = 0; k < SR_MAX_FACTIONS; k++) heat = SR_MAX(heat, g->heat.level[k]);
    if (street) {
      sr_guide_tip(g, SR_TIP_WELCOME);
      sr_guide_tip(g, SR_TIP_MAP);
      sr_guide_tip(g, SR_TIP_HELP);
    }
    if (g->ctx.kind != SR_CTX_NONE) sr_guide_tip(g, SR_TIP_INTERACT);
    if (g->route.n >= 2) sr_guide_tip(g, SR_TIP_ROUTE);
    if (g->route.automatic) sr_guide_tip(g, SR_TIP_JOB_ROUTE);
    if (g->route.length > 800.0f) sr_guide_tip(g, SR_TIP_METRO);
    if (p->satiety < 35.0f) sr_guide_tip(g, SR_TIP_HUNGER);
    if (p->fatigue > 75.0f) sr_guide_tip(g, SR_TIP_FATIGUE);
    if (p->rent > 0 && p->rent_due - t < 2 * 86400 && p->cred < p->rent) sr_guide_tip(g, SR_TIP_RENT);
    if (g->active_job < 0 && g->steps > 60 * 90) sr_guide_tip(g, SR_TIP_JOBS);
    if (heat >= 1) sr_guide_tip(g, SR_TIP_HEAT);
    if (g->site.active && !g->site.in.apartment) sr_guide_tip(g, SR_TIP_SITE);
    if (g->site.active && g->site.in.apartment) sr_guide_tip(g, SR_TIP_TERMINAL);
    if (g->combat_count > 0) sr_guide_tip(g, SR_TIP_COMBAT);
    if (p->health < p->max_health * 0.4f) sr_guide_tip(g, SR_TIP_HURT);
    if (g->dive.active) sr_guide_tip(g, SR_TIP_MATRIX);
  }
  if (gd->tip < 0 && gd->nq > 0) { /* the next tip, once the last has gone */
    gd->tip = gd->queue[0];
    memmove(gd->queue, gd->queue + 1, sizeof(int) * (size_t)(gd->nq - 1));
    gd->nq--;
    gd->tip_t = now;
    gd->seen |= 1ull << gd->tip;
    save_seen(g);
  }
}

/* ---- the route each step ----------------------------------------------------------------- */

static int32_t job_building(const SrJob *j) {
  if (j->executed) return j->client_building; /* report back */
  return j->target_building;
}

static void route_step(SrGame *g) {
  SrRoute *r = &g->route;
  /* follow the active job when nothing else is set */
  if (r->kind == SR_DEST_NONE || r->automatic) {
    SrJob *j = g->active_job >= 0 ? sr_jobs_get(&g->jobs, g->active_job) : NULL;
    int32_t want = j && j->id != r->declined_job ? job_building(j) : -1;
    if (want >= 0 && want < g->city->building_count && (r->kind == SR_DEST_NONE || r->building != want) &&
        want != r->done_building) {
      sr_route_set_building(g, want);
      r->automatic = true;
      r->job = j->id;
    } else if (r->automatic && !j) {
      sr_route_clear(g);
    }
  }
  if (r->kind == SR_DEST_NONE) return;
  SrVec3 at = g->player.pos;
  at.y = 0;
  if (dist_xz(at, r->target) < SR_ROUTE_ARRIVE) { /* arrived */
    char msg[64];
    snprintf(msg, sizeof msg, "ARRIVED: %s", r->label);
    sr_game_toast(g, msg);
    int32_t done = r->kind == SR_DEST_BUILDING ? r->building : -2;
    bool automatic = r->automatic;
    int32_t declined = r->declined_job;
    memset(r, 0, sizeof *r);
    r->job = -1;
    r->building = -1;
    r->declined_job = declined;
    r->done_building = automatic ? done : -2;
    return;
  }
  if (g->steps - r->planned >= SR_ROUTE_REPLAN || r->n < 2) sr_route_plan(g);
}

void sr_guide_step(SrGame *g) {
  if (g->dive.active) {
    tips_step(g);
    return;
  }
  route_step(g);
  tips_step(g);
}

/* ---- the current goal -------------------------------------------------------------------- */

void sr_guide_objective(const SrGame *g, char *out, int cap) {
  out[0] = '\0';
  if (!g->settings.guidance) return;
  const SrPlayer *p = &g->player;
  int64_t t = sr_game_time(g);
  const SrJob *j = NULL;
  for (int i = 0; i < g->jobs.count && g->active_job >= 0; i++)
    if (g->jobs.jobs[i].id == g->active_job) j = &g->jobs.jobs[i];
  if (j) {
    const SrBuilding *b = &g->city->buildings[j->executed ? j->client_building : j->target_building];
    const char *where = b->name[0] ? b->name : sr_poi_name((SrPoi)b->poi);
    if (j->executed)
      snprintf(out, (size_t)cap, "REPORT BACK: %s", where);
    else
      snprintf(out, (size_t)cap, "JOB: %s AT %s", sr_job_type_name((SrJobType)j->type), where);
  } else if (p->rent > 0 && p->rent_due - t < 2 * 86400 && p->cred < p->rent) {
    snprintf(out, (size_t)cap, "RENT DUE IN %d H: %d CRED. FIND WORK.", (int)((p->rent_due - t) / 3600), p->rent);
  } else if (p->satiety < 25.0f) {
    snprintf(out, (size_t)cap, "EAT SOMETHING");
  } else if (p->fatigue > 85.0f) {
    snprintf(out, (size_t)cap, "GET SOME SLEEP");
  } else {
    snprintf(out, (size_t)cap, "FIND WORK: A FIXER OR THE TERMINAL'S JOBS");
  }
  if (g->route.kind != SR_DEST_NONE && g->route.automatic) { /* the distance to this goal */
    size_t n = strlen(out);
    snprintf(out + n, (size_t)cap > n ? (size_t)cap - n : 0, " (%d M)", (int)sr_route_distance(g));
  }
}

/* ---- the city map ------------------------------------------------------------------------ */

void sr_map_open(SrGame *g) {
  if (g->map.span <= 0) g->map.span = 600.0f;
  g->map.cx = g->player.pos.x;
  g->map.cz = g->player.pos.z;
  g->screen = SR_SCREEN_MAP;
  g->clock.paused = true;
}

float sr_map_max_span(const SrGame *g) {
  return SR_MAX(1500.0f, (float)g->city->grid * (float)SR_BLOCK_CM / 100.0f * 1.1f);
}

void sr_map_zoom(SrGame *g, float factor) {
  g->map.span = SR_CLAMP(g->map.span * factor, SR_MAP_MIN_SPAN, sr_map_max_span(g));
}

void sr_map_close(SrGame *g) {
  g->screen = SR_SCREEN_PLAY;
  g->clock.paused = false;
}

void sr_map_world_at(const SrMapView *v, float aspect, float nx, float ny, float *x, float *z) {
  *x = v->cx + (nx - 0.5f) * v->span * aspect;
  *z = v->cz + (ny - 0.5f) * v->span;
}

void sr_map_screen_of(const SrMapView *v, float aspect, float x, float z, float *nx, float *ny) {
  *nx = 0.5f + (x - v->cx) / (v->span * aspect);
  *ny = 0.5f + (z - v->cz) / v->span;
}

bool sr_map_shows_poi(int filter, int poi) {
  switch (filter) {
    case SR_MAPF_FOOD: return poi == SR_POI_FOOD || poi == SR_POI_BAR || poi == SR_POI_GROCERY;
    case SR_MAPF_SERVICES:
      return poi == SR_POI_CLINIC || poi == SR_POI_RIPPERDOC || poi == SR_POI_GUNSHOP || poi == SR_POI_ELECTRONICS ||
             poi == SR_POI_CLOTHING || poi == SR_POI_FENCE || poi == SR_POI_CAPSULE || poi == SR_POI_MATRIX_BOOTH ||
             poi == SR_POI_UV_BOOTH;
    case SR_MAPF_WORK: return poi == SR_POI_FIXER || poi == SR_POI_CORP_HQ || poi == SR_POI_MEDIA;
    case SR_MAPF_TRAVEL: return false; /* stations only */
    default: return poi != SR_POI_NONE;
  }
}

const char *sr_map_filter_name(int filter) {
  static const char *const n[SR_MAPF_COUNT] = {"ALL PLACES", "FOOD", "SERVICES", "WORK", "TRAVEL"};
  return (unsigned)filter < SR_MAPF_COUNT ? n[filter] : "?";
}

void sr_map_pick(SrGame *g, float aspect, float nx, float ny) {
  float x, z;
  sr_map_world_at(&g->map, aspect, nx, ny, &x, &z);
  const SrCity *c = g->city;
  float reach = g->map.span * 0.035f, best = reach;
  int bb = -1, bs = -1;
  for (int b = 0; b < c->building_count; b++) {
    const SrBuilding *bd = &c->buildings[b];
    bool shown = sr_map_shows_poi(g->map.filter, bd->poi) || b == g->player.home;
    if (!shown) continue;
    int32_t cx, cy;
    sr_building_center(bd, &cx, &cy);
    float d = dist_xz(sr_v3((float)cx / 100.0f, 0, (float)cy / 100.0f), sr_v3(x, 0, z));
    if (d < best) {
      best = d;
      bb = b;
    }
  }
  if (g->map.filter == SR_MAPF_ALL || g->map.filter == SR_MAPF_TRAVEL)
    for (int s = 0; s < c->station_count; s++) {
      float d = dist_xz(sr_station_entrance(g, s), sr_v3(x, 0, z));
      if (d < best) {
        best = d;
        bs = s;
        bb = -1;
      }
    }
  if (bs >= 0)
    sr_route_set_station(g, bs);
  else if (bb >= 0)
    sr_route_set_building(g, bb);
  else
    sr_route_set_point(g, x, z);
}

/* ---- help -------------------------------------------------------------------------------- */

static const char *const k_help_titles[SR_HELP_PAGES] = {"CONTROLS",      "GETTING AROUND",   "MAKING MONEY",
                                                         "STAYING ALIVE", "HEAT AND THE LAW", "STEALTH AND COMBAT",
                                                         "THE MATRIX",    "GUIDANCE SETTINGS"};

static const char *const k_help_kbm[2][15] = {
    {"WASD MOVE, MOUSE LOOK, SPACE JUMP, C CROUCH, SHIFT SPRINT", "E INTERACT: ENTER, TALK, BUY, USE",
     "LEFT MOUSE FIRE, RIGHT MOUSE AIM, R RELOAD", "Q GADGET, 1-4 QUICK SLOTS", "M CITY MAP, T TERMINAL, ESC PAUSE",
     "J JACK IN (AT A BOOTH OR YOUR DECK), V NETSIGHT", NULL},
    {"M OPENS THE CITY MAP. WASD PANS, SPACE AND C ZOOM, R CYCLES THE FILTER.",
     "E SETS A DESTINATION AT THE CURSOR; Q CLEARS THE ROUTE.",
     "THE ROUTE GLOWS ON THE PAVEMENT; A BEACON MARKS THE END.",
     "THE COMPASS SHOWS THE DESTINATION AS A DIAMOND AND THE DISTANCE.",
     "WITH A JOB ACTIVE, THE ROUTE FOLLOWS IT BY ITSELF.",
     "THE METRO IS FASTER FOR LONG TRIPS: STATIONS ARE ON THE MAP.", NULL}};
static const char *const k_help_touch[2][15] = {
    {"LEFT THUMB MOVES, RIGHT THUMB LOOKS.", "THE HAND BUTTON INTERACTS: ENTER, TALK, BUY, USE.",
     "FIRE, AIM, JUMP, CROUCH, RELOAD AND GADGET HAVE THEIR OWN BUTTONS.", "TOP RIGHT: PAUSE, TERMINAL, MAP.", NULL},
    {"THE MAP BUTTON OPENS THE CITY MAP. TAP A PLACE TO GO THERE.",
     "THE BUTTONS BELOW THE MAP ZOOM, FILTER AND CLEAR THE ROUTE.",
     "THE ROUTE GLOWS ON THE PAVEMENT; A BEACON MARKS THE END.",
     "THE COMPASS SHOWS THE DESTINATION AS A DIAMOND AND THE DISTANCE.",
     "WITH A JOB ACTIVE, THE ROUTE FOLLOWS IT BY ITSELF.", NULL}};
static const char *const k_help_rest[SR_HELP_PAGES - 2][15] = {
    {"FIXERS OFFER JOBS; SO DOES THE JOBS TAB OF YOUR TERMINAL.",
     "EACH JOB HAS APPROACHES: GHOST, FACE, RUNNER OR HAMMER.",
     "PAY RISES WITH YOUR STREET CRED; CORPORATIONS PAY BEST, AND REMEMBER.",
     "SELL WHAT YOU FIND AT A FENCE. THE MARKET TAB SHOWS THE CORPORATIONS' SHARES.", NULL},
    {"EAT WHEN HUNGRY: FOOD STALLS, DINERS, GROCERIES.", "SLEEP AT HOME OR IN A CAPSULE HOTEL.",
     "RENT IS DUE EVERY SUNDAY NIGHT. MISS IT TOO OFTEN AND YOU ARE EVICTED.",
     "HEALTH COMES BACK SLOWLY; MEDKITS AND CLINICS ARE FASTER.",
     "IF YOU FLATLINE YOU WAKE UP POORER, BUT YOUR STASH AT HOME IS SAFE.", NULL},
    {"CRIMES SEEN BY WITNESSES OR CAMERAS RAISE YOUR HEAT WITH THE POLICE OR A CORPORATION.",
     "HEAT 1-2: PATROLS LOOK FOR YOU. 3: THEY COME ARMED. 4-5: KILL TEAMS.",
     "LIE LOW, CHANGE CLOTHES OR HAIR, OR ERASE RECORDS IN THE MATRIX.", "HEAT FALLS WITH TIME.", NULL},
    {"CROUCH TO MOVE QUIETLY. DARK CORNERS HIDE YOU; NEON GIVES YOU AWAY.",
     "GUARDS: A YELLOW MARKER MEANS THEY SUSPECT, RED MEANS COMBAT.",
     "TAKEDOWNS FROM BEHIND ARE SILENT. NON-LETHAL TOOLS KEEP A JOB CLEAN.",
     "A RED EDGE ON THE SCREEN SHOWS WHERE A HIT CAME FROM.", NULL},
    {"JACK IN AT A MATRIX BOOTH OR YOUR HOME DECK.", "NODES HOLD DATA, CAMERAS, DOORS AND ALARMS: BREACH THEM.",
     "ICE PATROLS THE HOST. WATCHERS RAISE THE ALARM, KILLER ICE HURTS YOUR BODY.",
     "WHAT YOU OPEN IN THE MATRIX STAYS OPEN WHEN YOU WALK IN.", NULL},
    {"G   TIPS AND THE CURRENT GOAL ON OR OFF", "N   MINIMAP ON OR OFF", "P   ROUTE ON THE PAVEMENT ON OR OFF",
     "R   SHOW ALL TIPS AGAIN", "ESC BACK", NULL},
};

const char *sr_help_title(int page) { return (unsigned)page < SR_HELP_PAGES ? k_help_titles[page] : ""; }

const char *const *sr_help_lines(int page, bool touch) {
  static const char *const none[1] = {NULL};
  if ((unsigned)page >= SR_HELP_PAGES) return none;
  if (page < 2) return touch ? k_help_touch[page] : k_help_kbm[page];
  return k_help_rest[page - 2];
}

void sr_guide_toggle(SrGame *g, SrGuideSetting which) {
  SrSettings *st = &g->settings;
  switch (which) {
    case SR_GUIDE_SET_GUIDANCE:
      st->guidance = !st->guidance;
      if (!st->guidance) { /* nothing showing or waiting */
        g->guide.tip = -1;
        g->guide.nq = 0;
      }
      break;
    case SR_GUIDE_SET_MINIMAP: st->minimap = !st->minimap; break;
    case SR_GUIDE_SET_NAV_PATH: st->nav_path = !st->nav_path; break;
    default: sr_guide_reset(g); break;
  }
}

void sr_help_open(SrGame *g) {
  g->help_return = g->screen == SR_SCREEN_HELP ? g->help_return : (int)g->screen;
  g->screen = SR_SCREEN_HELP;
  g->clock.paused = true;
}

void sr_help_close(SrGame *g) {
  g->screen = (SrScreen)g->help_return;
  if (g->screen == SR_SCREEN_HELP || g->screen == SR_SCREEN_PLAY) {
    g->screen = SR_SCREEN_PLAY;
    g->clock.paused = false;
  }
}
