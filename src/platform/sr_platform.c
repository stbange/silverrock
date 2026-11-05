#include "platform/sr_platform.h"

#include "core/sr_log.h"
#include "core/sr_str.h"
#include "game/sr_save.h"
#include "game/sr_script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define SETTINGS_FILE "settings.ini"

/* ---- file storage (atomic: temp file + rename) ------------------------------------------- */

static void join(const SrFileStore *fs, const char *name, char *out, size_t cap) {
  snprintf(out, cap, "%s%s", fs->root, name);
}

static void web_sync(void) {
#ifdef __EMSCRIPTEN__
  emscripten_run_script("if (typeof FS !== 'undefined' && FS.syncfs) FS.syncfs(false, function(e) {});");
#endif
}

static SrResult fs_write(void *user, const char *name, const void *data, size_t len) {
  char path[600], tmp[620];
  join((SrFileStore *)user, name, path, sizeof path);
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (!SDL_SaveFile(tmp, data, len)) return SR_ERR_IO;
  if (!SDL_RenamePath(tmp, path))
    return SR_ERR_IO; /* GCOVR_EXCL_LINE reason: rename failure needs a broken filesystem */
  web_sync();
  return SR_OK;
}

static SrResult fs_read(void *user, const char *name, void **data, size_t *len) {
  char path[600];
  join((SrFileStore *)user, name, path, sizeof path);
  *data = SDL_LoadFile(path, len);
  if (!*data) return SR_ERR_NOT_FOUND;
  /* SDL allocates with SDL_malloc; copy to plain malloc so callers can free() */
  void *copy = malloc(*len ? *len : 1);
  if (copy) memcpy(copy, *data, *len);
  SDL_free(*data);
  *data = copy;
  return copy ? SR_OK : SR_ERR_NOMEM;
}

static SrResult fs_remove(void *user, const char *name) {
  char path[600];
  join((SrFileStore *)user, name, path, sizeof path);
  if (!SDL_GetPathInfo(path, NULL)) return SR_ERR_NOT_FOUND;
  bool ok = SDL_RemovePath(path);
  web_sync();
  return ok ? SR_OK : SR_ERR_NOT_FOUND;
}

static bool fs_exists(void *user, const char *name) {
  char path[600];
  join((SrFileStore *)user, name, path, sizeof path);
  return SDL_GetPathInfo(path, NULL);
}

void sr_filestore_init(SrFileStore *fs, const char *root, SrStorage *out) {
  sr_strlcpy(fs->root, root, sizeof fs->root);
  size_t n = strlen(fs->root);
  if (n && fs->root[n - 1] != '/' && fs->root[n - 1] != '\\' && n + 1 < sizeof fs->root) {
    fs->root[n] = '/';
    fs->root[n + 1] = '\0';
  }
  SDL_CreateDirectory(fs->root);
  out->user = fs;
  out->write = fs_write;
  out->read = fs_read;
  out->remove = fs_remove;
  out->exists = fs_exists;
}

/* ---- logging -------------------------------------------------------------------------------- */

static void log_sink(void *user, SrLogLevel level, const char *sub, const char *msg) {
  SrApp *a = (SrApp *)user;
  char line[700];
  sr_log_format(line, sizeof line, (double)SDL_GetTicks() / 1000.0, level, sub, msg);
  if (a->log_file) {
    fprintf(a->log_file, "%s\n", line);
    fflush(a->log_file);
  }
  SDL_Log("%s", line); /* the browser console on the web */
}

/* ---- settings --------------------------------------------------------------------------------- */

static void apply_settings(SrApp *a) {
  SrSettings *s = &a->game->settings;
  a->input.mouse_sensitivity = s->mouse_sensitivity;
  a->input.touch_sensitivity = s->touch_sensitivity;
  a->input.toggle_mode[SR_ACT_AIM] = s->toggle_aim;
  a->input.toggle_mode[SR_ACT_CROUCH] = s->toggle_crouch;
  a->input.toggle_mode[SR_ACT_SPRINT] = s->toggle_sprint;
  if (s->bindings[0]) sr_input_bindings_load(&a->input, s->bindings);
}

