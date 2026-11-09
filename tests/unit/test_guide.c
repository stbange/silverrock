/* Guidance, navigation, the city map and help: routes and turn hints, following the job, tips,
 * the current goal, the map's view and picks, help pages, the settings, the HUD and the route in
 * the world. */
#include "game/sr_save.h"
#include "render/sr_render.h"
#include "ui/sr_ui.h"
#include "sr_test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SrGame *g_game;

static SrActionState idle(void) {
  SrActionState a;
  memset(&a, 0, sizeof a);
  return a;
}

/* A fresh street game: outside, no crowd, guidance on with no tips seen. */
static SrGame *street_game(void) {
  SrGame *g = g_game;
  sr_game_new(g, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Guide");
  g->crowd_enabled = false;
  sr_game_leave(g);
  g->settings.guidance = g->settings.minimap = g->settings.nav_path = true;
  sr_guide_reset(g);
  return g;
}

static int nearest_poi(const SrGame *g, int poi, float min_m) {
  const SrCity *c = g->city;
  int best = -1;
  float bd = 1e9f;
  for (int b = 0; b < c->building_count; b++) {
    if (c->buildings[b].poi != poi) continue;
    int32_t x, z;
    sr_building_center(&c->buildings[b], &x, &z);
    float d = hypotf((float)x / 100.0f - g->player.pos.x, (float)z / 100.0f - g->player.pos.z);
    if (d >= min_m && d < bd) bd = d, best = b;
  }
  return best;
}

static void steps(SrGame *g, int n) {
  SrActionState a = idle();
  for (int i = 0; i < n; i++) sr_game_step(g, &a);
}

/* ---- routes ------------------------------------------------------------------------------ */

static void test_routes(void) {
  SR_SPEC("navigation", "Route to a chosen place");
  SrGame *g = street_game();
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  SR_CHECK(sr_route_distance(g) == 0);
  char ins[64];
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(ins[0] == '\0');
  SR_CHECK(!sr_route_plan(g));
  int food = nearest_poi(g, SR_POI_FOOD, 150.0f);
  SR_CHECK(food >= 0);
  sr_route_set_building(g, food);
  SR_CHECK_EQ(g->route.kind, SR_DEST_BUILDING);
  SR_CHECK(g->route.n >= 3 && g->route.n <= SR_ROUTE_MAX);
  /* the route runs along the walking lines: longer than the straight line, not absurdly */
  float straight = hypotf(g->route.target.x - g->player.pos.x, g->route.target.z - g->player.pos.z);
  SR_CHECK(g->route.length >= straight - 1.0f && g->route.length < straight * 2.0f + 40.0f);
  SR_CHECK(fabsf(sr_route_distance(g) - g->route.length) < 0.01f);
  float sum = 0;
  for (int i = 1; i < g->route.n; i++)
    sum += hypotf(g->route.pts[i].x - g->route.pts[i - 1].x, g->route.pts[i].z - g->route.pts[i - 1].z);
  SR_CHECK(fabsf(sum - g->route.length) < 0.1f);
  /* corners only: no three points in a line */
  for (int i = 2; i < g->route.n - 1; i++) {
    SrVec3 a = g->route.pts[i - 2], b = g->route.pts[i - 1], c = g->route.pts[i];
    float cross = (b.x - a.x) * (c.z - b.z) - (b.z - a.z) * (c.x - b.x);
    SR_CHECK(fabsf(cross) >= 0.5f);
  }
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strstr(ins, "TURN") || strstr(ins, "CONTINUE"));
  SR_CHECK(g->route.label[0] != '\0');
  /* a marked point, a metro station, home */
  sr_route_set_point(g, g->player.pos.x + 60.0f, g->player.pos.z);
  SR_CHECK_EQ(g->route.kind, SR_DEST_POINT);
  SR_CHECK(strcmp(g->route.label, "MARKED POINT") == 0);
  sr_route_set_station(g, 0);
  SR_CHECK_EQ(g->route.kind, SR_DEST_STATION);
  SR_CHECK(strncmp(g->route.label, "METRO", 5) == 0);
  SrVec3 e = sr_station_entrance(g, 0);
  SR_CHECK(fabsf(g->route.target.x - e.x) < 0.01f && fabsf(g->route.target.z - e.z) < 0.01f);
  sr_route_set_building(g, g->player.home);
  SR_CHECK(strcmp(g->route.label, "HOME") == 0);
  /* out-of-range places change nothing */
  sr_route_set_building(g, -1);
  sr_route_set_building(g, g->city->building_count);
  sr_route_set_station(g, g->city->station_count);
  SR_CHECK_EQ(g->route.kind, SR_DEST_BUILDING);
  /* inside a site the route is a straight line to the target */
  g->site.active = true;
  SR_CHECK(sr_route_plan(g));
  SR_CHECK_EQ(g->route.n, 2);
  g->site.active = false;
  /* clearing forgets the destination */
  sr_route_clear(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  SR_CHECK_EQ(g->route.n, 0);
}

static void test_instructions(void) {
  SR_SPEC("navigation", "Turn hints");
  SrGame *g = street_game();
  SrRoute *r = &g->route;
  r->kind = SR_DEST_POINT;
  /* north, then east after 50 m: a right turn (facing -z, +x is to the right) */
  r->n = 3;
  r->pts[0] = sr_v3(0, 0, 0);
  r->pts[1] = sr_v3(0, 0, -50);
  r->pts[2] = sr_v3(30, 0, -50);
  r->length = 80;
  char ins[64];
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strcmp(ins, "TURN RIGHT IN 50 M") == 0);
  r->pts[2] = sr_v3(-30, 0, -50);
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strcmp(ins, "TURN LEFT IN 50 M") == 0);
  /* a turn close by */
  r->pts[1] = sr_v3(0, 0, -9);
  r->pts[2] = sr_v3(-40, 0, -9);
  r->length = 49;
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strcmp(ins, "TURN LEFT NOW") == 0);
  /* the step from a door onto the walking line is not a turn */
  r->pts[1] = sr_v3(0, 0, -4);
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strncmp(ins, "CONTINUE", 8) == 0);
  /* straight on, and the last stretch */
  r->n = 2;
  r->pts[1] = sr_v3(0, 0, -120);
  r->length = 120;
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strcmp(ins, "CONTINUE 120 M") == 0);
  r->length = 20;
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strcmp(ins, "DESTINATION AHEAD") == 0);
  /* a nearly straight bend is not a turn */
  r->n = 3;
  r->pts[1] = sr_v3(0, 0, -60);
  r->pts[2] = sr_v3(5, 0, -120);
  r->length = 120;
  sr_route_instruction(g, ins, sizeof ins);
  SR_CHECK(strncmp(ins, "CONTINUE", 8) == 0);
  sr_route_clear(g);
}

