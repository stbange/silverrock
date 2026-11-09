/* SDL3 platform layer: window and GL context, events to input, fixed-timestep loop, storage,
 * logging, focus handling, web glue. app/main.c forwards SDL's main callbacks here. */
#ifndef SR_PLATFORM_H
#define SR_PLATFORM_H

#include <SDL3/SDL.h>
#include <stdio.h>

#include "game/sr_game.h"
#include "input/sr_input.h"
#include "render/gl/sr_gl.h"
#include "ui/sr_ui.h"

typedef struct SrFileStore {
  char root[512];
} SrFileStore;

typedef struct SrApp {
  SDL_Window *window;
  SDL_GLContext gl;
  bool gles;
  SrGlBackend gfx;
  SrGame *game;
  bool map_drag; /* the finger on the map has moved: no tap when it lifts */
  float map_touch_x, map_touch_y;
  SrInput input;
  SrLoop loop;
  SrSceneCache scene;
  SrSceneCache body_scene; /* meat-cam: the body's surroundings during a dive */
  SrUi ui;
  SrFrame frame;
  SrFrame body_frame;
  SrDynRes dynres;
  SrFileStore files;
  SrStorage storage;
  SDL_Gamepad *pad;
  uint64_t last_ticks;
  bool quit;
  bool headless;
  bool portrait;
  bool context_lost;
  bool relative_mouse;
  int win_w, win_h;      /* pixels */
  float css_w, css_h;    /* logical size */
  double fixed_frame_dt; /* tests: run frames with a fixed dt instead of the clock (0 = real) */
  int frames;
  char log_path[600];
  FILE *log_file;
} SrApp;

/* Initialises everything; headless skips window and GL. Returns false on fatal error. */
bool sr_app_init(SrApp *a, int argc, char **argv);
void sr_app_shop_tap(SrApp *a, float ny);
/* The city map by pointer (nx, ny normalised 0..1): the buttons on touch, else go to the place there. */
void sr_app_map_tap(SrApp *a, float nx, float ny, bool touch);
/* Help by pointer: a topic, a setting on the last page, or the title to go back. */
void sr_app_help_tap(SrApp *a, float nx, float ny);
/* Help by key: G/N/P/R settings, up and down pages; true when the key was used. */
bool sr_app_help_key(SrApp *a, SDL_Scancode sc);
void sr_app_event(SrApp *a, const SDL_Event *e);
/* One frame: input, fixed steps, render, present. */
void sr_app_iterate(SrApp *a);
void sr_app_quit(SrApp *a);

/* Exposed for tests */
SrKey sr_platform_key(SDL_Scancode sc);
SrPad sr_platform_pad_button(int button);
void sr_filestore_init(SrFileStore *fs, const char *root, SrStorage *out);
SrResult sr_app_save_settings(SrApp *a);
void sr_app_load_settings(SrApp *a);
void sr_app_start_game(SrApp *a, uint64_t seed);
const char *sr_app_test_exec(SrApp *a, const char *cmd);

#endif
