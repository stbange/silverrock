/* improve-animation: skeleton and clips, gaits, the cache, street life, rain, reactions, vehicles,
 * combat, the player's body and world motion. */
#include "core/sr_rng.h"
#include "game/sr_save.h"
#include "render/sr_render.h"
#include "ui/sr_ui.h"
#include "sr_test.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SrGame *g_game;

static SrAppearance look_of(int body, int role) {
  SrAppearance a = sr_appearance_generate(40u + (uint64_t)body, (SrRole)role, 0);
  a.body = (uint8_t)body;
  return a;
}

static SrAnimInput input_for(const SrAppearance *a, int base) {
  SrAnimInput in;
  memset(&in, 0, sizeof in);
  in.key = 77;
  in.look = a;
  in.role = (uint8_t)(a->outfit / 4);
  in.dir = sr_v3(0, 0, 1);
  in.base = (uint8_t)base;
  in.once_t0 = -1;
  return in;
}

/* ---- skeleton and clips --------------------------------------------------------------------- */

static void test_clips(void) {
  SR_SPEC("retro-rendering", "Clips loop without a jump");
  SR_SPEC("retro-rendering", "Joints stay in their limits");
  SR_SPEC("retro-rendering", "Poses are pinned");
  for (int c = 1; c < SR_CLIP_COUNT; c++) {
    SR_CHECK(sr_anim_clip_name(c)[0] != '\0');
    SR_CHECK(sr_anim_clip_len(c) > 0.1f);
    SrClipKind k = sr_anim_clip_kind(c);
    if (k == SR_CK_LOOP || k == SR_CK_OVERLAY) { /* the step across the loop point is like any other */
      float max_step[SR_J_COUNT] = {0}, wrap[SR_J_COUNT] = {0};
      SrPose prev, cur, first;
      sr_anim_sample(c, 0.0f, &first);
      prev = first;
      for (int i = 1; i <= 128; i++) {
        sr_anim_sample(c, (float)i / 128.0f, &cur);
        for (int j = 0; j < SR_J_COUNT; j++) {
          float d = fabsf(cur.j[j] - prev.j[j]);
          if (i < 128) max_step[j] = fmaxf(max_step[j], d);
          if (i == 128) wrap[j] = d;
        }
        prev = cur;
      }
      for (int j = 0; j < SR_J_COUNT; j++) SR_CHECK(wrap[j] <= max_step[j] * 1.5f + 1e-3f);
    }
    if (k == SR_CK_ADD_ONCE) { /* additive one-shots start and end at rest */
      SrPose a, b;
      sr_anim_sample(c, 0.0f, &a);
      sr_anim_sample(c, 1.0f, &b);
      for (int j = 0; j < SR_J_COUNT; j++) SR_CHECK(fabsf(a.j[j]) < 1e-4f && fabsf(b.j[j]) < 1e-4f);
    }
    /* authored keys stay inside the joint limits (a small overshoot of the spline is allowed) */
    for (int i = 0; i < 64; i++) {
      SrPose p;
      sr_anim_sample(c, (float)i / 64.0f, &p);
      for (int j = 0; j < SR_J_FX; j++) {
        float lo, hi;
        sr_anim_limits(j, &lo, &hi);
        SR_CHECK(p.j[j] >= lo - 0.08f && p.j[j] <= hi + 0.08f);
      }
    }
  }
  /* every clip, every body type: the full pose keeps knees and elbows bending the right way */
  for (int body = 0; body < 4; body++) {
    SrAppearance a = look_of(body, SR_ROLE_WORKER);
    SrSkel s;
    sr_anim_skel(&a, &s);
    for (int c = 1; c < SR_CLIP_COUNT; c++)
      for (int i = 0; i < 64; i++) {
        SrAnimInput in = input_for(&a, SR_CLIP_IDLE);
        SrClipKind k = sr_anim_clip_kind(c);
        if (k == SR_CK_GAIT) {
          in.base = (uint8_t)c;
          in.speed = 1.0f + (float)(i % 4);
        } else if (k == SR_CK_LOOP) {
          in.base = (uint8_t)c;
        } else if (k == SR_CK_OVERLAY) {
          in.upper = (uint8_t)c;
        } else {
          in.once = (uint8_t)c;
          in.once_t0 = 0;
          if (k == SR_CK_BASE_ONCE) in.base = (uint8_t)c;
        }
        SrPose p;
        sr_anim_pose(&in, &s, (double)i / 64.0 * sr_anim_clip_len(c), (float)i / 64.0f, &p);
        for (int j = 0; j < SR_J_COUNT; j++) {
          float lo, hi;
          sr_anim_limits(j, &lo, &hi);
          SR_CHECK(p.j[j] >= lo - 1e-5f && p.j[j] <= hi + 1e-5f);
        }
        SR_CHECK(p.j[SR_J_KNEE_L] >= 0 && p.j[SR_J_KNEE_R] >= 0);
        SR_CHECK(p.j[SR_J_ELB_L] >= 0 && p.j[SR_J_ELB_R] >= 0);
      }
  }
  SrPose p = {{0}};
  p.j[SR_J_KNEE_L] = -1.0f;
  p.j[SR_J_HIP_L] = 0.3f;
  p.j[SR_J_ROOT_ROLL] = 0.2f;
  sr_anim_clamp(&p);
  SR_CHECK_NEAR(p.j[SR_J_KNEE_L], 0, 1e-6);
  sr_anim_mirror(&p);
  SR_CHECK_NEAR(p.j[SR_J_HIP_R], 0.3f, 1e-6);
  SR_CHECK_NEAR(p.j[SR_J_ROOT_ROLL], -0.2f, 1e-6);
  SR_CHECK(sr_anim_clip_kind(-5) == SR_CK_LOOP && sr_anim_clip_kind(999) == SR_CK_LOOP);
  SR_CHECK(sr_anim_clip_keys_joint(SR_CLIP_UMBRELLA, SR_J_SHO_R));
  SR_CHECK(!sr_anim_clip_keys_joint(SR_CLIP_UMBRELLA, SR_J_SHO_L));
  uint64_t d = sr_anim_digest();
  printf("  pose digest %016llx\n", (unsigned long long)d);
  SR_CHECK_EQ(d, 0x4a18620e38cba210ull);
}

/* ---- locomotion ----------------------------------------------------------------------------- */

static void test_gait(void) {
  SR_SPEC("retro-rendering", "People walk with knees and elbows");
  SR_SPEC("retro-rendering", "Feet do not slide");
  SrAppearance a = look_of(1, SR_ROLE_WORKER);
  SrSkel s;
  sr_anim_skel(&a, &s);
  /* one stride of the walk: knees and elbows bend, the pelvis rises and falls twice */
  float kmax = 0, emin = 9, py[64];
  for (int i = 0; i < 64; i++) {
    SrPose p;
    sr_anim_gait(&s, 1.4f, (float)i / 64.0f, SR_CLIP_WALK, 5, &p);
    kmax = fmaxf(kmax, p.j[SR_J_KNEE_L]);
    emin = fminf(emin, p.j[SR_J_ELB_R]);
    py[i] = p.j[SR_J_PELVIS_Y];
  }
  SR_CHECK(kmax > 0.3f);
  SR_CHECK(emin > 0.1f);
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < 64; i++) {
    lo = fminf(lo, py[i]);
    hi = fmaxf(hi, py[i]);
  }
  int rises = 0; /* upward crossings of the middle height */
  float mid = 0.5f * (lo + hi);
  for (int i = 0; i < 64; i++) rises += py[(i + 63) % 64] < mid && py[i] >= mid;
  SR_CHECK_EQ(rises, 2);
  SR_CHECK(hi - lo > 0.02f && hi - lo < 0.09f); /* a bob of a few centimetres */
  /* every body type, gait and speed: the foot on the ground stays put in the world */
  static const float speeds[] = {0.8f, 1.4f, 2.1f, 3.4f, 4.5f};
  static const int gaits[] = {SR_CLIP_WALK, SR_CLIP_CROUCH_WALK, SR_CLIP_LIMP};
  for (int body = 0; body < 4; body++) {
    SrAppearance b = look_of(body, SR_ROLE_EXEC);
    sr_anim_skel(&b, &s);
    for (int gi = 0; gi < 3; gi++)
      for (int si = 0; si < 5; si++) {
        float v = speeds[si], stride = sr_anim_stride(&s, v, gaits[gi]);
        float slip = 0, moved = 0;
        SrPose prev;
        sr_anim_gait(&s, v, 0, gaits[gi], 9, &prev);
        for (int i = 1; i <= 200; i++) {
          float d0 = (float)(i - 1) * 0.01f * stride, d1 = (float)i * 0.01f * stride;
          SrPose cur;
          sr_anim_gait(&s, v, d1 / stride, gaits[gi], 9, &cur);
          for (int side = 0; side < 2; side++) {
            SrVec3 f0 = sr_anim_point(&s, &prev, side), f1 = sr_anim_point(&s, &cur, side);
            if (f0.y > s.ankle + 0.002f || f1.y > s.ankle + 0.002f) continue; /* swinging */
            slip += fabsf((d1 + f1.z) - (d0 + f0.z));
            moved += d1 - d0;
          }
          prev = cur;
        }
        SR_CHECK(moved > 0);
        SR_CHECK(slip <= 0.1f * moved);
      }
  }
}

static void test_turns_and_blends(void) {
  SR_SPEC("retro-rendering", "Turns are smooth");
  SR_SPEC("retro-rendering", "Blends are continuous");
  SrAnimCache *c = (SrAnimCache *)malloc(sizeof *c);
  sr_anim_cache_init(c);
  SrAppearance a = look_of(0, SR_ROLE_WORKER);
  SrAnimInput in = input_for(&a, SR_CLIP_IDLE);
  in.speed = 1.4f;
  SrMesh m;
  sr_mesh_init(&m);
  double t = 1.0;
  for (int i = 0; i < 10; i++, t += 1.0 / 60.0) {
    sr_anim_cache_begin(c, t);
    in.walk += 1.4f / 60.0f;
    sr_mesh_clear(&m);
    sr_anim_character(c, &in, &m);
    sr_anim_cache_end(c);
  }
  in.dir = sr_v3(0, 0, -1); /* turn around in one step */
  float prev = sr_anim_cache_find(c, in.key)->heading, max_rate = 0, max_lean = 0;
  for (int i = 0; i < 60; i++, t += 1.0 / 60.0) {
    sr_anim_cache_begin(c, t);
    in.walk += 1.4f / 60.0f;
    sr_mesh_clear(&m);
    sr_anim_character(c, &in, &m);
    sr_anim_cache_end(c);
    const SrAnimSlot *sl = sr_anim_cache_find(c, in.key);
    max_rate = fmaxf(max_rate, fabsf(sr_wrap_angle(sl->heading - prev)) * 60.0f);
    max_lean = fmaxf(max_lean, fabsf(sl->lean));
    prev = sl->heading;
  }
  SR_CHECK(max_rate <= 2.0f * SR_PI + 1e-3f); /* at most 360 degrees a second */
  SR_CHECK(max_lean > 0.01f);
  SR_CHECK(fabsf(sr_wrap_angle(prev - SR_PI)) < 0.05f);
  /* stopping and sitting down blend over time: no joint jumps */
  in.speed = 0;
  in.base = SR_CLIP_SIT;
  SrPose last = sr_anim_cache_find(c, in.key)->out;
  float max_step = 0;
  SrPose at_100ms = last;
  for (int i = 0; i < 40; i++, t += 1.0 / 60.0) {
    sr_anim_cache_begin(c, t);
    sr_mesh_clear(&m);
    sr_anim_character(c, &in, &m);
    sr_anim_cache_end(c);
    SrPose cur = sr_anim_cache_find(c, in.key)->out;
    for (int j = 0; j < SR_J_COUNT; j++) max_step = fmaxf(max_step, fabsf(cur.j[j] - last.j[j]));
    if (i == 6) at_100ms = cur;
    last = cur;
  }
  SR_CHECK(max_step <= 0.2f);
  SrSkel s;
  sr_anim_skel(&a, &s);
  SrPose sit;
  sr_anim_pose(&in, &s, t, 0, &sit);
  SR_CHECK(fabsf(at_100ms.j[SR_J_HIP_L] - sit.j[SR_J_HIP_L]) > 0.1f); /* not there yet after 0.1 s */
  SR_CHECK(fabsf(last.j[SR_J_HIP_L] - sit.j[SR_J_HIP_L]) < 0.05f);
  /* entries not seen for two seconds are dropped; a NULL cache draws the target pose */
  sr_anim_cache_begin(c, t + 5.0);
  sr_anim_cache_end(c);
  SR_CHECK(sr_anim_cache_find(c, in.key) == NULL);
  SR_CHECK_EQ(c->live, 0);
  sr_mesh_clear(&m);
  sr_anim_character(NULL, &in, &m);
  SR_CHECK(m.nv > 0);
  /* the cache never overflows: keys past the probe window draw without blending */
  sr_anim_cache_begin(c, t + 6.0);
  for (uint64_t k = 0; k < 700; k++) {
    in.key = k * 7919u;
    sr_mesh_clear(&m);
    sr_anim_character(c, &in, &m);
  }
  SR_CHECK(c->live <= SR_ANIM_SLOTS);
  sr_mesh_free(&m);
  free(c);
}