static void test_job_follow(void) {
  SR_SPEC("navigation", "Route follows the job");
  SrGame *g = street_game();
  int fixer = g->city->factions.fixer_building[g->city->blocks[g->city->buildings[g->player.home].block].district];
  SR_CHECK(sr_game_jobs_open(g, fixer));
  SR_CHECK_EQ(sr_game_shop_buy(g, 0), SR_OK);
  SR_CHECK(g->active_job >= 0);
  SrJob *j = sr_jobs_get(&g->jobs, g->active_job);
  sr_guide_step(g);
  SR_CHECK(g->route.automatic);
  SR_CHECK_EQ(g->route.building, j->target_building);
  SR_CHECK_EQ(g->route.job, j->id);
  char goal[160];
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strncmp(goal, "JOB: ", 5) == 0 && strstr(goal, " M)"));
  /* replanned once a second as the player moves */
  uint64_t planned = g->route.planned;
  g->steps += SR_ROUTE_REPLAN;
  sr_guide_step(g);
  SR_CHECK(g->route.planned > planned);
  /* a destination set by hand wins over the job */
  sr_route_set_point(g, g->player.pos.x + 40.0f, g->player.pos.z);
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_POINT);
  SR_CHECK(!g->route.automatic);
  /* clearing the job's route by hand: it is not followed again */
  sr_route_clear(g);
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_BUILDING);
  sr_route_clear(g);
  SR_CHECK_EQ(g->route.declined_job, j->id);
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  /* arriving: a message, and no new route to the same place while the job stays there */
  g->route.declined_job = -1;
  sr_guide_step(g);
  SR_CHECK(g->route.automatic);
  SR_SPEC("navigation", "Arrival");
  g->player.pos.x = g->route.target.x + 2.0f;
  g->player.pos.z = g->route.target.z;
  g->toast_time = 0;
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  SR_CHECK(g->toast_time > 0 && strncmp(g->toast, "ARRIVED", 7) == 0);
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  /* once the job is done it reports back elsewhere: the route follows */
  j->executed = true;
  sr_guide_step(g);
  if (j->client_building != j->target_building) {
    SR_CHECK(g->route.automatic);
    SR_CHECK_EQ(g->route.building, j->client_building);
    sr_guide_objective(g, goal, sizeof goal);
    SR_CHECK(strncmp(goal, "REPORT BACK", 11) == 0);
  }
  /* no job: the automatic route goes away */
  g->active_job = -1;
  sr_guide_step(g);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  /* during a dive only the tips run */
  g->dive.active = true;
  sr_guide_step(g);
  g->dive.active = false;
}

