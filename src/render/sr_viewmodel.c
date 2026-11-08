/* The player's body in first person: camera motion (head bob, landing dip, eased crouch, recoil,
 * flinch, shake) and the view model (hands and weapon swaying, bobbing, kicking, reloading,
 * swinging, throwing and reaching), with muzzle flashes that light the room. Presentation only. */
#include "render/sr_render.h"

#include <math.h>
#include <string.h>

static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float bump(float t, float len) { return t >= 0 && t < len ? sinf(SR_PI * t / len) : 0.0f; }

int sr_view_category(const SrGame *g) {
  const SrPlayer *p = &g->player;
  if (p->equipped < 0 || p->equipped >= p->inv.count) return -1;
  const SrItem *w = &p->inv.items[p->equipped];
  if (w->kind != SR_IK_WEAPON || !sr_archetype(w->type)) return -1;
  return sr_archetype(w->type)->category;
}

/* step factor for the bob: 0 standing, 1 walking, 2 sprinting, half in a crouch */
static float bob_scale(const SrGame *g) {
  if (!g->on_ground || g->speed < 0.3f) return 0.0f;
  if (g->player.crouched) return 0.5f * clamp01(g->speed / 2.0f);
  return g->speed > 5.0f ? 2.0f : clamp01(g->speed / 4.0f);
}

SrViewCam sr_view_camera(const SrGame *g) {
  const SrPlayer *p = &g->player;
  SrViewCam c;
  c.eye = p->pos;
  c.yaw = p->yaw;
  c.pitch = p->pitch;
  if (g->dive.active) return c;
  double now = sr_game_fx_now(g);
  const SrViewFx *fx = &g->fx;
  if (fx->eye_y > 0 && g->on_ground && fabsf(fx->eye_y - p->pos.y) < 1.0f) c.eye.y = fx->eye_y; /* eased crouch */
  /* head bob: up and down twice a stride, side to side once */
  float k = g->settings.head_bob ? bob_scale(g) : 0.0f;
  float ph = fx->bob / 0.75f * SR_PI;
  float s = sinf(ph);
  c.bob_y = -0.03f * k * s * s;
  c.bob_x = 0.015f * k * sinf(ph * 0.5f) * 2.0f;
  SrVec3 right = sr_v3(cosf(p->yaw), 0, -sinf(p->yaw));
  c.eye.y += c.bob_y;
  c.eye = sr_v3_add(c.eye, sr_v3_scale(right, c.bob_x));
  /* landing dip, proportional to the fall speed, at most 8 cm */
  float tl = (float)(now - fx->land_t);
  if (fx->land_t > 0 && tl >= 0 && tl < 0.35f) c.eye.y -= fminf(0.08f, fx->land_v * 0.012f) * bump(tl, 0.35f);
  /* recoil kick of the view, recovered */
  float ts = (float)(now - fx->shot_t);
  if (fx->shot_t > 0 && ts >= 0 && ts < 0.4f) c.pitch += 0.02f * fx->recoil * expf(-ts / 0.08f);
  /* a flinch away from the hit */
  float th = (float)(now - fx->hurt_t);
  if (fx->hurt_t > 0 && th >= 0 && th < 0.25f) {
    float f = 1.0f - th / 0.25f;
    if (fx->hurt_known) c.yaw += (fx->hurt_dir > 0 ? 1.0f : -1.0f) * 0.026f * f; /* 1.5 degrees */
    c.pitch += 0.01f * f;
  }
  /* shake from explosions and heavy landings, scaled by the setting */
  float tk = (float)(now - fx->shake_t);
  float amp = g->settings.camera_shake <= 0 ? 0.0f : g->settings.camera_shake == 1 ? 0.4f : 1.0f;
  if (fx->shake_t > 0 && tk >= 0 && tk < 0.5f && amp > 0) {
    float a = fx->shake * amp * (1.0f - tk / 0.5f);
    c.yaw += 0.012f * a * sinf(tk * 61.0f);
    c.pitch += 0.012f * a * sinf(tk * 47.0f + 1.0f);
    c.eye.y += 0.03f * a * sinf(tk * 53.0f);
  }
  return c;
}

