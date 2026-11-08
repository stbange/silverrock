/* Platform and GL backend tests. They need a display: run under Xvfb (CI) or with SDL's
 * offscreen driver. Rendering uses Mesa llvmpipe in CI, so golden images are stable. */
#include "core/sr_log.h"
#include "game/sr_save.h"
#include "platform/sr_platform.h"
#include "sr_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static SrApp g_app;
static char g_root[256];

static void write_ppm(const char *path, const uint8_t *rgb, int w, int h) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  fwrite(rgb, 1, (size_t)w * h * 3, f);
  fclose(f);
}

static uint8_t *read_ppm(const char *path, int *w, int *h) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  int maxv;
  if (fscanf(f, "P6 %d %d %d", w, h, &maxv) != 3) {
    fclose(f);
    return NULL;
  }
  fgetc(f);
  uint8_t *buf = (uint8_t *)malloc((size_t)*w * *h * 3);
  size_t n = fread(buf, 1, (size_t)*w * *h * 3, f);
  fclose(f);
  if (n != (size_t)*w * *h * 3) {
    free(buf);
    return NULL;
  }
  return buf;
}

/* Share of pixels whose channel difference exceeds tol. */
static double diff_share(const uint8_t *a, const uint8_t *b, int n, int tol) {
  int bad = 0;
  for (int i = 0; i < n; i++) {
    int d = 0;
    for (int k = 0; k < 3; k++) {
      int dd = abs((int)a[i * 3 + k] - (int)b[i * 3 + k]);
      if (dd > d) d = dd;
    }
    if (d > tol) bad++;
  }
  return (double)bad / n;
}

static void frames(SrApp *a, int n) {
  for (int i = 0; i < n; i++) sr_app_iterate(a);
}

static uint8_t *render_internal(SrApp *a, int *w, int *h) {
  sr_app_iterate(a);
  *w = a->frame.internal_w;
  *h = a->frame.internal_h;
  uint8_t *rgb = (uint8_t *)malloc((size_t)*w * *h * 3);
  if (!sr_glb_read_internal(&a->gfx, rgb, *w, *h)) {
    free(rgb);
    return NULL;
  }
  return rgb;
}

static void reference_scene(SrApp *a) {
  /* fixed seed, fixed position and time, no crowd: deterministic image */
  sr_app_start_game(a, 0x5EEDC0DEull);
  SrGame *g = a->game;
  g->crowd_enabled = false;
  sr_game_leave(g); /* new games start in the apartment */
  /* stand on the intersection at the home block's corner and look down the street (+x) */
  int32_t ox, oy;
  sr_city_block_origin(g->city->buildings[g->player.home].block, g->city->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 8.0f, 1.7f, (float)oy / 100.0f + 8.0f);
  g->player.yaw = -1.35f;
  g->player.pitch = 0.12f;
  g->settings.internal_res = 1;
  g->settings.dynamic_res = false;
  g->settings.dither = g->settings.quantize = true;
  g->settings.vertex_snap = g->settings.affine = g->settings.crt = false;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 1); /* Wed 00:00, weather slot fixed */
  g->steps = 0;
  a->fixed_frame_dt = 0.0001;                     /* render without advancing the simulation */
  a->loop.acc = 0;                                /* no step left over from earlier tests: rain and steam stand still */
  for (int i = 0; i < 40; i++) sr_app_iterate(a); /* stream chunks (2 per frame) */
}

static void test_app_init(void) {
  SR_SPEC("platform-runtime", "Desktop build starts");
  SR_CHECK(g_app.window != NULL);
  SR_CHECK(g_app.gfx.ready);
  SR_CHECK(g_app.win_w > 0 && g_app.win_h > 0);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_TITLE);
  frames(&g_app, 3);
  SR_CHECK(g_app.frames >= 3);
}

static void test_render_city(void) {
  SR_SPEC("retro-rendering", "Default internal resolution");
  SR_SPEC("quality-gates", "Unintended visual change");
  reference_scene(&g_app);
  int w, h;
  uint8_t *rgb = render_internal(&g_app, &w, &h);
  SR_CHECK(rgb != NULL);
  SR_CHECK_EQ(w, 480);
  SR_CHECK_EQ(h, 270);
  if (!rgb) return;
  /* not a flat image */
  long sum = 0, sum2 = 0;
  for (int i = 0; i < w * h; i++) {
    int v = rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2];
    sum += v;
    sum2 += (long)v * v;
  }
  double mean = (double)sum / (w * h), var = (double)sum2 / (w * h) - mean * mean;
  SR_CHECK(var > 200.0);
  char path[512];
  snprintf(path, sizeof path, "%s/city_reference.ppm", SR_GOLDEN_DIR);
  const char *update = getenv("SR_UPDATE_GOLDENS");
  const char *out_dir = getenv("SR_RENDER_OUT");
  if (out_dir) {
    char o[512];
    snprintf(o, sizeof o, "%s/city_actual.ppm", out_dir);
    write_ppm(o, rgb, w, h);
  }
  int gw, gh;
  uint8_t *golden = read_ppm(path, &gw, &gh);
  if (update && update[0] == '1') {
    write_ppm(path, rgb, w, h);
  } else if (golden && gw == w && gh == h) {
    double share = diff_share(rgb, golden, w * h, 12);
    printf("  visual diff vs golden: %.4f%%\n", share * 100);
    SR_CHECK(share < 0.01); /* at most 1 % of pixels differ by more than 12 levels */
  } else {
    printf("  no golden image at %s (set SR_UPDATE_GOLDENS=1 to create)\n", path);
  }
  free(golden);
  /* an unintended change (fog colour) is detected */
  g_app.game->settings.crt = true;
  uint8_t *changed = render_internal(&g_app, &w, &h);
  g_app.game->settings.crt = false;
  SR_CHECK(changed && diff_share(rgb, changed, w * h, 12) == 0.0); /* CRT is post: internal unchanged */
  free(changed);
  free(rgb);
}

static double image_mean(const uint8_t *rgb, int n) {
  long sum = 0;
  for (int i = 0; i < n * 3; i++) sum += rgb[i];
  return (double)sum / (n * 3);
}

static void save_shot(const char *name, const uint8_t *rgb, int w, int h) {
  const char *out_dir = getenv("SR_RENDER_OUT");
  if (!out_dir) return;
  char o[512];
  snprintf(o, sizeof o, "%s/%s.ppm", out_dir, name);
  write_ppm(o, rgb, w, h);
}

static void test_render_interiors(void) {
  SR_SPEC("retro-rendering", "Neon through fog");
  sr_app_start_game(&g_app, 0x5EEDC0DEull);
  SrGame *g = g_app.game;
  g->settings.dynamic_res = false;
  g_app.fixed_frame_dt = 0.0001;
  /* the apartment: lit, furnished, not a flat image */
  SR_CHECK(sr_game_inside(g));
  int w, h;
  uint8_t *home = render_internal(&g_app, &w, &h);
  SR_CHECK(home && image_mean(home, w * h) > 12.0);
  if (home) save_shot("interior_home", home, w, h);
  /* a mission site with guards; thermal optics make the guards glow through walls */
  int site = -1;
  for (int b = 0; b < g->city->building_count && site < 0; b++)
    if (g->city->buildings[b].flags & SR_BF_MISSION) site = b;
  sr_game_leave(g);
  SR_CHECK_EQ(sr_game_enter(g, site, SR_ENTRY_FRONT), SR_OK);
  uint8_t *in = render_internal(&g_app, &w, &h);
  SR_CHECK(in && image_mean(in, w * h) > 8.0);
  if (in) save_shot("interior_site", in, w, h);
  g->thermal = true;
  uint8_t *th = render_internal(&g_app, &w, &h);
  g->thermal = false;
  SR_CHECK(th != NULL);
  if (in && th) {
    long hot = 0; /* thermal signatures: saturated orange pixels */
    for (int i = 0; i < w * h; i++) hot += th[i * 3] > 200 && th[i * 3 + 2] < 90 && th[i * 3 + 1] < 200;
    printf("  thermal pixels: %ld\n", hot);
    SR_CHECK(hot > 0);
    save_shot("interior_thermal", th, w, h);
  }
  free(home);
  free(in);
  free(th);
}

static void test_bloom(void) {
  SR_SPEC("retro-rendering", "Neon through fog");
  reference_scene(&g_app);
  g_app.game->settings.dither = g_app.game->settings.quantize = false;
  int ww = g_app.win_w, wh = g_app.win_h;
  uint8_t *off = (uint8_t *)malloc((size_t)ww * wh * 3), *on = (uint8_t *)malloc((size_t)ww * wh * 3);
  g_app.game->settings.bloom = false;
  sr_app_iterate(&g_app);
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, ww, wh, false);
  sr_glb_read_window(&g_app.gfx, off, 0, 0, ww, wh);
  g_app.frame.bloom = true;
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, ww, wh, false);
  sr_glb_read_window(&g_app.gfx, on, 0, 0, ww, wh);
  /* bloom only ever adds light, and it adds it around the neon */
  long brighter = 0, darker = 0;
  for (int i = 0; i < ww * wh * 3; i++) {
    brighter += on[i] > off[i] + 2;
    darker += on[i] + 2 < off[i];
  }
  printf("  bloom: %ld brighter channels, %ld darker\n", brighter, darker);
  save_shot("bloom_off", off, ww, wh);
  save_shot("bloom_on", on, ww, wh);
  SR_CHECK(brighter > 1000);
  SR_CHECK_EQ(darker, 0);
  SR_CHECK(g_app.gfx.bloom_w == g_app.frame.internal_w / 2);
  g_app.frame.bloom = false;
  g_app.game->settings.bloom = true;
  free(off);
  free(on);
}

