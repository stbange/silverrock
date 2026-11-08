/* Character animation: skeleton, keyframe clips, gaits with planted feet, blending, props.
 * See sr_anim.h for the conventions. */
#include "render/sr_anim.h"

#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <string.h>

#define J_RP SR_J_ROOT_PITCH
#define J_RR SR_J_ROOT_ROLL
#define J_RY SR_J_ROOT_Y
#define J_PX SR_J_PELVIS_X
#define J_PY SR_J_PELVIS_Y
#define J_PP SR_J_PELVIS_PITCH
#define J_PR SR_J_PELVIS_ROLL
#define J_PW SR_J_PELVIS_YAW
#define J_CP SR_J_CHEST_PITCH
#define J_CW SR_J_CHEST_YAW
#define J_CR SR_J_CHEST_ROLL
#define J_HP SR_J_HEAD_PITCH
#define J_HW SR_J_HEAD_YAW
#define J_HL SR_J_HIP_L
#define J_HR SR_J_HIP_R
#define J_BL SR_J_HIPA_L
#define J_BR SR_J_HIPA_R
#define J_KL SR_J_KNEE_L
#define J_KR SR_J_KNEE_R
#define J_SL SR_J_SHO_L
#define J_SR SR_J_SHO_R
#define J_OL SR_J_SHOA_L
#define J_OR SR_J_SHOA_R
#define J_EL SR_J_ELB_L
#define J_ER SR_J_ELB_R
#define J_FX SR_J_FX

static float clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static float smooth(float x) {
  x = clampf(x, 0, 1);
  return x * x * (3 - 2 * x);
}

/* ---- keyframe clips ----------------------------------------------------------------------- */

typedef struct KV {
  int8_t j;
  float v;
} KV;

typedef struct Key {
  float t;
  KV kv[18];
} Key;

enum { G_NONE, G_ALWAYS, G_FADE_OUT, G_FADE_IN };

typedef struct ClipDef {
  const char *name;
  uint8_t kind, ground;
  float len;
  int nkeys;
  const Key *keys;
} ClipDef;

/* arms hang slightly out and bent */
#define ARMS_REST                                                                                                      \
  {J_OL, 0.07f}, {J_OR, 0.07f}, {J_EL, 0.12f}, { J_ER, 0.12f }
#define CROUCH_LEGS                                                                                                    \
  {J_HL, 1.35f}, {J_HR, 1.3f}, {J_KL, 2.1f}, { J_KR, 2.05f }
#define KNEEL_LEGS                                                                                                     \
  {J_HL, 0.15f}, {J_HR, 0.15f}, {J_KL, 1.95f}, { J_KR, 1.95f }