/* ---- street life ---------------------------------------------------------------------------- */

static SrAgent agent_at(SrVec3 pos, uint64_t key, int role) {
  SrAgent a;
  memset(&a, 0, sizeof a);
  a.key = key;
  a.look = sr_appearance_generate(key, (SrRole)role, 0);
  a.role = (uint8_t)role;
  a.pos = pos;
  a.dir = sr_v3(0, 0, 1);
  a.target = pos;
  a.speed = 1.3f;
  a.npc = -1;
  return a;
}

static SrCrowdCtx ctx_at(SrVec3 eye) {
  SrCrowdCtx x;
  memset(&x, 0, sizeof x);
  x.now = 100.0;
  x.player = x.eye = eye;
  x.max_near = 48;
  x.max_splashes = 48;
  return x;
}

static void test_street_life(void) {
  SR_SPEC("population-simulation", "Walkers start and stop");
  SR_SPEC("population-simulation", "Activities fit the place");
  SR_SPEC("population-simulation", "The street is busy with small things");
  SR_SPEC("retro-rendering", "Nobody stands frozen");
  SR_SPEC("retro-rendering", "Far crowd keeps the gait");
  const SrCity *c = g_game->city;
  /* every prop that decides an activity gets one of its own */
  int checked = 0;
  for (int blk = 0; blk < c->grid * c->grid && checked < 400; blk++) {
    SrStreetProp props[SR_MAX_BLOCK_PROPS];
    int n = sr_dressing_block(c, blk, props, SR_MAX_BLOCK_PROPS);
    for (int i = 0; i < n; i++) {
      SrVec3 at = sr_v3(props[i].x + 0.5f, 0, props[i].z + 0.5f);
      SrVec3 pp;
      int kind = sr_activity_prop_near(c, blk, at, &pp, NULL);
      if (kind < 0) continue;
      SrVec3 face;
      SrStreetAct act = sr_activity_pick(c, blk, at, (uint64_t)i * 31u + (uint64_t)blk, SR_ROLE_WORKER, &face);
      SR_CHECK(sr_activity_fits_prop(kind, act));
      SR_CHECK(sr_v3_len(face) > 0.5f);
      checked++;
    }
  }
  SR_CHECK(checked > 50);
  SR_CHECK(sr_activity_fits_prop(SR_SP_BENCH, SR_SA_SIT) && !sr_activity_fits_prop(SR_SP_BENCH, SR_SA_DANCE));
  SR_CHECK(sr_activity_fits_prop(SR_SP_LAMP, SR_SA_DANCE)); /* props without a list allow anything */
  SR_CHECK_EQ(sr_activity_prop_near(c, -1, sr_v3(0, 0, 0), NULL, NULL), -1);
  /* walkers accelerate and brake instead of jumping between speeds */
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  SrVec3 home = sr_v3((float)hx / 100.0f, 0, (float)hy / 100.0f);
  int hblk = sr_city_block_at(c, hx, hy);
  cr->agents[0] = agent_at(home, 11, SR_ROLE_WORKER);
  cr->agents[0].block = hblk;
  cr->agents[0].target = sr_v3(home.x + 30.0f, 0, home.z);
  cr->count = 1;
  float t_full = -1;
  for (int i = 0; i < 120; i++) {
    sr_crowd_update(cr, c, 1.0f / 60.0f, 0.0f);
    if (t_full < 0 && cr->agents[0].vel >= cr->agents[0].speed - 1e-4f) t_full = (float)(i + 1) / 60.0f;
  }
  SR_CHECK(t_full >= 0.25f);
  cr->agents[0].state = SR_AG_IDLE;
  cr->agents[0].timer = 30.0f;
  float t_stop = -1;
  for (int i = 0; i < 120; i++) {
    sr_crowd_update(cr, c, 1.0f / 60.0f, 0.0f);
    if (t_stop < 0 && cr->agents[0].vel <= 0.0f) t_stop = (float)(i + 1) / 60.0f;
  }
  SR_CHECK(t_stop >= 0.25f);
  /* a night street over ten minutes: stopped citizens do something that fits the place */
  sr_crowd_init(cr);
  SrCamera cam = {sr_v3(home.x, 1.7f, home.z + 20.0f), sr_v3(0, 0, -1), 0.5f};
  int64_t t = 2 * 86400 + 22 * 3600;
  sr_crowd_refresh(cr, c, NULL, &cam, t, SR_MAX_AGENTS);
  SR_CHECK(cr->count > 0);
  cam.pos = sr_v3(cr->agents[0].pos.x, 1.7f, cr->agents[0].pos.z); /* on a pavement among the walkers */
  int standing = 0, busy = 0, fits = 0, near_prop = 0;
  for (int step = 0; step < 600 * 4; step++) {
    sr_crowd_update(cr, c, 0.25f, 0.0f);
    if (step % 40 != 39) continue;
    for (int i = 0; i < cr->count; i++) {
      const SrAgent *ag = &cr->agents[i];
      if (ag->state != SR_AG_IDLE || ag->vel > 0.0f || ag->activity == SR_SA_NONE) continue;
      standing++; /* the crowd lives within its radius around the camera */
      busy += ag->activity != SR_SA_IDLE;
      int kind = sr_activity_prop_near(c, ag->block, ag->pos, NULL, NULL);
      if (kind >= 0 && ag->activity != SR_SA_TALK) {
        near_prop++;
        fits += sr_activity_fits_prop(kind, (SrStreetAct)ag->activity);
      }
    }
  }
  printf("  street: %d standing around the camera, %d busy; %d by props, %d fitting\n", standing, busy, near_prop,
         fits);
  SR_CHECK(standing > 10);
  SR_CHECK(busy * 2 >= standing);
  SR_CHECK_EQ(fits, near_prop);
  /* standing people keep moving: breathing, weight shifts, gestures */
  SrAppearance la = look_of(2, SR_ROLE_WORKER);
  SrSkel sk;
  sr_anim_skel(&la, &sk);
  for (int clip = SR_CLIP_IDLE; clip <= SR_CLIP_LOOK_AROUND; clip++) {
    if (clip == SR_CLIP_SLEEP) continue;
    SrAnimInput in = input_for(&la, clip);
    SrPose p0, p1;
    sr_anim_pose(&in, &sk, 10.0, 0, &p0);
    float d = 0;
    for (int k = 1; k < 8; k++) {
      sr_anim_pose(&in, &sk, 10.0 + sr_anim_clip_len(clip) * 0.11 * k, 0, &p1);
      for (int j = 0; j < SR_J_COUNT; j++) d = fmaxf(d, fabsf(p1.j[j] - p0.j[j]));
    }
    SR_CHECK(d > 0.01f);
  }
  /* far citizens are instanced with their gait or pose */
  sr_crowd_init(cr);
  SrVec3 far = sr_v3(home.x + 60.0f, 0, home.z);
  cr->agents[0] = agent_at(far, 1, SR_ROLE_WORKER);
  cr->agents[0].vel = 1.4f;
  cr->agents[0].walk = 3.0f;
  cr->agents[1] = agent_at(sr_v3(far.x, 0, far.z + 3), 2, SR_ROLE_WORKER);
  cr->agents[1].state = SR_AG_IDLE;
  cr->agents[1].activity = SR_SA_SIT;
  cr->agents[2] = agent_at(sr_v3(far.x, 0, far.z + 6), 3, SR_ROLE_WORKER);
  cr->agents[2].state = SR_AG_COVER;
  cr->agents[2].activity = SR_SA_COWER;
  cr->agents[3] = agent_at(sr_v3(far.x, 0, far.z + 9), 4, SR_ROLE_DRIFTER);
  cr->agents[3].state = SR_AG_IDLE;
  cr->agents[3].activity = SR_SA_SLEEP;
  cr->agents[4] = agent_at(sr_v3(far.x, 0, far.z + 12), 5, SR_ROLE_WORKER);
  cr->count = 5;
  SrCrowdCtx x = ctx_at(home);
  SrMesh near;
  sr_mesh_init(&near);
  float *inst = (float *)calloc(SR_MAX_AGENTS * SR_INST_FLOATS, sizeof(float));
  int n = sr_build_crowd_lod(cr, NULL, &x, home, 28.0f, 200.0f, &near, inst, SR_MAX_AGENTS);
  SR_CHECK_EQ(n, 5);
  SR_CHECK_NEAR(inst[0 * SR_INST_FLOATS + 7], 1, 0); /* walking */
  SR_CHECK(inst[0 * SR_INST_FLOATS + 4] >= 0 && inst[0 * SR_INST_FLOATS + 4] < 1);
  SR_CHECK_NEAR(inst[1 * SR_INST_FLOATS + 7], 2, 0); /* sitting */
  SR_CHECK_NEAR(inst[2 * SR_INST_FLOATS + 7], 3, 0); /* cowering */
  SR_CHECK_NEAR(inst[3 * SR_INST_FLOATS + 7], 4, 0); /* asleep */
  SR_CHECK_NEAR(inst[4 * SR_INST_FLOATS + 7], 0, 0); /* standing */
  /* the near budget: past max_near, close citizens are instanced too */
  x.max_near = 0;
  for (int i = 0; i < 5; i++) cr->agents[i].pos.x = home.x + 3.0f;
  n = sr_build_crowd_lod(cr, NULL, &x, home, 28.0f, 200.0f, &near, inst, SR_MAX_AGENTS);
  SR_CHECK_EQ(n, 5);
  free(inst);
  sr_mesh_free(&near);
  free(cr);
}

static void test_groups(void) {
  SR_SPEC("population-simulation", "People talk in groups");
  const SrCity *c = g_game->city;
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  int blk = sr_city_block_at(c, hx, hy);
  /* a spot on the pavement with no prop that decides an activity */
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  SrVec3 spot = sr_v3(0, 0, 0);
  bool found = false;
  for (int k = 0; k < 200 && !found; k++) {
    spot = sr_v3((float)ox / 100.0f + 20.0f + (float)k * 0.37f, 0, (float)oy / 100.0f + 17.5f);
    found = sr_activity_prop_near(c, blk, spot, NULL, NULL) < 0;
  }
  SR_CHECK(found);
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  /* the first stands chatting on the phone; the second arrives and joins */
  uint64_t key_b = 0;
  for (uint64_t k = 100; k < 1000 && !key_b; k++)
    if (k % 10u < 6u) key_b = k;
  cr->agents[0] = agent_at(spot, 99, SR_ROLE_WORKER);
  cr->agents[0].state = SR_AG_IDLE;
  cr->agents[0].activity = SR_SA_PHONE;
  cr->agents[0].timer = 30.0f;
  cr->agents[0].block = blk;
  cr->agents[1] = agent_at(sr_v3(spot.x + 1.6f, 0, spot.z), key_b, SR_ROLE_WORKER);
  cr->agents[1].block = blk;
  cr->agents[1].target = sr_v3(spot.x + 1.2f, 0, spot.z);
  cr->agents[1].pending = SR_SA_IDLE;
  cr->agents[1].vel = 1.3f;
  cr->count = 2;
  for (int i = 0; i < 30; i++) sr_crowd_update(cr, c, 1.0f / 30.0f, 0.0f);
  const SrAgent *a = &cr->agents[0], *b = &cr->agents[1];
  if (b->activity != SR_SA_TALK) /* the arrival picked a non-free activity: nothing to check */
    printf("  group: second citizen chose %d\n", b->activity);
  SR_CHECK_EQ(b->activity, SR_SA_TALK);
  SR_CHECK_EQ(a->activity, SR_SA_TALK);
  SR_CHECK(a->group && a->group == b->group);
  SR_CHECK_EQ(a->group_n, 2);
  SR_CHECK(sr_v3_dot(a->dir, b->dir) < -0.9f); /* face each other */
  /* one speaks at a time */
  SrCrowdCtx x = ctx_at(spot);
  for (int k = 0; k < 20; k++) {
    x.now = 100.0 + k * 1.1;
    SrAnimInput ia, ib;
    sr_agent_anim(a, &x, &ia);
    sr_agent_anim(b, &x, &ib);
    SR_CHECK_EQ((ia.base == SR_CLIP_TALK) + (ib.base == SR_CLIP_TALK), 1);
  }
  SR_CHECK_EQ(sr_group_speaker(5, 0, 1.0), -1);
  free(cr);
}