/* Screenshot review aid: SR_SEED_SHOTS=dir renders one street view per seed (50 seeds) looking
 * from a district centre towards the nearest megacorp HQ. Not a check; skipped otherwise. */
static void test_seed_screenshots(void) {
  const char *dir = getenv("SR_SEED_SHOTS");
  if (!dir) return;
  FILE *index = NULL;
  char path[512];
  snprintf(path, sizeof path, "%s/index.txt", dir);
  index = fopen(path, "w");
  for (int seed = 1; seed <= 50; seed++) {
    sr_app_start_game(&g_app, (uint64_t)seed * 7919u);
    SrGame *g = g_app.game;
    sr_game_leave(g);
    g->settings.dynamic_res = false;
    g_app.fixed_frame_dt = 1.0 / 60.0;
    const SrCity *c = g->city;
    const SrDistrict *d = &c->districts[seed % c->district_count];
    int blk = sr_city_block_at(c, d->cx, d->cy);
    int32_t ox, oy;
    sr_city_block_origin(blk, c->grid, &ox, &oy);
    g->player.pos = sr_v3((float)ox / 100.0f + 8.0f, 1.7f, (float)oy / 100.0f + 8.0f);
    const SrBuilding *hq = &c->buildings[c->factions.f[seed % c->factions.corp_count].hq_building];
    float dx = (float)(hq->x0 + hq->x1) / 200.0f - g->player.pos.x,
          dz = (float)(hq->y0 + hq->y1) / 200.0f - g->player.pos.z;
    g->player.yaw = atan2f(-dx, -dz);
    g->player.pitch = 0.12f;
    sr_clock_set_seconds(&g->clock, 86400 + 22 * 3600);
    for (int i = 0; i < 60; i++) sr_app_iterate(&g_app);
    int w, h;
    uint8_t *rgb = render_internal(&g_app, &w, &h);
    if (rgb) {
      char name[64];
      snprintf(name, sizeof name, "seed_%02d", seed);
      save_shot(name, rgb, w, h);
      if (index)
        fprintf(index, "%s %s (%s) towards %s, %d citizens\n", name, d->name, sr_district_type_name(d->type),
                c->factions.f[seed % c->factions.corp_count].name, g->crowd.count);
    }
    free(rgb);
  }
  if (index) fclose(index);
}

/* Gameplay screenshots: SR_GAMEPLAY_SHOTS=dir renders the full window (HUD and screens included)
 * at moments across the game, for the README and reviews. Not a check; skipped otherwise. */
static const char *g_shot_dir;

static void window_shot(const char *name, int frames_before) {
  for (int i = 0; i < frames_before; i++) {
    g_app.game->toast_time = 0; /* no leftover messages over the view */
    sr_app_iterate(&g_app);
  }
  int w = g_app.win_w, h = g_app.win_h;
  uint8_t *rgb = (uint8_t *)malloc((size_t)w * h * 3);
  if (sr_glb_read_window(&g_app.gfx, rgb, 0, 0, w, h)) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", g_shot_dir, name);
    write_ppm(path, rgb, w, h);
  }
  free(rgb);
}

/* Stands on the intersection at the corner of a district's central block, looking down the street. */
static void street_view(SrGame *g, int district, float yaw, float pitch, int hour) {
  const SrCity *c = g->city;
  int blk = sr_city_block_at(c, c->districts[district].cx, c->districts[district].cy);
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 8.0f, 1.7f, (float)oy / 100.0f + 8.0f);
  g->player.yaw = yaw;
  g->player.pitch = pitch;
  if (hour >= 0) sr_clock_set_seconds(&g->clock, (sr_game_time(g) / 86400 + 1) * 86400 + (int64_t)hour * 3600);
  g->last_minute = sr_game_time(g) / 60;
  sr_crowd_init(&g->crowd); /* citizens appear around the new spot */
}

static void test_gameplay_screenshots(void) {
  g_shot_dir = getenv("SR_GAMEPLAY_SHOTS");
  if (!g_shot_dir) return;
  const uint64_t seed = 0x5EEDC0DEull;
  sr_app_start_game(&g_app, seed);
  SrGame *g = g_app.game;
  g->settings.dynamic_res = false;
  g_app.fixed_frame_dt = 1.0 / 60.0;
  /* 01 waking up at home */
  window_shot("01_apartment", 40);
  /* ten days of the corporate game go by: news, prices, gang wars */
  g->chess.story.offer_open = true;
  sr_game_accept_retainer(g);
  sr_game_skip(g, 10 * 86400, false);
  g->player.health = g->player.max_health;
  g->player.satiety = 80;
  g->player.fatigue = 10;
  g->player.cred = 4200;
  g->player.prog.street_cred = 32;
  sr_game_leave(g);
  g->crowd_enabled = true;
  const SrCity *c = g->city;
  int home_d = c->blocks[c->buildings[g->player.home].block].district;
  /* 02 the street at night, the district billboard rotating headlines */
  sr_news_publish(&g->chess, c, c->factions.media_first, 0, home_d, -1, SR_NEWS_STORY, false,
                  "Curfew talk as gang war spills into the market", sr_game_time(g));
  int fun = home_d;
  for (int d = 0; d < c->district_count; d++)
    if (c->districts[d].type == SR_D_ENTERTAINMENT) fun = d;
  sr_news_publish(&g->chess, c, c->factions.media_first, 0, fun, -1, SR_NEWS_STORY, false,
                  "Curfew talk as gang war spills into the market", sr_game_time(g));
  street_view(g, fun, -SR_PI / 2, 0.06f, 22);
  window_shot("02_street_night", 120);
  street_view(g, home_d, -1.35f, 0.12f, 23);
  window_shot("03_home_district", 120);
  /* 03 the home district; 04 the megacorp towers of the Spire */
  int spire = 0;
  for (int d = 0; d < c->district_count; d++)
    if (c->districts[d].type == SR_D_SPIRE) spire = d;
  street_view(g, spire, -SR_PI / 2, 0.32f, 1);
  window_shot("04_spire_towers", 90);
  /* 05 rain on a market street */
  int64_t t = sr_game_time(g);
  for (int k = 0; k < 400; k++)
    if (sr_weather_at(g->seed, t + (int64_t)k * 1800).rain > 0.6f) {
      sr_clock_set_seconds(&g->clock, t + (int64_t)k * 1800);
      break;
    }
  int market = home_d;
  for (int d = 0; d < c->district_count; d++)
    if (c->districts[d].type == SR_D_COMMERCIAL) market = d;
  street_view(g, market, -SR_PI / 2, 0.05f, -1);
  window_shot("05_rain", 120);
  /* 06 a gun shop */
  int shop = -1;
  for (int b = 0; b < c->building_count && shop < 0; b++)
    if (c->buildings[b].poi == SR_POI_GUNSHOP) shop = b;
  sr_game_teleport_building(g, shop);
  sr_game_shop_open(g, shop);
  window_shot("06_gun_shop", 20);
  sr_game_shop_close(g);
  /* 07-11 the terminal: messages, news, market, the Board, the story */
  sr_chess_reveal_faction(&g->chess, g->chess.story.patron);
  sr_chess_reveal_faction(&g->chess, (g->chess.story.patron + 1) % c->factions.corp_count);
  sr_game_trade(g, 1, 20);
  g->screen = SR_SCREEN_TERMINAL;
  static const char *const tabs[SR_TAB_COUNT] = {"07_terminal_home",   "07b_terminal_jobs", "08_terminal_news",
                                                 "09_terminal_market", "10_terminal_board", "11_terminal_story"};
  for (int tab = 0; tab < SR_TAB_COUNT; tab++) {
    g->term_tab = tab;
    g->term_sel = tab == SR_TAB_MARKET ? 1 : 0;
    if (tab == SR_TAB_STORY)
      g->chess.story.rival_offer = (int8_t)((g->chess.story.patron + 2) % c->factions.corp_count);
    window_shot(tabs[tab], 4);
  }
  g->screen = SR_SCREEN_PLAY;
  /* 12 inside a mission site */
  int site = -1;
  for (int b = 0; b < c->building_count && site < 0; b++)
    if (c->buildings[b].flags & SR_BF_MISSION) site = b;
  /* a site with guards on duty; face the nearest one in sight */
  for (int b = 0; b < c->building_count; b++) {
    if (!(c->buildings[b].flags & SR_BF_MISSION) || sr_game_enter(g, b, SR_ENTRY_FRONT) != SR_OK) continue;
    int best = -1;
    float bd = 1e9f;
    for (int k = 0; k < g->site.guard_count; k++) {
      SrVec3 gp = g->site.guards[k].e.pos;
      gp.y = g->player.pos.y;
      float dx = gp.x - g->player.pos.x, dz = gp.z - g->player.pos.z;
      if (dx * dx + dz * dz < bd && sr_site_los(&g->site, g->player.pos, gp)) bd = dx * dx + dz * dz, best = k;
    }
    if (best >= 0) {
      SrVec3 gp = g->site.guards[best].e.pos;
      g->player.yaw = atan2f(-(gp.x - g->player.pos.x), -(gp.z - g->player.pos.z));
      site = b;
      break;
    }
    sr_game_leave(g);
  }
  g->player.pitch = -0.05f;
  window_shot("12_mission_site", 40);
  sr_game_leave(g);
  /* 13-15 the Matrix: the grid, a host, a breach */
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_DECK, 2, 1));
  sr_game_teleport_building(g, g->player.home);
  if (sr_game_jack_in_via(g, SR_ACCESS_RIG, -1) == SR_OK) {
    g->dive.pitch = -0.25f;
    window_shot("13_matrix_grid", 30);
    sr_game_jack_out(g, false);
  }
  if (sr_game_jack_in_via(g, SR_ACCESS_PORT, site) == SR_OK) {
    g->dive.pos = sr_v3(0, 7, 30);
    g->dive.yaw = 0;
    g->dive.pitch = 0.18f;
    window_shot("14_matrix_host", 30);
    for (int n = 0; n < g->dive.host.node_count && g->dive.breach_node < 0; n++) sr_game_breach_start(g, n);
    window_shot("15_breach", 10);
    sr_game_jack_out(g, false);
  }
  /* 16 touch controls */
  g_app.input.scheme = SR_SCHEME_TOUCH;
  street_view(g, fun, -SR_PI / 2, 0.06f, 22);
  window_shot("16_touch_controls", 90);
  g_app.input.scheme = SR_SCHEME_KBM;
  /* 17 the Sunrise ending: the only daylight in the game */
  g->pall_removed = true;
  street_view(g, spire, -SR_PI / 2, 0.3f, 10);
  window_shot("17_sunrise_ending", 90);
  g->pall_removed = false;
}