/* ---- view model ----------------------------------------------------------------------------- */

typedef struct Frame {
  SrVec3 o, x, y, z;
} Frame;

static SrVec3 fp(const Frame *f, SrVec3 p) {
  return sr_v3_add(f->o, sr_v3_add(sr_v3_scale(f->x, p.x), sr_v3_add(sr_v3_scale(f->y, p.y), sr_v3_scale(f->z, p.z))));
}

static Frame camera_frame(SrVec3 eye, float yaw, float pitch) {
  Frame f;
  f.o = eye;
  f.z = sr_dir_from_yaw_pitch(yaw, pitch);
  f.x = sr_v3(cosf(yaw), 0, -sinf(yaw));
  f.y = sr_v3_cross(f.x, f.z);
  return f;
}

/* rotates a frame about its own axes: yaw (y), pitch (x, nose up), roll (z) */
static Frame turned(Frame f, float yaw, float pitch, float roll) {
  float cy = cosf(yaw), sy = sinf(yaw);
  SrVec3 x = sr_v3_add(sr_v3_scale(f.x, cy), sr_v3_scale(f.z, -sy)),
         z = sr_v3_add(sr_v3_scale(f.x, sy), sr_v3_scale(f.z, cy));
  f.x = x;
  f.z = z;
  float cp = cosf(pitch), sp = sinf(pitch);
  SrVec3 y = sr_v3_add(sr_v3_scale(f.y, cp), sr_v3_scale(f.z, -sp));
  z = sr_v3_add(sr_v3_scale(f.y, sp), sr_v3_scale(f.z, cp));
  f.y = y;
  f.z = z;
  float cr = cosf(roll), sr = sinf(roll);
  x = sr_v3_add(sr_v3_scale(f.x, cr), sr_v3_scale(f.y, sr));
  y = sr_v3_add(sr_v3_scale(f.x, -sr), sr_v3_scale(f.y, cr));
  f.x = x;
  f.y = y;
  return f;
}