static const Key k_idle[] = {
    {0.0f, {ARMS_REST, {J_CP, 0.02f}}},
    {0.25f, {{J_OL, 0.08f}, {J_OR, 0.08f}, {J_EL, 0.15f}, {J_ER, 0.15f}, {J_CP, 0.035f}, {J_HP, 0.02f}}},
    {0.5f,
     {ARMS_REST,
      {J_CP, 0.02f},
      {J_PX, 0.025f},
      {J_BL, 0.032f},
      {J_BR, -0.032f},
      {J_KL, 0.12f},
      {J_HL, 0.05f},
      {J_CR, -0.02f},
      {J_HW, 0.15f}}},
    {0.75f,
     {{J_OL, 0.08f},
      {J_OR, 0.08f},
      {J_EL, 0.15f},
      {J_ER, 0.15f},
      {J_CP, 0.035f},
      {J_PX, 0.02f},
      {J_BL, 0.026f},
      {J_BR, -0.026f},
      {J_KL, 0.1f},
      {J_HL, 0.04f},
      {J_HW, 0.1f}}},
};
static const Key k_crouch[] = {
    {0.0f, {CROUCH_LEGS, {J_CP, 0.35f}, {J_HP, -0.25f}, {J_SL, 0.5f}, {J_SR, 0.5f}, {J_EL, 0.9f}, {J_ER, 0.9f}}},
    {0.5f,
     {CROUCH_LEGS,
      {J_CP, 0.39f},
      {J_HP, -0.22f},
      {J_SL, 0.52f},
      {J_SR, 0.5f},
      {J_EL, 0.92f},
      {J_ER, 0.9f},
      {J_HW, 0.2f}}},
};
static const Key k_sit[] = {
    {0.0f,
     {{J_HL, 1.5f},
      {J_HR, 1.5f},
      {J_KL, 1.45f},
      {J_KR, 1.45f},
      {J_CP, 0.05f},
      {J_SL, 0.25f},
      {J_SR, 0.25f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, 0.1f},
      {J_OR, 0.1f}}},
    {0.5f,
     {{J_HL, 1.5f},
      {J_HR, 1.5f},
      {J_KL, 1.45f},
      {J_KR, 1.5f},
      {J_CP, 0.08f},
      {J_SL, 0.27f},
      {J_SR, 0.25f},
      {J_EL, 0.9f},
      {J_ER, 0.95f},
      {J_OL, 0.1f},
      {J_OR, 0.1f},
      {J_HW, 0.3f}}},
    {0.75f,
     {{J_HL, 1.5f},
      {J_HR, 1.5f},
      {J_KL, 1.45f},
      {J_KR, 1.45f},
      {J_CP, 0.06f},
      {J_SL, 0.25f},
      {J_SR, 0.25f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, 0.1f},
      {J_OR, 0.1f},
      {J_HW, -0.15f}}},
};
static const Key k_sleep[] = {
    {0.0f,
     {{J_RR, 1.5f},
      {J_RY, 0.3f},
      {J_HL, 0.9f},
      {J_HR, 0.8f},
      {J_KL, 1.2f},
      {J_KR, 1.3f},
      {J_SL, 0.9f},
      {J_SR, 0.8f},
      {J_EL, 1.4f},
      {J_ER, 1.5f},
      {J_HP, 0.2f}}},
    {0.5f,
     {{J_RR, 1.5f},
      {J_RY, 0.3f},
      {J_HL, 0.9f},
      {J_HR, 0.8f},
      {J_KL, 1.2f},
      {J_KR, 1.3f},
      {J_SL, 0.9f},
      {J_SR, 0.8f},
      {J_EL, 1.4f},
      {J_ER, 1.5f},
      {J_HP, 0.2f},
      {J_CP, 0.04f}}},
};
static const Key k_eat[] = {
    {0.0f, {{J_SR, 0.9f}, {J_ER, 2.2f}, {J_OR, 0.1f}, {J_SL, 0.45f}, {J_EL, 1.3f}, {J_OL, 0.05f}, {J_HP, 0.12f}}},
    {0.4f, {{J_SR, 1.0f}, {J_ER, 2.45f}, {J_OR, 0.12f}, {J_SL, 0.45f}, {J_EL, 1.3f}, {J_OL, 0.05f}, {J_HP, -0.05f}}},
    {0.7f, {{J_SR, 0.7f}, {J_ER, 1.95f}, {J_OR, 0.1f}, {J_SL, 0.48f}, {J_EL, 1.35f}, {J_OL, 0.05f}, {J_HP, 0.18f}}},
};
static const Key k_buy[] = {
    {0.0f, {ARMS_REST, {J_HP, 0.1f}}},
    {0.3f, {{J_SR, 1.2f}, {J_ER, 0.4f}, {J_OL, 0.07f}, {J_EL, 0.12f}, {J_CP, 0.1f}, {J_HP, 0.15f}}},
    {0.5f, {{J_SR, 1.15f}, {J_ER, 0.45f}, {J_OL, 0.07f}, {J_EL, 0.12f}, {J_CP, 0.1f}, {J_HP, 0.2f}}},
    {0.7f, {{J_SR, 0.6f}, {J_ER, 1.4f}, {J_OL, 0.07f}, {J_EL, 0.12f}, {J_CP, 0.15f}, {J_HP, 0.3f}}},
    {0.9f, {{J_SR, 0.1f}, {J_ER, 0.3f}, {J_OL, 0.07f}, {J_EL, 0.12f}, {J_HP, 0.1f}}},
};
static const Key k_lean[] = {
    {0.0f,
     {{J_RP, -0.1f},
      {J_HL, 0.25f},
      {J_KL, 0.2f},
      {J_SL, -0.15f},
      {J_SR, -0.15f},
      {J_EL, 0.3f},
      {J_ER, 0.3f},
      {J_OL, 0.15f},
      {J_OR, 0.15f}}},
    {0.5f,
     {{J_RP, -0.1f},
      {J_HL, 0.25f},
      {J_KL, 0.2f},
      {J_SL, -0.15f},
      {J_SR, -0.15f},
      {J_EL, 0.3f},
      {J_ER, 0.3f},
      {J_OL, 0.15f},
      {J_OR, 0.15f},
      {J_HW, 0.45f}}},
};
static const Key k_smoke[] = {
    {0.0f, {{J_SR, 0.15f}, {J_ER, 1.5f}, {J_OR, 0.1f}, {J_OL, 0.07f}, {J_EL, 0.2f}, {J_FX, 0.3f}}},
    {0.3f, {{J_SR, 0.55f}, {J_ER, 2.5f}, {J_OR, 0.15f}, {J_OL, 0.07f}, {J_EL, 0.2f}, {J_HP, -0.05f}, {J_FX, 1.0f}}},
    {0.45f, {{J_SR, 0.55f}, {J_ER, 2.5f}, {J_OR, 0.15f}, {J_OL, 0.07f}, {J_EL, 0.2f}, {J_HP, -0.05f}, {J_FX, 1.0f}}},
    {0.6f, {{J_SR, 0.2f}, {J_ER, 1.6f}, {J_OR, 0.1f}, {J_OL, 0.07f}, {J_EL, 0.2f}, {J_HP, -0.15f}, {J_FX, 0.4f}}},
    {0.85f, {{J_SR, 0.15f}, {J_ER, 1.4f}, {J_OR, 0.1f}, {J_OL, 0.07f}, {J_EL, 0.2f}, {J_FX, 0.3f}}},
};
static const Key k_phone[] = {
    {0.0f,
     {{J_SL, 0.45f},
      {J_SR, 0.45f},
      {J_EL, 1.4f},
      {J_ER, 1.4f},
      {J_OL, -0.1f},
      {J_OR, -0.1f},
      {J_HP, 0.45f},
      {J_FX, 1.0f}}},
    {0.5f,
     {{J_SL, 0.47f},
      {J_SR, 0.45f},
      {J_EL, 1.42f},
      {J_ER, 1.38f},
      {J_OL, -0.1f},
      {J_OR, -0.1f},
      {J_HP, 0.5f},
      {J_FX, 0.8f}}},
};
static const Key k_call[] = {
    {0.0f,
     {{J_SR, 0.5f},
      {J_ER, 2.6f},
      {J_OR, 0.25f},
      {J_SL, -0.1f},
      {J_EL, 1.6f},
      {J_OL, 0.5f},
      {J_HW, 0.1f},
      {J_HP, 0.05f},
      {J_FX, 0.6f}}},
    {0.5f,
     {{J_SR, 0.5f},
      {J_ER, 2.6f},
      {J_OR, 0.25f},
      {J_SL, -0.1f},
      {J_EL, 1.6f},
      {J_OL, 0.5f},
      {J_HW, -0.25f},
      {J_HP, 0.12f},
      {J_CR, 0.03f},
      {J_FX, 0.6f}}},
};
static const Key k_window[] = {
    {0.0f,
     {{J_SL, -0.15f},
      {J_SR, -0.15f},
      {J_EL, 0.3f},
      {J_ER, 0.3f},
      {J_OL, 0.12f},
      {J_OR, 0.12f},
      {J_HP, 0.08f},
      {J_HW, 0.3f},
      {J_CP, 0.06f}}},
    {0.5f,
     {{J_SL, -0.15f},
      {J_SR, -0.15f},
      {J_EL, 0.3f},
      {J_ER, 0.3f},
      {J_OL, 0.12f},
      {J_OR, 0.12f},
      {J_HP, 0.1f},
      {J_HW, -0.3f},
      {J_CP, 0.06f}}},
};
static const Key k_wait[] = {
    {0.0f, {ARMS_REST}},
    {0.2f, {ARMS_REST, {J_HW, 0.65f}, {J_CW, 0.1f}}},
    {0.4f, {ARMS_REST, {J_KL, 0.25f}, {J_HL, 0.12f}}},
    {0.6f, {ARMS_REST, {J_HW, -0.65f}, {J_CW, -0.1f}}},
    {0.8f, {ARMS_REST, {J_KL, 0.25f}, {J_HL, 0.12f}}},
};
static const Key k_dance[] = {
    {0.0f,
     {{J_KL, 0.4f},
      {J_KR, 0.4f},
      {J_HL, 0.2f},
      {J_HR, 0.2f},
      {J_SL, 0.6f},
      {J_SR, 0.2f},
      {J_EL, 1.2f},
      {J_ER, 1.6f},
      {J_CR, 0.1f},
      {J_HP, 0.1f}}},
    {0.25f,
     {{J_KL, 0.05f},
      {J_KR, 0.05f},
      {J_SL, 0.3f},
      {J_SR, 0.7f},
      {J_EL, 1.4f},
      {J_ER, 1.4f},
      {J_CR, -0.1f},
      {J_HW, 0.2f}}},
    {0.5f,
     {{J_KL, 0.4f},
      {J_KR, 0.4f},
      {J_HL, 0.2f},
      {J_HR, 0.2f},
      {J_SL, 0.2f},
      {J_SR, 0.6f},
      {J_EL, 1.6f},
      {J_ER, 1.2f},
      {J_CR, -0.1f},
      {J_HP, 0.1f}}},
    {0.75f,
     {{J_KL, 0.05f},
      {J_KR, 0.05f},
      {J_SL, 0.7f},
      {J_SR, 0.3f},
      {J_EL, 1.4f},
      {J_ER, 1.4f},
      {J_CR, 0.1f},
      {J_HW, -0.2f}}},
};
static const Key k_watch[] = {
    {0.0f, {ARMS_REST, {J_HP, -0.35f}, {J_CP, -0.05f}}},
    {0.5f, {ARMS_REST, {J_HP, -0.3f}, {J_CP, -0.05f}, {J_HW, 0.18f}}},
};
static const Key k_point[] = {
    {0.0f, {{J_SR, 1.5f}, {J_ER, 0.1f}, {J_OR, 0.1f}, {J_OL, 0.07f}, {J_EL, 0.3f}, {J_HP, -0.2f}, {J_CW, 0.2f}}},
    {0.5f, {{J_SR, 1.4f}, {J_ER, 0.15f}, {J_OR, 0.12f}, {J_OL, 0.07f}, {J_EL, 0.3f}, {J_HP, -0.25f}, {J_CW, 0.25f}}},
};
static const Key k_warm[] = {
    {0.0f,
     {{J_SL, 0.9f},
      {J_SR, 0.9f},
      {J_EL, 1.0f},
      {J_ER, 1.0f},
      {J_OL, -0.05f},
      {J_OR, -0.05f},
      {J_CP, 0.15f},
      {J_HP, 0.2f}}},
    {0.5f,
     {{J_SL, 0.95f},
      {J_SR, 0.85f},
      {J_EL, 1.05f},
      {J_ER, 0.95f},
      {J_OL, -0.05f},
      {J_OR, -0.05f},
      {J_CP, 0.15f},
      {J_HP, 0.2f}}},
};
static const Key k_beg[] = {
    {0.0f, {KNEEL_LEGS, {J_CP, 0.3f}, {J_HP, 0.3f}, {J_SR, 0.9f}, {J_ER, 0.5f}, {J_OL, 0.07f}, {J_EL, 0.4f}}},
    {0.5f, {KNEEL_LEGS, {J_CP, 0.32f}, {J_HP, 0.22f}, {J_SR, 1.0f}, {J_ER, 0.45f}, {J_OL, 0.07f}, {J_EL, 0.4f}}},
};
static const Key k_talk[] = {
    {0.0f, {{J_SR, 0.5f}, {J_ER, 1.3f}, {J_SL, 0.3f}, {J_EL, 1.0f}, {J_HW, 0.1f}, {J_HP, 0.02f}}},
    {0.3f, {{J_SR, 0.7f}, {J_ER, 1.0f}, {J_OR, 0.3f}, {J_SL, 0.3f}, {J_EL, 1.0f}, {J_HP, -0.05f}}},
    {0.6f, {{J_SL, 0.6f}, {J_EL, 1.2f}, {J_OL, 0.3f}, {J_SR, 0.3f}, {J_ER, 1.4f}, {J_HW, -0.1f}}},
    {0.85f, {{J_SR, 0.45f}, {J_ER, 1.3f}, {J_SL, 0.25f}, {J_EL, 1.0f}}},
};
static const Key k_listen[] = {
    {0.0f, {{J_SL, 0.3f}, {J_SR, 0.3f}, {J_EL, 1.9f}, {J_ER, 1.9f}, {J_OL, -0.15f}, {J_OR, -0.15f}, {J_HP, 0.05f}}},
    {0.2f, {{J_SL, 0.3f}, {J_SR, 0.3f}, {J_EL, 1.9f}, {J_ER, 1.9f}, {J_OL, -0.15f}, {J_OR, -0.15f}, {J_HP, 0.22f}}},
    {0.4f, {{J_SL, 0.3f}, {J_SR, 0.3f}, {J_EL, 1.9f}, {J_ER, 1.9f}, {J_OL, -0.15f}, {J_OR, -0.15f}, {J_HP, 0.05f}}},
    {0.7f, {{J_SL, 0.3f}, {J_SR, 0.3f}, {J_EL, 1.9f}, {J_ER, 1.9f}, {J_OL, -0.15f}, {J_OR, -0.15f}, {J_HW, 0.12f}}},
};
static const Key k_film[] = {
    {0.0f,
     {{J_SL, 1.25f},
      {J_SR, 1.25f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, -0.2f},
      {J_OR, -0.2f},
      {J_HP, 0.05f},
      {J_FX, 1.0f}}},
    {0.5f,
     {{J_SL, 1.3f},
      {J_SR, 1.2f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, -0.2f},
      {J_OR, -0.2f},
      {J_CW, 0.1f},
      {J_FX, 1.0f}}},
};
static const Key k_cower[] = {
    {0.0f,
     {{J_HL, 1.4f},
      {J_HR, 1.4f},
      {J_KL, 2.2f},
      {J_KR, 2.2f},
      {J_CP, 0.7f},
      {J_HP, 0.5f},
      {J_SL, 2.4f},
      {J_SR, 2.4f},
      {J_EL, 2.2f},
      {J_ER, 2.2f},
      {J_OL, 0.3f},
      {J_OR, 0.3f}}},
    {0.5f,
     {{J_HL, 1.4f},
      {J_HR, 1.4f},
      {J_KL, 2.2f},
      {J_KR, 2.2f},
      {J_CP, 0.74f},
      {J_HP, 0.52f},
      {J_SL, 2.45f},
      {J_SR, 2.35f},
      {J_EL, 2.2f},
      {J_ER, 2.2f},
      {J_OL, 0.3f},
      {J_OR, 0.3f}}},
};
static const Key k_look_around[] = {
    {0.0f, {ARMS_REST}},
    {0.25f, {ARMS_REST, {J_HW, 0.85f}, {J_CW, 0.3f}}},
    {0.5f, {ARMS_REST}},
    {0.75f, {ARMS_REST, {J_HW, -0.85f}, {J_CW, -0.3f}}},
};
#define AIM_ARMS                                                                                                       \
  {J_SR, 0.9f}, {J_ER, 0.67f}, {J_OR, -0.1f}, {J_SL, 1.2f}, {J_EL, 0.37f}, { J_OL, -0.35f }