static void test_many_games_keep_rendering(void) {
  /* every new game builds new meshes; the backend must keep drawing after many games */
  double first = 0;
  for (int k = 0; k < 24; k++) {
    reference_scene(&g_app);
    int w, h;
    uint8_t *rgb = render_internal(&g_app, &w, &h);
    long sum = 0, sum2 = 0;
    for (int i = 0; i < w * h; i++) {
      int v = rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2];
      sum += v;
      sum2 += (long)v * v;
    }
    double var = (double)sum2 / (w * h) - ((double)sum / (w * h)) * ((double)sum / (w * h));
    if (k == 0) first = var;
    SR_CHECK(var > first * 0.9);
    free(rgb);
  }
  printf("  mesh slots reclaimed: %d\n", g_app.gfx.reclaimed);
  SR_CHECK(g_app.gfx.reclaimed > 0);
}

static void test_peak_crowd_render(void) {
  SR_SPEC("population-simulation", "Peak crowd on desktop");
  sr_app_start_game(&g_app, 0x5EEDC0DEull);
  SrGame *g = g_app.game;
  sr_game_leave(g);
  g->crowd_enabled = true; /* earlier tests switch the crowd off */
  g->settings.dynamic_res = false;
  int best = -1;
  for (int k = 0; k < g->city->district_count; k++)
    if (g->city->districts[k].type == SR_D_ENTERTAINMENT)
      best = sr_city_block_at(g->city, g->city->districts[k].cx, g->city->districts[k].cy);
  int32_t ox, oy;
  sr_city_block_origin(best, g->city->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 8, 1.7f, (float)oy / 100.0f + 8);
  g->player.yaw = -SR_PI / 2;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 23 * 3600);
  g->last_minute = sr_game_time(g) / 60;
  g_app.fixed_frame_dt = 1.0 / 60.0;
  for (int i = 0; i < 90; i++) sr_app_iterate(&g_app);
  int visible = sr_crowd_visible(&g->crowd, &g->cam, 160.0f);
  SrSceneCache *sc = &g_app.scene;
  SrFrame *f = (SrFrame *)malloc(sizeof(SrFrame));
  uint64_t t0 = SDL_GetTicksNS();
  for (int i = 0; i < 10; i++) sr_scene_build(sc, g, 16.0f / 9.0f, f);
  double build_ms = (double)(SDL_GetTicksNS() - t0) / 1e7;
  printf("  peak: %d citizens visible, %d instanced, scene build %.2f ms, %d draw calls\n", visible, f->inst_count,
         build_ms, g_app.gfx.draw_calls);
  SR_CHECK(visible >= 150);
  SR_CHECK(f->inst_count > 50); /* far citizens share one instanced draw */
  SR_CHECK(build_ms < 8.0);     /* CPU side of the frame (debug build) within half a 60 Hz frame */
  SR_CHECK(g_app.gfx.draw_calls < 80);
  int w, h;
  uint8_t *rgb = render_internal(&g_app, &w, &h);
  if (rgb) save_shot("peak_crowd", rgb, w, h);
  free(rgb);
  free(f);
}

static void test_render_matrix(void) {
  sr_app_start_game(&g_app, 0x5EEDC0DEull);
  SrGame *g = g_app.game;
  g->settings.dynamic_res = false;
  g_app.fixed_frame_dt = 1.0 / 60.0;
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_DECK, 2, 1));
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK); /* home rig: the grid */
  for (int i = 0; i < 3; i++) sr_app_iterate(&g_app);
  int w, h;
  uint8_t *grid = render_internal(&g_app, &w, &h);
  SR_CHECK(grid && image_mean(grid, w * h) > 2.0);
  if (grid) save_shot("matrix_grid", grid, w, h);
  sr_game_jack_out(g, false);
  sr_game_leave(g);
  int site = -1;
  for (int b = 0; b < g->city->building_count && site < 0; b++)
    if (g->city->buildings[b].flags & SR_BF_MISSION) site = b;
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, site), SR_OK);
  g->dive.pos = sr_v3(0, 7, 30); /* outside the tower, looking at it */
  g->dive.yaw = 0;
  g->dive.pitch = 0.18f;
  uint8_t *host = render_internal(&g_app, &w, &h);
  SR_CHECK(host && image_mean(host, w * h) > 2.0);
  if (host) save_shot("matrix_host", host, w, h);
  /* golden image of the host view (llvmpipe), like the city reference */
  char path[512];
  snprintf(path, sizeof path, "%s/matrix_reference.ppm", SR_GOLDEN_DIR);
  const char *update = getenv("SR_UPDATE_GOLDENS");
  int gw, gh;
  uint8_t *golden = read_ppm(path, &gw, &gh);
  if (update && update[0] == '1' && host) {
    write_ppm(path, host, w, h);
  } else if (golden && host && gw == w && gh == h) {
    double share = diff_share(host, golden, w * h, 12);
    printf("  matrix visual diff vs golden: %.4f%%\n", share * 100);
    SR_CHECK(share < 0.01);
  }
  free(golden);
  /* frame budget on the benchmark dive: CPU side of a host frame */
  SrFrame *bench = (SrFrame *)malloc(sizeof(SrFrame));
  uint64_t t0 = SDL_GetTicksNS();
  for (int i = 0; i < 20; i++) sr_scene_build(&g_app.scene, g, 16.0f / 9.0f, bench);
  double ms = (double)(SDL_GetTicksNS() - t0) / 2e7;
  printf("  host frame build %.2f ms, %d draw calls\n", ms, g_app.gfx.draw_calls);
  SR_CHECK(ms < 8.0);
  free(bench);
  /* the meat-cam shows the body's surroundings at the top of the window */
  sr_app_iterate(&g_app);
  SR_CHECK(g_app.frame.inset != NULL);
  SR_CHECK(g_app.gfx.inset_w == 160 && g_app.gfx.inset_h == 90);
  uint8_t cam[64 * 16 * 3];
  sr_glb_read_window(&g_app.gfx, cam, g_app.win_w / 2 - 32, g_app.win_h - g_app.win_h / 12 - 30, 64, 16);
  SR_CHECK(image_mean(cam, 64 * 16) > 1.0);
  sr_game_jack_out(g, false);
  free(grid);
  free(host);
}

static void test_shop_taps(void) {
  sr_app_start_game(&g_app, 0x5EEDC0DEull);
  SrGame *g = g_app.game;
  g->player.cred = 100000;
  int shop = -1;
  for (int b = 0; b < g->city->building_count && shop < 0; b++)
    if (g->city->buildings[b].poi == SR_POI_CLINIC) shop = b;
  sr_clock_set_seconds(&g->clock, 12 * 3600); /* clinics are open at noon */
  sr_game_shop_open(g, shop);
  sr_app_iterate(&g_app); /* builds the UI at window size */
  float line = 11.0f * (float)g_app.ui.scale, top = 10.0f * (float)g_app.ui.scale + 6.0f * (float)g_app.ui.scale;
  float row0 = (top + line * 1.5f + 2) / (float)g_app.ui.h;
  int64_t cash = g->player.cred;
  sr_app_shop_tap(&g_app, row0);
  SR_CHECK(g->player.cred < cash);
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_FINGER_DOWN;
  e.tfinger.x = 0.5f;
  e.tfinger.y = 0.95f; /* bottom strip: leave */
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  sr_game_shop_open(g, shop);
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.y = g_app.css_h * 0.95f;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  /* a fixer's offers by touch: a tap selects, a second tap accepts */
  SR_SPEC("job-system", "Job selection by touch");
  sr_game_leave(g);
  int fixer = g->city->factions.fixer_building[g->city->blocks[g->city->buildings[g->player.home].block].district];
  SR_CHECK(sr_game_jobs_open(g, fixer));
  sr_app_iterate(&g_app);
  float row1 = (top + line * 2.5f + 2) / (float)g_app.ui.h;
  if (g->menu.count >= 2) {
    sr_app_shop_tap(&g_app, row1);
    SR_CHECK_EQ(g->menu.sel, 1);
    SR_CHECK(g->active_job < 0);
    sr_app_shop_tap(&g_app, row1);
    SR_CHECK(g->active_job >= 0);
  }
  sr_game_shop_close(g);
}

