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
  SrInput input;
  SrLoop loop;
  SrSceneCache scene;
  SrUi ui;
  SrFrame frame;
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