static const Key k_crouch_aim[] = {
    {0.0f, {CROUCH_LEGS, AIM_ARMS, {J_CP, 0.3f}, {J_HP, -0.2f}}},
    {0.5f, {CROUCH_LEGS, AIM_ARMS, {J_CP, 0.32f}, {J_HP, -0.2f}}},
};
static const Key k_peek[] = {
    {0.0f, {CROUCH_LEGS, AIM_ARMS, {J_CP, 0.3f}, {J_HP, -0.2f}}},
    {0.4f, {CROUCH_LEGS, AIM_ARMS, {J_CP, 0.25f}, {J_HP, -0.2f}, {J_CR, -0.45f}, {J_CW, 0.2f}}},
    {0.7f, {CROUCH_LEGS, AIM_ARMS, {J_CP, 0.25f}, {J_HP, -0.2f}, {J_CR, -0.45f}, {J_CW, 0.2f}}},
};
static const Key k_surrender[] = {
    {0.0f, {{J_SL, 2.7f}, {J_SR, 2.7f}, {J_EL, 0.5f}, {J_ER, 0.5f}, {J_OL, 0.5f}, {J_OR, 0.5f}, {J_HP, 0.1f}}},
    {0.5f, {{J_SL, 2.75f}, {J_SR, 2.65f}, {J_EL, 0.55f}, {J_ER, 0.5f}, {J_OL, 0.5f}, {J_OR, 0.5f}, {J_HP, 0.12f}}},
};
static const Key k_restrained[] = {
    {0.0f,
     {KNEEL_LEGS,
      {J_SL, -0.5f},
      {J_SR, -0.5f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, -0.1f},
      {J_OR, -0.1f},
      {J_CP, 0.25f},
      {J_HP, 0.35f}}},
    {0.5f,
     {KNEEL_LEGS,
      {J_SL, -0.5f},
      {J_SR, -0.5f},
      {J_EL, 0.9f},
      {J_ER, 0.9f},
      {J_OL, -0.1f},
      {J_OR, -0.1f},
      {J_CP, 0.27f},
      {J_HP, 0.4f},
      {J_HW, 0.15f}}},
};
static const Key k_stunned[] = {
    {0.0f,
     {{J_SL, 1.2f},
      {J_SR, 1.2f},
      {J_EL, 2.5f},
      {J_ER, 2.5f},
      {J_OL, -0.2f},
      {J_OR, -0.2f},
      {J_CP, 0.35f},
      {J_HP, 0.4f}}},
    {0.5f,
     {{J_SL, 1.2f},
      {J_SR, 1.2f},
      {J_EL, 2.5f},
      {J_ER, 2.5f},
      {J_OL, -0.2f},
      {J_OR, -0.2f},
      {J_CP, 0.3f},
      {J_HP, 0.35f},
      {J_CR, 0.12f},
      {J_KL, 0.2f}}},
};
#define LYING_BACK_KEY                                                                                                 \
  {J_RP, -1.5f}, {J_RY, 0.16f}, {J_OL, 0.9f}, {J_OR, 0.8f}, {J_SL, 0.3f}, {J_SR, 0.4f}, {J_EL, 0.3f}, {J_ER, 0.6f},    \
      {J_HL, 0.1f}, {J_KR, 0.4f}, {J_HR, 0.2f}, {                                                                      \
    J_HW, 0.5f                                                                                                         \
  }
#define LYING_FRONT_KEY                                                                                                \
  {J_RP, 1.5f}, {J_RY, 0.16f}, {J_OL, 0.7f}, {J_OR, 0.7f}, {J_SL, 0.5f}, {J_SR, 0.4f}, {J_EL, 0.8f}, {J_ER, 0.8f},     \
      {J_HW, 0.9f}, {                                                                                                  \
    J_KL, 0.3f                                                                                                         \
  }
#define LYING_SIDE_KEY                                                                                                 \
  {J_RR, 1.5f}, {J_RY, 0.3f}, {J_HL, 0.5f}, {J_HR, 0.4f}, {J_KL, 0.7f}, {J_KR, 0.8f}, {J_SL, 0.6f}, {J_SR, 0.5f},      \
      {J_EL, 0.8f}, {J_ER, 0.7f}, {                                                                                    \
    J_HP, 0.2f                                                                                                         \
  }
static const Key k_lying_back[] = {{0.0f, {LYING_BACK_KEY}}};
static const Key k_lying_front[] = {{0.0f, {LYING_FRONT_KEY}}};
static const Key k_lying_side[] = {{0.0f, {LYING_SIDE_KEY}}};
static const Key k_aim[] = {{0.0f, {AIM_ARMS, {J_HP, 0.08f}, {J_CW, 0.15f}}}};
static const Key k_aim_pistol[] = {
    {0.0f, {{J_SR, 1.5f}, {J_ER, 0.07f}, {J_OR, -0.05f}, {J_SL, 1.45f}, {J_EL, 0.1f}, {J_OL, -0.3f}}}};
static const Key k_ready[] = {
    {0.0f, {{J_SR, 0.5f}, {J_ER, 0.9f}, {J_OR, -0.05f}, {J_SL, 0.7f}, {J_EL, 0.9f}, {J_OL, -0.35f}}}};
static const Key k_slung[] = {
    {0.0f, {{J_SR, 0.35f}, {J_ER, 1.3f}, {J_OR, -0.2f}, {J_SL, 0.4f}, {J_EL, 1.5f}, {J_OL, -0.3f}}}};
static const Key k_beat[] = {
    {0.0f, {{J_SL, -0.1f}, {J_SR, -0.1f}, {J_EL, 1.2f}, {J_ER, 1.2f}, {J_OL, 0.45f}, {J_OR, 0.45f}}}};
static const Key k_wave[] = {
    {0.0f, {{J_SR, 0.9f}, {J_ER, 1.2f}, {J_OR, 0.3f}}},
    {0.5f, {{J_SR, 1.1f}, {J_ER, 0.6f}, {J_OR, 0.45f}}},
};
static const Key k_stop[] = {{0.0f, {{J_SR, 1.5f}, {J_ER, 0.2f}, {J_OR, 0.1f}}}};
static const Key k_umbrella[] = {{0.0f, {{J_SR, 0.55f}, {J_ER, 2.4f}, {J_OR, 0.12f}}}};
static const Key k_cover_head[] = {
    {0.0f, {{J_SL, 2.2f}, {J_SR, 2.2f}, {J_EL, 1.6f}, {J_ER, 1.6f}, {J_OL, 0.45f}, {J_OR, 0.45f}}}};
static const Key k_flee_arms[] = {
    {0.0f, {{J_SL, 2.3f}, {J_EL, 1.7f}, {J_OL, 0.4f}, {J_HW, 0.0f}}},
    {0.35f, {{J_SL, 2.3f}, {J_EL, 1.7f}, {J_OL, 0.4f}, {J_HW, 1.3f}}},
    {0.55f, {{J_SL, 2.3f}, {J_EL, 1.7f}, {J_OL, 0.4f}, {J_HW, 0.0f}}},
};
static const Key k_die_back[] = {
    {0.0f, {{J_RP, 0.0f}}},
    {0.25f,
     {{J_CP, -0.3f},
      {J_HP, -0.5f},
      {J_SL, 0.6f},
      {J_SR, 0.6f},
      {J_OL, 0.4f},
      {J_KL, 0.5f},
      {J_KR, 0.5f},
      {J_RP, -0.25f}}},
    {0.6f,
     {{J_RP, -1.15f},
      {J_KL, 0.6f},
      {J_KR, 0.3f},
      {J_OL, 0.8f},
      {J_OR, 0.8f},
      {J_SL, 0.3f},
      {J_SR, 0.3f},
      {J_HP, -0.2f},
      {J_RY, 0.08f}}},
    {1.0f, {LYING_BACK_KEY}},
};
static const Key k_die_front[] = {
    {0.0f, {{J_RP, 0.0f}}},
    {0.3f,
     {{J_KL, 1.0f}, {J_KR, 1.0f}, {J_HL, 0.5f}, {J_HR, 0.5f}, {J_CP, 0.5f}, {J_RP, 0.1f}, {J_SL, 0.4f}, {J_SR, 0.4f}}},
    {0.65f,
     {{J_RP, 1.1f}, {J_KL, 0.5f}, {J_KR, 0.4f}, {J_SL, 1.2f}, {J_SR, 1.2f}, {J_EL, 0.9f}, {J_ER, 0.9f}, {J_RY, 0.1f}}},
    {1.0f, {LYING_FRONT_KEY}},
};
static const Key k_die_side[] = {
    {0.0f, {{J_RR, 0.0f}}},
    {0.3f, {{J_RR, 0.4f}, {J_KL, 0.6f}, {J_HL, 0.3f}, {J_CR, 0.3f}, {J_OL, 0.5f}, {J_HP, 0.2f}}},
    {0.65f, {{J_RR, 1.25f}, {J_KL, 0.7f}, {J_KR, 0.6f}, {J_HL, 0.4f}, {J_SL, 0.5f}, {J_EL, 0.6f}, {J_RY, 0.2f}}},
    {1.0f, {LYING_SIDE_KEY}},
};
static const Key k_slump[] = {
    {0.0f, {{J_RR, 0.0f}}},
    {0.35f,
     {{J_KL, 1.5f},
      {J_KR, 1.5f},
      {J_HL, 0.8f},
      {J_HR, 0.8f},
      {J_CP, 0.6f},
      {J_HP, 0.6f},
      {J_SL, 0.2f},
      {J_SR, 0.2f},
      {J_EL, 0.3f}}},
    {0.7f,
     {{J_RR, 1.0f}, {J_KL, 1.2f}, {J_KR, 1.1f}, {J_HL, 0.7f}, {J_HR, 0.6f}, {J_CP, 0.3f}, {J_HP, 0.4f}, {J_RY, 0.2f}}},
    {1.0f, {LYING_SIDE_KEY}},
};
static const Key k_getup[] = {
    {0.0f, {LYING_SIDE_KEY}},
    {0.4f,
     {{J_RR, 0.8f},
      {J_RY, 0.15f},
      {J_KL, 1.6f},
      {J_KR, 1.6f},
      {J_HL, 1.2f},
      {J_HR, 1.2f},
      {J_SL, 0.9f},
      {J_EL, 0.3f},
      {J_CP, 0.5f}}},
    {0.7f, {{J_RR, 0.1f}, CROUCH_LEGS, {J_CP, 0.4f}, {J_SL, 0.4f}, {J_SR, 0.4f}}},
    {1.0f, {ARMS_REST}},
};
static const Key k_flinch[] = {
    {0.0f, {{J_CP, 0.0f}}},
    {0.25f,
     {{J_CP, -0.18f},
      {J_CW, 0.25f},
      {J_HP, -0.25f},
      {J_HW, 0.2f},
      {J_SL, 0.2f},
      {J_SR, 0.2f},
      {J_EL, 0.3f},
      {J_ER, 0.3f}}},
    {1.0f, {{J_CP, 0.0f}}},
};
static const Key k_stagger[] = {
    {0.0f, {{J_CP, 0.0f}}},
    {0.2f, {{J_CP, -0.25f}, {J_HL, -0.3f}, {J_KL, 0.3f}, {J_CW, 0.2f}, {J_OL, 0.3f}, {J_OR, 0.3f}}},
    {0.5f, {{J_CP, -0.1f}, {J_HR, -0.2f}, {J_KR, 0.25f}, {J_OL, 0.15f}, {J_OR, 0.15f}}},
    {1.0f, {{J_CP, 0.0f}}},
};
static const Key k_headshot[] = {
    {0.0f, {{J_HP, 0.0f}}},
    {0.2f, {{J_HP, -0.7f}, {J_CP, -0.2f}}},
    {1.0f, {{J_HP, 0.0f}}},
};
static const Key k_shake[] = {
    {0.0f, {{J_CW, 0.0f}}},
    {0.15f, {{J_CW, 0.3f}, {J_CR, 0.1f}, {J_OL, 0.3f}, {J_OR, 0.3f}}},
    {0.3f, {{J_CW, -0.3f}, {J_CR, -0.1f}, {J_OL, 0.3f}, {J_OR, 0.3f}}},
    {0.45f, {{J_CW, 0.25f}, {J_OL, 0.2f}, {J_OR, 0.2f}}},
    {0.6f, {{J_CW, -0.2f}}},
    {0.8f, {{J_SL, 0.4f}, {J_SR, 0.4f}, {J_EL, 1.0f}, {J_ER, 1.0f}}},
    {1.0f, {{J_CW, 0.0f}}},
};
static const Key k_recoil[] = {
    {0.0f, {{J_CP, 0.0f}}},
    {0.3f, {{J_CP, -0.3f}, {J_HP, -0.3f}, {J_SL, 0.6f}, {J_SR, 0.6f}, {J_EL, 1.2f}, {J_ER, 1.2f}}},
    {1.0f, {{J_CP, 0.0f}}},
};