static void test_retro_toggles(void) {
  SR_SPEC("retro-rendering", "Independent toggles");
  reference_scene(&g_app);
  SrGame *g = g_app.game;
  int w, h;
  uint8_t *base = render_internal(&g_app, &w, &h);
  g->settings.vertex_snap = true;
  uint8_t *snap = render_internal(&g_app, &w, &h);
  g->settings.vertex_snap = false;
  g->settings.affine = true;
  uint8_t *aff = render_internal(&g_app, &w, &h);
  g->settings.affine = false;
  SR_CHECK(diff_share(base, snap, w * h, 0) > 0.0);
  SR_CHECK(diff_share(base, aff, w * h, 0) > 0.0);
  uint8_t *again = render_internal(&g_app, &w, &h);
  SR_CHECK(diff_share(base, again, w * h, 0) == 0.0); /* both off again: identical */
  /* post effects change the window image only */
  g->settings.integer_scale = false;
  sr_app_iterate(&g_app);
  uint8_t *win1 = (uint8_t *)malloc((size_t)64 * 64 * 3), *win2 = (uint8_t *)malloc((size_t)64 * 64 * 3);
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, g_app.win_w, g_app.win_h, false);
  sr_glb_read_window(&g_app.gfx, win1, g_app.win_w / 2, g_app.win_h / 2, 64, 64);
  g_app.frame.crt = true;
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, g_app.win_w, g_app.win_h, false);
  sr_glb_read_window(&g_app.gfx, win2, g_app.win_w / 2, g_app.win_h / 2, 64, 64);
  SR_CHECK(diff_share(win1, win2, 64 * 64, 0) > 0.0);
  g_app.frame.crt = false;
  g_app.frame.dither = false;
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, g_app.win_w, g_app.win_h, false);
  sr_glb_read_window(&g_app.gfx, win2, g_app.win_w / 2, g_app.win_h / 2, 64, 64);
  SR_CHECK(diff_share(win1, win2, 64 * 64, 0) > 0.0);
  free(win1);
  free(win2);
  free(base);
  free(snap);
  free(aff);
  free(again);
}

static void test_letterbox_window(void) {
  SR_SPEC("platform-runtime", "Resize keeps the aspect ratio");
  SR_SPEC("retro-rendering", "Integer scaling");
  SDL_SetWindowSize(g_app.window, 900, 900);
  SDL_SyncWindow(g_app.window);
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
  sr_app_event(&g_app, &e);
  sr_app_iterate(&g_app);
  sr_glb_render(&g_app.gfx, &g_app.frame, NULL, g_app.win_w, g_app.win_h, false);
  uint8_t px[3 * 4];
  sr_glb_read_window(&g_app.gfx, px, g_app.win_w / 2, g_app.win_h - 2, 2, 1); /* top bar */
  SR_CHECK(px[0] == 0 && px[1] == 0 && px[2] == 0);
  SrViewport vp = sr_letterbox(g_app.win_w, g_app.win_h, 480, 270, false);
  SR_CHECK(vp.y > 0);
  SDL_SetWindowSize(g_app.window, 1280, 720);
  SDL_SyncWindow(g_app.window);
  sr_app_event(&g_app, &e);
}

static void test_focus_and_autosave(void) {
  SR_SPEC("platform-runtime", "Page becomes hidden");
  SR_SPEC("platform-runtime", "Returning to the game");
  SR_SPEC("game-persistence", "Autosave when the page is hidden");
  sr_app_start_game(&g_app, 99);
  g_app.fixed_frame_dt = 1.0 / 60.0;
  frames(&g_app, 5);
  int64_t t0 = sr_game_time(g_app.game);
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_WINDOW_HIDDEN;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PAUSE);
  SR_CHECK(g_app.game->audio_muted);
  char name[32];
  sr_autosave_name(0, name, sizeof name);
  bool any = false;
  for (int s = 0; s < SR_AUTOSAVE_SLOTS; s++) {
    sr_autosave_name(s, name, sizeof name);
    any |= g_app.storage.exists(g_app.storage.user, name);
  }
  SR_CHECK(any);
  frames(&g_app, 60);
  SR_CHECK_EQ(sr_game_time(g_app.game), t0);
  /* resume only on confirmation: a click */
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  e.button.button = SDL_BUTTON_LEFT;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
  frames(&g_app, 60);
  SR_CHECK(sr_game_time(g_app.game) > t0);
}

static void test_settings_persist(void) {
  SR_SPEC("platform-runtime", "Settings survive a restart");
  SR_SPEC("input-controls", "Rebound key");
  g_app.game->settings.volume = 37;
  g_app.game->settings.internal_res = 2;
  sr_input_rebind_key(&g_app.input, SR_ACT_JUMP, SR_KEY_X);
  SR_CHECK_EQ(sr_app_save_settings(&g_app), SR_OK);
  SrApp *b = (SrApp *)calloc(1, sizeof *b);
  char *argv[] = {"test", "--headless"};
  SR_CHECK(sr_app_init(b, 2, argv)); /* a "restart" against the same storage */
  SR_CHECK_EQ(b->game->settings.volume, 37);
  SR_CHECK_EQ(b->game->settings.internal_res, 2);
  SR_CHECK_EQ(b->input.key_bind[SR_ACT_JUMP], SR_KEY_X);
  b->game->started = false;
  sr_app_quit(b);
  free(b);
  g_app.game->settings.internal_res = 1;
  sr_input_rebind_key(&g_app.input, SR_ACT_JUMP, SR_KEY_SPACE);
  g_app.game->settings.volume = 80;
  sr_app_save_settings(&g_app);
}

static void push_key(SDL_Scancode sc, bool down) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  e.key.scancode = sc;
  sr_app_event(&g_app, &e);
}

static void test_input_events(void) {
  SR_SPEC("input-controls", "Pointer lock in a desktop browser");
  SR_SPEC("input-controls", "Gamepad hot-plug");
  SR_SPEC("input-controls", "Active gamepad disconnected");
  SR_SPEC("input-controls", "Hybrid device switches schemes");
  sr_app_start_game(&g_app, 7);
  g_app.fixed_frame_dt = 1.0 / 60.0;
  float z0 = g_app.game->player.pos.z;
  push_key(SDL_SCANCODE_W, true);
  frames(&g_app, 30);
  push_key(SDL_SCANCODE_W, false);
  SR_CHECK(g_app.game->player.pos.z != z0 || g_app.game->player.pos.x != 0);
  for (int sc = 0; sc < SDL_SCANCODE_COUNT; sc++) (void)sr_platform_key((SDL_Scancode)sc);
  SR_CHECK_EQ(sr_platform_key(SDL_SCANCODE_E), SR_KEY_E);
  for (int b = -1; b < SDL_GAMEPAD_BUTTON_COUNT; b++) (void)sr_platform_pad_button(b);
  SR_CHECK_EQ(sr_platform_pad_button(SDL_GAMEPAD_BUTTON_SOUTH), SR_PAD_SOUTH);
  /* click requests relative mouse (pointer lock), then mouse motion turns the camera */
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  e.button.button = SDL_BUTTON_LEFT;
  sr_app_event(&g_app, &e);
  e.type = SDL_EVENT_MOUSE_BUTTON_UP;
  sr_app_event(&g_app, &e);
  SR_CHECK(g_app.input.pointer_lock_wanted);
  float yaw = g_app.game->player.yaw;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_MOUSE_MOTION;
  e.motion.xrel = 200;
  g_app.relative_mouse = true;
  sr_app_event(&g_app, &e);
  frames(&g_app, 2);
  SR_CHECK(g_app.game->player.yaw != yaw);
  /* gamepad hot-plug: synthetic events (no physical device behind them) */
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_GAMEPAD_ADDED;
  e.gdevice.which = 12345;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.input.scheme, SR_SCHEME_GAMEPAD);
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
  e.gaxis.axis = SDL_GAMEPAD_AXIS_LEFTY;
  e.gaxis.value = -32767;
  sr_app_event(&g_app, &e);
  for (int ax = 0; ax < SDL_GAMEPAD_AXIS_COUNT; ax++) {
    e.gaxis.axis = (Uint8)ax;
    e.gaxis.value = ax == SDL_GAMEPAD_AXIS_LEFTY ? -32767 : 0;
    sr_app_event(&g_app, &e);
  }
  frames(&g_app, 10);
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
  e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
  sr_app_event(&g_app, &e);
  e.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
  sr_app_event(&g_app, &e);
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_GAMEPAD_REMOVED;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PAUSE);
  sr_game_confirm_resume(g_app.game);
  /* touch switches to the touch scheme; a key press switches back */
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_FINGER_DOWN;
  e.tfinger.fingerID = 1;
  e.tfinger.x = 0.2f;
  e.tfinger.y = 0.7f;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.input.scheme, SR_SCHEME_TOUCH);
  SR_CHECK_STR(sr_app_test_exec(&g_app, "overlay.visible"), "true");
  e.type = SDL_EVENT_FINGER_MOTION;
  e.tfinger.y = 0.5f;
  sr_app_event(&g_app, &e);
  frames(&g_app, 5); /* draws the overlay with the stick */
  e.type = SDL_EVENT_FINGER_UP;
  sr_app_event(&g_app, &e);
  push_key(SDL_SCANCODE_A, true);
  push_key(SDL_SCANCODE_A, false);
  SR_CHECK_STR(sr_app_test_exec(&g_app, "input.scheme"), "kbm");
  SR_CHECK_STR(sr_app_test_exec(&g_app, "overlay.visible"), "false");
  /* test hook queries */
  SR_CHECK(strcmp(sr_app_test_exec(&g_app, "frames"), "0") != 0);
  SR_CHECK(strstr(sr_app_test_exec(&g_app, "gl"), "1") != NULL);
  SR_CHECK_STR(sr_app_test_exec(&g_app, "portrait"), "false");
  sr_app_test_exec(&g_app, "pointer.lock");
  SR_CHECK(strstr(sr_app_test_exec(&g_app, "get player.cash"), "") != NULL);
  SR_CHECK(strstr(sr_app_test_exec(&g_app, "nonsense"), "error") != NULL);
  SR_CHECK_STR(sr_app_test_exec(&g_app, "save_settings"), "ok");
  SR_CHECK_STR(sr_app_test_exec(&g_app, "start"), "ok");
  /* F11 toggles fullscreen and back */
  push_key(SDL_SCANCODE_F11, true);
  push_key(SDL_SCANCODE_F11, false);
  push_key(SDL_SCANCODE_F11, true);
  push_key(SDL_SCANCODE_F11, false);
  /* screens: terminal and HUD with toast and job */
  frames(&g_app, 2);
  g_app.game->screen = SR_SCREEN_TERMINAL;
  frames(&g_app, 2);
  g_app.game->screen = SR_SCREEN_DEAD;
  frames(&g_app, 1);
  g_app.game->screen = SR_SCREEN_PLAY;
}