void sr_app_load_settings(SrApp *a) {
  void *data = NULL;
  size_t len = 0;
  if (a->input.coarse_pointer) a->game->settings.bloom = false; /* mobile preset: no bloom by default */
  if (a->storage.read(a->storage.user, SETTINGS_FILE, &data, &len) == SR_OK) {
    char *text = (char *)malloc(len + 1);
    memcpy(text, data, len);
    text[len] = '\0';
    sr_settings_parse(&a->game->settings, text);
    free(text);
    free(data);
  }
  apply_settings(a);
}

SrResult sr_app_save_settings(SrApp *a) {
  sr_input_bindings_save(&a->input, a->game->settings.bindings, sizeof a->game->settings.bindings);
  char text[2048];
  sr_settings_write(&a->game->settings, text, sizeof text);
  SrResult r = a->storage.write(a->storage.user, SETTINGS_FILE, text, strlen(text));
  if (r != SR_OK) sr_log(SR_LOG_ERROR, "settings", "could not write settings (%s)", sr_result_name(r));
  return r;
}

/* ---- key and button mapping --------------------------------------------------------------------- */

SrKey sr_platform_key(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_W: return SR_KEY_W;
    case SDL_SCANCODE_A: return SR_KEY_A;
    case SDL_SCANCODE_S: return SR_KEY_S;
    case SDL_SCANCODE_D: return SR_KEY_D;
    case SDL_SCANCODE_SPACE: return SR_KEY_SPACE;
    case SDL_SCANCODE_C: return SR_KEY_C;
    case SDL_SCANCODE_E: return SR_KEY_E;
    case SDL_SCANCODE_R: return SR_KEY_R;
    case SDL_SCANCODE_Q: return SR_KEY_Q;
    case SDL_SCANCODE_F: return SR_KEY_F;
    case SDL_SCANCODE_1: return SR_KEY_1;
    case SDL_SCANCODE_2: return SR_KEY_2;
    case SDL_SCANCODE_3: return SR_KEY_3;
    case SDL_SCANCODE_4: return SR_KEY_4;
    case SDL_SCANCODE_TAB: return SR_KEY_TAB;
    case SDL_SCANCODE_M: return SR_KEY_M;
    case SDL_SCANCODE_J: return SR_KEY_J;
    case SDL_SCANCODE_V: return SR_KEY_V;
    case SDL_SCANCODE_ESCAPE: return SR_KEY_ESCAPE;
    case SDL_SCANCODE_LSHIFT: return SR_KEY_LSHIFT;
    case SDL_SCANCODE_T: return SR_KEY_T;
    case SDL_SCANCODE_UP: return SR_KEY_UP;
    case SDL_SCANCODE_DOWN: return SR_KEY_DOWN;
    case SDL_SCANCODE_LEFT: return SR_KEY_LEFT;
    case SDL_SCANCODE_RIGHT: return SR_KEY_RIGHT;
    case SDL_SCANCODE_RETURN: return SR_KEY_RETURN;
    case SDL_SCANCODE_X: return SR_KEY_X;
    case SDL_SCANCODE_Z: return SR_KEY_Z;
    case SDL_SCANCODE_N: return SR_KEY_N;
    case SDL_SCANCODE_Y: return SR_KEY_Y;
    default: return SR_KEY_NONE;
  }
}