static void test_rain_and_props(void) {
  SR_SPEC("population-simulation", "Rain changes the walk");
  SR_SPEC("retro-rendering", "Umbrellas are held");
  SR_SPEC("retro-rendering", "Footsteps splash");
  SR_SPEC("retro-rendering", "Accessories show");
  const SrCity *c = g_game->city;
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  SrVec3 home = sr_v3((float)hx / 100.0f, 0, (float)hy / 100.0f);
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  cr->agents[0] = agent_at(home, 12, SR_ROLE_WORKER);
  cr->agents[0].target = sr_v3(home.x + 40.0f, 0, home.z);
  cr->agents[0].block = sr_city_block_at(c, hx, hy);
  cr->count = 1;
  for (int i = 0; i < 120; i++) sr_crowd_update(cr, c, 1.0f / 60.0f, 0.35f); /* drizzle */
  SR_CHECK(cr->agents[0].vel > cr->agents[0].speed * 1.2f);
  SrCrowdCtx x = ctx_at(home);
  x.rain = 0.35f;
  SrAnimInput in;
  sr_agent_anim(&cr->agents[0], &x, &in);
  SR_CHECK(in.hunch > 0.3f);
  /* the four ways to walk in the rain without an umbrella */
  int hoods = 0, covers = 0;
  for (uint64_t k = 0; k < 8; k++) {
    cr->agents[0].key = k;
    sr_agent_anim(&cr->agents[0], &x, &in);
    hoods += in.hood;
    covers += in.upper == SR_CLIP_COVER_HEAD;
  }
  SR_CHECK(hoods > 0 && covers > 0);
  /* umbrella: open in a raised hand, the canopy tilted into the wind */
  cr->agents[0].umbrella = true;
  cr->agents[0].look.accessory = 1;
  x.wind = sr_v3(3.0f, 0, 0);
  sr_agent_anim(&cr->agents[0], &x, &in);
  SR_CHECK_EQ(in.prop_r, SR_HP_UMBRELLA);
  SR_CHECK_EQ(in.upper, SR_CLIP_UMBRELLA);
  SrSkel sk;
  sr_anim_skel(&cr->agents[0].look, &sk);
  SrPose p;
  sr_anim_pose(&in, &sk, 1.0, 0.2f, &p);
  SrVec3 hand = sr_anim_point(&sk, &p, 3);
  SR_CHECK(hand.y > sk.hip + sk.sho_y - 0.05f); /* the hand is up at the shoulder or higher */
  SrMesh calm, windy;
  sr_mesh_init(&calm);
  sr_mesh_init(&windy);
  sr_anim_mesh(&windy, &in, &sk, &p, 0);
  in.wind_x = 0;
  sr_anim_mesh(&calm, &in, &sk, &p, 0);
  float xw = 0, xc = 0, cut = sk.tall + 0.35f;
  int nw = 0, nc = 0;
  for (int v = 0; v < windy.nv; v++)
    if (windy.v[v].y > cut) {
      xw += windy.v[v].x;
      nw++;
    }
  for (int v = 0; v < calm.nv; v++)
    if (calm.v[v].y > cut) {
      xc += calm.v[v].x;
      nc++;
    }
  SR_CHECK(nw > 0 && nc > 0);
  if (nw && nc) SR_CHECK(xw / (float)nw < xc / (float)nc - 0.03f); /* the canopy tilts toward the wind */
  x.rain = 0;
  sr_agent_anim(&cr->agents[0], &x, &in);
  SR_CHECK(in.prop_r == SR_HP_BRIEFCASE || in.prop_r == SR_HP_BAG || in.prop_r == SR_HP_UMBRELLA_CLOSED);
  /* accessories in the hand; cigarettes and phones glow */
  for (int acc = 0; acc < 8; acc++) {
    SrAppearance la = look_of(1, SR_ROLE_EXEC);
    la.accessory = (uint8_t)acc;
    int prop = sr_anim_accessory(&la);
    SrAnimInput ai = input_for(&la, prop == SR_HP_CIGARETTE ? SR_CLIP_SMOKE : SR_CLIP_IDLE);
    ai.prop_r = (uint8_t)prop;
    SrSkel s2;
    sr_anim_skel(&la, &s2);
    SrPose p2;
    sr_anim_pose(&ai, &s2, 2.0, 0, &p2);
    SrMesh with, without;
    sr_mesh_init(&with);
    sr_mesh_init(&without);
    sr_anim_mesh(&with, &ai, &s2, &p2, 0);
    ai.prop_r = SR_HP_NONE;
    sr_anim_mesh(&without, &ai, &s2, &p2, 0);
    if (prop) SR_CHECK(with.nv > without.nv);
    if (prop == SR_HP_CIGARETTE || prop == SR_HP_PHONE) {
      int glow = 0;
      for (int v = without.nv; v < with.nv; v++) glow += with.v[v].emissive > 50;
      SR_CHECK(glow > 0);
    }
    sr_mesh_free(&with);
    sr_mesh_free(&without);
  }
  SrAppearance cop = look_of(0, SR_ROLE_COP);
  SR_CHECK_EQ(sr_anim_accessory(&cop), SR_HP_NONE);
  SrAppearance coat = look_of(0, SR_ROLE_EXEC);
  coat.outfit = (uint8_t)(SR_ROLE_EXEC * 4 + 2);
  SR_CHECK(sr_anim_has_coat(&coat));
  coat.accessory = 6;
  coat.outfit = (uint8_t)(SR_ROLE_WORKER * 4);
  SR_CHECK(sr_anim_has_backpack(&coat));
  /* footfalls on wet ground splash, within the cap */
  sr_crowd_init(cr);
  for (int i = 0; i < 6; i++) {
    cr->agents[i] = agent_at(sr_v3(home.x + (float)i, 0, home.z + 3.0f), 30u + (uint64_t)i, SR_ROLE_WORKER);
    cr->agents[i].vel = 1.4f;
  }
  cr->count = 6;
  SrAnimCache *anim = (SrAnimCache *)malloc(sizeof *anim);
  sr_anim_cache_init(anim);
  SrMesh near, fx;
  sr_mesh_init(&near);
  sr_mesh_init(&fx);
  float *inst = (float *)calloc(SR_MAX_AGENTS * SR_INST_FLOATS, sizeof(float));
  x = ctx_at(home);
  x.wet = 0.8f;
  x.fx = &fx;
  x.max_splashes = 4;
  int max_seen = 0;
  for (int f = 0; f < 90; f++) {
    x.now = 10.0 + f / 30.0;
    x.splashes = 0;
    for (int i = 0; i < 6; i++) cr->agents[i].walk += 1.4f / 30.0f;
    sr_anim_cache_begin(anim, x.now);
    sr_build_crowd_lod(cr, anim, &x, home, 28.0f, 200.0f, &near, inst, SR_MAX_AGENTS);
    sr_anim_cache_end(anim);
    max_seen = SR_MAX(max_seen, x.splashes);
  }
  SR_CHECK(fx.nv > 0);
  SR_CHECK(max_seen <= 4);
  x.wet = 0.1f; /* dry enough: no splashes */
  sr_mesh_clear(&fx);
  x.splashes = 0;
  for (int i = 0; i < 6; i++) cr->agents[i].walk += 0.7f;
  sr_anim_cache_begin(anim, x.now + 0.5);
  sr_build_crowd_lod(cr, anim, &x, home, 28.0f, 200.0f, &near, inst, SR_MAX_AGENTS);
  SR_CHECK_EQ(fx.nv, 0);
  free(inst);
  free(anim);
  sr_mesh_free(&near);
  sr_mesh_free(&fx);
  sr_mesh_free(&calm);
  sr_mesh_free(&windy);
  free(cr);
}

static void test_reactions(void) {
  SR_SPEC("population-simulation", "Panic reads as panic");
  SR_SPEC("population-simulation", "Police show their intent");
  const SrCity *c = g_game->city;
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  SrVec3 home = sr_v3((float)hx / 100.0f, 0, (float)hy / 100.0f);
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  for (int i = 0; i < 40; i++) {
    cr->agents[i] =
        agent_at(sr_v3(home.x + (float)(i % 8), 0, home.z + (float)(i / 8)), 500u + (uint64_t)i, SR_ROLE_WORKER);
    cr->agents[i].block = sr_city_block_at(c, hx, hy);
    cr->agents[i].bravery = (uint8_t)(i * 6);
  }
  cr->count = 40;
  sr_crowd_gunfire(cr, c, home, 30.0f, NULL, 0);
  for (int i = 0; i < 120; i++) sr_crowd_update(cr, c, 1.0f / 60.0f, 0.0f);
  SrCrowdCtx x = ctx_at(home);
  int flee = 0, cower = 0, gawk = 0;
  for (int i = 0; i < cr->count; i++) {
    const SrAgent *a = &cr->agents[i];
    SrAnimInput in;
    sr_agent_anim(a, &x, &in);
    if (a->state == SR_AG_FLEE) {
      flee++;
      SR_CHECK_NEAR(a->vel, SR_FLEE_SPEED, 1e-3);
      SR_CHECK_EQ(in.upper, SR_CLIP_FLEE_ARMS);
      SR_CHECK_EQ(in.once, SR_CLIP_RECOIL);
      /* the run gait, an arm over the head, and a look back */
      SrSkel s;
      sr_anim_skel(&a->look, &s);
      float look_back = 0;
      for (int k = 0; k < 12; k++) {
        SrPose p;
        sr_anim_pose(&in, &s, x.now + k * 0.25, 0.3f, &p);
        look_back = fmaxf(look_back, fabsf(p.j[SR_J_HEAD_YAW]));
        SR_CHECK(p.j[SR_J_SHO_L] > 2.0f);
        SR_CHECK(p.j[SR_J_ELB_R] > 1.0f); /* running arms */
      }
      SR_CHECK(look_back > 0.9f);
    }
    if (a->state == SR_AG_COVER) {
      cower++;
      SR_CHECK_EQ(in.base, SR_CLIP_COWER);
    }
    if (a->state == SR_AG_GAWK) {
      gawk++;
      SR_CHECK(in.base == SR_CLIP_FILM || in.base == SR_CLIP_POINT || in.base == SR_CLIP_CALL ||
               in.base == SR_CLIP_IDLE);
    }
  }
  SR_CHECK(flee > 0);
  printf("  gunfire: %d flee, %d cower, %d gawk\n", flee, cower, gawk);
  /* officers: a beat walk, the weapon ready at Heat 3, aimed at Heat 4 */
  SrAgent cop = agent_at(sr_v3(home.x + 5, 0, home.z), 900, SR_ROLE_COP);
  SrAnimInput in;
  sr_agent_anim(&cop, &x, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_BEAT);
  x.heat = 3;
  sr_agent_anim(&cop, &x, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_READY);
  SR_CHECK_EQ(in.prop_r, SR_HP_PISTOL);
  x.heat = 4;
  sr_agent_anim(&cop, &x, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_AIM_PISTOL);
  SR_CHECK(in.look_at);
  x.heat = 0;
  cop.activity = SR_SA_WAIT;
  sr_agent_anim(&cop, &x, &in);
  SR_CHECK(in.upper == SR_CLIP_WAVE || in.upper == SR_CLIP_STOP);
  /* shaking off the rain on reaching shelter */
  SrAgent wet = agent_at(home, 901, SR_ROLE_WORKER);
  wet.state = SR_AG_SHELTER;
  wet.covered = true;
  wet.state_time = 0.3f;
  sr_agent_anim(&wet, &x, &in);
  SR_CHECK_EQ(in.once, SR_CLIP_SHAKE);
  free(cr);
}

/* ---- vehicles --------------------------------------------------------------------------- */