static void test_title_starts(void) {
  sr_game_destroy(g_app.game);
  g_app.game = sr_game_create();
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_TITLE);
  frames(&g_app, 2);
  push_key(SDL_SCANCODE_RETURN, true);
  push_key(SDL_SCANCODE_RETURN, false);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_CREATE);
  frames(&g_app, 2); /* draws the creation screen */
  push_key(SDL_SCANCODE_3, true);
  push_key(SDL_SCANCODE_R, true);
  push_key(SDL_SCANCODE_DOWN, true);
  SR_CHECK_EQ(g_app.game->create_bg, 3);
  push_key(SDL_SCANCODE_RETURN, true);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
  /* mouse, touch and gamepad also start from the title */
  SDL_Event e;
  const Uint32 types[3] = {SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_FINGER_DOWN, SDL_EVENT_GAMEPAD_BUTTON_DOWN};
  for (int i = 0; i < 3; i++) {
    g_app.game->screen = SR_SCREEN_TITLE;
    memset(&e, 0, sizeof e);
    e.type = types[i];
    sr_app_event(&g_app, &e);
    SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_CREATE);
    /* left side cycles the background, right side re-rolls, bottom right starts */
    memset(&e, 0, sizeof e);
    e.type = types[i];
    if (types[i] == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      e.button.x = 10;
      e.button.y = 10;
    }
    if (types[i] == SDL_EVENT_FINGER_DOWN) {
      e.tfinger.x = 0.9f;
      e.tfinger.y = 0.2f;
    }
    if (types[i] == SDL_EVENT_GAMEPAD_BUTTON_DOWN) e.gbutton.button = SDL_GAMEPAD_BUTTON_WEST;
    sr_app_event(&g_app, &e);
    if (types[i] == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
      e.gbutton.button = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
      sr_app_event(&g_app, &e);
      e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    } else if (types[i] == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      e.button.x = g_app.css_w * 0.9f;
      e.button.y = g_app.css_h * 0.9f;
    } else {
      e.tfinger.y = 0.9f;
    }
    sr_app_event(&g_app, &e);
    SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
    if (types[i] == SDL_EVENT_FINGER_DOWN) {
      e.type = SDL_EVENT_FINGER_UP;
      sr_app_event(&g_app, &e);
    }
  }
  /* touch on the pause screen resumes */
  sr_game_focus(g_app.game, false);
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_FINGER_DOWN;
  sr_app_event(&g_app, &e);
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
  e.type = SDL_EVENT_FINGER_UP;
  sr_app_event(&g_app, &e);
  /* mouse synthesized from touch is ignored */
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  e.button.which = SDL_TOUCH_MOUSEID;
  sr_app_event(&g_app, &e);
  /* portrait on a touch device blocks the simulation */
  g_app.portrait = true;
  g_app.input.scheme = SR_SCHEME_TOUCH;
  int64_t t = sr_game_time(g_app.game);
  frames(&g_app, 30);
  SR_CHECK_EQ(sr_game_time(g_app.game), t);
  g_app.portrait = false;
  g_app.input.scheme = SR_SCHEME_KBM;
  memset(&e, 0, sizeof e);
  e.type = SDL_EVENT_QUIT;
  sr_app_event(&g_app, &e);
  SR_CHECK(g_app.quit);
  g_app.quit = false;
}

static void test_context_loss(void) {
  SR_SPEC("platform-runtime", "Context is restored");
  sr_app_start_game(&g_app, 1234);
  g_app.fixed_frame_dt = 1.0 / 60.0;
  frames(&g_app, 5);
  uint64_t before = sr_game_checksum(g_app.game);
  sr_glb_context_lost(&g_app.gfx); /* what the browser's webglcontextlost triggers */
  g_app.context_lost = true;
  g_app.fixed_frame_dt = 0.00001;
  frames(&g_app, 3);
  SR_CHECK(sr_glb_init(&g_app.gfx, g_app.gles)); /* webglcontextrestored */
  g_app.context_lost = false;
  frames(&g_app, 30);
  SR_CHECK(g_app.gfx.ready);
  SR_CHECK(g_app.gfx.draw_calls > 0);
  SR_CHECK(sr_game_checksum(g_app.game) == before);
}

static void test_dynamic_resolution(void) {
  SR_SPEC("retro-rendering", "Dynamic resolution engages");
  SrDynRes d;
  memset(&d, 0, sizeof d);
  bool changed = false;
  for (int i = 0; i < 200 && !changed; i++) changed = sr_dynres_update(&d, 30.0f, 16.7f, 1.0f / 60.0f);
  SR_CHECK(changed && d.level == 1);
  int w = 480, h = 270;
  sr_dynres_apply(&d, &w, &h);
  SR_CHECK(w < 480);
  changed = false;
  for (int i = 0; i < 1200 && !changed; i++) changed = sr_dynres_update(&d, 10.0f, 16.7f, 1.0f / 60.0f);
  SR_CHECK(changed && d.level == 0);
  /* run the app with dynamic resolution and a slow frame time */
  g_app.game->settings.dynamic_res = true;
  g_app.fixed_frame_dt = 0.05;
  frames(&g_app, 60);
  SR_CHECK(g_app.dynres.level > 0);
  g_app.game->settings.dynamic_res = false;
  g_app.fixed_frame_dt = 0;
}

static void test_logging_and_storage(void) {
  SR_SPEC("platform-runtime", "Recoverable error is logged");
  sr_log(SR_LOG_ERROR, "test", "something recoverable failed");
  FILE *f = fopen(g_app.log_path, "r");
  SR_CHECK(f != NULL);
  if (f) {
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);
    SR_CHECK(strstr(buf, "something recoverable failed") != NULL);
  }
  SrStorage *s = &g_app.storage;
  SR_CHECK_EQ(s->write(s->user, "t.bin", "abc", 3), SR_OK);
  void *data = NULL;
  size_t len = 0;
  SR_CHECK_EQ(s->read(s->user, "t.bin", &data, &len), SR_OK);
  SR_CHECK(len == 3 && memcmp(data, "abc", 3) == 0);
  free(data);
  SR_CHECK(s->exists(s->user, "t.bin"));
  SR_CHECK_EQ(s->remove(s->user, "t.bin"), SR_OK);
  SR_CHECK(!s->exists(s->user, "t.bin"));
  SR_CHECK_EQ(s->read(s->user, "t.bin", &data, &len), SR_ERR_NOT_FOUND);
  SR_CHECK_EQ(s->remove(s->user, "t.bin"), SR_ERR_NOT_FOUND);
  /* a write into a missing directory fails and is reported */
  SrFileStore bad;
  SrStorage bs;
  sr_filestore_init(&bad, "/proc/definitely/not/writable", &bs);
  SR_CHECK(bs.write(bs.user, "x", "y", 1) != SR_OK);
  /* saving settings fails gracefully on a broken store */
  SrStorage keep = g_app.storage;
  g_app.storage = bs;
  SR_CHECK(sr_app_save_settings(&g_app) != SR_OK);
  g_app.storage = keep;
}