SrPad sr_platform_pad_button(int b) {
  switch (b) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return SR_PAD_SOUTH;
    case SDL_GAMEPAD_BUTTON_EAST: return SR_PAD_EAST;
    case SDL_GAMEPAD_BUTTON_WEST: return SR_PAD_WEST;
    case SDL_GAMEPAD_BUTTON_NORTH: return SR_PAD_NORTH;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return SR_PAD_LB;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return SR_PAD_RB;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return SR_PAD_LSTICK;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return SR_PAD_RSTICK;
    case SDL_GAMEPAD_BUTTON_START: return SR_PAD_START;
    case SDL_GAMEPAD_BUTTON_BACK: return SR_PAD_BACK;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return SR_PAD_DUP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return SR_PAD_DDOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return SR_PAD_DLEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return SR_PAD_DRIGHT;
    default: return SR_PAD_NONE;
  }
}

/* ---- lifecycle ------------------------------------------------------------------------------------ */

static bool coarse_pointer(void) {
#ifdef __EMSCRIPTEN__
  return emscripten_run_script_int("(window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0") !=
         0;
#else
  return SDL_GetHintBoolean("SR_COARSE_POINTER", false);
#endif
}

static void update_size(SrApp *a) {
  if (!a->window) return;
  int lw = 1280, lh = 720;
  SDL_GetWindowSizeInPixels(a->window, &a->win_w, &a->win_h);
  SDL_GetWindowSize(a->window, &lw, &lh);
  a->css_w = (float)lw;
  a->css_h = (float)lh;
  a->portrait = lh > lw;
  sr_input_set_screen(&a->input, a->css_w, a->css_h, a->input.layout.inset_l, a->input.layout.inset_r,
                      a->input.layout.inset_t, a->input.layout.inset_b);
}

static bool create_gl(SrApp *a) {
#ifdef __EMSCRIPTEN__
  const bool try_core = false;
#else
  const bool try_core = !SDL_GetHintBoolean("SR_FORCE_GLES", false);
#endif
  for (int attempt = try_core ? 0 : 1; attempt < 2; attempt++) {
    a->gles = attempt == 1;
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, a->gles ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, a->gles ? 0 : 3);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    a->window = SDL_CreateWindow("Silverrock", 1280, 720,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!a->window) continue; /* GCOVR_EXCL_LINE reason: window creation only fails without a display */
    a->gl = SDL_GL_CreateContext(a->window);
    if (a->gl) break;
    SDL_DestroyWindow(a->window); /* GCOVR_EXCL_START reason: depends on the driver's GL support */
    a->window = NULL;
  } /* GCOVR_EXCL_STOP */
  if (!a->gl) {
    sr_log(SR_LOG_ERROR, "gl", "no OpenGL 3.3 or OpenGL ES 3.0 context: %s", SDL_GetError());
    return false;
  }
  SDL_GL_MakeCurrent(a->window, a->gl);
  SDL_GL_SetSwapInterval(1);
  if (!sr_gl_load((SrGlGetProc)SDL_GL_GetProcAddress)) {
    sr_log(SR_LOG_ERROR, "gl", "missing OpenGL entry points"); /* GCOVR_EXCL_LINE reason: driver failure */
    return false;                                              /* GCOVR_EXCL_LINE */
  }
  sr_log(SR_LOG_INFO, "gl", "using %s", a->gles ? "OpenGL ES 3.0" : "OpenGL 3.3 Core");
  return sr_glb_init(&a->gfx, a->gles);
}

static SrApp *g_web_app; /* the web exports need a handle; platform layer only */