static void test_vehicles(void) {
  SR_SPEC("population-simulation", "Cars brake and pull away");
  SR_SPEC("population-simulation", "Cars turn at junctions");
  SR_SPEC("retro-rendering", "Headlights cut through the rain");
  SR_SPEC("retro-rendering", "Wheels turn");
  const SrCity *c = g_game->city;
  float B = (float)SR_BLOCK_CM / 100.0f;
  /* a car that will turn: find one heading +x whose next junction is a turn onto land */
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  int bx = hx / SR_BLOCK_CM, by = hy / SR_BLOCK_CM;
  int turned = 0, signalled = 0, braked = 0;
  for (uint64_t key = 1; key < 60; key++) {
    SrVehicle v;
    memset(&v, 0, sizeof v);
    v.key = key;
    v.dir = sr_v3(1, 0, 0);
    v.pos = sr_v3((float)bx * B - 30.0f, 0, (float)by * B + 4.0f);
    v.speed = v.vel = 10.0f;
    bool was_signalling = false;
    for (int i = 0; i < 300 && !v.turning; i++) {
      sr_vehicle_drive(&v, c, NULL, 0, 1.0f / 30.0f);
      was_signalling |= v.indicator != 0;
    }
    if (!v.turning) continue;
    signalled += was_signalling;
    SrVec3 d1 = v.turn_d1;
    for (int i = 0; i < 300 && v.turning; i++) sr_vehicle_drive(&v, c, NULL, 0, 1.0f / 30.0f);
    SR_CHECK(!v.turning);
    SR_CHECK(fabsf(v.dir.x - d1.x) < 1e-4f && fabsf(v.dir.z - d1.z) < 1e-4f);
    /* it ends on the cross lane's offset, on the road */
    float lane = fmodf(v.pos.x, B);
    SR_CHECK(fabsf(lane - (d1.z > 0 ? 4.0f : 12.0f)) < 0.02f);
    int blk = sr_city_block_at(c, (int32_t)(v.pos.x * 100.0f), (int32_t)(v.pos.z * 100.0f));
    SR_CHECK(blk >= 0 && !c->blocks[blk].water);
    turned++;
    if (turned >= 6) break;
  }
  SR_CHECK(turned > 0);
  SR_CHECK_EQ(signalled, turned);
  /* braking for a pedestrian: no faster than the brake limit, stops short, then pulls away */
  SrVehicle v;
  memset(&v, 0, sizeof v);
  v.key = 999;
  v.dir = sr_v3(0, 0, 1);
  v.pos = sr_v3(12.0f, 0, 20.0f); /* mid-block: no junction to turn at */
  v.speed = v.vel = 12.0f;
  SrVec3 ped = sr_v3(12.0f, 0, 42.0f);
  float max_decel = 0;
  for (int i = 0; i < 180; i++) {
    float before = v.vel;
    v.turning = false;
    sr_vehicle_drive(&v, c, &ped, 1, 1.0f / 60.0f);
    max_decel = fmaxf(max_decel, (before - v.vel) * 60.0f);
    braked += v.stopped;
  }
  SR_CHECK(braked > 0);
  SR_CHECK(max_decel <= SR_CAR_BRAKE + 1e-3f);
  SR_CHECK_NEAR(v.vel, 0, 1e-4);
  SR_CHECK(v.pos.z < ped.z - 0.5f);
  for (int i = 0; i < 10; i++) sr_vehicle_drive(&v, c, NULL, 0, 1.0f / 60.0f);
  SR_CHECK(v.vel > 0 && v.vel < 1.0f); /* accelerating, not jumping to speed */
  SR_CHECK(v.accel > 0);
  SR_CHECK(sr_vehicle_turn_at(&v, 3) >= -1 && sr_vehicle_turn_at(&v, 3) <= 1);
  /* wheels turn with the distance driven */
  SrVehicle w;
  memset(&w, 0, sizeof w);
  w.odo = 1.0f;
  SR_CHECK_NEAR(sr_wheel_angle(&w), 1.0f / 0.34f, 1e-4);
  /* headlights: two beams per car within 60 m, capped */
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  for (int i = 0; i < 6; i++) {
    cr->vehicles[i].key = (uint64_t)i + 1;
    cr->vehicles[i].pos = sr_v3((float)i * 8.0f, 0, 0);
    cr->vehicles[i].dir = sr_v3(0, 0, 1);
    cr->vehicles[i].vel = 8.0f;
  }
  cr->vehicles[5].pos.x = 500.0f; /* too far */
  cr->vehicle_count = 6;
  SrMesh fx, mesh;
  sr_mesh_init(&fx);
  sr_mesh_init(&mesh);
  SR_CHECK_EQ(sr_build_headlights(cr, sr_v3(0, 1.7f, 0), 0.9f, 10, 16, &fx), 10);
  SR_CHECK_EQ(sr_build_headlights(cr, sr_v3(0, 1.7f, 0), 0.9f, 10, 4, &fx), 4);
  /* bodies: wheels, lights, indicators, wipers, braking */
  SrCrowdCtx x = ctx_at(sr_v3(0, 1.7f, 0));
  x.rain = 0.5f;
  x.now = 0.1;
  cr->vehicles[0].indicator = 1;
  cr->vehicles[0].turning = true;
  cr->vehicles[1].accel = -3.0f;
  sr_build_vehicles(cr, &x, sr_v3(0, 1.7f, 0), 100.0f, &mesh);
  SR_CHECK(mesh.nv > 5 * 100);
  sr_build_vehicles(cr, NULL, sr_v3(0, 1.7f, 0), 100.0f, &mesh);
  sr_mesh_free(&fx);
  sr_mesh_free(&mesh);
  free(cr);
}

static void test_aerodynes(void) {
  SR_SPEC("population-simulation", "Aerodynes land on roofs");
  const SrCity *c = g_game->city;
  /* lane traffic banks into its weave and bobs */
  float max_roll = 0;
  for (int k = 0; k < 60; k++) {
    SrAerodyne v = sr_air_vehicle(&g_game->air, 0, k % SR_AIR_PER_LANE, (double)k * 1.3);
    max_roll = fmaxf(max_roll, fabsf(v.roll));
    SR_CHECK(fabsf(v.roll) <= 0.44f + 1e-5f);
    SR_CHECK_EQ(v.mode, SR_AIR_CRUISE);
  }
  SR_CHECK(max_roll > 0.05f);
  /* a helipad near the player: descend, stand, lift off, at the same times in every game */
  int pad = -1;
  for (int b = 0; b < c->building_count && pad < 0; b++) {
    SrAerodyne a;
    for (int t = 0; t < 300 && pad < 0; t += 5)
      if (sr_air_lander(c, b, (double)t, &a)) pad = b;
  }
  SR_CHECK(pad >= 0);
  if (pad < 0) return;
  int seen[4] = {0};
  float landed_y = -1, min_y = 1e9f, max_y = -1e9f;
  for (int t = 0; t < 400; t++) {
    SrAerodyne a, b;
    if (!sr_air_lander(c, pad, (double)t, &a)) continue;
    SR_CHECK(sr_air_lander(c, pad, (double)t, &b));
    SR_CHECK(a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z);
    seen[a.mode]++;
    if (a.mode == SR_AIR_LANDED) landed_y = a.pos.y;
    min_y = fminf(min_y, a.pos.y);
    max_y = fmaxf(max_y, a.pos.y);
  }
  SR_CHECK(seen[SR_AIR_DESCEND] > 0 && seen[SR_AIR_LANDED] >= 20 && seen[SR_AIR_LIFT] > 0);
  SR_CHECK_NEAR(landed_y, min_y, 1e-3);
  SR_CHECK(max_y > landed_y + 20.0f); /* it comes down from the sky lanes */
  /* within 200 m of the camera it is drawn; buildings without a helipad never get one */
  int32_t cx, cy;
  sr_building_center(&c->buildings[pad], &cx, &cy);
  SrAerodyne out[16];
  int found = 0;
  for (int t = 0; t < 400 && !found; t += 3)
    found = sr_air_landers(c, (double)t, sr_v3((float)cx / 100.0f, 0, (float)cy / 100.0f), 200.0f, out, 16);
  SR_CHECK(found > 0);
  SR_CHECK_EQ(sr_air_landers(c, 0.0, sr_v3(-9000, 0, -9000), 200.0f, out, 16), 0);
  SrMesh m, fx;
  sr_mesh_init(&m);
  sr_mesh_init(&fx);
  sr_build_aerodynes(&g_game->air, c, 10.0, sr_v3((float)cx / 100.0f, 50, (float)cy / 100.0f), 600.0f, &m, &fx);
  SR_CHECK(m.nv > 0 && fx.nv > 0);
  sr_build_aerodynes(&g_game->air, NULL, 10.5, sr_v3(0, 50, 0), 600.0f, &m, NULL);
  sr_mesh_free(&m);
  sr_mesh_free(&fx);
}

/* ---- combat ----------------------------------------------------------------------------- */

static void test_guards(void) {
  SR_SPEC("combat", "Alert state shows in the stance");
  SR_SPEC("combat", "Hits show");
  SR_SPEC("combat", "The dead fall");
  SR_SPEC("combat", "Bodies stay down after a load");
  SR_SPEC("combat", "Surrender shows");
  SR_SPEC("combat", "Enemy fire can be traced");
  SrSite *site = (SrSite *)calloc(1, sizeof *site);
  site->guard_count = 2;
  SrGuard *g = &site->guards[0];
  g->id = 3; /* a pistol */
  g->e.pos = sr_v3(5, 0, 5);
  g->e.yaw = 0;           /* faces -z */
  site->guards[1].id = 4; /* a rifle */
  site->guards[1].e.pos = sr_v3(8, 0, 5);
  SrAppearance a = sr_appearance_generate(g->id, SR_ROLE_GUARD, 0);
  SrVec3 player = sr_v3(5, 1.6f, -3);
  SrAnimInput in;
  site->guards[1].e.alert = SR_ALERT_UNAWARE;
  sr_guard_anim(site, &site->guards[1], &a, 1.0, player, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_SLUNG);
  SR_CHECK_EQ(in.prop_r, SR_HP_RIFLE);
  site->guards[1].pause = 1.0f;
  sr_guard_anim(site, &site->guards[1], &a, 1.0, player, &in);
  SR_CHECK_EQ(in.base, SR_CLIP_LOOK_AROUND);
  g->e.alert = SR_ALERT_UNAWARE;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.prop_r, SR_HP_NONE); /* pistol holstered */
  g->e.alert = SR_ALERT_SUSPICIOUS;
  g->e.investigate = sr_v3(9, 0, 5);
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK(in.look_at && in.upper == SR_CLIP_READY);
  g->e.alert = SR_ALERT_SEARCHING;
  g->speed = 1.0f;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_READY);
  g->e.alert = SR_ALERT_COMBAT;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.upper, SR_CLIP_AIM_PISTOL);
  SR_CHECK(in.look_at);
  site->guards[1].e.alert = SR_ALERT_COMBAT;
  site->guards[1].speed = 0;
  sr_guard_anim(site, &site->guards[1], &a, 1.0, player, &in);
  SR_CHECK_EQ(in.base, SR_CLIP_PEEK);
  /* hits: a flinch away from the shot, a stagger on heavy hits, a snap on headshots */
  SrSkel s;
  sr_anim_skel(&a, &s);
  g->speed = 0;
  g->hit_time = 2.0f;
  g->hit_dir = 0.0f; /* from +z: behind the guard */
  for (int kind = 1; kind <= 3; kind++) {
    g->hit_kind = (uint8_t)kind;
    sr_guard_anim(site, g, &a, 2.1, player, &in);
    static const int clips[4] = {0, SR_CLIP_FLINCH, SR_CLIP_STAGGER, SR_CLIP_HEADSHOT};
    SR_CHECK_EQ(in.once, clips[kind]);
    SrPose hit, calm;
    sr_anim_pose(&in, &s, 2.0 + sr_anim_clip_len(clips[kind]) * 0.25, 0, &hit);
    in.once = 0;
    sr_anim_pose(&in, &s, 2.0 + sr_anim_clip_len(clips[kind]) * 0.25, 0, &calm);
    float d = 0;
    for (int j = 0; j < SR_J_COUNT; j++) d = fmaxf(d, fabsf(hit.j[j] - calm.j[j]));
    SR_CHECK(d > 0.15f);
  }
  sr_guard_anim(site, g, &a, 4.0, player, &in);
  SR_CHECK_EQ(in.once, 0); /* the flinch is over */
  /* falls: away from the hit, ending on the floor within 0.6-0.9 s */
  g->hit_kind = 0;
  g->e.dead = true;
  g->down_time = 10.0f;
  static const float dirs[3] = {SR_PI, 0.0f, SR_PI * 0.5f}; /* shot from the front, behind, the side */
  static const int falls[3] = {SR_CLIP_DIE_BACK, SR_CLIP_DIE_FRONT, SR_CLIP_DIE_SIDE};
  for (int k = 0; k < 3; k++) {
    g->hit_dir = dirs[k];
    sr_guard_anim(site, g, &a, 10.0, player, &in);
    SR_CHECK_EQ(in.once, falls[k]);
    float len = sr_anim_clip_len(in.once);
    SR_CHECK(len >= 0.6f && len <= 0.9f);
    SrPose p0, p1;
    sr_anim_pose(&in, &s, 10.05, 0, &p0);
    sr_anim_pose(&in, &s, 10.0 + len + 0.5, 0, &p1);
    SR_CHECK(sr_anim_point(&s, &p0, 4).y > 1.3f);
    SR_CHECK(sr_anim_point(&s, &p1, 4).y < 0.6f);
  }
  g->hit_dir = SR_PI; /* fell back: away from a shot from the front (forward is -z) */
  sr_guard_anim(site, g, &a, 12.0, player, &in);
  SrPose lying;
  sr_anim_pose(&in, &s, 12.0, 0, &lying);
  SR_CHECK(sr_anim_point(&s, &lying, 4).z < -0.8f); /* the head lies behind the feet */
  g->e.dead = false;
  g->e.ko = true;
  sr_guard_anim(site, g, &a, 10.2, player, &in);
  SR_CHECK_EQ(in.once, SR_CLIP_SLUMP);
  /* a body from an earlier visit lies still */
  g->down_time = -1.0f;
  sr_guard_anim(site, g, &a, 0.01, player, &in);
  SR_CHECK(in.once_t0 < 0);
  SrPose still;
  sr_anim_pose(&in, &s, 0.01, 0, &still);
  SR_CHECK(sr_anim_point(&s, &still, 4).y < 0.6f);
  /* status poses */
  g->e.restrained = true;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.base, SR_CLIP_RESTRAINED);
  g->e.ko = g->e.restrained = false;
  g->e.surrendered = true;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.base, SR_CLIP_SURRENDER);
  SR_CHECK_EQ(in.prop_r, SR_HP_NONE); /* the weapon is dropped */
  SrPose up;
  sr_anim_pose(&in, &s, 1.0, 0, &up);
  SR_CHECK(sr_anim_point(&s, &up, 2).y > sr_anim_point(&s, &up, 4).y - 0.1f); /* hands up by the head */
  g->e.surrendered = false;
  g->stun = 2.0f;
  sr_guard_anim(site, g, &a, 1.0, player, &in);
  SR_CHECK_EQ(in.base, SR_CLIP_STUNNED);
  g->stun = 0;
  /* guards draw with the cache; the blood pool spreads */
  SrMesh m, fx;
  sr_mesh_init(&m);
  sr_mesh_init(&fx);
  SrAnimCache *anim = (SrAnimCache *)malloc(sizeof *anim);
  sr_anim_cache_init(anim);
  g->e.dead = true;
  g->down_time = 1.0f;
  site->guards[1].e.drone = true;
  sr_anim_cache_begin(anim, 1.5);
  sr_build_guards(site, anim, 1.5, player, &m);
  int early = m.nv;
  SR_CHECK(early > 0);
  sr_anim_cache_end(anim);
  /* enemy fire: a flash, a light and (every third round) a tracer */
  g->e.dead = false;
  g->shot_time = 3.0f;
  g->shot_tracer = true;
  SrLight lights[8];
  int nl = sr_build_enemy_fire(site, 3.05, player, player, &fx, lights, 0, 8);
  SR_CHECK_EQ(nl, 1);
  SR_CHECK(fx.nv >= 4 * 8);
  SR_CHECK(lights[0].radius > 3.0f);
  sr_mesh_clear(&fx);
  SR_CHECK_EQ(sr_build_enemy_fire(site, 3.5, player, player, &fx, lights, 0, 8), 0); /* the flash is over */
  SR_CHECK_EQ(fx.nv, 0);
  free(anim);
  sr_mesh_free(&m);
  sr_mesh_free(&fx);
  free(site);
}