/* ---- tips and the goal ------------------------------------------------------------------- */

static void test_tips(void) {
  SR_SPEC("guidance", "Tips explain what happens");
  SrGame *g = street_game();
  SR_CHECK_EQ(g->guide.tip, -1);
  float alpha = 0;
  SR_CHECK(sr_guide_tip_text(g, false, &alpha) == NULL);
  g->steps = 30;
  sr_guide_step(g);
  SR_CHECK_EQ(g->guide.tip, SR_TIP_WELCOME);
  SR_CHECK(g->guide.nq >= 2); /* the map and help tips wait their turn */
  SR_CHECK(g->guide.seen & (1ull << SR_TIP_WELCOME));
  SR_CHECK(strtoull(g->settings.tips_seen, NULL, 16) & (1ull << SR_TIP_WELCOME));
  g->steps += 9; /* fading in */
  const char *txt = sr_guide_tip_text(g, false, &alpha);
  SR_CHECK(txt && strstr(txt, "WASD"));
  SR_CHECK(alpha > 0.4f && alpha < 0.6f);
  txt = sr_guide_tip_text(g, true, &alpha);
  SR_CHECK(txt && strstr(txt, "THUMB"));
  g->steps += 120;
  sr_guide_tip_text(g, false, &alpha);
  SR_CHECK(alpha >= 0.999f);
  /* shown once: asking again does nothing, even after a restart that kept the settings */
  SR_CHECK(!sr_guide_tip(g, SR_TIP_WELCOME));
  SR_CHECK(!sr_guide_tip(g, SR_TIP_MAP)); /* already waiting */
  SR_CHECK(!sr_guide_tip(g, SR_TIP_COUNT));
  sr_guide_init(&g->guide);
  SR_CHECK(!sr_guide_tip(g, SR_TIP_WELCOME));
  /* it goes after SR_TIP_SECONDS, the next one follows */
  g->steps = 30;
  sr_guide_step(g);
  int first = g->guide.tip;
  g->steps += (uint64_t)(SR_TIP_SECONDS * 60.0) + 1;
  sr_guide_step(g);
  SR_CHECK(g->guide.tip >= 0 && g->guide.tip != first);
  /* triggers: hunger, fatigue, hurt, heat, a long route, combat */
  sr_guide_reset(g);
  g->guide.tip = SR_TIP_WELCOME; /* hold the queue */
  g->guide.tip_t = sr_game_fx_now(g);
  g->player.satiety = 10;
  g->player.fatigue = 90;
  g->player.health = g->player.max_health * 0.2f;
  g->heat.level[0] = 2;
  g->combat_count = 1;
  g->steps = 600;
  sr_guide_step(g);
  bool hunger = false, fatigue = false, hurt = false, heat = false, combat = false;
  for (int i = 0; i < g->guide.nq; i++) {
    hunger |= g->guide.queue[i] == SR_TIP_HUNGER;
    fatigue |= g->guide.queue[i] == SR_TIP_FATIGUE;
    hurt |= g->guide.queue[i] == SR_TIP_HURT;
    heat |= g->guide.queue[i] == SR_TIP_HEAT;
    combat |= g->guide.queue[i] == SR_TIP_COMBAT;
  }
  SR_CHECK(hunger && fatigue && hurt && heat && combat);
  SR_CHECK(g->guide.nq <= 8);
  for (int t = 0; t < SR_TIP_COUNT; t++) {
    SR_CHECK(strcmp(sr_guide_tip_name((SrTip)t), "?") != 0);
    g->guide.tip = t;
    SR_CHECK(sr_guide_tip_text(g, false, NULL) != NULL && sr_guide_tip_text(g, true, NULL) != NULL);
  }
  SR_CHECK(strcmp(sr_guide_tip_name(SR_TIP_COUNT), "?") == 0);
  /* the goal follows the player's needs */
  SR_SPEC("guidance", "Current goal");
  char goal[160];
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strcmp(goal, "EAT SOMETHING") == 0);
  g->player.satiety = 80;
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strcmp(goal, "GET SOME SLEEP") == 0);
  g->player.fatigue = 10;
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strncmp(goal, "FIND WORK", 9) == 0);
  g->player.rent = 500;
  g->player.cred = 10;
  g->player.rent_due = sr_game_time(g) + 86400;
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strncmp(goal, "RENT DUE IN 24 H", 16) == 0);
  /* turned off: no tip, no goal, nothing queued; the job still shows plainly in the HUD */
  SR_SPEC("guidance", "Guidance can be turned off");
  sr_guide_toggle(g, SR_GUIDE_SET_GUIDANCE);
  SR_CHECK(!g->settings.guidance);
  SR_CHECK_EQ(g->guide.tip, -1);
  SR_CHECK_EQ(g->guide.nq, 0);
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(goal[0] == '\0');
  SR_CHECK(!sr_guide_tip(g, SR_TIP_RENT));
  g->guide.tip = SR_TIP_RENT;
  SR_CHECK(sr_guide_tip_text(g, false, NULL) == NULL);
  sr_guide_step(g);
  SR_CHECK_EQ(g->guide.tip, -1);
  sr_guide_toggle(g, SR_GUIDE_SET_GUIDANCE);
  SR_CHECK(g->settings.guidance);
  sr_guide_toggle(g, SR_GUIDE_SET_MINIMAP);
  sr_guide_toggle(g, SR_GUIDE_SET_NAV_PATH);
  SR_CHECK(!g->settings.minimap && !g->settings.nav_path);
  sr_guide_toggle(g, SR_GUIDE_SET_MINIMAP);
  sr_guide_toggle(g, SR_GUIDE_SET_NAV_PATH);
  /* reset: every tip shows again */
  g->guide.seen = 0xFF;
  sr_guide_toggle(g, SR_GUIDE_SET_RESET);
  SR_CHECK_EQ(g->guide.seen, 0);
  SR_CHECK(g->settings.tips_seen[0] == '\0');
  SR_CHECK(sr_guide_tip(g, SR_TIP_WELCOME));
}