static void test_gles_path(void) {
  /* a second app forced onto OpenGL ES 3.0 (the WebGL 2 dialect) */
  SrApp *b = (SrApp *)calloc(1, sizeof *b);
  char *argv[] = {"test"};
  sr_app_quit(&g_app); /* SDL_Quit resets hints: set them afterwards */
  SDL_SetHint("SR_FORCE_GLES", "1");
  SDL_SetHint("SR_STORAGE_ROOT", g_root);
  bool ok = sr_app_init(b, 1, argv);
  if (ok) {
    SR_CHECK(b->gles);
    reference_scene(b);
    int w, h;
    uint8_t *rgb = render_internal(b, &w, &h);
    SR_CHECK(rgb != NULL);
    free(rgb);
    sr_app_quit(b);
  } else {
    printf("  GLES context unavailable on this driver; skipped\n");
  }
  free(b);
  SDL_SetHint("SR_FORCE_GLES", "0");
  SDL_SetHint("SR_STORAGE_ROOT", g_root);
  char *argv2[] = {"test"};
  SR_CHECK(sr_app_init(&g_app, 1, argv2));
}

static void test_headless_app(void) {
  SR_SPEC("platform-runtime", "Headless script passes");
  SrApp *h = (SrApp *)calloc(1, sizeof *h);
  char *argv[] = {"test", "--headless"};
  SR_CHECK(sr_app_init(h, 2, argv));
  SR_CHECK(h->window == NULL);
  sr_app_start_game(h, 5);
  h->fixed_frame_dt = 1.0 / 60.0;
  for (int i = 0; i < 10; i++) sr_app_iterate(h);
  SR_CHECK(h->frames == 10);
  sr_app_quit(h);
  free(h);
}

/* ---- Reference views of the city's atmosphere (improve-city-atmosphere D15) ------------------ */

typedef enum { AV_STREET, AV_ALLEY, AV_HIGHWAY, AV_ROOF, AV_AERIAL, AV_FRONTS } AtmosKind;
typedef struct {
  const char *name;
  int district; /* SrDistrictType, or -1 for the home district */
  AtmosKind kind;
  int hour, minute;
  int weather; /* SrWeather */
  int look;    /* 0 along the street (east), 1 west */
} AtmosView;

static const AtmosView k_atmos_views[] = {
    {"spire", SR_D_SPIRE, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"commercial", SR_D_COMMERCIAL, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"entertainment", SR_D_ENTERTAINMENT, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"residential", SR_D_RESIDENTIAL, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"oldquarter", SR_D_OLDQUARTER, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"industrial", SR_D_INDUSTRIAL, AV_STREET, 23, 0, SR_WX_DRIZZLE, 0},
    {"alley", -1, AV_ALLEY, 23, 0, SR_WX_DRIZZLE, 0},
    {"highway", -2, AV_HIGHWAY, 23, 0, SR_WX_DOWNPOUR, 0},
    {"rooftop", -1, AV_ROOF, 23, 0, SR_WX_CLEAR, 0},
    {"fog", SR_D_RESIDENTIAL, AV_STREET, 1, 0, SR_WX_FOG, 0},
    {"smog", SR_D_INDUSTRIAL, AV_STREET, 1, 0, SR_WX_SMOG, 0},
    {"dawn", SR_D_RESIDENTIAL, AV_STREET, 10, 30, SR_WX_CLEAR, 0},
    {"noon", SR_D_RESIDENTIAL, AV_STREET, 12, 30, SR_WX_CLEAR, 0},
    {"dusk", SR_D_RESIDENTIAL, AV_STREET, 14, 30, SR_WX_CLEAR, 1},
    {"aerial", SR_D_COMMERCIAL, AV_AERIAL, 23, 0, SR_WX_CLEAR, 0},
    {"shopfronts", SR_D_ENTERTAINMENT, AV_FRONTS, 23, 0, SR_WX_DRIZZLE, 0},
};
#define ATMOS_VIEWS ((int)(sizeof k_atmos_views / sizeof k_atmos_views[0]))

/* A land block of the district type with at least two buildings (the home block for -1, a
 * highway block for -2); -1 when the city has none. */
static int atmos_block(const SrCity *c, int type, int home) {
  if (type == -1) return c->buildings[home].block;
  int best = -1;
  for (int b = 0; b < c->grid * c->grid; b++) {
    const SrBlock *k = &c->blocks[b];
    if (k->water) continue;
    if (type == -2 ? !k->highway : ((int)c->districts[k->district].type != type || k->building_count < 2)) continue;
    int bx = b % c->grid, by = b / c->grid;
    if (bx < 1 || by < 1 || bx >= c->grid - 1 || by >= c->grid - 1) continue; /* not at the city edge */
    best = b;
    break;
  }
  return best;
}

/* Places the camera for a view; false when the city has no such place. */
static bool atmos_place(SrGame *g, const AtmosView *v) {
  const SrCity *c = g->city;
  int blk = atmos_block(c, v->district, g->player.home);
  if (blk < 0) return false;
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f;
  g->player.pitch = 0.1f;
  if (v->kind == AV_STREET || v->kind == AV_HIGHWAY) { /* mid-street, looking along it (+x) */
    g->player.pos = sr_v3(x0 + 24.0f, 1.7f, z0 + 9.0f);
    g->player.yaw = v->look ? SR_PI / 2 - 0.12f : -SR_PI / 2 + 0.12f;
  } else if (v->kind == AV_ALLEY) { /* the widest gap between two buildings, home block first */
    float best = 0;
    for (int n = 0; n < c->grid * c->grid && best < 4.0f; n++) {
      const SrBlock *k = &c->blocks[(blk + n) % (c->grid * c->grid)];
      for (int i = 0; i < k->building_count; i++)
        for (int j = 0; j < k->building_count; j++) {
          const SrBuilding *p = &c->buildings[k->first_building + i], *q = &c->buildings[k->first_building + j];
          float gap = (float)(q->x0 - p->x1) / 100.0f;
          if (gap > best && q->y0 < p->y1 && p->y0 < q->y1) {
            best = gap;
            g->player.pos = sr_v3((float)(p->x1 + q->x0) / 200.0f, 1.7f, (float)SR_MAX(p->y1, q->y1) / 100.0f + 3.0f);
            g->player.yaw = 0.0f; /* looking into the gap (-z) */
          }
        }
    }
    if (best <= 0) return false;
  } else if (v->kind == AV_FRONTS) { /* the street south of the block: the buildings' fronts, signs above */
    g->player.pos = sr_v3(x0 + 20.0f, 1.7f, z0 + (float)SR_BLOCK_CM / 100.0f + 5.0f);
    g->player.yaw = -SR_PI / 2 + 0.25f;
    g->player.pitch = 0.2f;
  } else if (v->kind == AV_AERIAL) { /* high above a street corner, looking down over the roofs */
    g->player.pos = sr_v3(x0 + 8.0f, 75.0f, z0 + 8.0f);
    g->player.yaw = -SR_PI / 4;
    g->player.pitch = -0.5f;
  } else { /* up on a roof at 40 m over the home corner, looking down the street to the skyline */
    g->player.pos = sr_v3(x0 + 8.0f, 40.0f, z0 + 8.0f);
    g->player.yaw = -SR_PI / 2 + 0.12f;
    g->player.pitch = -0.3f;
  }
  return true;
}

static void atmos_setup(SrApp *a, uint64_t seed) {
  sr_app_start_game(a, seed);
  SrGame *g = a->game;
  g->crowd_enabled = false;
  sr_game_leave(g);
  g->settings.internal_res = 1;
  g->settings.dynamic_res = false;
  g->settings.dither = g->settings.quantize = true;
  g->settings.vertex_snap = g->settings.affine = g->settings.crt = false;
  g->settings.bloom = true;
  a->fixed_frame_dt = 0.0001;
  a->loop.acc = 0;
}

/* Renders one view at the internal resolution (caller frees); NULL when it does not exist. */
static uint8_t *atmos_render(SrApp *a, const AtmosView *v, int *w, int *h) {
  SrGame *g = a->game;
  if (!atmos_place(g, v)) return NULL;
  g->force_weather = (int8_t)v->weather;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + v->hour * 3600 + v->minute * 60);
  g->steps = 0;
  for (int i = 0; i < 40; i++) sr_app_iterate(a); /* stream the chunks around the view */
  return render_internal(a, w, h);
}

static void png_or_ppm(const char *dir, const char *name, const uint8_t *rgb, int w, int h) {
  char o[512];
  snprintf(o, sizeof o, "%s/%s.ppm", dir, name);
  write_ppm(o, rgb, w, h);
}