static void vbox(SrMesh *m, const Frame *f, SrVec3 lo, SrVec3 hi, SrColor col, int layer, int em) {
  SrVec3 c[8];
  for (int i = 0; i < 8; i++) c[i] = fp(f, sr_v3((i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z));
  SrColor top = {(uint8_t)(col.r * 4 / 5), (uint8_t)(col.g * 4 / 5), (uint8_t)(col.b * 4 / 5), col.a};
  sr_mesh_quad(m, c[4], c[5], c[7], c[6], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[1], c[0], c[2], c[3], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[5], c[1], c[3], c[7], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[0], c[4], c[6], c[2], 0, 1, 1, 0, col, layer, em, 0);
  sr_mesh_quad(m, c[6], c[7], c[3], c[2], 0, 1, 1, 0, top, layer, em, 0);
  sr_mesh_quad(m, c[0], c[1], c[5], c[4], 0, 1, 1, 0, top, layer, em, 0);
}

/* where the barrel ends, in the weapon's frame */
static SrVec3 muzzle_local(int cat) {
  switch (cat) {
    case SR_WC_PISTOL: return sr_v3(0, 0.045f, 0.22f);
    case SR_WC_LONG: return sr_v3(0, 0.04f, 0.64f);
    case SR_WC_HEAVY: return sr_v3(0, 0.04f, 0.74f);
    case SR_WC_NONLETHAL: return sr_v3(0, 0.04f, 0.19f);
    default: return sr_v3(0, 0.02f, 0.5f);
  }
}

typedef struct Pose {
  Frame weapon;   /* grip frame */
  SrVec3 left;    /* left hand, camera space */
  bool left_free; /* the left hand is off the weapon */
  bool weapon_on; /* the weapon is in view */
} Pose;

static Pose view_pose(const SrGame *g, SrViewCam c, float sway_x, float sway_y, int cat) {
  double now = sr_game_fx_now(g);
  const SrViewFx *fx = &g->fx;
  Pose p;
  memset(&p, 0, sizeof p);
  Frame cam = camera_frame(c.eye, c.yaw, c.pitch);
  bool long_gun = cat == SR_WC_LONG || cat == SR_WC_HEAVY;
  SrVec3 grip = long_gun ? sr_v3(0.14f, -0.15f, 0.3f) : sr_v3(0.18f, -0.19f, 0.38f);
  float yaw = 0, pitch = 0, roll = 0;
  /* sway lags the look; the weapon bobs with the steps */
  grip.x -= sway_x;
  grip.y += sway_y;
  float k = bob_scale(g), ph = fx->bob / 0.75f * SR_PI;
  grip.x += 0.008f * k * sinf(ph * 0.5f) * 2.0f;
  grip.y -= 0.01f * k * fabsf(sinf(ph));
  if (g->speed > 5.0f && g->on_ground) { /* sprinting: lowered and turned in */
    grip.y -= 0.07f;
    yaw -= 0.4f;
    roll += 0.4f;
  }
  if (g->player.crouched) grip.y -= 0.02f;
  float ts = (float)(now - fx->shot_t);
  if (fx->shot_t > 0 && ts >= 0 && ts < 0.4f) { /* kick back and up, recovering */
    float kick = fx->recoil * expf(-ts / 0.07f);
    grip.z -= 0.05f * kick;
    pitch += 0.12f * kick;
  }
  float tr = (float)(now - fx->reload_t);
  float rl = bump(tr, 1.1f);
  if (rl > 0) { /* lowered and tilted while the magazine is swapped */
    grip.y -= 0.07f * rl;
    pitch += 0.25f * rl;
    roll += 0.45f * rl;
    p.left_free = true;
    p.left = sr_v3(grip.x - 0.06f, grip.y - 0.14f + 0.06f * sinf(tr * 9.0f), grip.z + 0.05f);
  }
  float tm = (float)(now - fx->melee_t);
  if (fx->melee_t > 0 && tm >= 0 && tm < 0.45f) { /* a swing from right to left */
    float u = tm / 0.45f;
    yaw += 1.0f - 2.0f * clamp01(u * 1.4f);
    grip.z += 0.15f * bump(tm, 0.45f);
    grip.x -= 0.12f * u;
  }
  float tt = (float)(now - fx->throw_t);
  if (fx->throw_t > 0 && tt >= 0 && tt < 0.6f) { /* the off hand comes up and throws */
    grip.y -= 0.2f * bump(tt, 0.6f);
    p.left_free = true;
    float u = tt / 0.6f;
    p.left = u < 0.5f ? sr_v3(-0.18f, -0.2f + 0.45f * u, 0.25f)
                      : sr_v3(-0.12f, 0.02f - 0.2f * (u - 0.5f), 0.25f + 0.6f * (u - 0.5f));
  }
  float te = (float)(now - fx->reach_t);
  if (fx->reach_t > 0 && te >= 0 && te < 0.5f) { /* reaching for a door, terminal or ledge */
    p.left_free = true;
    float r = bump(te, 0.5f);
    p.left = sr_v3(-0.1f + 0.05f * r, -0.25f + 0.2f * r, 0.3f + 0.25f * r);
  }
  p.weapon_on = cat >= 0;
  p.weapon = turned(cam, yaw, pitch, roll);
  p.weapon.o = fp(&cam, grip);
  if (!p.left_free) p.left = long_gun ? sr_v3(grip.x - 0.08f, grip.y - 0.03f, grip.z + 0.32f) : grip;
  return p;
}

static SrColor hand_color(const SrGame *g) {
  if (g->player.look.cyber >= 4) return (SrColor){170, 175, 185, 255};
  return sr_anim_skin(&g->player.look);
}

int sr_build_view_model(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrMesh *out) {
  sr_mesh_clear(out);
  if (g->dive.active) return 0;
  int cat = sr_view_category(g);
  double now = sr_game_fx_now(g);
  Pose p = view_pose(g, c, sway_x, sway_y, cat);
  bool busy = p.left_free;
  if (cat < 0 && !busy) return 0; /* unarmed: hands only for interactions */
  SrColor hand = hand_color(g);
  int hl = g->player.look.cyber >= 4 ? SR_LAYER_METAL : SR_LAYER_WHITE;
  SrColor sleeve = sr_anim_cloth(&g->player.look);
  Frame cam = camera_frame(c.eye, c.yaw, c.pitch);
  if (cat >= 0) {
    const SrItem *w = &g->player.inv.items[g->player.equipped];
    SrWeaponStats st = sr_weapon_stats(g->seed, w);
    SrColor body = {32, 33, 38, 255}, accent = sr_corp_color(st.style * 3 + 1);
    const Frame *f = &p.weapon;
    switch (cat) {
      case SR_WC_PISTOL:
        vbox(out, f, sr_v3(-0.025f, 0.02f, -0.02f), sr_v3(0.025f, 0.07f, 0.2f), body, SR_LAYER_METAL, 0);
        vbox(out, f, sr_v3(-0.022f, -0.08f, -0.01f), sr_v3(0.022f, 0.02f, 0.05f), body, SR_LAYER_WHITE, 0);
        vbox(out, f, sr_v3(-0.026f, 0.05f, 0.1f), sr_v3(0.026f, 0.06f, 0.18f), accent, SR_LAYER_WHITE, 60);
        break;
      case SR_WC_LONG:
        vbox(out, f, sr_v3(-0.03f, 0.0f, -0.25f), sr_v3(0.03f, 0.08f, 0.3f), body, SR_LAYER_METAL, 0);
        vbox(out, f, sr_v3(-0.012f, 0.03f, 0.3f), sr_v3(0.012f, 0.05f, 0.62f), body, SR_LAYER_METAL, 0);
        vbox(out, f, sr_v3(-0.025f, -0.03f, -0.45f), sr_v3(0.025f, 0.06f, -0.25f), body, SR_LAYER_WHITE, 0);
        vbox(out, f, sr_v3(-0.02f, -0.12f, 0.02f), sr_v3(0.02f, 0.0f, 0.1f), body, SR_LAYER_WHITE, 0);
        vbox(out, f, sr_v3(-0.031f, 0.06f, -0.1f), sr_v3(0.031f, 0.07f, 0.2f), accent, SR_LAYER_WHITE, 60);
        break;
      case SR_WC_HEAVY:
        vbox(out, f, sr_v3(-0.05f, -0.02f, -0.3f), sr_v3(0.05f, 0.1f, 0.35f), body, SR_LAYER_METAL, 0);
        vbox(out, f, sr_v3(-0.025f, 0.02f, 0.35f), sr_v3(0.025f, 0.06f, 0.72f), body, SR_LAYER_METAL, 0);
        vbox(out, f, sr_v3(-0.06f, -0.16f, -0.05f), sr_v3(0.06f, -0.02f, 0.12f), accent, SR_LAYER_WHITE, 30);
        break;
      case SR_WC_NONLETHAL:
        vbox(out, f, sr_v3(-0.03f, 0.0f, -0.02f), sr_v3(0.03f, 0.08f, 0.17f), (SrColor){60, 60, 30, 255},
             SR_LAYER_WHITE, 0);
        vbox(out, f, sr_v3(-0.031f, 0.02f, 0.0f), sr_v3(0.031f, 0.04f, 0.17f), (SrColor){240, 200, 20, 255},
             SR_LAYER_WHITE, 40);
        vbox(out, f, sr_v3(-0.022f, -0.08f, 0.0f), sr_v3(0.022f, 0.0f, 0.05f), body, SR_LAYER_WHITE, 0);
        break;
      default: { /* melee: a blade or a baton, angled up */
        Frame b = turned(*f, 0, 0.7f, 0);
        bool blade = w->type % 2 == 0;
        vbox(out, &b, sr_v3(-0.015f, -0.02f, -0.12f), sr_v3(0.015f, 0.02f, 0.02f), body, SR_LAYER_WHITE, 0);
        vbox(out, &b, sr_v3(blade ? -0.006f : -0.018f, -0.015f, 0.02f), sr_v3(blade ? 0.006f : 0.018f, 0.02f, 0.5f),
             blade ? (SrColor){190, 195, 205, 255} : body, SR_LAYER_METAL, blade ? 30 : 0);
        break;
      }
    }
    /* the right hand on the grip, with a cuff of the sleeve */
    vbox(out, f, sr_v3(-0.03f, -0.075f, -0.035f), sr_v3(0.03f, 0.0f, 0.04f), hand, hl, 0);
    vbox(out, f, sr_v3(-0.04f, -0.11f, -0.22f), sr_v3(0.04f, -0.03f, -0.035f), sleeve, SR_LAYER_WHITE, 0);
  }
  /* the left hand: on the weapon, or reaching, throwing, swapping a magazine */
  if (cat >= 0 || busy) {
    Frame lf = cam;
    lf.o = fp(&cam, p.left);
    vbox(out, &lf, sr_v3(-0.03f, -0.03f, -0.04f), sr_v3(0.03f, 0.03f, 0.04f), hand, hl, 0);
    vbox(out, &lf, sr_v3(-0.045f, -0.05f, -0.24f), sr_v3(0.045f, 0.03f, -0.05f), sleeve, SR_LAYER_WHITE, 0);
    if (busy && g->fx.throw_t > 0 && now - g->fx.throw_t < 0.3) /* the gadget in the hand */
      vbox(out, &lf, sr_v3(-0.03f, 0.03f, -0.03f), sr_v3(0.03f, 0.09f, 0.03f), (SrColor){70, 90, 70, 255},
           SR_LAYER_METAL, 0);
  }
  return out->ni / 3;
}

bool sr_view_muzzle(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrVec3 *pos) {
  int cat = sr_view_category(g);
  if (cat < 0 || cat == SR_WC_MELEE || g->fx.shot_t <= 0) return false;
  float t = (float)(sr_game_fx_now(g) - g->fx.shot_t);
  if (t < 0 || t >= 2.5f / 60.0f) return false; /* lit for about two frames */
  Pose p = view_pose(g, c, sway_x, sway_y, cat);
  *pos = fp(&p.weapon, muzzle_local(cat));
  return true;
}

void sr_build_muzzle_fx(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrMesh *fx) {
  int cat = sr_view_category(g);
  if (cat < 0 || cat == SR_WC_MELEE || g->fx.shot_t <= 0) return;
  float t = (float)(sr_game_fx_now(g) - g->fx.shot_t);
  if (t < 0 || t > 0.45f) return;
  Pose p = view_pose(g, c, sway_x, sway_y, cat);
  SrBillboard bb = sr_billboard(c.eye, c.yaw, c.pitch);
  if (t < 0.05f) {
    SrVec3 m = fp(&p.weapon, muzzle_local(cat));
    float sz = cat == SR_WC_HEAVY ? 0.09f : 0.06f;
    sr_mesh_sprite(fx, &bb, m, sz, sz, SR_SPR_FLARE, SR_SPRITE_FACING, 0, (SrColor){200, 150, 80, 255}, 255);
  }
  if (cat != SR_WC_NONLETHAL) { /* a shell flips out to the right */
    SrVec3 s = fp(&p.weapon, sr_v3(0.03f + 0.5f * t, 0.06f + 1.2f * t - 4.9f * t * t, 0.05f));
    sr_mesh_sprite(fx, &bb, s, 0.025f, 0.025f, SR_SPR_GLOW, SR_SPRITE_FACING, 0, (SrColor){160, 120, 40, 255}, 255);
  }
}

float sr_view_hurt_edge(const SrGame *g, float *dir, bool *known) {
  float t = (float)(sr_game_fx_now(g) - g->fx.hurt_t);
  *dir = g->fx.hurt_dir;
  *known = g->fx.hurt_known;
  if (g->fx.hurt_t <= 0 || t < 0 || t >= 0.6f) return 0.0f;
  return 1.0f - t / 0.6f;
}