/* ---- the city map ------------------------------------------------------------------------ */

static void test_map(void) {
  SR_SPEC("navigation", "City map");
  SrGame *g = street_game();
  int64_t t0 = sr_game_time(g);
  SrActionState a = idle();
  a.pressed[SR_ACT_MAP] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_MAP);
  SR_CHECK(g->clock.paused);
  SR_CHECK(fabsf(g->map.cx - g->player.pos.x) < 0.01f && fabsf(g->map.cz - g->player.pos.z) < 0.01f);
  steps(g, 30);
  SR_CHECK_EQ(sr_game_time(g), t0); /* time stands still on the map */
  /* the transforms are inverses */
  float x, z, nx, ny;
  sr_map_world_at(&g->map, 16.0f / 9.0f, 0.25f, 0.75f, &x, &z);
  sr_map_screen_of(&g->map, 16.0f / 9.0f, x, z, &nx, &ny);
  SR_CHECK(fabsf(nx - 0.25f) < 1e-4f && fabsf(ny - 0.75f) < 1e-4f);
  sr_map_world_at(&g->map, 16.0f / 9.0f, 0.5f, 0.5f, &x, &z);
  SR_CHECK(fabsf(x - g->map.cx) < 1e-3f && fabsf(z - g->map.cz) < 1e-3f);
  /* the stick pans, jump and crouch zoom within the limits */
  a = idle();
  a.move_y = 1;
  float cz = g->map.cz;
  sr_game_step(g, &a);
  SR_CHECK(g->map.cz < cz);
  a = idle();
  a.down[SR_ACT_JUMP] = true;
  for (int i = 0; i < 400; i++) sr_game_step(g, &a);
  SR_CHECK(fabsf(g->map.span - SR_MAP_MIN_SPAN) < 0.01f);
  a = idle();
  a.down[SR_ACT_CROUCH] = true;
  for (int i = 0; i < 400; i++) sr_game_step(g, &a);
  SR_CHECK(fabsf(g->map.span - sr_map_max_span(g)) < 0.01f);
  SR_CHECK(sr_map_max_span(g) >= 1500.0f);
  sr_map_zoom(g, 0.1f);
  SR_CHECK(g->map.span < sr_map_max_span(g));
  /* reload cycles the filter */
  for (int f = 0; f < SR_MAPF_COUNT; f++) SR_CHECK(strlen(sr_map_filter_name(f)) > 0);
  a = idle();
  a.pressed[SR_ACT_RELOAD] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->map.filter, SR_MAPF_FOOD);
  SR_CHECK(sr_map_shows_poi(SR_MAPF_FOOD, SR_POI_FOOD) && !sr_map_shows_poi(SR_MAPF_FOOD, SR_POI_FIXER));
  SR_CHECK(sr_map_shows_poi(SR_MAPF_WORK, SR_POI_FIXER) && sr_map_shows_poi(SR_MAPF_SERVICES, SR_POI_CLINIC));
  SR_CHECK(sr_map_shows_poi(SR_MAPF_ALL, SR_POI_FOOD) && !sr_map_shows_poi(SR_MAPF_ALL, SR_POI_NONE));
  /* a pick on a place of the filter sets it as the destination */
  int food = nearest_poi(g, SR_POI_FOOD, 100.0f);
  int32_t bx, bz;
  sr_building_center(&g->city->buildings[food], &bx, &bz);
  g->map.span = 400;
  sr_map_screen_of(&g->map, 16.0f / 9.0f, (float)bx / 100.0f + 3.0f, (float)bz / 100.0f, &nx, &ny);
  sr_map_pick(g, 16.0f / 9.0f, nx, ny);
  SR_CHECK_EQ(g->route.kind, SR_DEST_BUILDING);
  SR_CHECK_EQ(g->route.building, food);
  /* interact picks at the centre: centre the map on a station and it is chosen */
  SrVec3 st = sr_station_entrance(g, 0);
  g->map.filter = SR_MAPF_TRAVEL;
  g->map.cx = st.x;
  g->map.cz = st.z;
  a = idle();
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->route.kind, SR_DEST_STATION);
  /* nothing near: a marked point */
  g->map.filter = SR_MAPF_WORK;
  g->map.cx = -500;
  g->map.cz = -500;
  sr_map_pick(g, 16.0f / 9.0f, 0.5f, 0.5f);
  SR_CHECK_EQ(g->route.kind, SR_DEST_POINT);
  /* the gadget button clears, the map button closes and time runs again */
  a = idle();
  a.pressed[SR_ACT_GADGET] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  a = idle();
  a.pressed[SR_ACT_MAP] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  SR_CHECK(!g->clock.paused);
  sr_map_open(g);
  a = idle();
  a.pressed[SR_ACT_PAUSE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  sr_game_step(g, NULL);
  /* no map in the Matrix */
  g->dive.active = true;
  a = idle();
  a.pressed[SR_ACT_MAP] = true;
  sr_game_step(g, &a);
  SR_CHECK(g->screen != SR_SCREEN_MAP);
  g->dive.active = false;
}

