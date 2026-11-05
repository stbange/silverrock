#include "input/sr_input.h"

#include "core/sr_str.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define WHEEL_HOLD_SECONDS 0.4
#define SPRINT_EDGE        0.9f

const char *sr_action_name(SrAction a) {
  static const char *const n[SR_ACT_COUNT] = {"fire",      "aim",    "jump",   "crouch",   "sprint", "interact",
                                              "reload",    "gadget", "quick1", "quick2",   "quick3", "quick4",
                                              "inventory", "map",    "jackin", "netsight", "pause",  "terminal"};
  return (unsigned)a < SR_ACT_COUNT ? n[a] : "?";
}

const char *sr_key_name(SrKey k) {
  static const char *const n[SR_KEY_COUNT] = {
      "none",      "W",          "A",  "S",    "D",    "Space", "C",      "E", "R",      "Q",      "F",
      "1",         "2",          "3",  "4",    "Tab",  "M",     "J",      "V", "Escape", "LShift", "T",
      "MouseLeft", "MouseRight", "Up", "Down", "Left", "Right", "Return", "X", "Z",      "N",      "Y"};
  return (unsigned)k < SR_KEY_COUNT ? n[k] : "?";
}

/* Touch layout in CSS pixels, inside the safe area:
 *  - top-right cluster (pause, terminal, map), so the HUD in the top-left stays clear;
 *  - right-hand columns from the bottom up, their row spacing shrinking on short screens so the
 *    highest button never reaches the top cluster; every button stays >= 48 CSS px;
 *  - the left half belongs to the floating move stick. */
void sr_touch_layout_build(SrTouchLayout *l) {
  /* columns from the right edge, each stacked bottom-up */
  static const SrAction cols[3][4] = {{SR_ACT_FIRE, SR_ACT_AIM, SR_ACT_RELOAD, SR_ACT_JACKIN},
                                      {SR_ACT_JUMP, SR_ACT_CROUCH, SR_ACT_GADGET, SR_ACT_COUNT},
                                      {SR_ACT_INTERACT, SR_ACT_COUNT, SR_ACT_COUNT, SR_ACT_COUNT}};
  static const float sizes[3][4] = {{72, 56, 52, 52}, {56, 52, 52, 0}, {60, 0, 0, 0}};
  static const SrAction top[3] = {SR_ACT_PAUSE, SR_ACT_TERMINAL, SR_ACT_MAP};
  float right = l->width - l->inset_r, bottom = l->height - l->inset_b, top_y = l->inset_t;
  float floor_y = top_y + 8 + 48 + 8; /* nothing in the columns above the top cluster */
  bool small = bottom - floor_y < 300;
  int n = 0;
  float x = right - 8 - (small ? 32 : 36);
  for (int c = 0; c < 3; c++) {
    float total = 0;
    int count = 0;
    for (int r = 0; r < 4 && cols[c][r] != SR_ACT_COUNT; r++, count++)
      total += (c == 0 && r == 0 && small) ? 64 : sizes[c][r];
    float gap = (bottom - 8 - floor_y - total) / (float)count;
    gap = gap > 24 ? 24 : gap < 4 ? 4 : gap;
    float y = bottom - 8;
    for (int r = 0; r < count; r++) {
      SrTouchButton *b = &l->buttons[n++];
      b->action = cols[c][r];
      b->size = (c == 0 && r == 0 && small) ? 64 : sizes[c][r];
      b->x = x;
      b->y = y - b->size / 2;
      y -= b->size + gap;
    }
    x -= c == 0 ? (small ? 32 : 36) + 30 + 10 : 28 + 30 + 12;
  }
  for (int t = 0; t < 3; t++) {
    SrTouchButton *b = &l->buttons[n++];
    b->action = top[t];
    b->size = 48;
    b->y = top_y + 8 + 24;
    b->x = right - 8 - 24 - (float)t * 56;
  }
  /* keep the historic slot order (fire, aim, jump, crouch, interact, reload, gadget, jack-in, map,
   * pause, terminal): tests and saved layouts address buttons by index */
  static const SrAction order[SR_TOUCH_BUTTONS] = {SR_ACT_FIRE,     SR_ACT_AIM,    SR_ACT_JUMP,    SR_ACT_CROUCH,
                                                   SR_ACT_INTERACT, SR_ACT_RELOAD, SR_ACT_GADGET,  SR_ACT_JACKIN,
                                                   SR_ACT_MAP,      SR_ACT_PAUSE,  SR_ACT_TERMINAL};
  SrTouchButton sorted[SR_TOUCH_BUTTONS];
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++)
    for (int j = 0; j < SR_TOUCH_BUTTONS; j++)
      if (l->buttons[j].action == order[i]) sorted[i] = l->buttons[j];
  memcpy(l->buttons, sorted, sizeof sorted);
  l->stick_radius = 60.0f;
}