bool sr_app_init(SrApp *a, int argc, char **argv) {
  memset(a, 0, sizeof *a);
  bool owner = g_web_app == NULL;
  if (owner) g_web_app = a;
  for (int i = 1; i < argc; i++)
    if (strcmp(argv[i], "--headless") == 0) a->headless = true;
#ifdef __EMSCRIPTEN__
  const char *root = "/save/";
#else
  char *pref = SDL_GetPrefPath("silverrock", "silverrock");
  char root[512];
  sr_strlcpy(root, pref ? pref : "./", sizeof root);
  SDL_free(pref);
  const char *override = SDL_GetHint("SR_STORAGE_ROOT");
  if (override) sr_strlcpy(root, override, sizeof root);
#endif
  sr_filestore_init(&a->files, root, &a->storage);
#ifndef __EMSCRIPTEN__
  snprintf(a->log_path, sizeof a->log_path, "%ssilverrock.log", a->files.root);
  if (owner) a->log_file = fopen(a->log_path, "w");
#endif
  if (owner) sr_log_set_sink(log_sink, a);
  a->game = sr_game_create();
  sr_input_init(&a->input, coarse_pointer());
  sr_app_load_settings(a);
  sr_scene_init(&a->scene, a->input.coarse_pointer);
  sr_ui_init(&a->ui);
  if (a->headless) return true;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
    sr_log(SR_LOG_ERROR, "platform", "SDL_Init failed: %s",
           SDL_GetError()); /* GCOVR_EXCL_LINE reason: needs a broken SDL */
    return false;           /* GCOVR_EXCL_LINE */
  }
  if (!create_gl(a)) return false; /* GCOVR_EXCL_LINE reason: depends on the GPU driver */
  update_size(a);
  a->last_ticks = SDL_GetTicksNS();
  return true;
}

void sr_app_start_game(SrApp *a, uint64_t seed) {
  sr_game_new(a->game, seed, NULL, SR_BG_STREET_KID, "Runner");
  a->game->max_agents = a->input.coarse_pointer ? 120 : 400;
  sr_scene_free(&a->scene);
  sr_scene_init(&a->scene, a->input.coarse_pointer);
}

static void autosave_if_needed(SrApp *a) {
  static int handled = 0; /* per-process: only one app exists */
  if (a->game->autosave_requests > handled && a->game->started) {
    handled = a->game->autosave_requests;
    sr_autosave(a->game, &a->storage);
  }
}

static void start_created(SrApp *a) {
  sr_game_create_confirm(a->game);
  a->game->max_agents = a->input.coarse_pointer ? 120 : 400;
  sr_scene_free(&a->scene);
  sr_scene_init(&a->scene, a->input.coarse_pointer);
}

/* Creation screen by pointer: left half cycles the background, right half re-rolls the look,
 * bottom right starts. */
static void create_tap(SrApp *a, float x, float y) {
  if (x < 0.5f)
    sr_game_create_select(a->game, (a->game->create_bg + 1) % SR_BG_COUNT);
  else if (y > 0.75f)
    start_created(a);
  else
    sr_game_create_randomize(a->game);
}

/* Shop by pointer: a row buys it, the bottom strip leaves. ny is normalised (0..1). */
void sr_app_shop_tap(SrApp *a, float ny) {
  if (ny > 0.88f) {
    sr_game_shop_close(a->game);
    return;
  }
  int row = sr_ui_shop_row_at(&a->ui, ny * (float)a->ui.h);
  if (row >= 0 && row < a->game->menu.count) sr_game_shop_buy(a->game, row);
}