/* ---- help -------------------------------------------------------------------------------- */

static void test_help(void) {
  SR_SPEC("guidance", "Help can be opened at any time");
  SrGame *g = street_game();
  for (int p = 0; p < SR_HELP_PAGES; p++) {
    SR_CHECK(strlen(sr_help_title(p)) > 0);
    for (int touch = 0; touch < 2; touch++) {
      const char *const *lines = sr_help_lines(p, touch != 0);
      int n = 0;
      while (lines[n]) {
        SR_CHECK(strlen(lines[n]) > 0);
        n++;
      }
      SR_CHECK(n >= 1 && n <= 14);
    }
  }
  SR_CHECK(strstr(sr_help_lines(0, false)[0], "WASD") != NULL);
  SR_CHECK(strstr(sr_help_lines(0, true)[0], "WASD") == NULL);
  /* from the pause screen and back */
  g->screen = SR_SCREEN_PAUSE;
  g->clock.paused = true;
  sr_help_open(g);
  SR_CHECK_EQ(g->screen, SR_SCREEN_HELP);
  sr_help_open(g); /* already open: still goes back to the pause screen */
  SrActionState a = idle();
  a.move_y = -1; /* down: the next page, once per push */
  sr_game_step(g, &a);
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->help_page, 1);
  a = idle();
  sr_game_step(g, &a);
  a.move_y = 1;
  sr_game_step(g, &a);
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->help_page, 0);
  a = idle();
  sr_game_step(g, &a);
  a.move_y = 1; /* up from the first page wraps to the last */
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->help_page, SR_HELP_PAGES - 1);
  a = idle();
  a.pressed[SR_ACT_PAUSE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PAUSE);
  /* from play it goes back to play, and the clock runs */
  g->screen = SR_SCREEN_PLAY;
  g->clock.paused = false;
  sr_help_open(g);
  SR_CHECK(g->clock.paused);
  sr_help_close(g);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  SR_CHECK(!g->clock.paused);
  sr_game_step(g, NULL);
  g->screen = SR_SCREEN_HELP;
  sr_game_step(g, NULL);
  g->screen = SR_SCREEN_PLAY;
}