/* Lowest y (CSS px) of the top button cluster: HUD text on the right must stay below it. */
float sr_touch_top_cluster_bottom(const SrTouchLayout *l) { return l->inset_t + 8 + 48; }
/* Left edge (CSS px) of the right-hand button area. */
float sr_touch_buttons_left(const SrTouchLayout *l) {
  float left = l->width;
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
    const SrTouchButton *b = &l->buttons[i];
    if (b->action == SR_ACT_PAUSE || b->action == SR_ACT_MAP || b->action == SR_ACT_TERMINAL) continue;
    if (b->x - b->size / 2 < left) left = b->x - b->size / 2;
  }
  return left;
}

void sr_input_set_screen(SrInput *in, float w, float h, float il, float ir, float it, float ib) {
  SrTouchLayout *l = &in->layout;
  l->width = w;
  l->height = h;
  l->inset_l = il;
  l->inset_r = ir;
  l->inset_t = it;
  l->inset_b = ib;
  sr_touch_layout_build(l);
}

int sr_touch_hit_button(const SrTouchLayout *l, float x, float y) {
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
    const SrTouchButton *b = &l->buttons[i];
    float r = b->size / 2 + 6; /* small forgiving margin */
    if (fabsf(x - b->x) <= r && fabsf(y - b->y) <= r) return i;
  }
  return -1;
}

bool sr_touch_button_in_safe_area(const SrTouchLayout *l, int i) {
  const SrTouchButton *b = &l->buttons[i];
  float h = b->size / 2;
  return b->x - h >= l->inset_l && b->x + h <= l->width - l->inset_r && b->y - h >= l->inset_t &&
         b->y + h <= l->height - l->inset_b;
}

void sr_input_init(SrInput *in, bool coarse) {
  memset(in, 0, sizeof *in);
  static const SrKey keys[SR_ACT_COUNT] = {SR_KEY_MOUSE_LEFT, SR_KEY_MOUSE_RIGHT, SR_KEY_SPACE, SR_KEY_C, SR_KEY_LSHIFT,
                                           SR_KEY_E,          SR_KEY_R,           SR_KEY_Q,     SR_KEY_1, SR_KEY_2,
                                           SR_KEY_3,          SR_KEY_4,           SR_KEY_TAB,   SR_KEY_M, SR_KEY_J,
                                           SR_KEY_V,          SR_KEY_ESCAPE,      SR_KEY_T};
  static const SrPad pads[SR_ACT_COUNT] = {SR_PAD_RT,    SR_PAD_LT,    SR_PAD_SOUTH, SR_PAD_EAST, SR_PAD_LSTICK,
                                           SR_PAD_WEST,  SR_PAD_RB,    SR_PAD_LB,    SR_PAD_DUP,  SR_PAD_DRIGHT,
                                           SR_PAD_DDOWN, SR_PAD_DLEFT, SR_PAD_NORTH, SR_PAD_BACK, SR_PAD_RSTICK,
                                           SR_PAD_NONE,  SR_PAD_START, SR_PAD_NONE};
  memcpy(in->key_bind, keys, sizeof keys);
  memcpy(in->pad_bind, pads, sizeof pads);
  in->mouse_sensitivity = 0.0025f;
  in->pad_sensitivity = 2.6f;
  in->touch_sensitivity = 0.006f;
  in->coarse_pointer = coarse;
  in->scheme = coarse ? SR_SCHEME_TOUCH : SR_SCHEME_KBM;
  sr_input_set_screen(in, 844, 390, 0, 0, 0, 0);
}