void sr_app_event(SrApp *a, const SDL_Event *e) {
  SrGame *g = a->game;
  switch (e->type) {
    case SDL_EVENT_QUIT: a->quit = true; break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      bool down = e->type == SDL_EVENT_KEY_DOWN;
      if (down && g->screen == SR_SCREEN_CREATE) {
        SDL_Scancode sc = e->key.scancode;
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_5) sr_game_create_select(g, (int)(sc - SDL_SCANCODE_1));
        if (sc == SDL_SCANCODE_R) sr_game_create_randomize(g);
        if (sc == SDL_SCANCODE_DOWN) sr_game_create_select(g, (g->create_bg + 1) % SR_BG_COUNT);
        if (sc == SDL_SCANCODE_RETURN) start_created(a);
        break;
      }
      if (down && e->key.scancode == SDL_SCANCODE_RETURN && g->screen == SR_SCREEN_TITLE) {
        sr_game_begin_create(g, (uint64_t)SDL_GetTicksNS() ^ 0x5EEDu);
        break;
      }
      if (down && e->key.scancode == SDL_SCANCODE_F11 && a->window) {
        g->settings.fullscreen = !g->settings.fullscreen;
        SDL_SetWindowFullscreen(a->window, g->settings.fullscreen);
      }
      sr_input_key(&a->input, sr_platform_key(e->key.scancode), down);
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      if (a->relative_mouse) sr_input_mouse_motion(&a->input, e->motion.xrel, e->motion.yrel);
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      bool down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
      if (e->button.which == SDL_TOUCH_MOUSEID) break; /* synthesized from touch */
      if (down && g->screen == SR_SCREEN_TITLE) {
        sr_game_begin_create(g, (uint64_t)SDL_GetTicksNS() ^ 0x5EEDu);
        break;
      }
      if (down && g->screen == SR_SCREEN_CREATE) {
        float mx = a->css_w > 0 ? e->button.x / a->css_w : 0.5f, my = a->css_h > 0 ? e->button.y / a->css_h : 0.5f;
        create_tap(a, mx, my);
        break;
      }
      if (down && g->screen == SR_SCREEN_PAUSE) sr_game_confirm_resume(g);
      if (down && g->screen == SR_SCREEN_SHOP) {
        sr_app_shop_tap(a, a->css_h > 0 ? e->button.y / a->css_h : 0.5f);
        break;
      }
      if (down && g->screen == SR_SCREEN_PLAY && !a->relative_mouse && a->window) {
        a->relative_mouse = SDL_SetWindowRelativeMouseMode(a->window, true); /* pointer lock on the web */
        a->input.pointer_lock_wanted = true;
      }
      sr_input_key(&a->input, e->button.button == SDL_BUTTON_RIGHT ? SR_KEY_MOUSE_RIGHT : SR_KEY_MOUSE_LEFT, down);
      break;
    }
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP: {
      int phase = e->type == SDL_EVENT_FINGER_DOWN ? 0 : e->type == SDL_EVENT_FINGER_MOTION ? 1 : 2;
      if (phase == 0 && g->screen == SR_SCREEN_TITLE) {
        sr_input_touch(&a->input, (int64_t)e->tfinger.fingerID, e->tfinger.x, e->tfinger.y, 0);
        sr_input_touch(&a->input, (int64_t)e->tfinger.fingerID, e->tfinger.x, e->tfinger.y, 2);
        sr_game_begin_create(g, (uint64_t)SDL_GetTicksNS() ^ 0x5EEDu);
        break;
      }
      if (g->screen == SR_SCREEN_CREATE) {
        if (phase == 0) create_tap(a, e->tfinger.x, e->tfinger.y);
        break;
      }
      if (phase == 0 && g->screen == SR_SCREEN_PAUSE) sr_game_confirm_resume(g);
      if (g->screen == SR_SCREEN_SHOP) {
        if (phase == 0) sr_app_shop_tap(a, e->tfinger.y);
        break;
      }
      sr_input_touch(&a->input, (int64_t)e->tfinger.fingerID, e->tfinger.x, e->tfinger.y, phase);
      if (a->relative_mouse && a->window) {
        SDL_SetWindowRelativeMouseMode(a->window, false);
        a->relative_mouse = false;
      }
      break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
      if (!a->pad) a->pad = SDL_OpenGamepad(e->gdevice.which);
      sr_input_gamepad_connected(&a->input, true);
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      if (a->pad) SDL_CloseGamepad(a->pad);
      a->pad = NULL;
      sr_input_gamepad_connected(&a->input, false);
      if (a->input.gamepad_lost && g->screen == SR_SCREEN_PLAY) {
        sr_game_focus(g, false);
        sr_game_toast(g, "Controller disconnected. Reconnect to continue.");
      }
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && g->screen == SR_SCREEN_TITLE) {
        sr_game_begin_create(g, (uint64_t)SDL_GetTicksNS() ^ 0x5EEDu);
        break;
      }
      if (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && g->screen == SR_SCREEN_CREATE) {
        if (e->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
          start_created(a);
        else if (e->gbutton.button == SDL_GAMEPAD_BUTTON_WEST)
          sr_game_create_randomize(g);
        else
          sr_game_create_select(g, (g->create_bg + 1) % SR_BG_COUNT);
        break;
      }
      sr_input_gamepad_button(&a->input, sr_platform_pad_button(e->gbutton.button),
                              e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
      break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      float v = (float)e->gaxis.value / 32767.0f;
      float lx = a->input.pad_lx, ly = a->input.pad_ly, rx = a->input.pad_rx, ry = a->input.pad_ry;
      switch (e->gaxis.axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: lx = v; break;
        case SDL_GAMEPAD_AXIS_LEFTY: ly = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTX: rx = v; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: ry = v; break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: sr_input_gamepad_button(&a->input, SR_PAD_LT, v > 0.5f); break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: sr_input_gamepad_button(&a->input, SR_PAD_RT, v > 0.5f); break;
        default: break;
      }
      sr_input_gamepad_axes(&a->input, lx, ly, rx, ry);
      break;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
      sr_game_focus(g, false);
      autosave_if_needed(a);
      if (a->relative_mouse && a->window) {
        SDL_SetWindowRelativeMouseMode(a->window, false);
        a->relative_mouse = false;
      }
      break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: update_size(a); break;
    default: break;
  }
}