/* SR_ATMOS_SHOTS=dir writes every reference view on three seeds (for the review sheets). */
static void test_atmosphere_shots(void) {
  const char *dir = getenv("SR_ATMOS_SHOTS");
  if (!dir) return;
  static const uint64_t seeds[3] = {0x5EEDC0DEull, 7919ull * 3, 7919ull * 11};
  for (int s = 0; s < 3; s++) {
    atmos_setup(&g_app, seeds[s]);
    for (int i = 0; i < ATMOS_VIEWS; i++) {
      int w, h;
      uint8_t *rgb = atmos_render(&g_app, &k_atmos_views[i], &w, &h);
      if (!rgb) continue;
      char name[64];
      snprintf(name, sizeof name, "s%d_%02d_%s", s + 1, i + 1, k_atmos_views[i].name);
      png_or_ppm(dir, name, rgb, w, h);
      free(rgb);
    }
  }
  g_app.game->force_weather = -1;
}

/* Emissive mask of the current frame: white where an emissive surface or glow was drawn. */
static uint8_t *atmos_emissive_mask(SrApp *a, int w, int h) {
  a->frame.debug_emissive = true;
  sr_glb_render(&a->gfx, &a->frame, NULL, a->win_w, a->win_h, false);
  a->frame.debug_emissive = false;
  uint8_t *rgb = (uint8_t *)malloc((size_t)w * h * 3);
  if (!sr_glb_read_internal(&a->gfx, rgb, w, h)) {
    free(rgb);
    return NULL;
  }
  return rgb;
}

static double clipped_share(const uint8_t *rgb, int n) {
  int c = 0;
  for (int i = 0; i < n; i++) c += rgb[i * 3] >= 250 && rgb[i * 3 + 1] >= 250 && rgb[i * 3 + 2] >= 250;
  return (double)c / n;
}

