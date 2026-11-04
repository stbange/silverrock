/* Platform and GL backend tests. They need a display: run under Xvfb (CI) or with SDL's
 * offscreen driver. Rendering uses Mesa llvmpipe in CI, so golden images are stable. */
#include "core/sr_log.h"
#include "game/sr_save.h"
#include "platform/sr_platform.h"
#include "sr_test.h"

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
  a->fixed_frame_dt = 0.0001; /* render without advancing the simulation */
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
  SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
  /* mouse, touch and gamepad also start from the title */
  SDL_Event e;
  const Uint32 types[3] = {SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_FINGER_DOWN, SDL_EVENT_GAMEPAD_BUTTON_DOWN};
  for (int i = 0; i < 3; i++) {
    g_app.game->screen = SR_SCREEN_TITLE;
    memset(&e, 0, sizeof e);
    e.type = types[i];
    sr_app_event(&g_app, &e);
    SR_CHECK_EQ(g_app.game->screen, SR_SCREEN_PLAY);
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