static int ui_scale(const SrApp *a) {
  float density = a->css_h > 0 ? (float)a->win_h / a->css_h : 1.0f;
  int s = (int)((float)sr_ui_auto_scale(a->css_h) * density + 0.5f);
  if (a->game->settings.ui_scale > 0) s = a->game->settings.ui_scale;
  return s < 1 ? 1 : s;
}

void sr_app_iterate(SrApp *a) {
  SrGame *g = a->game;
  uint64_t now = SDL_GetTicksNS();
  double dt = a->fixed_frame_dt > 0 ? a->fixed_frame_dt : (double)(now - a->last_ticks) / 1e9;
  a->last_ticks = now;
  int steps = sr_loop_advance(&a->loop, dt);
  bool rotate_block = a->portrait && a->input.scheme == SR_SCHEME_TOUCH;
  for (int i = 0; i < steps; i++) {
    SrActionState act = sr_input_tick(&a->input, SR_STEP_DT);
    if (a->input.gamepad_lost && g->screen == SR_SCREEN_PLAY) sr_game_focus(g, false);
    if (rotate_block) continue; /* portrait on a phone: paused behind the rotate prompt */
    sr_game_step(g, &act);
  }
  autosave_if_needed(a);
  a->frames++;
  if (a->headless || !a->window || a->context_lost) return;
  /* rendering */
  int iw, ih;
  sr_settings_internal_size(&g->settings, a->win_w, a->win_h, &iw, &ih);
  if (g->settings.dynamic_res) {
    float budget = a->input.coarse_pointer ? 33.3f : 16.7f;
    sr_dynres_update(&a->dynres, (float)(dt * 1000.0), budget, (float)dt);
    sr_dynres_apply(&a->dynres, &iw, &ih);
  }
  SrFrame *f = NULL;
  if (g->started) {
    sr_scene_build(&a->scene, g, (float)iw / (float)ih, &a->frame);
    a->frame.internal_w = iw;
    a->frame.internal_h = ih;
    f = &a->frame;
  }
  a->ui.px_per_css = a->css_w > 0 ? (float)a->win_w / a->css_w : 1.0f;
  sr_ui_begin(&a->ui, a->win_w, a->win_h, ui_scale(a));
  sr_ui_build(&a->ui, g, &a->input, a->portrait);
  sr_glb_render(&a->gfx, f, &a->ui, a->win_w, a->win_h, g->settings.integer_scale);
  SDL_GL_SwapWindow(a->window);
}