static void use_scheme(SrInput *in, SrScheme s) {
  in->scheme = s;
  in->pointer_lock_wanted = s == SR_SCHEME_KBM;
}

void sr_input_key(SrInput *in, SrKey k, bool down) {
  if ((unsigned)k >= SR_KEY_COUNT) return;
  in->key_down[k] = down;
  if (down) use_scheme(in, SR_SCHEME_KBM);
}

void sr_input_mouse_motion(SrInput *in, float dx, float dy) {
  in->mouse_dx += dx;
  in->mouse_dy += dy;
  if (fabsf(dx) + fabsf(dy) > 2.0f) use_scheme(in, SR_SCHEME_KBM);
}

void sr_input_gamepad_button(SrInput *in, SrPad b, bool down) {
  if ((unsigned)b >= SR_PAD_COUNT) return;
  in->pad_down[b] = down;
  if (down) use_scheme(in, SR_SCHEME_GAMEPAD);
}

void sr_input_gamepad_axes(SrInput *in, float lx, float ly, float rx, float ry) {
  in->pad_lx = lx;
  in->pad_ly = ly;
  in->pad_rx = rx;
  in->pad_ry = ry;
  if (fabsf(lx) + fabsf(ly) + fabsf(rx) + fabsf(ry) > 0.5f) use_scheme(in, SR_SCHEME_GAMEPAD);
}

void sr_input_gamepad_connected(SrInput *in, bool connected) {
  if (!connected && in->gamepad_connected && in->scheme == SR_SCHEME_GAMEPAD) in->gamepad_lost = true;
  if (connected) {
    in->gamepad_lost = false;
    use_scheme(in, SR_SCHEME_GAMEPAD);
  }
  in->gamepad_connected = connected;
}

static SrFinger *finger_slot(SrInput *in, int64_t id, bool create) {
  SrFinger *free_slot = NULL;
  for (int i = 0; i < SR_MAX_FINGERS; i++) {
    if (in->fingers[i].active && in->fingers[i].id == id) return &in->fingers[i];
    if (!in->fingers[i].active && !free_slot) free_slot = &in->fingers[i];
  }
  return create ? free_slot : NULL;
}

void sr_input_touch(SrInput *in, int64_t id, float x, float y, int phase) {
  use_scheme(in, SR_SCHEME_TOUCH);
  float cx = x * in->layout.width, cy = y * in->layout.height;
  if (phase == 0) {
    SrFinger *f = finger_slot(in, id, true);
    if (!f) return;
    memset(f, 0, sizeof *f);
    f->active = true;
    f->id = id;
    f->x = f->start_x = x;
    f->y = f->start_y = y;
    f->down_time = in->now;
    int b = sr_touch_hit_button(&in->layout, cx, cy);
    if (b >= 0) {
      f->role = 3;
      f->button = in->layout.buttons[b].action;
      in->touch_down[f->button] = true;
    } else if (x < 0.5f) {
      f->role = 1; /* floating stick centred where the thumb lands */
    } else {
      f->role = 2;
    }
    return;
  }
  SrFinger *f = finger_slot(in, id, false);
  if (!f) return;
  if (phase == 1) {
    if (f->role == 2) {
      in->look_accum_x += (x - f->x) * in->layout.width;
      in->look_accum_y += (y - f->y) * in->layout.height;
    }
    f->x = x;
    f->y = y;
    if (f->role == 3 && f->button == SR_ACT_GADGET && in->wheel_open) {
      float dx = x - f->start_x, dy = y - f->start_y;
      if (dx * dx + dy * dy > 0.0004f) {
        float ang = atan2f(dy, dx);
        in->wheel_choice = ((int)floorf((ang + 3.14159265f) / (2.0f * 3.14159265f) * 4.0f)) & 3;
      }
    }
    return;
  }
  /* up */
  if (f->role == 3) in->touch_down[f->button] = false;
  f->active = false;
}

