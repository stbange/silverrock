/* Player settings, stored as a small key=value text file in user storage. */
#ifndef SR_SETTINGS_H
#define SR_SETTINGS_H

#include "core/sr_types.h"

typedef struct SrSettings {
  int internal_res; /* 0: 320x180, 1: 480x270 (default), 2: 640x360, 3: native */
  bool integer_scale;
  bool fullscreen;
  bool dither, quantize, vertex_snap, affine, crt;
  bool bloom; /* neon glow (off in the mobile preset) */
  bool dynamic_res;
  int street_detail;   /* 0 low, 1 medium, 2 high: street props, sprite distance, rain, reflections */
  bool reflections;    /* neon reflections on wet ground */
  bool pixel_lighting; /* per-pixel lighting of the ground and lower facades (desktop) */
  int brightness;      /* -2..2: lifts the dark tones for dark screens and bright rooms */
  float mouse_sensitivity;
  float touch_sensitivity;
  float aim_assist; /* 0..1 */
  bool toggle_aim, toggle_crouch, toggle_sprint;
  int ui_scale;   /* 1..4, 0 = automatic */
  int volume;     /* 0..100 */
  int difficulty; /* SrDifficulty */
  int time_rate;  /* in-game seconds per real second */
  char bindings[512];
} SrSettings;

void sr_settings_default(SrSettings *s);
/* Parses key=value lines; unknown keys are ignored. Returns number of keys applied. */
int sr_settings_parse(SrSettings *s, const char *text);
void sr_settings_write(const SrSettings *s, char *out, size_t cap);
/* Internal resolution for a preset and a window size. */
void sr_settings_internal_size(const SrSettings *s, int window_w, int window_h, int *w, int *h);

#endif