#define CLIP(n, kind, g, len, keys)                                                                                    \
  { n, kind, g, len, (int)SR_ARRAY_LEN(keys), keys }
static const ClipDef k_clips[SR_CLIP_COUNT] = {
    [SR_CLIP_NONE] = {"none", SR_CK_LOOP, G_NONE, 1.0f, 0, NULL},
    [SR_CLIP_WALK] = {"walk", SR_CK_GAIT, G_NONE, 1.0f, 0, NULL},
    [SR_CLIP_CROUCH_WALK] = {"crouch_walk", SR_CK_GAIT, G_NONE, 1.0f, 0, NULL},
    [SR_CLIP_LIMP] = {"limp", SR_CK_GAIT, G_NONE, 1.0f, 0, NULL},
    [SR_CLIP_IDLE] = CLIP("idle", SR_CK_LOOP, G_ALWAYS, 4.0f, k_idle),
    [SR_CLIP_CROUCH] = CLIP("crouch", SR_CK_LOOP, G_ALWAYS, 3.0f, k_crouch),
    [SR_CLIP_SIT] = CLIP("sit", SR_CK_LOOP, G_ALWAYS, 6.0f, k_sit),
    [SR_CLIP_SLEEP] = CLIP("sleep", SR_CK_LOOP, G_NONE, 5.0f, k_sleep),
    [SR_CLIP_EAT] = CLIP("eat", SR_CK_LOOP, G_ALWAYS, 3.0f, k_eat),
    [SR_CLIP_BUY] = CLIP("buy", SR_CK_LOOP, G_ALWAYS, 4.0f, k_buy),
    [SR_CLIP_LEAN] = CLIP("lean", SR_CK_LOOP, G_ALWAYS, 6.0f, k_lean),
    [SR_CLIP_SMOKE] = CLIP("smoke", SR_CK_LOOP, G_ALWAYS, 6.0f, k_smoke),
    [SR_CLIP_PHONE] = CLIP("phone", SR_CK_LOOP, G_ALWAYS, 5.0f, k_phone),
    [SR_CLIP_CALL] = CLIP("call", SR_CK_LOOP, G_ALWAYS, 5.0f, k_call),
    [SR_CLIP_WINDOW] = CLIP("window", SR_CK_LOOP, G_ALWAYS, 7.0f, k_window),
    [SR_CLIP_WAIT] = CLIP("wait", SR_CK_LOOP, G_ALWAYS, 4.0f, k_wait),
    [SR_CLIP_DANCE] = CLIP("dance", SR_CK_LOOP, G_ALWAYS, 1.0f, k_dance),
    [SR_CLIP_WATCH] = CLIP("watch", SR_CK_LOOP, G_ALWAYS, 6.0f, k_watch),
    [SR_CLIP_POINT] = CLIP("point", SR_CK_LOOP, G_ALWAYS, 3.0f, k_point),
    [SR_CLIP_WARM] = CLIP("warm", SR_CK_LOOP, G_ALWAYS, 1.6f, k_warm),
    [SR_CLIP_BEG] = CLIP("beg", SR_CK_LOOP, G_ALWAYS, 4.0f, k_beg),
    [SR_CLIP_TALK] = CLIP("talk", SR_CK_LOOP, G_ALWAYS, 3.0f, k_talk),
    [SR_CLIP_LISTEN] = CLIP("listen", SR_CK_LOOP, G_ALWAYS, 3.0f, k_listen),
    [SR_CLIP_FILM] = CLIP("film", SR_CK_LOOP, G_ALWAYS, 4.0f, k_film),
    [SR_CLIP_COWER] = CLIP("cower", SR_CK_LOOP, G_ALWAYS, 0.6f, k_cower),
    [SR_CLIP_LOOK_AROUND] = CLIP("look_around", SR_CK_LOOP, G_ALWAYS, 4.0f, k_look_around),
    [SR_CLIP_CROUCH_AIM] = CLIP("crouch_aim", SR_CK_LOOP, G_ALWAYS, 2.0f, k_crouch_aim),
    [SR_CLIP_PEEK] = CLIP("peek", SR_CK_LOOP, G_ALWAYS, 2.4f, k_peek),
    [SR_CLIP_SURRENDER] = CLIP("surrender", SR_CK_LOOP, G_ALWAYS, 1.2f, k_surrender),
    [SR_CLIP_RESTRAINED] = CLIP("restrained", SR_CK_LOOP, G_ALWAYS, 4.0f, k_restrained),
    [SR_CLIP_STUNNED] = CLIP("stunned", SR_CK_LOOP, G_ALWAYS, 1.0f, k_stunned),
    [SR_CLIP_LYING_BACK] = CLIP("lying_back", SR_CK_LOOP, G_NONE, 1.0f, k_lying_back),
    [SR_CLIP_LYING_FRONT] = CLIP("lying_front", SR_CK_LOOP, G_NONE, 1.0f, k_lying_front),
    [SR_CLIP_LYING_SIDE] = CLIP("lying_side", SR_CK_LOOP, G_NONE, 1.0f, k_lying_side),
    [SR_CLIP_AIM] = CLIP("aim", SR_CK_OVERLAY, G_NONE, 1.0f, k_aim),
    [SR_CLIP_AIM_PISTOL] = CLIP("aim_pistol", SR_CK_OVERLAY, G_NONE, 1.0f, k_aim_pistol),
    [SR_CLIP_READY] = CLIP("ready", SR_CK_OVERLAY, G_NONE, 1.0f, k_ready),
    [SR_CLIP_SLUNG] = CLIP("slung", SR_CK_OVERLAY, G_NONE, 1.0f, k_slung),
    [SR_CLIP_BEAT] = CLIP("beat", SR_CK_OVERLAY, G_NONE, 1.0f, k_beat),
    [SR_CLIP_WAVE] = CLIP("wave", SR_CK_OVERLAY, G_NONE, 1.2f, k_wave),
    [SR_CLIP_STOP] = CLIP("stop", SR_CK_OVERLAY, G_NONE, 1.0f, k_stop),
    [SR_CLIP_UMBRELLA] = CLIP("umbrella", SR_CK_OVERLAY, G_NONE, 1.0f, k_umbrella),
    [SR_CLIP_COVER_HEAD] = CLIP("cover_head", SR_CK_OVERLAY, G_NONE, 1.0f, k_cover_head),
    [SR_CLIP_FLEE_ARMS] = CLIP("flee_arms", SR_CK_OVERLAY, G_NONE, 3.0f, k_flee_arms),
    [SR_CLIP_DIE_BACK] = CLIP("die_back", SR_CK_BASE_ONCE, G_FADE_OUT, 0.8f, k_die_back),
    [SR_CLIP_DIE_FRONT] = CLIP("die_front", SR_CK_BASE_ONCE, G_FADE_OUT, 0.9f, k_die_front),
    [SR_CLIP_DIE_SIDE] = CLIP("die_side", SR_CK_BASE_ONCE, G_FADE_OUT, 0.8f, k_die_side),
    [SR_CLIP_SLUMP] = CLIP("slump", SR_CK_BASE_ONCE, G_FADE_OUT, 1.0f, k_slump),
    [SR_CLIP_GETUP] = CLIP("getup", SR_CK_BASE_ONCE, G_FADE_IN, 1.4f, k_getup),
    [SR_CLIP_FLINCH] = CLIP("flinch", SR_CK_ADD_ONCE, G_NONE, 0.35f, k_flinch),
    [SR_CLIP_STAGGER] = CLIP("stagger", SR_CK_ADD_ONCE, G_NONE, 0.8f, k_stagger),
    [SR_CLIP_HEADSHOT] = CLIP("headshot", SR_CK_ADD_ONCE, G_NONE, 0.4f, k_headshot),
    [SR_CLIP_SHAKE] = CLIP("shake", SR_CK_ADD_ONCE, G_NONE, 1.2f, k_shake),
    [SR_CLIP_RECOIL] = CLIP("recoil", SR_CK_ADD_ONCE, G_NONE, 0.4f, k_recoil),
};

static const ClipDef *clip_def(int clip) { return &k_clips[clip > 0 && clip < SR_CLIP_COUNT ? clip : 0]; }

SrClipKind sr_anim_clip_kind(int clip) { return (SrClipKind)clip_def(clip)->kind; }
float sr_anim_clip_len(int clip) { return clip_def(clip)->len; }
const char *sr_anim_clip_name(int clip) { return clip_def(clip)->name; }

static void key_pose(const Key *k, float *out) {
  memset(out, 0, sizeof(float) * SR_J_COUNT);
  for (int i = 0; i < (int)SR_ARRAY_LEN(k->kv); i++)
    if (k->kv[i].j != 0 || k->kv[i].v != 0.0f) out[k->kv[i].j] = k->kv[i].v;
}

bool sr_anim_clip_keys_joint(int clip, int joint) {
  const ClipDef *d = clip_def(clip);
  for (int k = 0; k < d->nkeys; k++)
    for (int i = 0; i < (int)SR_ARRAY_LEN(d->keys[k].kv); i++) {
      const KV *kv = &d->keys[k].kv[i];
      if ((kv->j != 0 || kv->v != 0.0f) && kv->j == joint) return true;
    }
  return false;
}

static float catmull(float p0, float p1, float p2, float p3, float u) {
  return 0.5f * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u +
                 (-p0 + 3 * p1 - 3 * p2 + p3) * u * u * u);
}

void sr_anim_sample(int clip, float phase, SrPose *out) {
  const ClipDef *d = clip_def(clip);
  memset(out, 0, sizeof *out);
  if (d->nkeys == 0) return;
  if (d->nkeys == 1) {
    key_pose(&d->keys[0], out->j);
    return;
  }
  bool loop = d->kind == SR_CK_LOOP || d->kind == SR_CK_OVERLAY;
  int n = d->nkeys;
  float t = loop ? phase - floorf(phase) : clampf(phase, 0, 1);
  int i = 0;
  while (i + 1 < n && d->keys[i + 1].t <= t) i++;
  float t0 = d->keys[i].t, t1 = i + 1 < n ? d->keys[i + 1].t : (loop ? 1.0f + d->keys[0].t : 1.0f);
  float u = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0f;
  int i0 = loop ? (i - 1 + n) % n : SR_MAX(i - 1, 0);
  int i2 = loop ? (i + 1) % n : SR_MIN(i + 1, n - 1);
  int i3 = loop ? (i + 2) % n : SR_MIN(i + 2, n - 1);
  float p0[SR_J_COUNT], p1[SR_J_COUNT], p2[SR_J_COUNT], p3[SR_J_COUNT];
  key_pose(&d->keys[i0], p0);
  key_pose(&d->keys[i], p1);
  key_pose(&d->keys[i2], p2);
  key_pose(&d->keys[i3], p3);
  for (int j = 0; j < SR_J_COUNT; j++) out->j[j] = catmull(p0[j], p1[j], p2[j], p3[j], u);
}