static float stick_deflection(const SrInput *in, const SrFinger *f, float *ox, float *oy) {
  float dx = (f->x - f->start_x) * in->layout.width;
  float dy = (f->y - f->start_y) * in->layout.height;
  float r = in->layout.stick_radius;
  float len = sqrtf(dx * dx + dy * dy);
  float mag = len / r;
  if (mag > 1.0f) {
    dx /= mag;
    dy /= mag;
  }
  *ox = dx / r;
  *oy = -dy / r; /* screen up = forward */
  return len / r;
}

int sr_input_rebind_key(SrInput *in, SrAction a, SrKey k) {
  for (int i = 0; i < SR_ACT_COUNT; i++)
    if (i != (int)a && in->key_bind[i] == k) return i;
  in->key_bind[a] = k;
  return -1;
}

void sr_input_swap_keys(SrInput *in, SrAction a, SrAction b) {
  SrKey t = in->key_bind[a];
  in->key_bind[a] = in->key_bind[b];
  in->key_bind[b] = t;
}

void sr_input_bindings_save(const SrInput *in, char *out, size_t cap) {
  size_t o = 0;
  out[0] = '\0';
  for (int a = 0; a < SR_ACT_COUNT && o < cap; a++) {
    int n =
        snprintf(out + o, cap - o, "%s%s=%s", a ? ";" : "", sr_action_name((SrAction)a), sr_key_name(in->key_bind[a]));
    if (n < 0) break; /* GCOVR_EXCL_LINE reason: snprintf never fails here */
    o += (size_t)n;
  }
}

bool sr_input_bindings_load(SrInput *in, const char *text) {
  char buf[1024];
  sr_strlcpy(buf, text, sizeof buf);
  bool ok = true;
  char *save = buf;
  while (save && *save) {
    char *semi = strchr(save, ';');
    if (semi) *semi = '\0';
    char *eq = strchr(save, '=');
    if (eq) {
      *eq = '\0';
      int act = -1, key = -1;
      for (int a = 0; a < SR_ACT_COUNT; a++)
        if (strcmp(save, sr_action_name((SrAction)a)) == 0) act = a;
      for (int k = 0; k < SR_KEY_COUNT; k++)
        if (strcmp(eq + 1, sr_key_name((SrKey)k)) == 0) key = k;
      if (act >= 0 && key >= 0)
        in->key_bind[act] = (SrKey)key;
      else
        ok = false;
    } else {
      ok = false;
    }
    save = semi ? semi + 1 : NULL;
  }
  return ok;
}