/* ---- the player's body ------------------------------------------------------------------- */

static int arch_of(int cat) {
  for (int i = 0; i < sr_archetype_count(); i++)
    if (sr_archetype(i)->category == cat) return i;
  return 0;
}

static uint64_t view_digest(SrGame *g, SrMesh *m) {
  sr_build_view_model(g, sr_view_camera(g), 0, 0, m);
  return sr_mesh_digest(m);
}

static void test_view_model(void) {
  SR_SPEC("retro-rendering", "Hands and weapon in view");
  SR_SPEC("retro-rendering", "The weapon moves with the body");
  SR_SPEC("retro-rendering", "Shots kick and light the room");
  SrGame *g = sr_game_create();
  sr_game_new(g, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "View");
  sr_game_leave(g);
  g->steps = 6000;
  SrMesh m;
  sr_mesh_init(&m);
  g->player.inv.count = 0;
  g->player.equipped = -1;
  SR_CHECK_EQ(sr_view_category(g), -1);
  SR_CHECK_EQ(sr_build_view_model(g, sr_view_camera(g), 0, 0, &m), 0); /* unarmed, idle: nothing */
  g->fx.reach_t = sr_game_fx_now(g) - 0.2;
  SR_CHECK(sr_build_view_model(g, sr_view_camera(g), 0, 0, &m) > 0); /* a hand reaching for a door */
  g->fx.reach_t = 0;
  /* a model for every category, within the budget, always in front of the eye */
  for (int cat = 0; cat < SR_WC_COUNT; cat++) {
    g->player.inv.count = 0;
    sr_inv_add(&g->player.inv, sr_item_weapon(arch_of(cat), 0, 1, 5));
    g->player.equipped = 0;
    SR_CHECK_EQ(sr_view_category(g), cat);
    SrViewCam c = sr_view_camera(g);
    int tris = sr_build_view_model(g, c, 0, 0, &m);
    SR_CHECK(tris > 0 && tris <= 400);
    SrVec3 fwd = sr_dir_from_yaw_pitch(c.yaw, c.pitch);
    for (int v = 0; v < m.nv; v++) {
      SrVec3 d = sr_v3_sub(sr_v3(m.v[v].x, m.v[v].y, m.v[v].z), c.eye);
      SR_CHECK(sr_v3_len(d) < 1.3f);
      SR_CHECK(sr_v3_dot(d, fwd) > -0.6f);
    }
  }
  /* the rifle: bob, sprint, crouch, sway, shot, reload, swing, throw, reach all move it */
  g->player.inv.count = 0;
  sr_inv_add(&g->player.inv, sr_item_weapon(arch_of(SR_WC_LONG), 0, 1, 5));
  g->player.equipped = 0;
  g->on_ground = true;
  uint64_t idle = view_digest(g, &m);
  g->speed = 4.0f;
  g->fx.bob = 0.37f;
  uint64_t walking = view_digest(g, &m);
  SR_CHECK(walking != idle);
  g->speed = 7.0f;
  SR_CHECK(view_digest(g, &m) != walking);
  g->speed = 0;
  g->fx.bob = 0;
  g->player.crouched = true;
  SR_CHECK(view_digest(g, &m) != idle);
  g->player.crouched = false;
  sr_build_view_model(g, sr_view_camera(g), 0.03f, 0.02f, &m);
  SR_CHECK(sr_mesh_digest(&m) != idle);
  double now = sr_game_fx_now(g);
  double *timers[5] = {&g->fx.shot_t, &g->fx.reload_t, &g->fx.melee_t, &g->fx.throw_t, &g->fx.reach_t};
  g->fx.recoil = 1.0f;
  for (int k = 0; k < 5; k++) {
    *timers[k] = now - 0.1;
    SR_CHECK(view_digest(g, &m) != idle);
    *timers[k] = now - 5.0; /* long over: back at rest */
    SR_CHECK_EQ(view_digest(g, &m), idle);
    *timers[k] = 0;
  }
  /* a shot: the weapon kicks back, the view kicks up, the muzzle lights the room for two frames */
  SrViewCam c0 = sr_view_camera(g);
  SrVec3 mz;
  SR_CHECK(!sr_view_muzzle(g, c0, 0, 0, &mz));
  g->fx.shot_t = now - 0.01;
  SrViewCam c1 = sr_view_camera(g);
  SR_CHECK(c1.pitch > c0.pitch);
  SR_CHECK(sr_view_muzzle(g, c1, 0, 0, &mz));
  SR_CHECK(sr_v3_len(sr_v3_sub(mz, c1.eye)) < 1.2f);
  SrMesh fx;
  sr_mesh_init(&fx);
  sr_build_muzzle_fx(g, c1, 0, 0, &fx);
  SR_CHECK(fx.nv >= 8); /* the flash and a shell */
  g->fx.shot_t = now - 0.06;
  SR_CHECK(!sr_view_muzzle(g, sr_view_camera(g), 0, 0, &mz)); /* the light is gone after two frames */
  g->fx.shot_t = now - 1.0;
  SR_CHECK_NEAR(sr_view_camera(g).pitch, c0.pitch, 1e-6); /* recovered */
  /* firing through the game sets the timers, and a magazine's worth starts a reload */
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_AMMO, 0, 200));
  g->fx.shots = 0;
  g->fx.reload_t = 0;
  int mag = sr_archetype(arch_of(SR_WC_LONG))->mag;
  for (int i = 0; i < mag; i++) sr_game_fire(g);
  SR_CHECK_NEAR(g->fx.shot_t, now, 1e-9);
  SR_CHECK(g->fx.reload_t > now);
  /* in the scene: a muzzle light first in the frame, the view model drawn last */
  SrSceneCache *sc = (SrSceneCache *)calloc(1, sizeof *sc);
  sr_scene_init(sc, false);
  SrFrame *f = (SrFrame *)calloc(1, sizeof *f);
  g->fx.shot_t = now;
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SR_CHECK(f->nlights > 0 && f->lights[0].radius == 8.0f);
  bool view = false;
  for (int i = 0; i < f->ndraws; i++) view |= f->draws[i].pass == SR_PASS_VIEW;
  SR_CHECK(view);
  SR_CHECK(sc->view_tris > 0);
  sr_scene_free(sc);
  free(sc);
  free(f);
  sr_mesh_free(&fx);
  sr_mesh_free(&m);
  sr_game_destroy(g);
}