/* ---- limits and mirroring ----------------------------------------------------------------- */

void sr_anim_limits(int joint, float *lo, float *hi) {
  static const float k[SR_J_COUNT][2] = {
      [J_RP] = {-1.65f, 1.65f}, [J_RR] = {-1.65f, 1.65f}, [J_RY] = {-0.2f, 1.0f}, [J_PX] = {-0.3f, 0.3f},
      [J_PY] = {-1.2f, 0.6f},   [J_PP] = {-0.6f, 0.9f},   [J_PR] = {-0.5f, 0.5f}, [J_PW] = {-0.8f, 0.8f},
      [J_CP] = {-0.8f, 1.1f},   [J_CW] = {-0.9f, 0.9f},   [J_CR] = {-0.6f, 0.6f}, [J_HP] = {-0.9f, 0.9f},
      [J_HW] = {-1.4f, 1.4f},   [J_HL] = {-0.9f, 2.0f},   [J_HR] = {-0.9f, 2.0f}, [J_BL] = {-0.4f, 0.9f},
      [J_BR] = {-0.4f, 0.9f},   [J_KL] = {0.0f, 2.5f},    [J_KR] = {0.0f, 2.5f},  [J_SL] = {-1.0f, 3.0f},
      [J_SR] = {-1.0f, 3.0f},   [J_OL] = {-0.6f, 1.6f},   [J_OR] = {-0.6f, 1.6f}, [J_EL] = {0.0f, 2.7f},
      [J_ER] = {0.0f, 2.7f},    [J_FX] = {0.0f, 1.0f},
  };
  *lo = k[joint][0];
  *hi = k[joint][1];
}

void sr_anim_clamp(SrPose *p) {
  for (int j = 0; j < SR_J_COUNT; j++) {
    float lo, hi;
    sr_anim_limits(j, &lo, &hi);
    p->j[j] = clampf(p->j[j], lo, hi);
  }
}

void sr_anim_mirror(SrPose *p) {
  static const int pairs[][2] = {{J_HL, J_HR}, {J_BL, J_BR}, {J_KL, J_KR}, {J_SL, J_SR}, {J_OL, J_OR}, {J_EL, J_ER}};
  for (int i = 0; i < (int)SR_ARRAY_LEN(pairs); i++) {
    float t = p->j[pairs[i][0]];
    p->j[pairs[i][0]] = p->j[pairs[i][1]];
    p->j[pairs[i][1]] = t;
  }
  static const int neg[] = {J_RR, J_PX, J_PR, J_PW, J_CW, J_CR, J_HW};
  for (int i = 0; i < (int)SR_ARRAY_LEN(neg); i++) p->j[neg[i]] = -p->j[neg[i]];
}

/* ---- skeleton and forward kinematics ------------------------------------------------------ */

void sr_anim_skel(const SrAppearance *a, SrSkel *s) {
  memset(s, 0, sizeof *s);
  s->tall = 1.55f + 0.08f * (float)(a->body % 3);
  s->bw = 0.22f + 0.04f * (float)a->body;
  s->hip = s->tall * 0.48f;
  s->ankle = 0.06f;
  s->thigh = s->hip * 0.5f;
  s->shin = s->hip - s->thigh - s->ankle;
  s->ua = s->tall * 0.17f;
  s->fa = s->tall * 0.135f;
  s->hand = s->tall * 0.055f;
  s->hs = 0.12f + 0.01f * (float)(a->head % 3);
  s->chest_h = s->tall * 0.82f - s->hip - 0.08f;
  s->neck_y = s->tall * 0.84f - s->hip - 0.08f;
  s->sho_x = s->bw + 0.1f;
  s->sho_y = s->tall * 0.8f - s->hip - 0.08f;
  s->leg_w = (s->bw - 0.02f) * 0.5f;
  s->hip_x = (s->bw + 0.02f) * 0.5f;
}

typedef struct Xf {
  SrVec3 x, y, z, t;
} Xf;

static SrVec3 xf_dir(const Xf *f, SrVec3 v) {
  return sr_v3(f->x.x * v.x + f->y.x * v.y + f->z.x * v.z, f->x.y * v.x + f->y.y * v.y + f->z.y * v.z,
               f->x.z * v.x + f->y.z * v.y + f->z.z * v.z);
}
static SrVec3 xf_pt(const Xf *f, SrVec3 p) { return sr_v3_add(f->t, xf_dir(f, p)); }

static Xf xf_mul(const Xf *a, const Xf *b) {
  Xf r;
  r.x = xf_dir(a, b->x);
  r.y = xf_dir(a, b->y);
  r.z = xf_dir(a, b->z);
  r.t = xf_pt(a, b->t);
  return r;
}

/* rotation yaw * pitch * roll at a pivot (see the sign conventions in sr_anim.h) */
static Xf xf_joint(SrVec3 pivot, float yaw, float pitch, float roll) {
  float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch), cr = cosf(roll), sr = sinf(roll);
  Xf ry = {sr_v3(cy, 0, -sy), sr_v3(0, 1, 0), sr_v3(sy, 0, cy), sr_v3(0, 0, 0)};
  Xf rx = {sr_v3(1, 0, 0), sr_v3(0, cp, sp), sr_v3(0, -sp, cp), sr_v3(0, 0, 0)};
  Xf rz = {sr_v3(cr, sr, 0), sr_v3(-sr, cr, 0), sr_v3(0, 0, 1), sr_v3(0, 0, 0)};
  Xf a = xf_mul(&ry, &rx), r = xf_mul(&a, &rz);
  r.t = pivot;
  return r;
}

typedef struct Bones {
  Xf root, pelvis, chest, head, thigh[2], shin[2], foot[2], uarm[2], farm[2];
} Bones;

static void bones(const SrSkel *s, const SrPose *p, const Xf *world, Bones *b) {
  const float *j = p->j;
  Xf root = xf_joint(sr_v3(0, j[J_RY], 0), 0, j[J_RP], j[J_RR]);
  b->root = xf_mul(world, &root);
  Xf pel = xf_joint(sr_v3(j[J_PX], s->hip + j[J_PY], 0), j[J_PW], j[J_PP], j[J_PR]);
  b->pelvis = xf_mul(&b->root, &pel);
  Xf ch = xf_joint(sr_v3(0, 0.08f, 0), j[J_CW], j[J_CP], j[J_CR]);
  b->chest = xf_mul(&b->pelvis, &ch);
  Xf hd = xf_joint(sr_v3(0, s->neck_y, 0), j[J_HW], j[J_HP], 0);
  b->head = xf_mul(&b->chest, &hd);
  for (int side = 0; side < 2; side++) {
    float sx = side == 0 ? -1.0f : 1.0f;
    Xf th = xf_joint(sr_v3(sx * s->hip_x, 0, 0), 0, -j[J_HL + side], sx * j[J_BL + side]);
    b->thigh[side] = xf_mul(&b->pelvis, &th);
    Xf sh = xf_joint(sr_v3(0, -s->thigh, 0), 0, j[J_KL + side], 0);
    b->shin[side] = xf_mul(&b->thigh[side], &sh);
    b->foot[side] = b->pelvis; /* feet stay level with the pelvis */
    b->foot[side].t = xf_pt(&b->shin[side], sr_v3(0, -s->shin, 0));
    Xf ua = xf_joint(sr_v3(sx * s->sho_x, s->sho_y, 0), 0, -j[J_SL + side], sx * j[J_OL + side]);
    b->uarm[side] = xf_mul(&b->chest, &ua);
    Xf fa = xf_joint(sr_v3(0, -s->ua, 0), 0, -j[J_EL + side], 0);
    b->farm[side] = xf_mul(&b->uarm[side], &fa);
  }
}

static const Xf k_ident = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};

SrVec3 sr_anim_point(const SrSkel *s, const SrPose *p, int which) {
  Bones b;
  bones(s, p, &k_ident, &b);
  if (which < 2) return b.foot[which].t;
  if (which < 4) return xf_pt(&b.farm[which - 2], sr_v3(0, -s->fa - s->hand * 0.5f, 0));
  return xf_pt(&b.head, sr_v3(0, 2 * s->hs, 0));
}

/* ---- gaits ------------------------------------------------------------------------------- */

typedef struct Gait {
  float speed, stride, duty, lift, kick, arm, elb, elbsw, lean;
} Gait;

static const Gait k_gaits[] = {
    {0.4f, 0.9f, 0.7f, 0.06f, 0.5f, 0.12f, 0.2f, 0.05f, 0.02f},
    {1.4f, 1.3f, 0.62f, 0.1f, 0.5f, 0.32f, 0.25f, 0.15f, 0.03f},
    {2.1f, 1.65f, 0.55f, 0.15f, 0.45f, 0.45f, 0.45f, 0.25f, 0.08f},
    {3.4f, 2.6f, 0.38f, 0.32f, 0.32f, 0.7f, 1.35f, 0.3f, 0.16f},
    {4.8f, 3.0f, 0.33f, 0.4f, 0.3f, 0.85f, 1.45f, 0.3f, 0.2f},
};

static Gait gait_at(float speed, int base) {
  int n = (int)SR_ARRAY_LEN(k_gaits);
  Gait g = k_gaits[0];
  if (speed >= k_gaits[n - 1].speed) {
    g = k_gaits[n - 1];
  } else {
    for (int i = 0; i + 1 < n; i++) {
      if (speed > k_gaits[i + 1].speed) continue;
      float u = clampf((speed - k_gaits[i].speed) / (k_gaits[i + 1].speed - k_gaits[i].speed), 0, 1);
      const float *a = &k_gaits[i].speed, *b = &k_gaits[i + 1].speed;
      float *o = &g.speed;
      for (int k = 0; k < (int)(sizeof(Gait) / sizeof(float)); k++) o[k] = a[k] + (b[k] - a[k]) * u;
      break;
    }
  }
  if (base == SR_CLIP_CROUCH_WALK) {
    g.stride *= 0.8f;
    g.duty = SR_MAX(g.duty, 0.62f);
    g.lift *= 0.8f;
    g.lean += 0.35f;
    g.arm *= 0.5f;
    g.elb += 0.6f;
  } else if (base == SR_CLIP_LIMP) {
    g.stride *= 0.75f;
    g.duty = SR_MAX(g.duty, 0.66f);
    g.arm *= 0.6f;
    g.lean += 0.08f;
  }
  return g;
}

