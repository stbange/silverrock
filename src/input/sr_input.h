/* Device-independent input: raw events from any device are mapped to actions. Game logic only
 * ever reads SrActionState. Includes rebinding, scheme detection and the touch overlay. */
#ifndef SR_INPUT_H
#define SR_INPUT_H

#include "core/sr_types.h"

typedef enum SrAction {
  SR_ACT_FIRE, SR_ACT_AIM, SR_ACT_JUMP, SR_ACT_CROUCH, SR_ACT_SPRINT, SR_ACT_INTERACT,
  SR_ACT_RELOAD, SR_ACT_GADGET, SR_ACT_QUICK1, SR_ACT_QUICK2, SR_ACT_QUICK3, SR_ACT_QUICK4,
  SR_ACT_INVENTORY, SR_ACT_MAP, SR_ACT_JACKIN, SR_ACT_NETSIGHT, SR_ACT_PAUSE,
  SR_ACT_TERMINAL, SR_ACT_COUNT
} SrAction;

typedef enum SrScheme { SR_SCHEME_KBM, SR_SCHEME_GAMEPAD, SR_SCHEME_TOUCH } SrScheme;

/* Device-neutral key codes (subset); the platform layer translates SDL scancodes. */
typedef enum SrKey {
  SR_KEY_NONE = 0, SR_KEY_W, SR_KEY_A, SR_KEY_S, SR_KEY_D, SR_KEY_SPACE, SR_KEY_C, SR_KEY_E,
  SR_KEY_R, SR_KEY_Q, SR_KEY_F, SR_KEY_1, SR_KEY_2, SR_KEY_3, SR_KEY_4, SR_KEY_TAB, SR_KEY_M,
  SR_KEY_J, SR_KEY_V, SR_KEY_ESCAPE, SR_KEY_LSHIFT, SR_KEY_T, SR_KEY_MOUSE_LEFT,
  SR_KEY_MOUSE_RIGHT, SR_KEY_UP, SR_KEY_DOWN, SR_KEY_LEFT, SR_KEY_RIGHT, SR_KEY_RETURN,
  SR_KEY_X, SR_KEY_Z, SR_KEY_N, SR_KEY_Y, SR_KEY_COUNT
} SrKey;

typedef enum SrPad {
  SR_PAD_NONE = 0, SR_PAD_SOUTH, SR_PAD_EAST, SR_PAD_WEST, SR_PAD_NORTH, SR_PAD_LB, SR_PAD_RB,
  SR_PAD_LT, SR_PAD_RT, SR_PAD_LSTICK, SR_PAD_RSTICK, SR_PAD_START, SR_PAD_BACK, SR_PAD_DUP,
  SR_PAD_DDOWN, SR_PAD_DLEFT, SR_PAD_DRIGHT, SR_PAD_COUNT
} SrPad;

#define SR_MAX_FINGERS 10

typedef struct SrFinger {
  int64_t id;
  bool active;
  float x, y;           /* normalised 0..1 */
  float start_x, start_y;
  int role;             /* 0 none, 1 stick, 2 look, 3 button */
  int button;           /* SrAction for buttons */
  double down_time;
} SrFinger;

typedef struct SrTouchButton {
  SrAction action;
  float x, y, size; /* centre and size in CSS pixels */
} SrTouchButton;

#define SR_TOUCH_BUTTONS 11

typedef struct SrTouchLayout {
  float width, height;     /* screen in CSS pixels */
  float inset_l, inset_r, inset_t, inset_b;
  float stick_radius;      /* CSS pixels */
  SrTouchButton buttons[SR_TOUCH_BUTTONS];
} SrTouchLayout;

typedef struct SrActionState {
  bool down[SR_ACT_COUNT];
  bool pressed[SR_ACT_COUNT];  /* went down this tick */
  bool released[SR_ACT_COUNT]; /* went up this tick */
  float move_x, move_y;        /* -1..1, y forward */
  float look_x, look_y;        /* radians this tick */
  bool sprint_from_stick;
  float vertical;              /* Matrix flight: up/down */
} SrActionState;

typedef struct SrInput {
  SrKey key_bind[SR_ACT_COUNT];
  SrPad pad_bind[SR_ACT_COUNT];
  bool toggle_mode[SR_ACT_COUNT]; /* aim/crouch/sprint may be toggles */
  bool toggled[SR_ACT_COUNT];
  bool key_down[SR_KEY_COUNT];
  bool pad_down[SR_PAD_COUNT];
  bool touch_down[SR_ACT_COUNT];
  float pad_lx, pad_ly, pad_rx, pad_ry;
  float mouse_dx, mouse_dy;
  float mouse_sensitivity;  /* radians per pixel */
  float pad_sensitivity;    /* radians per second at full deflection */
  float touch_sensitivity;  /* radians per CSS pixel */
  SrScheme scheme;
  bool gamepad_connected;
  bool gamepad_lost;        /* active gamepad disconnected: pause */
  bool pointer_lock_wanted;
  bool coarse_pointer;      /* platform hint */
  SrFinger fingers[SR_MAX_FINGERS];
  SrTouchLayout layout;
  float stick_x, stick_y;   /* current stick deflection -1..1 */
  float look_accum_x, look_accum_y;
  bool wheel_open;
  int wheel_choice;
  double now;
  bool prev_down[SR_ACT_COUNT];
  SrAction interact_icon_action;
  int interact_icon;        /* context icon id shown on the interact button, 0 none */
} SrInput;

void sr_input_init(SrInput *in, bool coarse_pointer);
const char *sr_action_name(SrAction a);
const char *sr_key_name(SrKey k);

/* Raw events (from the platform layer or tests). */
void sr_input_key(SrInput *in, SrKey k, bool down);
void sr_input_mouse_motion(SrInput *in, float dx, float dy);
void sr_input_gamepad_button(SrInput *in, SrPad b, bool down);
void sr_input_gamepad_axes(SrInput *in, float lx, float ly, float rx, float ry);
void sr_input_gamepad_connected(SrInput *in, bool connected);
/* Touch coordinates are normalised (0..1) over the screen. */
void sr_input_touch(SrInput *in, int64_t finger, float x, float y, int phase); /* 0 down 1 move 2 up */
void sr_input_set_screen(SrInput *in, float css_w, float css_h, float inset_l, float inset_r,
                         float inset_t, float inset_b);

/* Rebinding: returns the conflicting action, or -1 when the key was free (binding applied). */
int sr_input_rebind_key(SrInput *in, SrAction a, SrKey k);
void sr_input_swap_keys(SrInput *in, SrAction a, SrAction b);
/* Serialises bindings to "action=key;..." for settings storage. */
void sr_input_bindings_save(const SrInput *in, char *out, size_t cap);
bool sr_input_bindings_load(SrInput *in, const char *text);

/* Produces the action state for one simulation tick and clears per-tick deltas. */
SrActionState sr_input_tick(SrInput *in, double dt);

/* Touch layout helpers */
void sr_touch_layout_build(SrTouchLayout *l);
int sr_touch_hit_button(const SrTouchLayout *l, float css_x, float css_y);
bool sr_touch_button_in_safe_area(const SrTouchLayout *l, int i);

/* Aim assistance: returns the look multiplier for the current scheme when the crosshair is
 * `angle` radians away from a hostile target (negative: none in view). */
float sr_aim_assist(SrScheme scheme, float strength, float angle_to_target);

#endif