static void test_camera_motion(void) {
  SR_SPEC("player-life", "Head bob");
  SR_SPEC("player-life", "Landing dips the view");
  SR_SPEC("player-life", "Damage shows its direction");
  SR_SPEC("player-life", "Head bob can be turned off");
  SR_SPEC("player-life", "Shake can be turned off");
  SrGame *g = sr_game_create();
  sr_game_new(g, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Cam");
  sr_game_leave(g);
  g->steps = 6000;
  g->on_ground = true;
  g->fx.eye_y = g->player.pos.y;
  /* walking: the eye rises and falls about 3 cm, sways 1.5 cm; double sprinting, half crouched */
  float range[3] = {0}, sway[3] = {0};
  float speeds[3] = {4.0f, 7.0f, 2.0f};
  for (int mode = 0; mode < 3; mode++) {
    g->speed = speeds[mode];
    g->player.crouched = mode == 2;
    float lo = 1e9f, hi = -1e9f, sl = 1e9f, sh = -1e9f;
    for (int i = 0; i < 100; i++) {
      g->fx.bob = (float)i * 0.03f;
      SrViewCam c = sr_view_camera(g);
      lo = fminf(lo, c.bob_y);
      hi = fmaxf(hi, c.bob_y);
      sl = fminf(sl, c.bob_x);
      sh = fmaxf(sh, c.bob_x);
    }
    range[mode] = hi - lo;
    sway[mode] = (sh - sl) * 0.5f;
  }
  SR_CHECK_NEAR(range[0], 0.03f, 0.004f);
  SR_CHECK_NEAR(sway[0], 0.015f * 2.0f, 0.004f);
  SR_CHECK_NEAR(range[1], 2.0f * range[0], 0.005f);
  SR_CHECK_NEAR(range[2], 0.5f * range[0], 0.003f);
  /* off: a constant eye height; the position the game aims from never moves */
  g->settings.head_bob = false;
  g->player.crouched = false;
  g->speed = 7.0f;
  SrVec3 aim = g->player.pos;
  for (int i = 0; i < 50; i++) {
    g->fx.bob = (float)i * 0.05f;
    SrViewCam c = sr_view_camera(g);
    SR_CHECK_NEAR(c.eye.y, g->fx.eye_y, 1e-6);
    SR_CHECK_NEAR(c.bob_x, 0, 1e-6);
  }
  SR_CHECK(g->player.pos.x == aim.x && g->player.pos.y == aim.y);
  g->settings.head_bob = true;
  g->speed = 0;
  /* landing: a dip proportional to the fall speed, at most 8 cm, recovered */
  double now = sr_game_fx_now(g);
  float base = sr_view_camera(g).eye.y;
  g->fx.land_t = now - 0.175;
  g->fx.land_v = 3.0f;
  float dip_soft = base - sr_view_camera(g).eye.y;
  g->fx.land_v = 30.0f;
  float dip_hard = base - sr_view_camera(g).eye.y;
  SR_CHECK(dip_soft > 0.02f && dip_soft < dip_hard);
  SR_CHECK(dip_hard <= 0.0801f);
  g->fx.land_t = now - 1.0;
  SR_CHECK_NEAR(sr_view_camera(g).eye.y, base, 1e-6);
  /* a real jump lands through the game */
  SrActionState act;
  memset(&act, 0, sizeof act);
  act.pressed[SR_ACT_JUMP] = true;
  sr_game_step(g, &act);
  act.pressed[SR_ACT_JUMP] = false;
  for (int i = 0; i < 120 && !g->on_ground; i++) sr_game_step(g, &act);
  SR_CHECK(g->on_ground);
  SR_CHECK(g->fx.land_v > 3.0f);
  /* damage: the view flinches away and the red edge shows the side */
  float yaw0 = sr_view_camera(g).yaw;
  now = sr_game_fx_now(g);
  SrVec3 right = sr_v3_add(g->player.pos, sr_v3(cosf(g->player.yaw) * 5.0f, 0, -sinf(g->player.yaw) * 5.0f));
  sr_game_fx_hurt(g, 10.0f, true, right, 0);
  SR_CHECK(g->fx.hurt_dir > 1.2f && g->fx.hurt_dir < 1.9f);
  SR_CHECK(sr_view_camera(g).yaw > yaw0);
  float dir;
  bool known;
  SR_CHECK(sr_view_hurt_edge(g, &dir, &known) > 0.9f);
  SR_CHECK(known && dir > 0);
  SrUi ui;
  sr_ui_init(&ui);
  SrInput in;
  sr_input_init(&in, false);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  int red = 0;
  for (int v = 0; v < ui.mesh.nv; v++) red += ui.mesh.v[v].r == 200 && ui.mesh.v[v].g == 10 && ui.mesh.v[v].x > 1100;
  SR_CHECK(red >= 4); /* on the right edge */
  sr_game_fx_hurt(g, 5.0f, false, right, 0);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  sr_ui_free(&ui);
  g->fx.hurt_t = now - 2.0;
  SR_CHECK_NEAR(sr_view_hurt_edge(g, &dir, &known), 0, 0);
  sr_game_fx_hurt(g, 0, false, right, 0); /* nothing felt */
  /* shake: full, low, off */
  sr_game_fx_hurt(g, 0, false, right, 1.0f);
  g->fx.shake_t = sr_game_fx_now(g) - 0.05;
  float moved[3];
  for (int k = 0; k < 3; k++) {
    g->settings.camera_shake = 2 - k;
    SrViewCam c = sr_view_camera(g);
    moved[k] = fabsf(c.yaw - g->player.yaw) + fabsf(c.pitch - g->player.pitch);
  }
  SR_CHECK(moved[0] > moved[1] && moved[1] > 0);
  SR_CHECK_NEAR(moved[2], 0, 1e-6);
  /* settings round trip */
  SrSettings st;
  sr_settings_default(&st);
  SR_CHECK(st.head_bob && st.camera_shake == 2);
  SR_CHECK(sr_settings_parse(&st, "head_bob=0\ncamera_shake=1\n") == 2);
  SR_CHECK(!st.head_bob && st.camera_shake == 1);
  char buf[2048];
  sr_settings_write(&st, buf, sizeof buf);
  SR_CHECK(strstr(buf, "head_bob=0") && strstr(buf, "camera_shake=1"));
  /* the eye eases into a crouch over about 0.15 s */
  g->settings.camera_shake = 2;
  act.down[SR_ACT_CROUCH] = true;
  float e0 = g->fx.eye_y;
  sr_game_step(g, &act);
  SR_CHECK(g->fx.eye_y < e0 && g->fx.eye_y > e0 - 0.3f);
  for (int i = 0; i < 12; i++) sr_game_step(g, &act);
  SR_CHECK_NEAR(g->fx.eye_y, g->player.pos.y, 1e-4);
  sr_game_destroy(g);
}

/* ---- world motion ----------------------------------------------------------------------- */

static void test_world_motion(void) {
  SR_SPEC("retro-rendering", "Doors swing");
  SR_SPEC("retro-rendering", "Shutters roll at opening time");
  SR_SPEC("population-simulation", "Trains stop at stations");
  /* a door swings from shut to open over about 0.3 s */
  SrSite *site = (SrSite *)calloc(1, sizeof *site);
  site->in.w = 2;
  site->in.h = 2;
  site->in.doors[0] = 3; /* east and south */
  float ang[SR_INT_MAX_ROOMS * 2] = {0};
  SrMesh m;
  sr_mesh_init(&m);
  float at_150 = 0;
  for (int i = 0; i < 18; i++) {
    sr_mesh_clear(&m);
    sr_build_doors(site, ang, 1.0f / 60.0f, false, &m);
    if (i == 8) at_150 = ang[0];
  }
  SR_CHECK(at_150 > 0.35f && at_150 < 0.65f);
  SR_CHECK_NEAR(ang[0], 1.0f, 1e-4);
  SR_CHECK_NEAR(ang[1], 1.0f, 1e-4);
  SR_CHECK(m.nv == 2 * 24);
  float x_open = -1e9f;
  for (int v = 0; v < 24; v++) x_open = fmaxf(x_open, m.v[v].x);
  SR_CHECK(x_open > site->ox + 4.0f + 1.0f); /* the east door swung into the next room */
  site->in.doors[0] = 0;                     /* gone: nothing drawn */
  sr_mesh_clear(&m);
  sr_build_doors(site, ang, 1.0f / 60.0f, true, &m);
  SR_CHECK_EQ(m.nv, 0);
  free(site);
  /* shutters roll over about three real seconds at opening and closing */
  const SrCity *c = g_game->city;
  int shop = -1;
  for (int b = 0; b < c->building_count && shop < 0; b++) {
    if (c->buildings[b].poi == SR_POI_NONE) continue;
    int open, hours;
    sr_poi_hours(c, b, &open, &hours);
    if (hours < 24) shop = b;
  }
  SR_CHECK(shop >= 0);
  int open, hours;
  sr_poi_hours(c, shop, &open, &hours);
  int64_t t0 = 3 * 86400 + (int64_t)open * 3600;
  int rate = 20;
  SR_CHECK_NEAR(sr_shutter_open(c, shop, t0 - 60, rate), 0, 0);
  float mid = sr_shutter_open(c, shop, t0 + 30, rate);
  SR_CHECK(mid > 0.3f && mid < 0.7f);
  SR_CHECK_NEAR(sr_shutter_open(c, shop, t0 + 3 * rate, rate), 1, 1e-6);
  int64_t close = t0 + (int64_t)hours * 3600;
  float closing = sr_shutter_open(c, shop, close - 30, rate);
  SR_CHECK(closing > 0.3f && closing < 0.7f);
  SR_CHECK_NEAR(sr_shutter_open(c, shop, close + 10, rate), 0, 0);
  int32_t sx, sy;
  sr_building_center(&c->buildings[shop], &sx, &sy);
  SrDeltas *dl = (SrDeltas *)&g_game->deltas;
  sr_deltas_set(dl, c->buildings[shop].id, 1);
  sr_mesh_clear(&m);
  sr_build_poi_fronts(c, dl, sr_v3((float)sx / 100.0f, 1.7f, (float)sy / 100.0f), t0 + 30, rate, &m);
  sr_mesh_clear(&m);
  sr_build_poi_fronts(c, dl, sr_v3(-9000, 0, -9000), t0, rate, &m);
  SR_CHECK_EQ(m.nv, 0);
  /* trains brake into the station, stand with their doors open, and pull away */
  int edge = -1;
  for (int e = 0; e < c->metro_edge_count && edge < 0; e++) edge = e;
  if (edge >= 0) {
    int states[3] = {0};
    SrVec3 prev = sr_v3(0, 0, 0);
    int dwell_run = 0, best_run = 0;
    float slow = 1e9f, fast = 0;
    for (int64_t t = 86400; t < 86400 + 900; t++) {
      SrVec3 pos, dir;
      int st = sr_metro_train_state(c, edge, t, &pos, &dir);
      states[st]++;
      dwell_run = st == 2 ? dwell_run + 1 : 0;
      best_run = SR_MAX(best_run, dwell_run);
      if (st == 1 && t > 86400) {
        float v = sr_v3_len(sr_v3_sub(pos, prev));
        if (v < 50.0f) {
          slow = fminf(slow, v);
          fast = fmaxf(fast, v);
        }
      }
      prev = pos;
    }
    SR_CHECK(states[2] > 0 && states[1] > 0);
    SR_CHECK(best_run >= SR_TRAIN_DWELL_S - 1);
    SR_CHECK(slow < fast * 0.3f); /* it slows into the station */
    SrMesh tm;
    sr_mesh_init(&tm);
    for (int64_t t = 86400; t < 86400 + 900; t++) {
      SrVec3 pos, dir;
      if (sr_metro_train_state(c, edge, t, &pos, &dir) != 2) continue;
      SR_CHECK(sr_build_trains(c, t, pos, &tm) >= 1);
      break;
    }
    sr_mesh_free(&tm);
  }
  /* ICE: barriers ripple, black ICE throbs, watchers eye the avatar */
  SrGame *g = sr_game_create();
  sr_game_new(g, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Ice");
  g->dive.ice_count = 3;
  for (int k = 0; k < 3; k++) {
    g->dive.ice[k].active = true;
    g->dive.ice[k].e.pos = sr_v3((float)k * 5.0f, 1, 0);
  }
  g->dive.ice[0].type = SR_ICE_BARRIER;
  g->dive.ice[1].type = SR_ICE_BLACK;
  g->dive.ice[2].type = SR_ICE_WATCHER;
  g->dive.pos = sr_v3(10, 1, 6);
  SrMesh a, b;
  sr_mesh_init(&a);
  sr_mesh_init(&b);
  sr_build_host(g, 1.0f, &a);
  sr_build_host(g, 1.3f, &b);
  SR_CHECK(sr_mesh_digest(&a) != sr_mesh_digest(&b));
  sr_mesh_free(&a);
  sr_mesh_free(&b);
  sr_game_destroy(g);
  sr_mesh_free(&m);
}

/* ---- budgets and saves ------------------------------------------------------------------- */

static void test_budget_and_saves(void) {
  SR_SPEC("retro-rendering", "Animation within budget");
  SR_SPEC("retro-rendering", "Saves unchanged by animation");
  /* the most a near character can carry: coat, backpack, hood or hair, optics, marker, umbrella */
  int worst = 0;
  for (int k = 0; k < 256; k++) {
    SrAppearance a = sr_appearance_generate((uint64_t)k, (SrRole)(k % SR_ROLE_COUNT), k % 4);
    a.cyber = (uint8_t)(k % 6);
    SrAnimInput in = input_for(&a, SR_CLIP_IDLE);
    in.prop_r = (uint8_t)(k % 2 ? SR_HP_UMBRELLA : SR_HP_RIFLE);
    in.prop_l = SR_HP_BAG;
    in.hood = k % 3 == 0;
    SrMesh m;
    sr_mesh_init(&m);
    sr_anim_character(NULL, &in, &m);
    worst = SR_MAX(worst, m.ni / 3);
    sr_mesh_free(&m);
  }
  printf("  triangles per near character: at most %d\n", worst);
  SR_CHECK(worst <= 380);
  /* a peak crowd at every detail level: near skeletons, beams and lights within the budget */
  SrGame *g = sr_game_create();
  sr_game_new(g, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Budget");
  sr_game_leave(g);
  g->crowd_enabled = true;
  g->force_weather = SR_WX_DOWNPOUR;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 22 * 3600);
  g->last_minute = sr_game_time(g) / 60;
  SrActionState act;
  memset(&act, 0, sizeof act);
  for (int i = 0; i < 120; i++) sr_game_step(g, &act);
  SrSceneCache *s = (SrSceneCache *)calloc(1, sizeof *s);
  sr_scene_init(s, false);
  SrFrame *f = (SrFrame *)malloc(sizeof *f);
  static const int near_max[3] = {24, 36, 48}, beams_max[3] = {8, 12, 16};
  for (int level = 0; level <= 2; level++) {
    g->settings.street_detail = level;
    double t0 = (double)clock();
    for (int i = 0; i < 4; i++) sr_scene_build(s, g, 16.0f / 9.0f, f);
    double ms = ((double)clock() - t0) / CLOCKS_PER_SEC * 1000.0 / 4.0;
    SR_CHECK(s->near_drawn <= near_max[level]);
    SR_CHECK(s->beams_drawn <= beams_max[level]);
    SR_CHECK(s->anim.live <= SR_ANIM_SLOTS);
    printf("  detail %d: %d near skeletons, %d beams, scene %.2f ms\n", level, s->near_drawn, s->beams_drawn, ms);
  }
  /* the view model stays under 400 triangles */
  g->player.inv.count = 0;
  sr_inv_add(&g->player.inv, sr_item_weapon(arch_of(SR_WC_HEAVY), 0, 1, 3));
  g->player.equipped = 0;
  sr_scene_build(s, g, 16.0f / 9.0f, f);
  SR_CHECK(s->view_tris > 0 && s->view_tris <= 400);
  /* pose evaluation: 48 characters well inside a frame (generous: debug build, shared CI) */
  SrAppearance la = look_of(1, SR_ROLE_WORKER);
  SrSkel sk;
  sr_anim_skel(&la, &sk);
  double t0 = (double)clock();
  for (int i = 0; i < 48 * 20; i++) {
    SrAnimInput in = input_for(&la, SR_CLIP_IDLE);
    in.speed = 1.4f;
    in.upper = SR_CLIP_UMBRELLA;
    SrPose p;
    sr_anim_pose(&in, &sk, (double)i * 0.01, (float)i * 0.03f, &p);
  }
  double per_frame = ((double)clock() - t0) / CLOCKS_PER_SEC * 1000.0 / 20.0;
  printf("  pose evaluation: %.3f ms for 48 characters\n", per_frame);
  SR_CHECK(per_frame < 8.0);
  /* saves: the format is unchanged and nothing of the animation is stored */
  SR_CHECK_EQ(SR_SAVE_VERSION, 6u); /* v6 added the saga, nothing of the animation */
  g->fx.shot_t = 99.0;
  g->fx.bob = 3.0f;
  g->fx.eye_y = 1.2f;
  uint64_t before = sr_game_checksum(g);
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, SR_SAVE_VERSION, 1), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_save_read(l, w.data, w.len), SR_OK);
  SR_CHECK(sr_game_checksum(l) == before);
  SR_CHECK(l->fx.shot_t == 0 && l->fx.bob == 0);
  SR_CHECK_EQ(sr_facade_digest(l->city), sr_facade_digest(g->city)); /* the city is the same */
  sr_writer_free(&w);
  sr_game_destroy(l);
  sr_scene_free(s);
  free(s);
  free(f);
  sr_game_destroy(g);
}