float sr_anim_stride(const SrSkel *s, float speed, int base) { return gait_at(speed, base).stride * s->hip; }

static float gait_seed(uint64_t key, int salt) {
  return (float)(sr_mix64(key * 0x9E3779B97F4A7C15ull + (uint64_t)salt) % 1000u) / 999.0f * 2.0f - 1.0f;
}

void sr_anim_gait(const SrSkel *s, float speed, float phase, int base, uint64_t key, SrPose *out) {
  memset(out, 0, sizeof *out);
  Gait g = gait_at(speed, base);
  float L = s->hip, S = g.stride * L;
  float arm = g.arm * (1.0f + 0.15f * gait_seed(key, 1));
  float lean = g.lean + 0.04f * gait_seed(key, 2);
  float ph = phase - floorf(phase);
  float fy[2], fz[2];
  for (int side = 0; side < 2; side++) {
    float u = ph + (side == 1 ? 0.5f : 0.0f);
    u -= floorf(u);
    if (u < g.duty) {
      fz[side] = S * (g.duty * 0.5f - u);
      fy[side] = 0;
    } else {
      float t = (u - g.duty) / (1.0f - g.duty);
      float lift = g.lift * L * (base == SR_CLIP_LIMP && side == 1 ? 0.35f : 1.0f);
      float p = logf(0.5f) / logf(g.kick);
      fz[side] = -S * g.duty * 0.5f + S * g.duty * smooth(t);
      fy[side] = lift * sinf(SR_PI * powf(t, p));
    }
  }
  /* the pelvis rides as high as both legs can reach: the bob of the walk comes from the legs */
  float a = s->thigh, b = s->shin, R = (a + b) * 0.985f;
  float cap = (s->hip - s->ankle) * (base == SR_CLIP_CROUCH_WALK ? 0.74f : 0.975f);
  /* walking: a smooth rise and fall twice a stride, lowest just after each footfall */
  float zmax = clampf(S * g.duty * 0.5f, 0, R * 0.99f), hmin = sqrtf(R * R - zmax * zmax);
  float walkness = clampf((g.duty - 0.45f) / 0.1f, 0, 1);
  float curve = 0.5f * (cap + hmin) - 0.5f * (cap - hmin) * cosf(4.0f * SR_PI * (ph - (g.duty - 0.5f) * 0.5f));
  float h = cap + (fminf(cap, curve) - cap) * walkness;
  for (int side = 0; side < 2; side++) {
    float z = clampf(fz[side], -R * 0.99f, R * 0.99f);
    h = fminf(h, fy[side] + sqrtf(R * R - z * z));
  }
  out->j[J_PY] = h + s->ankle - s->hip;
  for (int side = 0; side < 2; side++) { /* two-bone leg in the side plane */
    float dz = fz[side], dy = fy[side] - h;
    float d = clampf(sqrtf(dz * dz + dy * dy), fabsf(a - b) + 1e-3f, (a + b) * 0.9999f);
    float ck = clampf((a * a + b * b - d * d) / (2 * a * b), -1, 1);
    float ca = clampf((a * a + d * d - b * b) / (2 * a * d), -1, 1);
    out->j[J_KL + side] = SR_PI - acosf(ck);
    out->j[J_HL + side] = atan2f(dz, -dy) + acosf(ca);
  }
  float c = cosf(2.0f * SR_PI * ph);
  out->j[J_SL] = -arm * c;
  out->j[J_SR] = arm * c;
  out->j[J_EL] = g.elb + g.elbsw * fmaxf(0.0f, c);
  out->j[J_ER] = g.elb + g.elbsw * fmaxf(0.0f, -c);
  out->j[J_OL] = out->j[J_OR] = 0.06f + 0.04f * g.lean;
  out->j[J_CP] = lean;
  out->j[J_CW] = 0.12f * arm * c;
  out->j[J_CR] = 0.03f * sinf(2.0f * SR_PI * ph) + (base == SR_CLIP_LIMP ? 0.08f * sinf(2.0f * SR_PI * ph) : 0.0f);
  out->j[J_HP] = -lean * 0.6f + (base == SR_CLIP_LIMP ? 0.15f : 0.0f);
  out->j[J_HW] = -0.5f * out->j[J_CW];
}

/* ---- full pose --------------------------------------------------------------------------- */

static void posture(int role, uint64_t key, SrPose *p) {
  switch (role) {
    case SR_ROLE_GUARD:
    case SR_ROLE_COP: p->j[J_CP] -= 0.03f; break;
    case SR_ROLE_DRIFTER:
      p->j[J_CP] += 0.16f;
      p->j[J_HP] += 0.08f;
      break;
    case SR_ROLE_EXEC:
      p->j[J_SL] *= 0.6f;
      p->j[J_SR] *= 0.6f;
      break;
    case SR_ROLE_GANG:
      p->j[J_OL] += 0.12f;
      p->j[J_OR] += 0.12f;
      p->j[J_CR] += 0.04f;
      break;
    default: break;
  }
  p->j[J_CP] += 0.03f * gait_seed(key, 3);
}

/* Lowers (or raises) the pelvis so the lowest ankle stands on the ground. */
static void ground(const SrSkel *s, SrPose *p, float w) {
  if (w <= 0) return;
  SrVec3 l = sr_anim_point(s, p, 0), r = sr_anim_point(s, p, 1);
  float low = fminf(l.y, r.y);
  p->j[J_PY] += (s->ankle - low) * w;
}

static bool lying(int b) { return b == SR_CLIP_SLEEP || (b >= SR_CLIP_LYING_BACK && b <= SR_CLIP_LYING_SIDE); }

static bool moving(const SrAnimInput *in) {
  if (in->once && sr_anim_clip_kind(in->once) == SR_CK_BASE_ONCE) return false;
  if (in->base == SR_CLIP_SIT || lying(in->base)) return false;
  return in->speed > 0.3f;
}

static float key_offset(uint64_t key) { return (float)(sr_mix64(key ^ 0xA11CEull) % 1000u) / 1000.0f; }

void sr_anim_pose(const SrAnimInput *in, const SrSkel *s, double now, float gait_phase, SrPose *out) {
  int base = in->base ? in->base : SR_CLIP_IDLE;
  bool once_base = in->once && sr_anim_clip_kind(in->once) == SR_CK_BASE_ONCE;
  float gw = 0;
  if (once_base) {
    const ClipDef *d = clip_def(in->once);
    float ph = in->once_t0 < 0 ? 1.0f : clampf((float)(now - in->once_t0) / d->len, 0, 1);
    sr_anim_sample(in->once, ph, out);
    gw = d->ground == G_FADE_OUT ? 1.0f - smooth(ph / 0.6f) : d->ground == G_FADE_IN ? smooth((ph - 0.4f) / 0.6f) : 0;
    if (in->mirror) sr_anim_mirror(out);
  } else if (moving(in)) {
    sr_anim_gait(s, in->speed, gait_phase, sr_anim_clip_kind(base) == SR_CK_GAIT ? base : SR_CLIP_WALK, in->key, out);
    posture(in->role, in->key, out);
  } else {
    if (sr_anim_clip_kind(base) != SR_CK_LOOP) base = SR_CLIP_IDLE;
    const ClipDef *d = clip_def(base);
    sr_anim_sample(base, (float)(now / d->len) + key_offset(in->key), out);
    if (!lying(base)) posture(in->role, in->key, out);
    gw = d->ground == G_ALWAYS ? 1.0f : 0.0f;
  }
  bool upright = !once_base && !lying(base);
  if (upright && in->hunch > 0) {
    out->j[J_CP] += 0.22f * in->hunch;
    out->j[J_HP] += 0.12f * in->hunch;
  }
  if (upright && in->upper && sr_anim_clip_kind(in->upper) == SR_CK_OVERLAY) {
    SrPose o;
    const ClipDef *d = clip_def(in->upper);
    sr_anim_sample(in->upper, (float)(now / d->len) + key_offset(in->key), &o);
    for (int j = 0; j < SR_J_COUNT; j++)
      if (sr_anim_clip_keys_joint(in->upper, j)) out->j[j] = o.j[j];
  }
  if (upright && in->look_at) {
    float yaw = atan2f(in->dir.x, in->dir.z), want = atan2f(in->target.x - in->pos.x, in->target.z - in->pos.z);
    float d = sr_wrap_angle(want - yaw);
    if (fabsf(d) < 2.2f) {
      out->j[J_HW] = clampf(d * 0.7f, -1.2f, 1.2f);
      out->j[J_CW] += clampf(d * 0.3f, -0.5f, 0.5f);
    }
  }
  if (in->once && sr_anim_clip_kind(in->once) == SR_CK_ADD_ONCE && in->once_t0 >= 0) {
    const ClipDef *d = clip_def(in->once);
    float ph = (float)(now - in->once_t0) / d->len;
    if (ph >= 0 && ph < 1) {
      SrPose a;
      sr_anim_sample(in->once, ph, &a);
      if (in->mirror) sr_anim_mirror(&a);
      for (int j = 0; j < SR_J_COUNT; j++) out->j[j] += a.j[j];
    }
  }
  sr_anim_clamp(out);
  ground(s, out, gw);
}

/* ---- mesh -------------------------------------------------------------------------------- */

static SrColor shade(SrColor c, int pct) {
  return (SrColor){(uint8_t)SR_MIN(255, c.r * pct / 100), (uint8_t)SR_MIN(255, c.g * pct / 100),
                   (uint8_t)SR_MIN(255, c.b * pct / 100), c.a};
}

