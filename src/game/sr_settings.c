#include "game/sr_settings.h"

#include "core/sr_str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sr_settings_default(SrSettings *s) {
  memset(s, 0, sizeof *s);
  s->internal_res = 1;
  s->integer_scale = false;
  s->dither = s->quantize = true;
  s->vertex_snap = s->affine = false;
  s->crt = false;
  s->bloom = true;
  s->dynamic_res = true;
  s->street_detail = 2;
  s->reflections = true;
  s->pixel_lighting = true;
  s->head_bob = true;
  s->camera_shake = 2;
  s->brightness = 0;
  s->mouse_sensitivity = 0.0025f;
  s->touch_sensitivity = 0.006f;
  s->aim_assist = 0.6f;
  s->volume = 80;
  s->difficulty = 1;
  s->time_rate = 20;
}

typedef enum { T_INT, T_BOOL, T_FLOAT, T_STR } FieldType;
typedef struct {
  const char *key;
  FieldType type;
  size_t offset;
} Field;

#define F(name, type)                                                                                                  \
  { #name, type, offsetof(SrSettings, name) }
static const Field k_fields[] = {F(internal_res, T_INT),
                                 F(integer_scale, T_BOOL),
                                 F(fullscreen, T_BOOL),
                                 F(dither, T_BOOL),
                                 F(quantize, T_BOOL),
                                 F(vertex_snap, T_BOOL),
                                 F(affine, T_BOOL),
                                 F(crt, T_BOOL),
                                 F(bloom, T_BOOL),
                                 F(dynamic_res, T_BOOL),
                                 F(street_detail, T_INT),
                                 F(reflections, T_BOOL),
                                 F(pixel_lighting, T_BOOL),
                                 F(brightness, T_INT),
                                 F(head_bob, T_BOOL),
                                 F(camera_shake, T_INT),
                                 F(mouse_sensitivity, T_FLOAT),
                                 F(touch_sensitivity, T_FLOAT),
                                 F(aim_assist, T_FLOAT),
                                 F(toggle_aim, T_BOOL),
                                 F(toggle_crouch, T_BOOL),
                                 F(toggle_sprint, T_BOOL),
                                 F(ui_scale, T_INT),
                                 F(volume, T_INT),
                                 F(difficulty, T_INT),
                                 F(time_rate, T_INT),
                                 F(bindings, T_STR)};
#undef F

int sr_settings_parse(SrSettings *s, const char *text) {
  int applied = 0;
  const char *p = text;
  while (*p) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    char line[640];
    if (len >= sizeof line) len = sizeof line - 1;
    memcpy(line, p, len);
    line[len] = '\0';
    p = nl ? nl + 1 : p + len;
    sr_str_trim(line);
    char *eq = strchr(line, '=');
    if (!eq || line[0] == '#') continue;
    *eq = '\0';
    const char *key = line, *val = eq + 1;
    for (size_t i = 0; i < SR_ARRAY_LEN(k_fields); i++) {
      if (strcmp(k_fields[i].key, key) != 0) continue;
      char *dst = (char *)s + k_fields[i].offset;
      int64_t iv;
      switch (k_fields[i].type) {
        case T_INT:
          if (!sr_parse_i64(val, &iv)) break;
          *(int *)dst = (int)iv;
          applied++;
          break;
        case T_BOOL:
          *(bool *)dst = strcmp(val, "1") == 0 || strcmp(val, "true") == 0;
          applied++;
          break;
        case T_FLOAT:
          *(float *)dst = (float)atof(val);
          applied++;
          break;
        default:
          sr_strlcpy(dst, val, sizeof s->bindings);
          applied++;
          break;
      }
    }
  }
  return applied;
}

void sr_settings_write(const SrSettings *s, char *out, size_t cap) {
  size_t o = 0;
  out[0] = '\0';
  for (size_t i = 0; i < SR_ARRAY_LEN(k_fields) && o < cap; i++) {
    const char *src = (const char *)s + k_fields[i].offset;
    int n = 0;
    switch (k_fields[i].type) {
      case T_INT: n = snprintf(out + o, cap - o, "%s=%d\n", k_fields[i].key, *(const int *)src); break;
      case T_BOOL: n = snprintf(out + o, cap - o, "%s=%d\n", k_fields[i].key, *(const bool *)src ? 1 : 0); break;
      case T_FLOAT: n = snprintf(out + o, cap - o, "%s=%.5f\n", k_fields[i].key, (double)*(const float *)src); break;
      default: n = snprintf(out + o, cap - o, "%s=%s\n", k_fields[i].key, src); break;
    }
    if (n < 0) break; /* GCOVR_EXCL_LINE reason: snprintf does not fail here */
    o += (size_t)n;
  }
}

void sr_settings_internal_size(const SrSettings *s, int ww, int wh, int *w, int *h) {
  static const int presets[3][2] = {{320, 180}, {480, 270}, {640, 360}};
  if (s->internal_res >= 0 && s->internal_res < 3) {
    *w = presets[s->internal_res][0];
    *h = presets[s->internal_res][1];
    if (ww > 0 && wh > 0) { /* follow the window's shape (wide phones): no side bars */
      float aspect = SR_CLAMP((float)ww / (float)wh, 4.0f / 3.0f, 2.4f);
      *w = ((int)((float)*h * aspect + 0.5f) + 1) & ~1;
    }
  } else {
    *w = ww > 0 ? ww : 480;
    *h = wh > 0 ? wh : 270;
  }
}