SrActionState sr_input_tick(SrInput *in, double dt) {
  SrActionState s;
  memset(&s, 0, sizeof s);
  in->now += dt;
  bool raw[SR_ACT_COUNT];
  for (int a = 0; a < SR_ACT_COUNT; a++) {
    bool d = false;
    if (in->scheme == SR_SCHEME_KBM && in->key_bind[a] != SR_KEY_NONE) d = in->key_down[in->key_bind[a]];
    if (in->scheme == SR_SCHEME_GAMEPAD && in->pad_bind[a] != SR_PAD_NONE) d = in->pad_down[in->pad_bind[a]];
    if (in->scheme == SR_SCHEME_TOUCH) d = in->touch_down[a];
    raw[a] = d;
  }
  /* toggle actions flip on press */
  for (int a = 0; a < SR_ACT_COUNT; a++) {
    if (in->toggle_mode[a]) {
      if (raw[a] && !in->prev_down[a]) in->toggled[a] = !in->toggled[a];
      in->prev_down[a] = raw[a];
      s.down[a] = in->toggled[a];
    } else {
      s.down[a] = raw[a];
    }
  }
  for (int a = 0; a < SR_ACT_COUNT; a++) {
    bool was = in->toggle_mode[a] ? s.down[a] : in->prev_down[a];
    if (!in->toggle_mode[a]) {
      s.pressed[a] = s.down[a] && !was;
      s.released[a] = !s.down[a] && was;
      in->prev_down[a] = s.down[a];
    }
  }
  switch (in->scheme) {
    case SR_SCHEME_KBM:
      s.move_y = (in->key_down[SR_KEY_W] ? 1.0f : 0.0f) - (in->key_down[SR_KEY_S] ? 1.0f : 0.0f);
      s.move_x = (in->key_down[SR_KEY_D] ? 1.0f : 0.0f) - (in->key_down[SR_KEY_A] ? 1.0f : 0.0f);
      s.look_x = in->mouse_dx * in->mouse_sensitivity;
      s.look_y = in->mouse_dy * in->mouse_sensitivity;
      s.vertical = (in->key_down[SR_KEY_SPACE] ? 1.0f : 0.0f) - (in->key_down[SR_KEY_C] ? 1.0f : 0.0f);
      break;
    case SR_SCHEME_GAMEPAD: {
      float dz = 0.15f;
      s.move_x = fabsf(in->pad_lx) > dz ? in->pad_lx : 0.0f;
      s.move_y = fabsf(in->pad_ly) > dz ? -in->pad_ly : 0.0f;
      s.look_x = (fabsf(in->pad_rx) > dz ? in->pad_rx : 0.0f) * in->pad_sensitivity * (float)dt;
      s.look_y = (fabsf(in->pad_ry) > dz ? in->pad_ry : 0.0f) * in->pad_sensitivity * (float)dt;
      s.vertical = (in->pad_down[SR_PAD_SOUTH] ? 1.0f : 0.0f) - (in->pad_down[SR_PAD_EAST] ? 1.0f : 0.0f);
      break;
    }
    case SR_SCHEME_TOUCH:
      in->stick_x = in->stick_y = 0;
      for (int i = 0; i < SR_MAX_FINGERS; i++) {
        SrFinger *f = &in->fingers[i];
        if (!f->active) continue;
        if (f->role == 1) {
          float len = stick_deflection(in, f, &in->stick_x, &in->stick_y);
          s.sprint_from_stick = len > SPRINT_EDGE;
        }
        if (f->role == 3 && f->button == SR_ACT_GADGET && !in->wheel_open &&
            in->now - f->down_time >= WHEEL_HOLD_SECONDS)
          in->wheel_open = true;
      }
      s.move_x = in->stick_x;
      s.move_y = in->stick_y;
      s.look_x = in->look_accum_x * in->touch_sensitivity;
      s.look_y = in->look_accum_y * in->touch_sensitivity;
      if (s.sprint_from_stick) s.down[SR_ACT_SPRINT] = true;
      s.vertical = (in->touch_down[SR_ACT_JUMP] ? 1.0f : 0.0f) - (in->touch_down[SR_ACT_CROUCH] ? 1.0f : 0.0f);
      break;
  }
  if (!in->touch_down[SR_ACT_GADGET] && in->scheme == SR_SCHEME_TOUCH) in->wheel_open = false;
  in->mouse_dx = in->mouse_dy = 0;
  in->look_accum_x = in->look_accum_y = 0;
  return s;
}

float sr_aim_assist(SrScheme scheme, float strength, float angle) {
  if (scheme == SR_SCHEME_KBM || strength <= 0.0f || angle < 0.0f) return 1.0f;
  const float zone = 0.08f; /* ~4.5 degrees */
  if (angle > zone) return 1.0f;
  return 1.0f - 0.6f * SR_CLAMP(strength, 0.0f, 1.0f) * (1.0f - angle / zone);
}