/* ---- settings and saves ------------------------------------------------------------------ */

static void test_guide_settings(void) {
  SR_SPEC("guidance", "Guidance settings persist");
  SrSettings s, back;
  sr_settings_default(&s);
  SR_CHECK(s.guidance && s.minimap && s.nav_path);
  SR_CHECK(s.tips_seen[0] == '\0');
  s.guidance = false;
  s.nav_path = false;
  snprintf(s.tips_seen, sizeof s.tips_seen, "%llx", 0x1abcdull);
  char text[4096];
  sr_settings_write(&s, text, sizeof text);
  SR_CHECK(strstr(text, "guidance=0") && strstr(text, "minimap=1") && strstr(text, "tips_seen=1abcd"));
  sr_settings_default(&back);
  SR_CHECK(sr_settings_parse(&back, text) > 0);
  SR_CHECK(!back.guidance && back.minimap && !back.nav_path);
  SR_CHECK(strcmp(back.tips_seen, "1abcd") == 0);
  /* a value longer than its field is cut to fit, never written past it */
  char big[1200];
  memset(big, 0, sizeof big);
  strcpy(big, "tips_seen=");
  memset(big + 10, 'f', 600);
  strcat(big, "\nvolume=33\n");
  sr_settings_parse(&back, big);
  SR_CHECK_EQ(strlen(back.tips_seen), sizeof back.tips_seen - 1);
  SR_CHECK_EQ(back.volume, 33);
  /* the route and the tips are not in the save; loading starts with no destination */
  SrGame *g = street_game();
  sr_route_set_building(g, nearest_poi(g, SR_POI_FOOD, 50.0f));
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, SR_SAVE_VERSION, 1), SR_OK);
  SR_CHECK_EQ(sr_save_read(g, w.data, w.len), SR_OK);
  SR_CHECK_EQ(g->route.kind, SR_DEST_NONE);
  sr_writer_free(&w);
}