/* ---- faces ------------------------------------------------------------------------------ */

static int face_px(const uint8_t *px, int style, int expr, int x, int y) {
  int cell = style * SR_EXPR_COUNT + expr, c = cell % 16;
  int ox = (c % 4) * 32, oy = (c / 4) * 32;
  return px[((oy + y) * SR_TEX_SIZE + ox + x) * 4];
}

static void test_faces(void) {
  SR_SPEC("retro-rendering", "Faces show feeling");
  SR_SPEC("retro-rendering", "Faces stay serious");
  SR_SPEC("retro-rendering", "People blink");
  SrAppearance a = look_of(1, SR_ROLE_WORKER);
  /* the expression follows the clip, the hit and the role */
  SrAnimInput in = input_for(&a, SR_CLIP_IDLE);
  in.key = 4242;
  struct {
    int base, upper, once;
    double t0;
    int want;
  } cases[] = {
      {SR_CLIP_IDLE, SR_CLIP_FLEE_ARMS, 0, -1, SR_EXPR_FEAR},
      {SR_CLIP_COWER, 0, 0, -1, SR_EXPR_FEAR},
      {SR_CLIP_SURRENDER, 0, 0, -1, SR_EXPR_FEAR},
      {SR_CLIP_IDLE, 0, SR_CLIP_FLINCH, 9.9, SR_EXPR_PAIN},
      {SR_CLIP_STUNNED, 0, 0, -1, SR_EXPR_PAIN},
      {SR_CLIP_DIE_BACK, 0, SR_CLIP_DIE_BACK, 9.0, SR_EXPR_CLOSED},
      {SR_CLIP_SLEEP, 0, 0, -1, SR_EXPR_CLOSED},
      {SR_CLIP_POINT, 0, 0, -1, SR_EXPR_SURPRISE},
      {SR_CLIP_IDLE, 0, SR_CLIP_RECOIL, 9.95, SR_EXPR_SURPRISE},
      {SR_CLIP_IDLE, SR_CLIP_AIM, 0, -1, SR_EXPR_STERN},
      {SR_CLIP_PEEK, 0, 0, -1, SR_EXPR_STERN},
      {SR_CLIP_DANCE, 0, 0, -1, SR_EXPR_CONTENT},
  };
  for (int i = 0; i < (int)SR_ARRAY_LEN(cases); i++) {
    in.base = (uint8_t)cases[i].base;
    in.upper = (uint8_t)cases[i].upper;
    in.once = (uint8_t)cases[i].once;
    in.once_t0 = cases[i].t0;
    /* at a moment when this person is not blinking */
    double t = 10.0;
    for (int k = 0; k < 40 && sr_anim_expression(&in, t) == SR_EXPR_CLOSED && cases[i].want != SR_EXPR_CLOSED; k++)
      t += 0.05;
    SR_CHECK_EQ(sr_anim_expression(&in, t), cases[i].want);
  }
  /* talking: the mouth opens and closes */
  in = input_for(&a, SR_CLIP_TALK);
  int open = 0, shut = 0;
  for (int k = 0; k < 60; k++) {
    SrExpression e = sr_anim_expression(&in, 5.0 + k / 30.0);
    open += e == SR_EXPR_TALK;
    shut += e == SR_EXPR_NEUTRAL;
  }
  SR_CHECK(open > 10 && shut > 10);
  /* the mesh carries the face cell of the style and expression on the head's front */
  in = input_for(&a, SR_CLIP_IDLE);
  in.expr = SR_EXPR_STERN;
  SrSkel sk;
  sr_anim_skel(&a, &sk);
  SrPose p;
  sr_anim_pose(&in, &sk, 1.0, 0, &p);
  SrMesh m;
  sr_mesh_init(&m);
  sr_anim_mesh(&m, &in, &sk, &p, 0);
  SrAtlasCell fc = sr_atlas_cell(SR_LAYER_FACES, sr_anim_face_cell(&a, SR_EXPR_STERN));
  int face_verts = 0;
  for (int v = 0; v < m.nv; v++)
    face_verts += m.v[v].layer == fc.layer && (fabsf(m.v[v].u - fc.u0) < 1e-5f || fabsf(m.v[v].u - fc.u1) < 1e-5f);
  SR_CHECK_EQ(face_verts, 4);
  /* women take the feminine cell of the same style and expression */
  SR_SPEC("retro-rendering", "Women have feminine faces");
  SrAppearance wf = a, wm = a;
  wf.body = (uint8_t)(wf.body | 8u);
  wm.body = (uint8_t)(wm.body & ~8u);
  SR_CHECK_EQ(sr_anim_face_cell(&wf, SR_EXPR_TALK), sr_anim_face_cell(&wm, SR_EXPR_TALK) + 4 * SR_EXPR_COUNT);
  SR_CHECK(sr_anim_face_cell(&wm, SR_EXPR_TALK) < 4 * SR_EXPR_COUNT);
  SR_CHECK(sr_atlas_cell(SR_LAYER_FACES, sr_anim_face_cell(&wf, SR_EXPR_CONTENT)).layer < SR_LAYER_RESERVED);
  sr_mesh_free(&m);
  /* serious faces: civilians rest neutral, guards, police and gangers stern */
  for (int role = 0; role < SR_ROLE_COUNT; role++) {
    SrAppearance r = look_of(0, role);
    SrAnimInput ri = input_for(&r, SR_CLIP_IDLE);
    ri.role = (uint8_t)role;
    double t = 20.0;
    while (sr_anim_expression(&ri, t) == SR_EXPR_CLOSED) t += 0.05;
    bool hard = role == SR_ROLE_GUARD || role == SR_ROLE_COP || role == SR_ROLE_GANG;
    SR_CHECK_EQ(sr_anim_expression(&ri, t), hard ? SR_EXPR_STERN : SR_EXPR_NEUTRAL);
  }
  /* the atlas: level brows and a straight mouth at rest; no grin anywhere; small eyes */
  uint8_t *px = (uint8_t *)malloc((size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  for (int layer = 0; layer < 4; layer++) {
    sr_texgen_layer(SR_LAYER_FACES + layer, px);
    for (int c = 0; c < 16; c++) {
      int cell = layer * 16 + c, style = cell / SR_EXPR_COUNT, e = cell % SR_EXPR_COUNT;
      bool fem = style >= 4;
      if (fem && e == SR_EXPR_NEUTRAL) { /* thin arched brows, lined eyes, tinted lips, a narrow chin */
        SR_CHECK(face_px(px, style, e, 9, 7) < 100 && face_px(px, style, e, 7, 9) < 140);
        SR_CHECK(face_px(px, style, e, 9, 8) > 200 && face_px(px, style, e, 9, 10) > 200); /* one texel thin */
        SR_CHECK(face_px(px, style, e, 7, 13) < 90);                                       /* the flick */
        int off = (((c / 4) * 32 + 24) * SR_TEX_SIZE + (c % 4) * 32 + 15) * 4;
        SR_CHECK(px[off] > px[off + 1] + 10); /* the lips are tinted, not grey */
        SR_CHECK(face_px(px, style, e, 3, 29) < 220 && face_px(px, style, e, 16, 29) >= 214);
      }
      if (!fem && e == SR_EXPR_NEUTRAL) {
        SR_CHECK_EQ(face_px(px, style, e, 7, 10), face_px(px, style, e, 12, 10)); /* level brow */
        SR_CHECK(face_px(px, style, e, 7, 10) < 90);
        for (int x = 12; x <= 19; x++) SR_CHECK(face_px(px, style, e, x, 24) < 110); /* a closed, level mouth */
      }
      /* mouth corners never rise more than one texel above the line, and only in the content face */
      int lifted = face_px(px, style, e, 11, 23) < 150 || face_px(px, style, e, 20, 23) < 150;
      if (fem) { /* the masculine rest checks below apply to both sets except the brows */
        if (e == SR_EXPR_NEUTRAL)
          for (int x = 12; x <= 19; x++) SR_CHECK(face_px(px, style, e, x, 24) < 110);
      }
      SR_CHECK(!lifted);
      int lift12 = face_px(px, style, e, 12, 23) < 150;
      SR_CHECK(!lift12 || e == SR_EXPR_CONTENT);
      /* eyes: dark texels in an eye's box span at most three rows */
      int rows = 0;
      for (int y = 11; y <= 15; y++) { /* below the brow, above a tattoo */
        if (fem) break;                /* lined eyes are checked above */
        bool dark = false;
        for (int x = 9; x <= 10; x++) dark |= face_px(px, style, e, x, y) < 130;
        rows += dark;
      }
      SR_CHECK(rows <= 3);
    }
  }
  free(px);
  /* blinking: about every three to six seconds, out of step between people */
  in = input_for(&a, SR_CLIP_IDLE);
  int closed = 0, blinks = 0, together = 0;
  bool was = false;
  SrAnimInput other = in;
  other.key = in.key + 1;
  for (int k = 0; k < 60 * 60; k++) {
    double t = 100.0 + k / 60.0;
    bool c = sr_anim_expression(&in, t) == SR_EXPR_CLOSED;
    closed += c;
    blinks += c && !was;
    was = c;
    together += c && sr_anim_expression(&other, t) == SR_EXPR_CLOSED;
  }
  SR_CHECK(blinks >= 9 && blinks <= 21);
  SR_CHECK(closed > 0 && closed < 60 * 60 / 15);
  SR_CHECK(together < closed);
  SR_CHECK(sr_anim_face_style(&a) >= 0 && sr_anim_face_style(&a) < 4);
}

/* ---- bodies ----------------------------------------------------------------------------- */

static void test_bodies(void) {
  SR_SPEC("population-simulation", "Bodies of every kind");
  SR_SPEC("population-simulation", "Builds fit the role");
  SR_SPEC("population-simulation", "Bodies match names");
  SR_SPEC("retro-rendering", "Chrome limbs show");
  SR_SPEC("game-persistence", "Older looks stay the same");
  SR_SPEC("player-life", "Choose the body");
  /* measures: muscle, fat, thinness, frame and height all change the skeleton */
  SrSkel sk[2][SR_BUILD_COUNT];
  for (int f = 0; f < 2; f++)
    for (int b = 0; b < SR_BUILD_COUNT; b++) {
      SrAppearance a = look_of(0, SR_ROLE_WORKER);
      sr_look_set_body(&a, b, f == 1, SR_HEIGHT_AVERAGE);
      sr_anim_skel(&a, &sk[f][b]);
      SR_CHECK(sk[f][b].female == (f == 1));
    }
  for (int f = 0; f < 2; f++) {
    SR_CHECK(sk[f][SR_BUILD_MUSCULAR].arm_w > sk[f][SR_BUILD_THIN].arm_w * 1.7f);
    SR_CHECK(sk[f][SR_BUILD_HEAVY].waist > sk[f][SR_BUILD_SLENDER].waist * 1.4f);
    SR_CHECK(sk[f][SR_BUILD_HEAVY].belly > 0 && sk[f][SR_BUILD_ATHLETIC].belly == 0);
    SR_CHECK(sk[f][SR_BUILD_CURVY].hip_w > sk[f][SR_BUILD_AVERAGE].hip_w * 1.15f);
    SR_CHECK(sk[f][SR_BUILD_MUSCULAR].bw > sk[f][SR_BUILD_AVERAGE].bw * 1.2f);
    SR_CHECK(sk[f][SR_BUILD_THIN].bw < sk[f][SR_BUILD_AVERAGE].bw * 0.9f);
  }
  for (int b = 0; b < SR_BUILD_COUNT; b++) { /* women: narrower shoulders, wider hips, a bust */
    SR_CHECK(sk[1][b].bw < sk[0][b].bw && sk[1][b].hip_w > sk[0][b].hip_w);
    SR_CHECK(sk[1][b].bust > 0 && sk[0][b].bust == 0);
    SR_CHECK(sk[1][b].tall < sk[0][b].tall);
    SR_CHECK(sk[1][b].hip_x >= sk[1][b].leg_w); /* the legs stay apart */
  }
  float h[4];
  for (int k = 0; k < 4; k++) {
    SrAppearance a = look_of(0, SR_ROLE_WORKER);
    a.head = 2;
    sr_look_set_body(&a, SR_BUILD_AVERAGE, false, k);
    SrSkel s;
    sr_anim_skel(&a, &s);
    h[k] = s.tall;
  }
  SR_CHECK(h[SR_HEIGHT_SHORT] < h[SR_HEIGHT_AVERAGE] && h[SR_HEIGHT_AVERAGE] < h[SR_HEIGHT_TALL] &&
           h[SR_HEIGHT_TALL] < h[SR_HEIGHT_VERY_TALL]);
  SR_CHECK(h[SR_HEIGHT_VERY_TALL] - h[SR_HEIGHT_SHORT] > 0.2f);
  /* every build and frame walks with planted feet, in its own way */
  for (int f = 0; f < 2; f++)
    for (int b = 0; b < SR_BUILD_COUNT; b++) {
      const SrSkel *s = &sk[f][b];
      float stride = sr_anim_stride(s, 1.4f, SR_CLIP_WALK), slip = 0, moved = 0;
      SrPose prev;
      sr_anim_gait(s, 1.4f, 0, SR_CLIP_WALK, 3, &prev);
      for (int i = 1; i <= 100; i++) {
        float d0 = (float)(i - 1) * 0.01f * stride, d1 = (float)i * 0.01f * stride;
        SrPose cur;
        sr_anim_gait(s, 1.4f, d1 / stride, SR_CLIP_WALK, 3, &cur);
        for (int side = 0; side < 2; side++) {
          SrVec3 f0 = sr_anim_point(s, &prev, side), f1 = sr_anim_point(s, &cur, side);
          if (f0.y > s->ankle + 0.002f || f1.y > s->ankle + 0.002f) continue;
          slip += fabsf((d1 + f1.z) - (d0 + f0.z));
          moved += d1 - d0;
        }
        prev = cur;
      }
      SR_CHECK(slip <= 0.1f * moved);
    }
  SrPose sway_f, sway_m;
  sr_anim_gait(&sk[1][SR_BUILD_CURVY], 1.3f, 0.25f, SR_CLIP_WALK, 3, &sway_f);
  sr_anim_gait(&sk[0][SR_BUILD_MUSCULAR], 1.3f, 0.25f, SR_CLIP_WALK, 3, &sway_m);
  SR_CHECK(fabsf(sway_f.j[SR_J_PELVIS_X]) > fabsf(sway_m.j[SR_J_PELVIS_X]) * 2.0f); /* hips that sway */
  SR_CHECK(sway_m.j[SR_J_SHOA_L] > sway_f.j[SR_J_SHOA_L] + 0.1f);                   /* arms held out */
  /* builds by role, frames half and half, every build somewhere */
  int seen[SR_BUILD_COUNT] = {0}, women = 0, strong_cops = 0, thin_drifters = 0, n = 2000;
  int sleeves[3] = {0}, skirts = 0, long_hair = 0;
  for (int k = 0; k < n; k++) {
    SrAppearance a = sr_appearance_generate((uint64_t)k * 7919u, (SrRole)(k % SR_ROLE_COUNT), 0);
    seen[sr_look_build(&a)]++;
    women += sr_look_female(&a);
    SR_CHECK(sr_look_build(&a) < SR_BUILD_COUNT && sr_look_height(&a) < 4 && sr_look_cyber_level(&a) <= 5);
    if (k % SR_ROLE_COUNT == SR_ROLE_COP)
      strong_cops += sr_look_build(&a) == SR_BUILD_ATHLETIC || sr_look_build(&a) == SR_BUILD_MUSCULAR;
    if (k % SR_ROLE_COUNT == SR_ROLE_DRIFTER) thin_drifters += sr_look_build(&a) == SR_BUILD_THIN;
    sleeves[sr_anim_sleeves(&a)]++;
    skirts += sr_anim_has_skirt(&a);
    long_hair += sr_anim_hair_style(&a) == 9 || sr_anim_hair_style(&a) == 10;
    if (sr_look_cyber_level(&a) >= 4) SR_CHECK((sr_look_limbs(&a) & 3) == 3); /* heavy chrome: both arms */
    if (sr_look_cyber_level(&a) < 2) SR_CHECK(sr_look_limbs(&a) == 0);
  }
  for (int b = 0; b < SR_BUILD_COUNT; b++) SR_CHECK(seen[b] > 0);
  SR_CHECK(women > n * 2 / 5 && women < n * 3 / 5);
  SR_CHECK(strong_cops * 5 > (n / SR_ROLE_COUNT) * 2);    /* at least 40 % */
  SR_CHECK(thin_drifters * 10 > (n / SR_ROLE_COUNT) * 3); /* at least 30 % */
  SR_CHECK(sleeves[0] > 0 && sleeves[1] > 0 && sleeves[2] > 0 && skirts > 0 && long_hair > 0);
  printf("  bodies: %d women of %d; sleeves %d/%d/%d, skirts %d, long hair %d\n", women, n, sleeves[0], sleeves[1],
         sleeves[2], skirts, long_hair);
  /* notables' bodies match their given names */
  const SrPopulation *pop = g_game->pop;
  int checked = 0;
  for (int i = 0; i < pop->count; i++) {
    int fr = sr_name_frame(pop->npcs[i].name);
    if (fr < 0) continue;
    SR_CHECK_EQ((int)sr_look_female(&pop->npcs[i].look), fr);
    checked++;
  }
  SR_CHECK(checked > 100);
  SR_CHECK_EQ(sr_name_frame("Quinn Halloway"), -1);
  SR_CHECK_EQ(sr_name_frame("Greta Voss"), 1);
  SR_CHECK_EQ(sr_name_frame("Dario Kesh"), 0);
  SR_CHECK_EQ(sr_name_frame(NULL), -1);
  /* chrome limbs: metal parts and glowing seams where the body has them */
  SrAppearance c = look_of(1, SR_ROLE_GANG);
  sr_look_set_cyber(&c, 3, 0, 0);
  SrAnimInput in = input_for(&c, SR_CLIP_IDLE);
  SrSkel s;
  sr_anim_skel(&c, &s);
  SrPose p;
  sr_anim_pose(&in, &s, 1.0, 0, &p);
  int metal_none = 0, metal_arm = 0, metal_all = 0, seams = 0;
  SrMesh m;
  sr_mesh_init(&m);
  sr_anim_mesh(&m, &in, &s, &p, 0);
  for (int v = 0; v < m.nv; v++) metal_none += m.v[v].layer == SR_LAYER_METAL;
  sr_look_set_cyber(&c, 3, SR_LIMB_ARM_R, 1);
  sr_mesh_clear(&m);
  sr_anim_mesh(&m, &in, &s, &p, 0);
  for (int v = 0; v < m.nv; v++) metal_arm += m.v[v].layer == SR_LAYER_METAL;
  sr_look_set_cyber(&c, 5, 7, 2);
  sr_mesh_clear(&m);
  sr_anim_mesh(&m, &in, &s, &p, 0);
  for (int v = 0; v < m.nv; v++) {
    metal_all += m.v[v].layer == SR_LAYER_METAL;
    seams += m.v[v].emissive >= 200 && m.v[v].layer == SR_LAYER_WHITE;
  }
  SR_CHECK_EQ(metal_none, 0);
  SR_CHECK(metal_arm >= 3 * 24 && metal_all > metal_arm * 2);
  SR_CHECK(seams > 0);
  SrColor hand, fore;
  int lay;
  sr_anim_arm_look(&c, 0, &hand, &fore, &lay);
  SR_CHECK_EQ(lay, SR_LAYER_METAL);
  /* older saves: body 0..3 and cyber 0..5 read as before, with the same keys */
  for (int body = 0; body < 4; body++)
    for (int cy = 0; cy <= 5; cy++) {
      SrAppearance o = look_of(0, SR_ROLE_WORKER);
      o.body = (uint8_t)body;
      o.cyber = (uint8_t)cy;
      SR_CHECK_EQ(sr_look_build(&o), body);
      SR_CHECK(!sr_look_female(&o) && sr_look_height(&o) == SR_HEIGHT_AVERAGE);
      SR_CHECK_EQ(sr_look_cyber_level(&o), cy);
      SR_CHECK_EQ(sr_look_limbs(&o), cy >= 4 ? 3 : 0);
      uint64_t k = o.body; /* the key as it was computed before */
      k = (k << 3) | o.head;
      k = (k << 3) | o.skin;
      k = (k << 4) | o.face;
      k = (k << 4) | o.hair;
      k = (k << 4) | o.hair_color;
      k = (k << 6) | o.outfit;
      k = (k << 4) | o.outfit_color;
      k = (k << 6) | o.marker;
      k = (k << 3) | o.cyber;
      k = (k << 3) | o.accessory;
      SR_CHECK_EQ(sr_appearance_key(&o), k);
    }
  sr_mesh_free(&m);
  /* character creation: build, frame, height and chrome can be chosen */
  SrGame *g = sr_game_create();
  sr_game_begin_create(g, 77);
  SrAppearance before = sr_game_create_preview(g);
  for (int w = 0; w < SR_CREATE_BODY_COUNT; w++) {
    const char *l0 = sr_game_create_label(g, (SrCreateBody)w);
    sr_game_create_cycle(g, (SrCreateBody)w);
    SR_CHECK(strcmp(l0, sr_game_create_label(g, (SrCreateBody)w)) != 0);
  }
  SrAppearance after = sr_game_create_preview(g);
  SR_CHECK(sr_look_build(&after) == (sr_look_build(&before) + 1) % SR_BUILD_COUNT);
  SR_CHECK(sr_look_female(&after) != sr_look_female(&before));
  for (int k = 0; k < 8; k++) sr_game_create_cycle(g, SR_CREATE_CHROME);
  SR_CHECK(sr_game_create_label(g, SR_CREATE_CHROME)[0] != '\0');
  sr_game_create_cycle(g, SR_CREATE_BODY_COUNT); /* ignored */
  SR_CHECK(sr_game_create_label(g, SR_CREATE_BODY_COUNT)[0] == '\0');
  SR_CHECK_EQ(sr_game_create_confirm(g), SR_OK);
  SR_CHECK_EQ(sr_look_build(&g->player.look), sr_look_build(&after));
  sr_game_create_randomize(g);
  sr_game_destroy(g);
}

void suite_anim(void) {
  g_game = sr_game_create();
  sr_game_new(g_game, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Anim");
  SR_RUN(test_clips);
  SR_RUN(test_gait);
  SR_RUN(test_turns_and_blends);
  SR_RUN(test_street_life);
  SR_RUN(test_groups);
  SR_RUN(test_rain_and_props);
  SR_RUN(test_reactions);
  SR_RUN(test_vehicles);
  SR_RUN(test_aerodynes);
  SR_RUN(test_guards);
  SR_RUN(test_view_model);
  SR_RUN(test_camera_motion);
  SR_RUN(test_world_motion);
  SR_RUN(test_budget_and_saves);
  SR_RUN(test_faces);
  SR_RUN(test_bodies);
  sr_game_destroy(g_game);
}