void sr_app_quit(SrApp *a) {
  if (a->game && a->game->started) sr_autosave(a->game, &a->storage);
  if (a->game) sr_app_save_settings(a);
  sr_scene_free(&a->scene);
  sr_ui_free(&a->ui);
  if (a->window) {
    sr_glb_shutdown(&a->gfx);
    SDL_GL_DestroyContext(a->gl);
    SDL_DestroyWindow(a->window);
  }
  if (a->pad) SDL_CloseGamepad(a->pad);
  sr_game_destroy(a->game);
  if (g_web_app == a) {
    sr_log_set_sink(NULL, NULL);
    g_web_app = NULL;
  }
  if (a->log_file) fclose(a->log_file);
  if (!a->headless) SDL_Quit();
  memset(a, 0, sizeof *a);
}

/* ---- test hooks (test builds only) ------------------------------------------------------------- */

#ifdef SR_TEST_HOOKS
const char *sr_app_test_exec(SrApp *a, const char *cmd) {
  static char out[512];
  out[0] = '\0';
  if (strcmp(cmd, "input.scheme") == 0) {
    static const char *const n[] = {"kbm", "gamepad", "touch"};
    snprintf(out, sizeof out, "%s", n[a->input.scheme]);
  } else if (strcmp(cmd, "overlay.visible") == 0) {
    snprintf(out, sizeof out, "%s",
             a->input.scheme == SR_SCHEME_TOUCH && a->game->screen == SR_SCREEN_PLAY ? "true" : "false");
  } else if (strcmp(cmd, "pointer.lock") == 0) {
    snprintf(out, sizeof out, "%s", a->relative_mouse ? "true" : "false");
  } else if (strcmp(cmd, "portrait") == 0) {
    snprintf(out, sizeof out, "%s", a->portrait ? "true" : "false");
  } else if (strcmp(cmd, "frames") == 0) {
    snprintf(out, sizeof out, "%d", a->frames);
  } else if (strcmp(cmd, "gl") == 0) {
    snprintf(out, sizeof out, "%s %d", a->gles ? "gles" : "core", a->gfx.ready);
  } else if (strcmp(cmd, "start") == 0) {
    sr_app_start_game(a, 0x5EEDC0DEull);
    snprintf(out, sizeof out, "ok");
  } else if (strcmp(cmd, "save_settings") == 0) {
    snprintf(out, sizeof out, "%s", sr_result_name(sr_app_save_settings(a)));
  } else {
    SrResult r = sr_game_command(a->game, cmd, out, sizeof out);
    if (r != SR_OK && !out[0]) snprintf(out, sizeof out, "error: %s", sr_result_name(r));
  }
  return out;
}
#endif

#ifdef __EMSCRIPTEN__
/* GCOVR_EXCL_START reason: Emscripten-only glue, verified by the Playwright tests */
#ifdef SR_TEST_HOOKS
EMSCRIPTEN_KEEPALIVE const char *sr_web_test_exec(const char *cmd) { return sr_app_test_exec(g_web_app, cmd); }
#endif
EMSCRIPTEN_KEEPALIVE void sr_web_set_safe_area(float l, float r, float t, float b) {
  SrInput *in = &g_web_app->input;
  sr_input_set_screen(in, in->layout.width, in->layout.height, l, r, t, b);
}
EMSCRIPTEN_KEEPALIVE void sr_web_context_lost(void) {
  g_web_app->context_lost = true;
  sr_glb_context_lost(&g_web_app->gfx);
}
EMSCRIPTEN_KEEPALIVE void sr_web_context_restored(void) {
  sr_glb_init(&g_web_app->gfx, true);
  g_web_app->context_lost = false;
}
/* GCOVR_EXCL_STOP */
#endif