/* ---- the HUD, map and help screens ------------------------------------------------------- */

static void test_nav_ui(void) {
  SR_SPEC("navigation", "Minimap and compass");
  SrGame *g = street_game();
  sr_route_set_building(g, nearest_poi(g, SR_POI_FOOD, 150.0f));
  g->guide.tip = SR_TIP_ROUTE;
  g->guide.tip_t = sr_game_fx_now(g) - 1.0;
  SrUi ui;
  sr_ui_init(&ui);
  SrInput in;
  memset(&in, 0, sizeof in);
  int base_nv;
  /* the HUD with the minimap, compass marker, goal and tip; then without guidance */
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  int with = ui.mesh.nv;
  g->settings.minimap = false;
  g->settings.guidance = false;
  g->active_job = -1;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  base_nv = ui.mesh.nv;
  SR_CHECK(with > base_nv + 100);
  g->settings.minimap = g->settings.guidance = true;
  in.scheme = SR_SCHEME_TOUCH;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  SR_CHECK(ui.mesh.nv > base_nv);
  in.scheme = SR_SCHEME_KBM;
  /* the map at three zooms and every filter; touch adds buttons */
  sr_map_open(g);
  for (int zoom = 0; zoom < 3; zoom++) {
    g->map.span = zoom == 0 ? 300.0f : zoom == 1 ? 1000.0f : sr_map_max_span(g);
    for (int f = 0; f < SR_MAPF_COUNT; f++) {
      g->map.filter = f;
      sr_ui_begin(&ui, 1280, 720, 2);
      sr_ui_build(&ui, g, &in, false);
      SR_CHECK(ui.mesh.nv > 400);
    }
  }
  in.scheme = SR_SCHEME_TOUCH;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  SR_CHECK_EQ(sr_ui_map_button_at(&ui, 60, 700), 1);
  SR_CHECK_EQ(sr_ui_map_button_at(&ui, 1220, 700), 5);
  SR_CHECK_EQ(sr_ui_map_button_at(&ui, 640, 300), 0);
  in.scheme = SR_SCHEME_KBM;
  sr_map_close(g);
  /* help: every page; on the settings page the rows sit under their states */
  sr_help_open(g);
  for (int p = 0; p < SR_HELP_PAGES; p++) {
    g->help_page = p;
    sr_ui_begin(&ui, 1280, 720, 2);
    sr_ui_build(&ui, g, &in, false);
    SR_CHECK(ui.mesh.nv > 100);
  }
  SR_CHECK_EQ(sr_ui_help_row_at(&ui, 720 * 0.18f + 720 * 0.08f * 3 + 5), 3);
  SR_CHECK_EQ(sr_ui_help_row_at(&ui, 20), -1);
  SR_CHECK_EQ(sr_ui_help_row_at(&ui, 715), -1);
  int found[3] = {0, 0, 0};
  for (int y = 0; y < 720; y++) {
    int r = sr_ui_help_setting_at(&ui, g, false, (float)y);
    if (r >= 0) found[r]++;
  }
  SR_CHECK(found[0] > 0 && found[1] > 0 && found[2] > 0);
  g->help_page = 0;
  SR_CHECK_EQ(sr_ui_help_setting_at(&ui, g, false, 400), -1);
  sr_help_close(g);
  /* the pause screen points to help */
  g->screen = SR_SCREEN_PAUSE;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  g->screen = SR_SCREEN_PLAY;
  /* inside a site: no minimap, the compass still points */
  g->site.active = true;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  g->site.active = false;
  sr_ui_free(&ui);
}