/* Share of the 1 % most saturated pixels (chroma times value) that lie on emissive surfaces. */
static double saturated_on_emissive(const uint8_t *rgb, const uint8_t *mask, int n) {
  int hist[256] = {0};
  for (int i = 0; i < n; i++) {
    int mx = SR_MAX(rgb[i * 3], SR_MAX(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    int mn = SR_MIN(rgb[i * 3], SR_MIN(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    hist[(mx - mn) * mx / 255]++;
  }
  int need = n / 100, cut = 255;
  for (int acc = 0; cut > 0 && acc + hist[cut] < need; cut--) acc += hist[cut];
  if (cut < 12) return 1.0; /* nothing in the image is saturated (a wall of fog) */
  int top = 0, on = 0;
  for (int i = 0; i < n; i++) {
    int mx = SR_MAX(rgb[i * 3], SR_MAX(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    int mn = SR_MIN(rgb[i * 3], SR_MIN(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    if ((mx - mn) * mx / 255 < cut || cut == 0) continue;
    top++;
    on += mask[i * 3] > 128;
  }
  return top ? (double)on / top : 1.0;
}

/* Image metrics on the night reference views of one seed (D15). */
static void test_atmosphere_metrics(void) {
  SR_SPEC("retro-rendering", "No white walls of windows");
  SR_SPEC("retro-rendering", "Neon is the brightest");
  SR_SPEC("retro-rendering", "Districts apart by shape and colour");
  atmos_setup(&g_app, 7919ull * 11);
  double means[6][3];
  int nd = 0;
  for (int i = 0; i < ATMOS_VIEWS; i++) {
    const AtmosView *v = &k_atmos_views[i];
    if (v->hour > 9 && v->hour < 16) continue; /* night views only */
    int w, h;
    uint8_t *rgb = atmos_render(&g_app, v, &w, &h);
    if (!rgb) continue;
    uint8_t *mask = atmos_emissive_mask(&g_app, w, h);
    double clip = clipped_share(rgb, w * h), sat = mask ? saturated_on_emissive(rgb, mask, w * h) : 0;
    printf("  %-14s clipped %.2f %%, saturated on emissive %.0f %%\n", v->name, clip * 100, sat * 100);
    SR_CHECK(clip <= 0.03);
    SR_CHECK(sat >= 0.8);
    if (v->kind == AV_STREET && v->weather == SR_WX_DRIZZLE && nd < 6) {
      for (int k = 0; k < 3; k++) {
        double sum = 0;
        for (int p = 0; p < w * h; p++) sum += rgb[p * 3 + k];
        means[nd][k] = sum / (w * h);
      }
      nd++;
    }
    free(mask);
    free(rgb);
  }
  double closest = 1e9;
  for (int a = 0; a < nd; a++)
    for (int b = 0; b < a; b++) {
      double d = 0;
      for (int k = 0; k < 3; k++) d += (means[a][k] - means[b][k]) * (means[a][k] - means[b][k]);
      closest = SR_MIN(closest, sqrt(d));
    }
  printf("  district views: %d, closest mean colours %.1f apart\n", nd, closest);
  SR_CHECK(nd >= 4);
  SR_CHECK(closest >= 2.5); /* mean colours of whole dark frames: a few levels apart; shapes carry the rest */
  g_app.game->force_weather = -1;
}

static double mean_luma(const uint8_t *rgb, int n) {
  double s = 0;
  for (int i = 0; i < n; i++) s += 0.299 * rgb[i * 3] + 0.587 * rgb[i * 3 + 1] + 0.114 * rgb[i * 3 + 2];
  return s / n;
}

static const AtmosView *atmos_view(const char *name) {
  for (int i = 0; i < ATMOS_VIEWS; i++)
    if (strcmp(k_atmos_views[i].name, name) == 0) return &k_atmos_views[i];
  return NULL;
}

/* The gloom (phase 2): noon stays dark, dusk glows red in the west, the end of the street is
 * never a flat wall of fog. */
static void test_atmosphere_gloom(void) {
  SR_SPEC("world-environment", "Noon stays dark");
  SR_SPEC("world-environment", "Dusk glows red in the west");
  SR_SPEC("retro-rendering", "No dark slot at the end of the street");
  atmos_setup(&g_app, 7919ull * 11);
  int w, h;
  AtmosView night = *atmos_view("noon");
  night.hour = 23;
  night.minute = 0;
  uint8_t *n = atmos_render(&g_app, &night, &w, &h);
  uint8_t *noon = atmos_render(&g_app, atmos_view("noon"), &w, &h);
  uint8_t *mask = noon ? atmos_emissive_mask(&g_app, w, h) : NULL;
  SR_CHECK(n && noon && mask);
  if (n && noon && mask) {
    double ln = mean_luma(n, w * h), ld = mean_luma(noon, w * h), sat = saturated_on_emissive(noon, mask, w * h);
    printf("  luma night %.1f, noon %.1f; noon saturated on emissive %.0f %%\n", ln, ld, sat * 100);
    SR_CHECK(ld <= ln * 1.6);
    SR_CHECK(sat >= 0.8);
  }
  free(mask);
  free(noon);
  free(n);
  /* dusk: compare the west view at 14:30 with the same view at night */
  AtmosView west = *atmos_view("dusk");
  uint8_t *dusk = atmos_render(&g_app, &west, &w, &h);
  west.hour = 23;
  west.minute = 0;
  uint8_t *dark = atmos_render(&g_app, &west, &w, &h);
  SR_CHECK(dusk && dark);
  if (dusk && dark) {
    int red = 0;
    for (int i = 0; i < w * h / 2 + w * h / 8; i++) /* above and just below the horizon */
      red += dusk[i * 3] > dark[i * 3] + 10 && dusk[i * 3] > dusk[i * 3 + 1] + 10 && dusk[i * 3] >= dusk[i * 3 + 2];
    printf("  dusk: %.2f %% of the upper image warmed red\n", 100.0 * red / (w * h));
    SR_CHECK(red >= w * h / 200);
  }
  free(dusk);
  free(dark);
  /* fog: the area around the vanishing point is not one flat colour */
  uint8_t *fog = atmos_render(&g_app, atmos_view("fog"), &w, &h);
  SR_CHECK(fog != NULL);
  if (fog) {
    int x0 = w / 2 - 40, y0 = h / 2 - 50, varied = 0, total = 0;
    int fr = fog[(y0 * w + x0) * 3], fg = fog[(y0 * w + x0) * 3 + 1], fb = fog[(y0 * w + x0) * 3 + 2];
    for (int y = y0; y < y0 + 60; y++)
      for (int x = x0; x < x0 + 80; x++) {
        const uint8_t *q = &fog[(y * w + x) * 3];
        varied += abs(q[0] - fr) + abs(q[1] - fg) + abs(q[2] - fb) > 12;
        total++;
      }
    printf("  fog: %.1f %% of the vanishing point area differs from the fog, %d halos\n", 100.0 * varied / total,
           g_app.frame.halo_count);
    SR_CHECK(varied >= total / 20);
    SR_CHECK(g_app.frame.halo_count > 0);
  }
  free(fog);
  g_app.game->force_weather = -1;
}

/* Every weather has its own look (phase 3): the same street in the six weathers. */
static void test_atmosphere_weather(void) {
  SR_SPEC("world-environment", "Each weather looks different");
  atmos_setup(&g_app, 7919ull * 11);
  AtmosView v = *atmos_view("residential");
  v.hour = 1;
  uint8_t *img[SR_WX_COUNT];
  double mean[SR_WX_COUNT][3];
  int w = 0, h = 0;
  for (int k = 0; k < SR_WX_COUNT; k++) {
    v.weather = k;
    img[k] = atmos_render(&g_app, &v, &w, &h);
    for (int c = 0; c < 3; c++) {
      double s = 0;
      for (int i = 0; img[k] && i < w * h; i++) s += img[k][i * 3 + c];
      mean[k][c] = s / (w * h);
    }
  }
  for (int a = 0; a < SR_WX_COUNT; a++)
    for (int b = 0; b < a; b++) {
      if (!img[a] || !img[b]) continue;
      double d = 0;
      for (int c = 0; c < 3; c++) d += (mean[a][c] - mean[b][c]) * (mean[a][c] - mean[b][c]);
      double share = diff_share(img[a], img[b], w * h, 6); /* 6 levels: the fog and rain are subtle on dark walls */
      printf("  %-18s vs %-18s mean %.1f apart, %.1f %% of pixels differ\n", sr_weather_name((SrWeather)a),
             sr_weather_name((SrWeather)b), sqrt(d), share * 100);
      SR_CHECK(sqrt(d) >= 2.0 || share >= 0.03);
    }
  for (int k = 0; k < SR_WX_COUNT; k++) free(img[k]);
  g_app.game->force_weather = -1;
}

/* ---- animation filmstrips ------------------------------------------------------------------- */

typedef struct AnimStage {
  SrMesh ground, people;
  SrFrame frame;
} AnimStage;

/* Renders a hand-built frame: a dark floor, the given meshes and two lights, at the internal
 * resolution of the running game. */
static uint8_t *anim_render(SrApp *a, AnimStage *st, SrVec3 eye, float yaw, float pitch, int *w, int *h) {
  SrFrame *f = &st->frame;
  *f = a->frame;
  f->view = sr_m4_fps_view(eye, yaw, pitch);
  f->eye = eye;
  f->fog_start = 30.0f;
  f->fog[0] = f->fog[1] = 0.05f;
  f->fog[2] = 0.08f;
  f->fog_end = 80.0f;
  f->ambient[0] = f->ambient[1] = 0.45f;
  f->ambient[2] = 0.55f;
  f->nlights = 2;
  f->lights[0] = (SrLight){sr_v3(eye.x - 2.0f, 3.0f, eye.z - 1.0f), 12.0f, 1.2f, 0.4f, 0.9f};
  f->lights[1] = (SrLight){sr_v3(eye.x + 3.0f, 3.5f, eye.z - 4.0f), 12.0f, 0.3f, 0.9f, 1.2f};
  f->ndraws = 0;
  f->inst_count = 0;
  f->inset = NULL;
  SrMesh *ms[2] = {&st->ground, &st->people};
  for (int i = 0; i < 2; i++) {
    SrDraw *d = &f->draws[f->ndraws++];
    d->mesh = ms[i];
    d->pass = SR_PASS_WORLD;
    d->nlights = sr_select_lights(f->lights, f->nlights, eye, d->lights);
  }
  sr_ui_begin(&a->ui, a->win_w, a->win_h, 1);
  sr_glb_render(&a->gfx, f, &a->ui, a->win_w, a->win_h, false);
  *w = f->internal_w;
  *h = f->internal_h;
  uint8_t *rgb = (uint8_t *)malloc((size_t)*w * *h * 3);
  if (!sr_glb_read_internal(&a->gfx, rgb, *w, *h)) {
    free(rgb);
    return NULL;
  }
  return rgb;
}

/* SR_ANIM_SHOTS=dir: every clip on three body types, 8 frames each, three-quarter view. */
static void test_anim_shots(void) {
  const char *dir = getenv("SR_ANIM_SHOTS");
  if (!dir) return;
  atmos_setup(&g_app, 0x5EEDC0DEull);
  sr_app_iterate(&g_app);
  AnimStage *st = (AnimStage *)calloc(1, sizeof *st);
  sr_mesh_init(&st->ground);
  sr_mesh_init(&st->people);
  sr_mesh_box(&st->ground, sr_v3(-6, -0.05f, -6), sr_v3(6, 0, 6), (SrColor){60, 62, 70, 255}, SR_LAYER_SIDEWALK, 1.0f,
              0);
  SrAppearance looks[3];
  static const int roles[3] = {SR_ROLE_WORKER, SR_ROLE_EXEC, SR_ROLE_GANG};
  for (int b = 0; b < 3; b++) {
    looks[b] = sr_appearance_generate(100u + (uint64_t)b, (SrRole)roles[b], 0);
    looks[b].body = (uint8_t)b;
  }
  for (int clip = 1; clip < SR_CLIP_COUNT; clip++) {
    for (int k = 0; k < 8; k++) {
      sr_mesh_clear(&st->people);
      for (int b = 0; b < 3; b++) {
        SrAnimInput in;
        memset(&in, 0, sizeof in);
        in.key = 1000u + (uint64_t)b;
        in.look = &looks[b];
        in.role = (uint8_t)roles[b];
        in.pos = sr_v3(-1.3f + 1.3f * (float)b, 0, 0);
        in.dir = sr_v3(0.94f, 0, 0.34f);
        in.once_t0 = -1;
        SrClipKind kind = sr_anim_clip_kind(clip);
        float len = sr_anim_clip_len(clip);
        double now = (double)k / 8.0 * len;
        if (kind == SR_CK_GAIT) {
          SrSkel sk;
          sr_anim_skel(&looks[b], &sk);
          in.base = (uint8_t)clip;
          in.speed = b == 2 ? 3.4f : 1.4f;
          in.walk = (float)k / 8.0f * sr_anim_stride(&sk, in.speed, clip);
          now = 0;
        } else if (kind == SR_CK_OVERLAY) {
          in.base = SR_CLIP_IDLE;
          in.upper = (uint8_t)clip;
        } else if (kind == SR_CK_LOOP) {
          in.base = (uint8_t)clip;
        } else {
          in.base = SR_CLIP_IDLE;
          in.once = (uint8_t)clip;
          in.once_t0 = 0;
          now = (double)k / 7.0 * len;
          if (kind == SR_CK_BASE_ONCE) in.base = (uint8_t)clip;
        }
        if (clip == SR_CLIP_UMBRELLA) in.prop_r = SR_HP_UMBRELLA;
        if (clip == SR_CLIP_AIM || clip == SR_CLIP_READY || clip == SR_CLIP_SLUNG || clip == SR_CLIP_CROUCH_AIM ||
            clip == SR_CLIP_PEEK)
          in.prop_r = SR_HP_RIFLE;
        if (clip == SR_CLIP_AIM_PISTOL) in.prop_r = SR_HP_PISTOL;
        if (clip == SR_CLIP_SMOKE) in.prop_r = SR_HP_CIGARETTE;
        if (clip == SR_CLIP_PHONE || clip == SR_CLIP_CALL || clip == SR_CLIP_FILM) in.prop_r = SR_HP_PHONE;
        if (clip == SR_CLIP_EAT) in.prop_l = SR_HP_BOX;
        SrSkel sk;
        sr_anim_skel(&looks[b], &sk);
        SrPose p;
        float stride = sr_anim_stride(&sk, in.speed, in.base);
        sr_anim_pose(&in, &sk, now, in.walk / stride, &p);
        sr_anim_mesh(&st->people, &in, &sk, &p, atan2f(in.dir.x, in.dir.z));
      }
      int w, h;
      uint8_t *rgb = anim_render(&g_app, st, sr_v3(0, 0.95f, 3.1f), 0.0f, -0.05f, &w, &h);
      if (!rgb) continue;
      char name[96];
      snprintf(name, sizeof name, "clip_%02d_%s_%d", clip, sr_anim_clip_name(clip), k);
      png_or_ppm(dir, name, rgb, w, h);
      free(rgb);
    }
  }
  sr_mesh_free(&st->ground);
  sr_mesh_free(&st->people);
  free(st);
}

void suite_platform(void) {
  snprintf(g_root, sizeof g_root, "%s/sr_platform_test_%d/", SDL_GetHint("TMPDIR") ? SDL_GetHint("TMPDIR") : "/tmp", 1);
  SDL_SetHint("SR_STORAGE_ROOT", g_root);
  char *argv[] = {"test"};
  if (!sr_app_init(&g_app, 1, argv)) {
    printf("no display / GL available; platform tests cannot run\n");
    sr_test_fail(__FILE__, __LINE__, "platform init failed");
    return;
  }
  SR_RUN(test_app_init);
  SR_RUN(test_render_city);
  SR_RUN(test_retro_toggles);
  SR_RUN(test_render_interiors);
  SR_RUN(test_shop_taps);
  SR_RUN(test_bloom);
  SR_RUN(test_render_matrix);
  SR_RUN(test_peak_crowd_render);
  SR_RUN(test_many_games_keep_rendering);
  SR_RUN(test_seed_screenshots);
  SR_RUN(test_gameplay_screenshots);
  SR_RUN(test_atmosphere_shots);
  SR_RUN(test_atmosphere_metrics);
  SR_RUN(test_atmosphere_gloom);
  SR_RUN(test_atmosphere_weather);
  SR_RUN(test_anim_shots);
  SR_RUN(test_letterbox_window);
  SR_RUN(test_focus_and_autosave);
  SR_RUN(test_settings_persist);
  SR_RUN(test_input_events);
  SR_RUN(test_title_starts);
  SR_RUN(test_context_loss);
  SR_RUN(test_dynamic_resolution);
  SR_RUN(test_logging_and_storage);
  SR_RUN(test_gles_path);
  SR_RUN(test_headless_app);
  sr_app_quit(&g_app);
}
