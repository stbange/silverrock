/* improve-city-atmosphere: texture layers, sprites, glows, window lighting, the gloom, facades,
 * street dressing, signs, sky, district kits and motion. */
#include "core/sr_rng.h"
#include "render/sr_render.h"
#include "sr_test.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void test_layer_map(void) {
  SR_SPEC("retro-rendering", "Every layer index is in range");
  SR_CHECK(SR_LAYER_RESERVED <= SR_TEX_LAYERS);
  SR_CHECK(SR_LAYER_WATER < SR_LAYER_WINMOD_FIRST);
  for (int poi = 0; poi < SR_POI_COUNT; poi++) {
    SrAtlasCell c = sr_icon_cell(poi);
    SR_CHECK(c.layer >= SR_LAYER_ICONS && c.layer < SR_LAYER_SPRITES);
    SR_CHECK(c.u0 >= 0 && c.u1 <= 1 && c.v0 >= 0 && c.v1 <= 1 && c.u0 < c.u1 && c.v0 < c.v1);
  }
  SR_CHECK(sr_icon_cell(SR_POI_HOME).layer < SR_TEX_LAYERS); /* the old layout read past the array */
  SR_CHECK(sr_icon_cell(-3).layer == SR_LAYER_ICONS);
  SR_CHECK(sr_atlas_cell(SR_LAYER_SPRITES, SR_SPR_COUNT - 1).layer < SR_LAYER_DECALS);
  SR_CHECK(sr_atlas_cell(SR_LAYER_DECALS, SR_DECAL_COUNT - 1).layer < SR_LAYER_ADS);
  SR_CHECK(sr_ad_cell(SR_LAYER_ADS, SR_AD_PLAIN + 1).layer < SR_LAYER_SCRIPT);
  for (int gl = 0; gl < 70; gl++) SR_CHECK(sr_script_cell(gl).layer == SR_LAYER_SCRIPT);
  /* every generated layer has content: no reserved magenta below the reserved range */
  uint8_t *px = (uint8_t *)malloc((size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  for (int l = 0; l < SR_TEX_LAYERS; l++) {
    sr_texgen_layer(l, px);
    int magenta = 0, opaque = 0;
    for (int i = 0; i < SR_TEX_SIZE * SR_TEX_SIZE; i++) {
      magenta += px[i * 4] == 255 && px[i * 4 + 1] == 0 && px[i * 4 + 2] == 255;
      opaque += px[i * 4 + 3] > 0;
    }
    if (l < SR_LAYER_RESERVED) SR_CHECK(magenta < SR_TEX_SIZE * SR_TEX_SIZE / 2);
    bool spare_atlas = (l >= SR_LAYER_SPRITES && l < SR_LAYER_DECALS && (l - SR_LAYER_SPRITES) * 16 >= SR_SPR_COUNT) ||
                       (l >= SR_LAYER_DECALS && l < SR_LAYER_ADS && (l - SR_LAYER_DECALS) * 16 >= SR_DECAL_COUNT);
    if (l != SR_LAYER_FONT && !spare_atlas) SR_CHECK(opaque > 0);
  }
  /* window modules mark glass for the shader (alpha 200) */
  for (int k = 0; k < 8; k++) {
    sr_texgen_layer(SR_LAYER_WINMOD_FIRST + k, px);
    int glassy = 0;
    for (int i = 0; i < SR_TEX_SIZE * SR_TEX_SIZE; i++) glassy += px[i * 4 + 3] == 200;
    SR_CHECK(glassy > 100);
  }
  free(px);
}

static SrVec3 quad_normal(const SrMesh *m, int q) {
  SrVec3 a = sr_v3(m->v[q * 4].x, m->v[q * 4].y, m->v[q * 4].z);
  SrVec3 b = sr_v3(m->v[q * 4 + 1].x, m->v[q * 4 + 1].y, m->v[q * 4 + 1].z);
  SrVec3 c = sr_v3(m->v[q * 4 + 2].x, m->v[q * 4 + 2].y, m->v[q * 4 + 2].z);
  return sr_v3_norm(sr_v3_cross(sr_v3_sub(b, a), sr_v3_sub(c, a)));
}

static void test_sprites(void) {
  SR_SPEC("retro-rendering", "Sprites face the camera");
  SrMesh m;
  sr_mesh_init(&m);
  SrColor white = {255, 255, 255, 255};
  SrVec3 at = sr_v3(10, 0, 10);
  /* two cameras 90 degrees apart around the sprite */
  SrVec3 eyes[2] = {sr_v3(10, 1.7f, 20), sr_v3(20, 1.7f, 10)};
  for (int k = 0; k < 2; k++) {
    sr_mesh_clear(&m);
    SrBillboard bb = sr_billboard(eyes[k], k == 0 ? 0.0f : SR_PI / 2, 0);
    sr_mesh_sprite(&m, &bb, at, 1.0f, 2.0f, SR_SPR_STEAM, SR_SPRITE_UPRIGHT, 0, white, 255);
    SR_CHECK_EQ(m.nv, 4);
    SrVec3 n = quad_normal(&m, 0), to = sr_v3_norm(sr_v3_sub(eyes[k], at));
    to.y = 0;
    to = sr_v3_norm(to);
    SR_CHECK(fabsf(n.y) < 1e-4f);              /* stays vertical */
    SR_CHECK(n.x * to.x + n.z * to.z > 0.99f); /* faces the camera */
    SR_CHECK_NEAR(m.v[0].y, 0.0f, 1e-5);       /* bottom at pos */
    SR_CHECK_NEAR(m.v[2].y, 2.0f, 1e-5);
    SR_CHECK(m.v[0].layer == SR_LAYER_SPRITES);
  }
  /* facing sprites turn with the camera's pitch; decals lie flat */
  SrBillboard up = sr_billboard(sr_v3(0, 0, 0), 0.3f, 0.5f);
  sr_mesh_clear(&m);
  sr_mesh_sprite(&m, &up, sr_v3(0, 2, -5), 1, 1, SR_SPR_GLOW, SR_SPRITE_FACING, 0, white, 255);
  SrVec3 n = quad_normal(&m, 0), f = sr_dir_from_yaw_pitch(0.3f, 0.5f);
  SR_CHECK(n.x * f.x + n.y * f.y + n.z * f.z < -0.99f); /* normal points back at the eye */
  sr_mesh_clear(&m);
  sr_mesh_sprite(&m, &up, sr_v3(1, 0.02f, 1), 2, 3, SR_SPR_POOL, SR_SPRITE_DECAL, 0.7f, white, 255);
  for (int i = 0; i < 4; i++) SR_CHECK_NEAR(m.v[i].y, 0.02f, 1e-6);
  SR_CHECK(fabsf(quad_normal(&m, 0).y) > 0.99f);
  /* an eye straight above an upright sprite falls back to the camera's right vector */
  SrBillboard top = sr_billboard(sr_v3(10, 30, 10), 0, -1.5f);
  sr_mesh_clear(&m);
  sr_mesh_sprite(&m, &top, at, 1, 1, SR_SPR_GLOW, SR_SPRITE_UPRIGHT, 0, white, 255);
  SR_CHECK_EQ(m.nv, 4);
  sr_mesh_free(&m);
}

static void test_animation_and_glow(void) {
  SR_SPEC("retro-rendering", "Glows fade with fog");
  for (int i = 0; i < 50; i++) {
    double t = i * 0.37;
    int f = sr_anim_frame(t, 8.0f, 4, 17u);
    SR_CHECK(f >= 0 && f < 4);
    SR_CHECK_EQ(f, sr_anim_frame(t, 8.0f, 4, 17u));
  }
  SR_CHECK_EQ(sr_anim_frame(-3.3, 8.0f, 4, 1u) >= 0, 1);
  SR_CHECK_EQ(sr_anim_frame(1.0, 8.0f, 1, 1u), 0);
  int changes = 0;
  for (int i = 1; i < 32; i++)
    changes += sr_anim_frame(i / 32.0, 8.0f, 4, 3u) != sr_anim_frame((i - 1) / 32.0, 8.0f, 4, 3u);
  SR_CHECK(changes >= 6 && changes <= 10); /* about 8 frames a second */
  SR_CHECK_NEAR(sr_glow_fade(5, 20, 100), 1.0, 1e-6);
  SR_CHECK_NEAR(sr_glow_fade(60, 20, 100), 0.5, 1e-6);
  SR_CHECK_NEAR(sr_glow_fade(100, 20, 100), 0.0, 1e-6);
  SR_CHECK_NEAR(sr_glow_fade(400, 20, 100), 0.0, 1e-6); /* beyond the fog end: no colour at all */
}

static void test_window_share(void) {
  SR_SPEC("retro-rendering", "Offices go dark at night");
  SR_SPEC("retro-rendering", "Homes light up in the evening");
  float office18 = sr_window_share(SR_USE_OFFICE, 18.0f), office02 = sr_window_share(SR_USE_OFFICE, 2.0f);
  SR_CHECK(office02 <= office18 / 4.0f);
  float home21 = sr_window_share(SR_USE_HOME, 21.0f), home03 = sr_window_share(SR_USE_HOME, 3.0f);
  SR_CHECK(home21 >= home03 * 2.0f);
  SR_CHECK_NEAR(sr_window_share(SR_USE_INDUSTRIAL, 4.0f), sr_window_share(SR_USE_INDUSTRIAL, 16.0f), 1e-6);
  SR_CHECK(sr_window_share(SR_USE_VACANT, 21.0f) < 0.05f);
  for (float h = -2.0f; h < 30.0f; h += 0.25f)
    for (int u = 0; u < 4; u++) {
      float s = sr_window_share(u, h);
      SR_CHECK(s >= 0.0f && s <= 0.8f);
    }
}

static SrGame *g_game;

/* Builds the frame twice at the same game time and compares every mesh. */
static void test_same_time_same_frame(void) {
  SR_SPEC("retro-rendering", "Animation follows game time");
  SrGame *g = g_game;
  SrSceneCache *s = (SrSceneCache *)calloc(1, sizeof *s);
  sr_scene_init(s, false);
  sr_game_leave(g);
  g->force_weather = SR_WX_DOWNPOUR;
  for (int i = 0; i < 30; i++) {
    sr_game_step(g, NULL);
    SrFrame *f = (SrFrame *)malloc(sizeof *f);
    sr_scene_build(s, g, 16.0f / 9.0f, f);
    free(f);
  }
  SrFrame *a = (SrFrame *)malloc(sizeof *a), *b = (SrFrame *)malloc(sizeof *b);
  sr_scene_build(s, g, 16.0f / 9.0f, a);
  uint64_t da[SR_MAX_DRAWS];
  for (int i = 0; i < a->ndraws; i++) da[i] = sr_mesh_digest(a->draws[i].mesh);
  sr_scene_build(s, g, 16.0f / 9.0f, b);
  SR_CHECK_EQ(a->ndraws, b->ndraws);
  for (int i = 0; i < a->ndraws && i < b->ndraws; i++) SR_CHECK(da[i] == sr_mesh_digest(b->draws[i].mesh));
  SR_CHECK(a->time_s == b->time_s);
  SR_CHECK(s->rain.nv > 0);
  g->force_weather = -1;
  free(a);
  free(b);
  sr_scene_free(s);
  free(s);
}

void suite_atmos(void) {
  g_game = sr_game_create();
  sr_game_new(g_game, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Atmos");
  SR_RUN(test_layer_map);
  SR_RUN(test_sprites);
  SR_RUN(test_animation_and_glow);
  SR_RUN(test_window_share);
  SR_RUN(test_same_time_same_frame);
  sr_game_destroy(g_game);
}