/* ---- the route in the world -------------------------------------------------------------- */

static void test_route_ar(void) {
  SR_SPEC("navigation", "Route on the pavement");
  SrRoute r;
  memset(&r, 0, sizeof r);
  SrMesh chev, beacon;
  sr_mesh_init(&chev);
  sr_mesh_init(&beacon);
  SR_CHECK_EQ(sr_build_route_ar(&r, sr_v3(0, 1.7f, 0), 0, &chev, &beacon), 0);
  r.kind = SR_DEST_POINT;
  r.n = 3;
  r.pts[0] = sr_v3(0, 0, 0);
  r.pts[1] = sr_v3(0, 0, 0); /* a repeated point is skipped */
  r.pts[2] = sr_v3(0, 0, -200);
  r.target = r.pts[2];
  r.length = 200;
  int n = sr_build_route_ar(&r, sr_v3(0, 1.7f, 0), 1.0, &chev, &beacon);
  /* chevrons every 2.5 m from 3 m to 60 m, plus the beacon */
  SR_CHECK(n >= 22 && n <= 25);
  SR_CHECK(chev.nv == (n - 1) * 16);
  SR_CHECK(beacon.nv > 0);
  for (int i = 0; i < chev.nv; i++) { /* flat on the ground, along the route, within reach */
    SR_CHECK(fabsf(chev.v[i].y - 0.14f) < 1e-4f);
    SR_CHECK(chev.v[i].z < 0 && chev.v[i].z > -62.0f);
  }
  /* the pulse runs along: the brightest chevron moves forward with time */
  int brightest_a = 0, brightest_b = 0, best = -1;
  for (int c = 0; c < n - 1; c++)
    if (chev.v[c * 16].b > best) best = chev.v[c * 16].b, brightest_a = c;
  sr_mesh_clear(&chev);
  sr_build_route_ar(&r, sr_v3(0, 1.7f, 0), 1.25, &chev, NULL);
  best = -1;
  for (int c = 0; c < n - 1; c++)
    if (chev.v[c * 16].b > best) best = chev.v[c * 16].b, brightest_b = c;
  SR_CHECK(brightest_b != brightest_a);
  /* only the beacon; standing on the destination: none */
  sr_mesh_clear(&beacon);
  SR_CHECK_EQ(sr_build_route_ar(&r, sr_v3(0, 1.7f, 0), 0, NULL, &beacon), 1);
  SR_CHECK_EQ(sr_build_route_ar(&r, sr_v3(0, 1.7f, -199), 0, NULL, &beacon), 0);
  sr_mesh_free(&chev);
  sr_mesh_free(&beacon);
  /* in the scene: drawn when the setting is on, not when it is off */
  SrGame *g = street_game();
  sr_route_set_building(g, nearest_poi(g, SR_POI_FOOD, 150.0f));
  SrSceneCache *sc = calloc(1, sizeof *sc);
  sr_scene_init(sc, false);
  SrFrame *f = calloc(1, sizeof *f);
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SR_CHECK(sc->route_drawn > 0);
  g->settings.nav_path = false;
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SR_CHECK_EQ(sc->route_drawn, 0);
  sr_scene_free(sc);
  free(sc);
  free(f);
}

void suite_guide(void) {
  g_game = sr_game_create();
  SR_RUN(test_routes);
  SR_RUN(test_instructions);
  SR_RUN(test_job_follow);
  SR_RUN(test_tips);
  SR_RUN(test_map);
  SR_RUN(test_help);
  SR_RUN(test_guide_settings);
  SR_RUN(test_nav_ui);
  SR_RUN(test_route_ar);
  sr_game_destroy(g_game);
}