static void box(SrMesh *m, const Xf *f, SrVec3 lo, SrVec3 hi, SrColor col, int layer, int em) {
  SrVec3 c[8];
  for (int i = 0; i < 8; i++)
    c[i] = xf_pt(f, sr_v3((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z));
  sr_mesh_quad(m, c[4], c[5], c[7], c[6], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[1], c[0], c[2], c[3], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[5], c[1], c[3], c[7], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[0], c[4], c[6], c[2], 0, 1, 1, 0, col, layer, em, 0);
  SrColor top = shade(col, 75);
  sr_mesh_quad(m, c[6], c[7], c[3], c[2], 0, 1, 1, 0, top, layer, em, 0);
  sr_mesh_quad(m, c[0], c[1], c[5], c[4], 0, 1, 1, 0, top, layer, em, 0);
}

/* A thin box from a to b. */
static void rod(SrMesh *m, SrVec3 a, SrVec3 b, float r, SrColor col, int em) {
  SrVec3 d = sr_v3_sub(b, a);
  float len = sr_v3_len(d);
  if (len < 1e-4f) return;
  Xf f;
  f.y = sr_v3_scale(d, 1.0f / len);
  SrVec3 up = fabsf(f.y.y) < 0.9f ? sr_v3(0, 1, 0) : sr_v3(1, 0, 0);
  f.x = sr_v3_norm(sr_v3_cross(up, f.y));
  f.z = sr_v3_cross(f.x, f.y);
  f.t = a;
  box(m, &f, sr_v3(-r, 0, -r), sr_v3(r, len, r), col, SR_LAYER_WHITE, em);
}

int sr_anim_accessory(const SrAppearance *a) {
  static const int k[8] = {SR_HP_NONE,      SR_HP_BRIEFCASE, SR_HP_BAG,  SR_HP_BOX,
                           SR_HP_CIGARETTE, SR_HP_PHONE,     SR_HP_NONE, SR_HP_NONE};
  int role = a->outfit / 4;
  int p = k[a->accessory & 7];
  if (p == SR_HP_BRIEFCASE && role != SR_ROLE_EXEC && role != SR_ROLE_FIXER && role != SR_ROLE_WORKER) p = SR_HP_BAG;
  if (role == SR_ROLE_GUARD || role == SR_ROLE_COP) p = SR_HP_NONE;
  return p;
}
bool sr_anim_has_coat(const SrAppearance *a) {
  int role = a->outfit / 4;
  return a->outfit % 4 == 2 && (role == SR_ROLE_EXEC || role == SR_ROLE_FIXER || role == SR_ROLE_DRIFTER ||
                                role == SR_ROLE_GANG || role == SR_ROLE_WORKER);
}
bool sr_anim_has_backpack(const SrAppearance *a) { return (a->accessory & 7) == 6 && a->outfit / 4 != SR_ROLE_EXEC; }

static void hand_prop(SrMesh *m, const Xf *hand, const SrSkel *s, int prop, float fx, const SrAnimInput *in) {
  float h = s->hand * 0.5f;
  switch (prop) {
    case SR_HP_BRIEFCASE:
      box(m, hand, sr_v3(-0.05f, -h - 0.32f, -0.2f), sr_v3(0.05f, -h - 0.02f, 0.2f), (SrColor){40, 28, 22, 255},
          SR_LAYER_WHITE, 0);
      break;
    case SR_HP_BAG:
      box(m, hand, sr_v3(-0.08f, -h - 0.34f, -0.15f), sr_v3(0.08f, -h - 0.04f, 0.15f), (SrColor){200, 190, 170, 255},
          SR_LAYER_WHITE, 0);
      break;
    case SR_HP_BOX:
      box(m, hand, sr_v3(-0.07f, -h - 0.02f, -0.07f), sr_v3(0.07f, -h + 0.08f, 0.07f), (SrColor){235, 225, 205, 255},
          SR_LAYER_WHITE, 20);
      break;
    case SR_HP_CIGARETTE:
      box(m, hand, sr_v3(-0.008f, -h - 0.01f, 0.0f), sr_v3(0.008f, -h + 0.006f, 0.08f), (SrColor){230, 230, 230, 255},
          SR_LAYER_WHITE, 0);
      box(m, hand, sr_v3(-0.01f, -h - 0.012f, 0.08f), sr_v3(0.01f, -h + 0.008f, 0.1f), (SrColor){255, 90, 30, 255},
          SR_LAYER_WHITE, (int)(90.0f + 165.0f * fx));
      break;
    case SR_HP_PHONE:
      box(m, hand, sr_v3(-0.04f, -h - 0.08f, -0.012f), sr_v3(0.04f, -h + 0.04f, 0.012f), (SrColor){20, 20, 24, 255},
          SR_LAYER_WHITE, 0);
      box(m, hand, sr_v3(-0.034f, -h - 0.07f, 0.012f), sr_v3(0.034f, -h + 0.03f, 0.016f), (SrColor){120, 200, 255, 255},
          SR_LAYER_WHITE, (int)(60.0f + 195.0f * fx));
      break;
    case SR_HP_UMBRELLA:
    case SR_HP_UMBRELLA_CLOSED: {
      SrColor uc = sr_corp_color((int)(in->look->accessory * 3 + in->look->outfit_color));
      bool open = prop == SR_HP_UMBRELLA;
      SrVec3 grip = xf_pt(hand, sr_v3(0, 0, 0));
      SrVec3 along = sr_v3_norm(xf_dir(hand, sr_v3(0, -1, 0)));
      if (!open) {
        SrVec3 tip = sr_v3_add(grip, sr_v3_scale(along, 0.8f));
        rod(m, grip, tip, 0.04f, shade(uc, 60), 0);
        break;
      }
      /* the rod points up; the canopy tilts into the wind */
      float tx = -in->wind_x * 0.12f, tz = -in->wind_z * 0.12f, tl = sqrtf(tx * tx + tz * tz);
      if (tl > 0.36f) { /* at most 20 degrees */
        tx *= 0.36f / tl;
        tz *= 0.36f / tl;
      }
      SrVec3 n = sr_v3_norm(sr_v3(tx, 1.0f, tz));
      SrVec3 top = sr_v3_add(grip, sr_v3_scale(n, 0.78f));
      rod(m, grip, top, 0.012f, (SrColor){60, 60, 60, 255}, 0);
      SrVec3 e1 = sr_v3_norm(sr_v3_cross(n, sr_v3(0, 0, 1))), e2 = sr_v3_cross(e1, n);
      SrVec3 apex = sr_v3_add(top, sr_v3_scale(n, 0.08f));
      for (int k = 0; k < 8; k++) {
        float a0 = (float)k / 8.0f * 2.0f * SR_PI, a1 = (float)(k + 1) / 8.0f * 2.0f * SR_PI;
        SrVec3 r0 = sr_v3_add(sr_v3_add(top, sr_v3_scale(e1, cosf(a0) * 0.58f)), sr_v3_scale(e2, sinf(a0) * 0.58f));
        SrVec3 r1 = sr_v3_add(sr_v3_add(top, sr_v3_scale(e1, cosf(a1) * 0.58f)), sr_v3_scale(e2, sinf(a1) * 0.58f));
        r0 = sr_v3_sub(r0, sr_v3_scale(n, 0.12f));
        r1 = sr_v3_sub(r1, sr_v3_scale(n, 0.12f));
        sr_mesh_quad(m, apex, r0, r1, apex, 0, 1, 1, 0, k & 1 ? uc : shade(uc, 80), SR_LAYER_WHITE, 30, 0);
      }
      break;
    }
    case SR_HP_PISTOL:
      box(m, hand, sr_v3(-0.025f, -h - 0.22f, -0.03f), sr_v3(0.025f, -h + 0.02f, 0.05f), (SrColor){35, 35, 40, 255},
          SR_LAYER_METAL, 0);
      break;
    case SR_HP_RIFLE:
      box(m, hand, sr_v3(-0.03f, -h - 0.62f, -0.04f), sr_v3(0.03f, -h + 0.32f, 0.07f), (SrColor){30, 32, 36, 255},
          SR_LAYER_METAL, 0);
      box(m, hand, sr_v3(-0.025f, -h - 0.15f, 0.07f), sr_v3(0.025f, -h - 0.02f, 0.16f), (SrColor){25, 25, 28, 255},
          SR_LAYER_METAL, 0);
      break;
    case SR_HP_BATON:
      box(m, hand, sr_v3(-0.02f, -h - 0.5f, -0.02f), sr_v3(0.02f, -h + 0.04f, 0.02f), (SrColor){25, 25, 30, 255},
          SR_LAYER_WHITE, 0);
      break;
    default: break;
  }
}

static SrColor cloth(const SrAppearance *a) {
  static const SrColor k[16] = {{40, 40, 46, 255},    {200, 200, 205, 255}, {150, 30, 40, 255},  {30, 60, 140, 255},
                                {40, 110, 60, 255},   {200, 160, 40, 255},  {90, 50, 120, 255},  {20, 20, 22, 255},
                                {230, 90, 30, 255},   {60, 160, 170, 255},  {170, 40, 140, 255}, {110, 80, 60, 255},
                                {240, 240, 240, 255}, {80, 90, 100, 255},   {255, 40, 120, 255}, {40, 220, 255, 255}};
  SrColor c = k[a->outfit_color & 15];
  return a->outfit % 4 != 0 ? c : shade(c, 80);
}
SrColor sr_anim_cloth(const SrAppearance *a) { return cloth(a); }
SrColor sr_anim_skin(const SrAppearance *a) {
  static const SrColor k[8] = {{250, 210, 180, 255}, {230, 180, 140, 255}, {200, 150, 110, 255}, {170, 120, 80, 255},
                               {140, 95, 60, 255},   {110, 70, 45, 255},   {80, 52, 35, 255},    {60, 40, 28, 255}};
  return k[a->skin & 7];
}

void sr_anim_mesh(SrMesh *m, const SrAnimInput *in, const SrSkel *s, const SrPose *p, float yaw) {
  static const SrColor hair[12] = {{20, 18, 16, 255},   {70, 45, 25, 255},    {160, 120, 60, 255}, {220, 210, 170, 255},
                                   {150, 40, 30, 255},  {200, 200, 200, 255}, {255, 40, 160, 255}, {40, 255, 200, 255},
                                   {120, 60, 255, 255}, {255, 230, 40, 255},  {40, 120, 255, 255}, {255, 90, 20, 255}};
  const SrAppearance *a = in->look;
  Xf world = xf_joint(in->pos, yaw, 0, 0);
  Bones b;
  bones(s, p, &world, &b);
  SrColor outfit = cloth(a), legs = shade(outfit, 60), sk = sr_anim_skin(a);
  SrColor sleeve = a->cyber >= 4 ? (SrColor){170, 175, 185, 255} : outfit; /* chrome arms */
  int arm_layer = a->cyber >= 4 ? SR_LAYER_METAL : SR_LAYER_WHITE;
  float bw = s->bw, w = s->leg_w;
  box(m, &b.pelvis, sr_v3(-bw - 0.02f, -0.1f, -0.13f), sr_v3(bw + 0.02f, 0.08f, 0.13f), legs, SR_LAYER_WHITE, 0);
  box(m, &b.chest, sr_v3(-bw - 0.04f, 0, -0.15f), sr_v3(bw + 0.04f, s->chest_h, 0.15f), outfit, SR_LAYER_WHITE, 0);
  float hs = s->hs;
  box(m, &b.head, sr_v3(-hs, 0, -hs), sr_v3(hs, 2 * hs, hs), sk, SR_LAYER_WHITE, 0);
  for (int side = 0; side < 2; side++) {
    box(m, &b.thigh[side], sr_v3(-w, -s->thigh, -0.11f), sr_v3(w, 0, 0.11f), legs, SR_LAYER_WHITE, 0);
    box(m, &b.shin[side], sr_v3(-w * 0.85f, -s->shin, -0.095f), sr_v3(w * 0.85f, 0, 0.095f), legs, SR_LAYER_WHITE, 0);
    box(m, &b.foot[side], sr_v3(-w * 0.9f, -s->ankle, -0.07f), sr_v3(w * 0.9f, 0, 0.17f), (SrColor){28, 26, 26, 255},
        SR_LAYER_WHITE, 0);
    box(m, &b.uarm[side], sr_v3(-0.05f, -s->ua, -0.065f), sr_v3(0.05f, 0, 0.065f), sleeve, arm_layer, 0);
    box(m, &b.farm[side], sr_v3(-0.045f, -s->fa, -0.055f), sr_v3(0.045f, 0, 0.055f), sleeve, arm_layer, 0);
    box(m, &b.farm[side], sr_v3(-0.04f, -s->fa - s->hand, -0.045f), sr_v3(0.04f, -s->fa, 0.045f),
        a->cyber >= 4 ? sleeve : sk, arm_layer, 0);
  }
  float top = 2 * hs;
  int style = a->hair % 12;
  SrColor hc = hair[a->hair_color % 12];
  int hem = a->hair_color >= 6 ? 120 : 0;
  if (in->hood) {
    box(m, &b.head, sr_v3(-hs - 0.03f, hs * 0.5f, -hs - 0.04f), sr_v3(hs + 0.03f, top + 0.04f, hs * 0.75f),
        shade(outfit, 85), SR_LAYER_WHITE, 0);
  } else if (style < 4) { /* mohawk / crest */
    box(m, &b.head, sr_v3(-0.03f, top, -hs), sr_v3(0.03f, top + 0.12f + 0.03f * (float)style, hs), hc, SR_LAYER_WHITE,
        hem);
  } else if (style < 9) { /* cap of hair */
    box(m, &b.head, sr_v3(-hs - 0.01f, top - 0.06f, -hs - 0.01f), sr_v3(hs + 0.01f, top + 0.03f, hs + 0.01f), hc,
        SR_LAYER_WHITE, hem);
  }
  if (a->cyber) /* glowing optics, on the face */
    box(m, &b.head, sr_v3(-hs * 0.8f, hs * 1.1f, hs), sr_v3(hs * 0.8f, hs * 1.35f, hs + 0.02f),
        (SrColor){255, 40, 40, 255}, SR_LAYER_WHITE, 255);
  if (a->marker) { /* faction marker: halo, shoulder plates or mask by marker id */
    SrColor mk = sr_corp_color(a->marker * 5);
    switch (a->marker % 3) {
      case 0:
        box(m, &b.head, sr_v3(-0.2f, top + 0.15f, -0.2f), sr_v3(0.2f, top + 0.18f, 0.2f), mk, SR_LAYER_WHITE, 255);
        break;
      case 1:
        box(m, &b.chest, sr_v3(-bw - 0.12f, s->chest_h - 0.06f, -0.16f), sr_v3(bw + 0.12f, s->chest_h + 0.02f, 0.16f),
            mk, SR_LAYER_WHITE, 60);
        break;
      default: box(m, &b.head, sr_v3(-hs, 0, hs), sr_v3(hs, hs, hs + 0.02f), mk, SR_LAYER_WHITE, 80); break;
    }
  }
  if (sr_anim_has_coat(a)) { /* coat tails follow the thighs at half their swing */
    for (int side = 0; side < 2; side++) {
      float sx = side == 0 ? -1.0f : 1.0f;
      float flap = 0.06f * sinf((float)in->key * 0.37f + in->pos.x * 0.5f) * (in->speed < 0.3f ? 1.0f : 0.0f);
      Xf tail = xf_joint(sr_v3(sx * s->hip_x, -0.04f, 0), 0, -0.5f * p->j[J_HL + side] - flap, 0);
      Xf t = xf_mul(&b.pelvis, &tail);
      box(m, &t, sr_v3(-w - 0.04f, -s->hip * 0.62f, -0.15f), sr_v3(w + 0.04f, 0, 0.15f), shade(outfit, 85),
          SR_LAYER_WHITE, 0);
    }
  }
  if (sr_anim_has_backpack(a))
    box(m, &b.chest, sr_v3(-bw + 0.02f, s->chest_h * 0.25f, -0.32f), sr_v3(bw - 0.02f, s->chest_h * 0.9f, -0.15f),
        shade(sr_corp_color(a->hair_color * 2), 50), SR_LAYER_WHITE, 0);
  for (int side = 0; side < 2; side++) {
    int prop = side == 1 ? in->prop_r : in->prop_l;
    if (!prop) continue;
    Xf hand = b.farm[side];
    hand.t = xf_pt(&b.farm[side], sr_v3(0, -s->fa - s->hand * 0.5f, 0));
    hand_prop(m, &hand, s, prop, p->j[J_FX], in);
  }
}

uint64_t sr_anim_digest(void) {
  uint64_t h = 0;
  for (int c = 1; c < SR_CLIP_COUNT; c++) {
    if (sr_anim_clip_kind(c) == SR_CK_GAIT) continue; /* gaits use libm: tested by their properties */
    for (int k = 0; k < 16; k++) {
      SrPose p;
      sr_anim_sample(c, (float)k / 16.0f, &p);
      for (int j = 0; j < SR_J_COUNT; j++) /* keyframe splines are plain arithmetic: the same everywhere */
        h = sr_hash_combine(h, (uint64_t)(int64_t)lrintf(p.j[j] * 1000.0f));
    }
  }
  return h;
}

/* ---- cache ------------------------------------------------------------------------------- */

void sr_anim_cache_init(SrAnimCache *c) { memset(c, 0, sizeof *c); }

void sr_anim_cache_begin(SrAnimCache *c, double now) {
  c->dt = c->now > 0 ? (float)clampf((float)(now - c->now), 0.0f, 0.1f) : 0.0f;
  c->now = now;
}

static SrAnimSlot *slot_for(SrAnimCache *c, uint64_t key, bool *fresh) {
  uint64_t h = sr_mix64(key);
  int free_i = -1;
  for (int i = 0; i < 16; i++) {
    SrAnimSlot *s = &c->slot[(h + (uint64_t)i) % SR_ANIM_SLOTS];
    if (s->used && s->key == key) {
      *fresh = false;
      return s;
    }
    if (!s->used && free_i < 0) free_i = (int)((h + (uint64_t)i) % SR_ANIM_SLOTS);
  }
  if (free_i < 0) return NULL;
  SrAnimSlot *s = &c->slot[free_i];
  memset(s, 0, sizeof *s);
  s->used = true;
  s->key = key;
  c->live++;
  *fresh = true;
  return s;
}

const SrAnimSlot *sr_anim_cache_find(const SrAnimCache *c, uint64_t key) {
  uint64_t h = sr_mix64(key);
  for (int i = 0; i < 16; i++) {
    const SrAnimSlot *s = &c->slot[(h + (uint64_t)i) % SR_ANIM_SLOTS];
    if (s->used && s->key == key) return s;
  }
  return NULL;
}

static uint32_t signature(const SrAnimInput *in) {
  uint32_t base = moving(in) ? 255u : in->base;
  uint32_t once = in->once && sr_anim_clip_kind(in->once) == SR_CK_BASE_ONCE ? in->once : 0u;
  return base | (uint32_t)in->upper << 8 | once << 16 | (uint32_t)(in->mirror && once) << 24;
}

void sr_anim_character(SrAnimCache *c, const SrAnimInput *in, SrMesh *out) {
  SrSkel s;
  sr_anim_skel(in->look, &s);
  float target = (fabsf(in->dir.x) + fabsf(in->dir.z)) > 1e-4f ? atan2f(in->dir.x, in->dir.z) : 0.0f;
  float stride = sr_anim_stride(&s, in->speed, in->base);
  bool fresh = true;
  SrAnimSlot *sl = c ? slot_for(c, in->key, &fresh) : NULL;
  double now = c ? c->now : 0.0;
  SrPose pose;
  if (!sl) {
    sr_anim_pose(in, &s, now, in->walk / stride, &pose);
    sr_anim_mesh(out, in, &s, &pose, target);
    return;
  }
  float dt = c->dt;
  uint32_t sig = signature(in);
  if (fresh) {
    sl->heading = target;
    sl->phase = in->walk / stride;
    sl->sig = sig;
    sl->blend_len = 0;
  } else {
    /* turn toward the direction of travel at a limited rate, leaning into the turn */
    float rate = (in->speed > 2.6f ? 540.0f : 360.0f) * SR_PI / 180.0f;
    float d = sr_wrap_angle(target - sl->heading), step = clampf(d, -rate * dt, rate * dt);
    sl->heading = sr_wrap_angle(sl->heading + step);
    float omega = dt > 0 ? step / dt : 0.0f;
    float lean = clampf(-omega * in->speed * 0.04f, -0.14f, 0.14f);
    sl->lean += (lean - sl->lean) * clampf(dt * 8.0f, 0, 1);
    float dw = in->walk - sl->last_walk;
    if (dw < 0 || dw > 5.0f)
      sl->phase = in->walk / stride;
    else
      sl->phase += dw / stride;
    sl->phase -= floorf(sl->phase);
    if (sig != sl->sig) {
      sl->snap = sl->out;
      sl->blend_t0 = now;
      bool gait_change = (sig & 0xFF) == 255u || (sl->sig & 0xFF) == 255u;
      sl->blend_len = gait_change ? 0.2f : 0.3f;
      if (sig >> 16) sl->blend_len = 0.12f; /* a fall starts at once */
      sl->sig = sig;
    }
  }
  sl->last_walk = in->walk;
  sl->seen = now;
  sr_anim_pose(in, &s, now, sl->phase, &pose);
  pose.j[J_RR] += sl->lean;
  if (sl->blend_len > 0) {
    float w = smooth((float)(now - sl->blend_t0) / sl->blend_len);
    if (w >= 1.0f) sl->blend_len = 0;
    for (int j = 0; j < SR_J_COUNT; j++) pose.j[j] = sl->snap.j[j] + (pose.j[j] - sl->snap.j[j]) * w;
  }
  sl->out = pose;
  sr_anim_mesh(out, in, &s, &pose, sl->heading);
}

void sr_anim_cache_end(SrAnimCache *c) {
  for (int i = 0; i < SR_ANIM_SLOTS; i++) {
    SrAnimSlot *s = &c->slot[i];
    if (s->used && c->now - s->seen > 2.0) {
      s->used = false;
      c->live--;
    }
  }
}
