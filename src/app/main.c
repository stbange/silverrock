/* Entry point: SDL3 main callbacks on every platform (on the web SDL drives them from
 * requestAnimationFrame). `--headless --script file.scn` runs a scenario and exits. */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include "game/sr_script.h"
#include "platform/sr_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GCOVR_EXCL_START reason: entry point; exercised by the app_headless_script game test, not unit tests */
static SrApp g_app;

static int run_headless_script(const char *path) {
  size_t len = 0;
  char *text = (char *)SDL_LoadFile(path, &len);
  if (!text) {
    fprintf(stderr, "cannot read %s\n", path);
    return 2;
  }
  char err[512] = "";
  int rc = sr_script_run(text, path, err, sizeof err);
  SDL_free(text);
  if (rc)
    fprintf(stderr, "FAIL %s\n", err);
  else
    printf("PASS %s\n", path);
  return rc;
}

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv) {
  *state = &g_app;
  bool headless = false;
  const char *script = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--headless") == 0) headless = true;
    if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) script = argv[++i];
  }
  if (headless && script) return run_headless_script(script) == 0 ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
  return sr_app_init(&g_app, argc, argv) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *event) {
  sr_app_event((SrApp *)state, event);
  return ((SrApp *)state)->quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *state) {
  sr_app_iterate((SrApp *)state);
  return ((SrApp *)state)->quit ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *state, SDL_AppResult result) {
  (void)result;
  SrApp *a = (SrApp *)state;
  if (a->game) sr_app_quit(a);
}
/* GCOVR_EXCL_STOP */
