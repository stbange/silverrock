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

static void test_halos(void) {
  SR_SPEC("retro-rendering", "Neon halos through fog");
  SrBeacon b[4];
  memset(b, 0, sizeof b);
  SrColor pink = {255, 60, 200, 255};
  float vis = 100.0f, dists[4] = {60.0f, 150.0f, 280.0f, 320.0f};
  for (int i = 0; i < 4; i++) {
    b[i].pos = sr_v3(0, 30, -dists[i]);
    b[i].size = 40;
    b[i].col = pink;
  }
  SrBillboard bb = sr_billboard(sr_v3(0, 1.7f, 0), 0, 0);
  SrMesh m;
  sr_mesh_init(&m);
  int n = sr_build_halos(b, 4, &bb, 90.0f, vis, &m);
  SR_CHECK_EQ(n, 2); /* 150 m and 280 m: between one and three visibility distances */
  SR_CHECK_EQ(m.nv, 8);
  SR_CHECK(m.v[0].r > m.v[4].r); /* the nearer halo is brighter */
  float z0 = m.v[0].z, z1 = m.v[4].z;
  SR_CHECK(fabsf(z0 + 150.0f) < 1.0f && fabsf(z1 + 280.0f) < 1.0f);
  /* at most 48, the strongest first */
  SrBeacon *many = (SrBeacon *)calloc(200, sizeof *many);
  for (int i = 0; i < 200; i++) {
    many[i].pos = sr_v3((float)(i % 20) * 5.0f, 20, -150.0f - (float)(i / 20) * 10.0f);
    many[i].size = i == 199 ? 400.0f : 10.0f;
    many[i].col = pink;
  }
  SR_CHECK_EQ(sr_build_halos(many, 200, &bb, 90.0f, vis, &m), 48);
  bool big = false;
  for (int v = 0; v < m.nv; v += 4) big = big || fabsf(m.v[v + 1].x - m.v[v].x) > 100.0f;
  SR_CHECK(big); /* the huge logo made the cut */
  free(many);
  /* near halos follow wet air */
  SrLight l = {sr_v3(0, 5, -10), 20, 1, 0.5f, 0.2f};
  sr_build_near_halos(&l, 1, &bb, 0.0f, &m);
  SR_CHECK_EQ(m.nv, 0);
  sr_build_near_halos(&l, 1, &bb, 1.0f, &m);
  SR_CHECK_EQ(m.nv, 4);
  sr_mesh_free(&m);
}

static void test_deck_and_sky(void) {
  SR_SPEC("world-environment", "The Pall glows over bright districts");
  SR_SPEC("world-environment", "No Pall at sunrise");
  SR_SPEC("story-campaign", "Sunrise shows daylight");
  SR_SPEC("retro-rendering", "No dark slot at the end of the street");
  SrGame *g = g_game;
  const SrCity *c = g->city;
  SrSceneCache *s = (SrSceneCache *)calloc(1, sizeof *s);
  sr_scene_init(s, false);
  sr_sky_prepare(s, c);
  SR_CHECK(s->nbeacons > c->factions.corp_count);
  /* a bright and a dark district: the deck above the bright one glows more, in its hue */
  int bright = -1, dark = -1;
  for (int b = 0; b < c->grid * c->grid; b++) {
    if (c->blocks[b].water) continue;
    SrDistrictType t = c->districts[c->blocks[b].district].type;
    if (t == SR_D_ENTERTAINMENT && bright < 0) bright = b;
    if (t == SR_D_BARRENS && dark < 0) dark = b;
  }
  SrSky night = sr_sky_at(0, false);
  float fog[3] = {0.1f, 0.05f, 0.1f};
  SrMesh base, glow;
  sr_mesh_init(&base);
  sr_mesh_init(&glow);
  float lum[2] = {0, 0};
  int which[2] = {bright, dark};
  for (int k = 0; k < 2; k++) {
    if (which[k] < 0) continue;
    int32_t ox, oy;
    sr_city_block_origin(which[k], c->grid, &ox, &oy);
    SrVec3 eye = sr_v3((float)ox / 100.0f + 48.0f, 1.7f, (float)oy / 100.0f + 48.0f);
    sr_build_deck(s, c, eye, &night, fog, 0, &base, &glow);
    SR_CHECK(base.nv == 32 * 32 * 4 && glow.nv == base.nv);
    float best = 1e9f;
    for (int v = 0; v < base.nv; v++) { /* the deck vertex right above the block */
      float dx = base.v[v].x - eye.x, dz = base.v[v].z - eye.z;
      if (dx * dx + dz * dz < best) {
        best = dx * dx + dz * dz;
        lum[k] = (float)(base.v[v].r + base.v[v].g + base.v[v].b);
      }
      SR_CHECK(base.v[v].y > 200.0f);
    }
  }
  if (bright >= 0 && dark >= 0) SR_CHECK(lum[0] > lum[1] + 20.0f);
  SR_CHECK(s->deck_light[bright >= 0 ? bright : 0].r + s->deck_light[bright >= 0 ? bright : 0].b > 0);
  /* by day the deck is slate, never white */
  SrSky day = sr_sky_at(12 * 3600 + 1800, false);
  sr_build_deck(s, c, sr_v3(100, 2, 100), &day, fog, 0, &base, &glow);
  for (int v = 0; v < base.nv; v++) SR_CHECK(base.v[v].r < 120 && base.v[v].g < 120 && base.v[v].b < 120);
  /* silhouettes just beyond the fog end, darker than the fog */
  sr_mesh_clear(&base);
  int found = 0;
  for (int b = 0; b < c->grid * c->grid && !found; b++) {
    int32_t ox, oy;
    sr_city_block_origin(b, c->grid, &ox, &oy);
    sr_build_silhouettes(c, sr_v3((float)ox / 100.0f + 8, 1.7f, (float)oy / 100.0f + 8), 120.0f, fog, &base);
    found = base.nv;
  }
  SR_CHECK(found > 0);
  SR_CHECK(base.v[0].r < (uint8_t)(fog[0] * 255.0f));
  sr_mesh_free(&base);
  sr_mesh_free(&glow);
  /* a frame of the gloom: dome, deck, halos; at Sunrise the deck is gone and the sun is out */
  sr_game_leave(g);
  g->force_weather = SR_WX_FOG;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 3600);
  SrFrame *f = (SrFrame *)malloc(sizeof *f);
  for (int i = 0; i < 10; i++) sr_scene_build(s, g, 16.0f / 9.0f, f);
  int sky = 0, deck = 0, halo = 0;
  for (int i = 0; i < f->ndraws; i++) {
    sky += f->draws[i].pass == SR_PASS_SKY;
    deck += f->draws[i].mesh == &s->deck;
    halo += f->draws[i].pass == SR_PASS_HALO;
  }
  SR_CHECK(sky == 1 && deck == 1 && halo >= 1);
  SR_CHECK(f->fog_end <= 60.0f + 1e-3f);
  SR_CHECK(f->halo_count > 0); /* neon shows through the fog at the end of the street */
  SR_CHECK(!f->daylight);
  g->pall_removed = true;
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 12 * 3600);
  sr_scene_build(s, g, 16.0f / 9.0f, f);
  deck = 0;
  int sun = 0;
  for (int i = 0; i < f->ndraws; i++) {
    deck += f->draws[i].mesh == &s->deck;
    sun += f->draws[i].mesh == &s->sun;
  }
  SR_CHECK(deck == 0 && sun == 1 && f->daylight);
  SrWeatherState w = sr_game_weather(g);
  SR_CHECK(w.rain == 0.0f && w.visibility > 200.0f); /* the rain stops and the fog lifts */
  g->pall_removed = false;
  g->force_weather = -1;
  free(f);
  sr_scene_free(s);
  free(s);
}

static void test_wet_streets(void) {
  SR_SPEC("retro-rendering", "Wet streets reflect neon");
  SR_SPEC("retro-rendering", "Rain splashes");
  SR_SPEC("world-environment", "Mist hugs the ground");
  SR_SPEC("world-environment", "Steam rises from the street");
  SrGame *g = g_game;
  const SrCity *c = g->city;
  SrMesh m;
  sr_mesh_init(&m);
  /* a sign 20 m away: reflected on the wet ground below it, not on a dry street */
  SrLight sign = {sr_v3(20, 4.5f, 0), 14, 1.0f, 0.2f, 0.8f};
  SrVec3 eye = sr_v3(0, 1.7f, 0);
  SR_CHECK_EQ(sr_build_reflections(&sign, 1, eye, 0.0f, 64, &m), 0);
  SR_CHECK_EQ(m.nv, 0);
  SR_CHECK_EQ(sr_build_reflections(&sign, 1, eye, 0.8f, 64, &m), 1);
  SR_CHECK_EQ(m.nv, 4);
  for (int i = 0; i < 4; i++) SR_CHECK(m.v[i].y < 0.1f && m.v[i].ground == 1);
  SR_CHECK(fabsf(m.v[0].x - 20.0f) < 2.0f && m.v[2].x < 20.0f); /* from the foot of the sign toward the eye */
  SR_CHECK(m.v[0].r > m.v[0].g);                                /* in the sign's colour */
  SrLight far = {sr_v3(60, 4.5f, 0), 14, 1, 1, 1};
  sr_mesh_clear(&m);
  SR_CHECK_EQ(sr_build_reflections(&far, 1, eye, 0.8f, 64, &m), 0); /* beyond 40 m */
  SrLight many[30];
  for (int i = 0; i < 30; i++) many[i] = (SrLight){sr_v3((float)i, 6, 5), 20, 1, 1, 1};
  sr_mesh_clear(&m);
  SR_CHECK_EQ(sr_build_reflections(many, 30, eye, 0.8f, 16, &m), 16); /* the low-detail limit */
  /* splashes on uncovered ground near the camera when it rains */
  sr_game_leave(g);
  SrVec3 street = g->player.pos;
  sr_mesh_clear(&m);
  sr_build_splashes(c, street, 0.2f, 100, &m);
  SR_CHECK_EQ(m.nv, 0);
  sr_build_splashes(c, street, 1.0f, 100, &m);
  SR_CHECK(m.nv > 40);
  for (int v = 0; v < m.nv; v += 4) {
    float dx = m.v[v].x - street.x, dz = m.v[v].z - street.z;
    SR_CHECK(dx * dx + dz * dz < 21.0f * 21.0f && m.v[v].y < 0.1f);
    SR_CHECK(!sr_point_covered(c, sr_v3((m.v[v].x + m.v[v + 1].x) * 0.5f, 0.1f, (m.v[v].z + m.v[v + 1].z) * 0.5f)));
  }
  /* mist: denser fog below 3 m than above */
  SR_CHECK(sr_fog_with_mist(0.3f, 0.3f, 0.5f, 40.0f) > sr_fog_with_mist(0.3f, 0.3f, 5.0f, 40.0f));
  SR_CHECK_NEAR(sr_fog_with_mist(0.3f, 0.3f, 5.0f, 40.0f), 0.3f, 1e-6);
  SR_CHECK_NEAR(sr_fog_with_mist(0.9f, 0.5f, 0.0f, 40.0f), 1.0f, 1e-6);
  int under = -1, docks = -1;
  for (int b = 0; b < c->grid * c->grid && (under < 0 || docks < 0); b++) {
    if (c->blocks[b].water) continue;
    SrDistrictType t = c->districts[c->blocks[b].district].type;
    if (t == SR_D_UNDERCITY && under < 0) under = b;
    if (t == SR_D_DOCKS && docks < 0) docks = b;
  }
  SrSceneCache *s = (SrSceneCache *)calloc(1, sizeof *s);
  sr_scene_init(s, false);
  SrFrame *f = (SrFrame *)malloc(sizeof *f);
  int low = under >= 0 ? under : docks;
  if (low >= 0) {
    int32_t ox, oy;
    sr_city_block_origin(low, c->grid, &ox, &oy);
    g->player.pos = sr_v3((float)ox / 100.0f + 8.0f, 1.7f, (float)oy / 100.0f + 30.0f);
    g->force_weather = SR_WX_FOG;
    sr_scene_build(s, g, 1.0f, f);
    float misty = f->mist;
    g->player.pos = street;
    g->force_weather = SR_WX_CLEAR;
    sr_scene_build(s, g, 1.0f, f);
    SR_CHECK(misty > f->mist + 0.3f);
  }
  /* steam: twice as many puffs while it rains */
  SrStreetProp vent = {SR_SP_VENT, 0, SR_SPF_STEAM, SR_BAND_ALLEY, 3, 3, 0};
  SrStreetProp bin = {SR_SP_BIN, 0, 0, SR_BAND_KERB, 3, 4, 0};
  SrStreetProp pr[2] = {vent, bin};
  sr_mesh_clear(&m);
  sr_build_steam(pr, 2, eye, 0.0f, 10.0f, 60.0f, &m);
  int dry = m.nv;
  sr_mesh_clear(&m);
  sr_build_steam(pr, 2, eye, 0.8f, 10.0f, 60.0f, &m);
  SR_CHECK(dry > 0 && m.nv == dry * 2);
  for (int v = 0; v < m.nv; v += 4) /* puffs rise above the vent */
    SR_CHECK((m.v[v].y + m.v[v + 1].y + m.v[v + 2].y + m.v[v + 3].y) * 0.25f > 0.3f);
  sr_mesh_clear(&m);
  sr_build_steam(pr, 2, sr_v3(200, 1.7f, 200), 0.8f, 10.0f, 60.0f, &m);
  SR_CHECK_EQ(m.nv, 0); /* beyond the sprite distance */
  /* the street has steaming manholes and puddles somewhere */
  int steaming = 0, puddles = 0;
  SrStreetProp props[SR_MAX_BLOCK_PROPS];
  for (int b = 0; b < c->grid * c->grid; b++) {
    int n = sr_dressing_block(c, b, props, SR_MAX_BLOCK_PROPS);
    for (int i = 0; i < n; i++) {
      steaming += (props[i].flags & SR_SPF_STEAM) != 0;
      puddles += props[i].kind == SR_SP_PUDDLE;
    }
  }
  SR_CHECK(steaming > 20 && puddles > 50);
  g->force_weather = -1;
  free(f);
  sr_scene_free(s);
  free(s);
  sr_mesh_free(&m);
}

static void test_facades(void) {
  SR_SPEC("city-generation", "Derived detail keeps the digest");
  SR_SPEC("retro-rendering", "Neighbours differ");
  for (int seed = 1; seed <= 5; seed++) {
    SrCity *c = sr_city_create((uint64_t)seed * 7919u, NULL);
    uint64_t before = sr_city_digest(c);
    uint64_t fd = sr_facade_digest(c), dd = sr_dressing_digest(c);
    SR_CHECK(sr_city_digest(c) == before); /* derived detail never touches the city */
    SR_CHECK(fd == sr_facade_digest(c) && dd == sr_dressing_digest(c));
    if (seed == 1) { /* pinned: the derived detail changes only on purpose (and alike on every target) */
      printf("  facade digest %016llx, dressing digest %016llx\n", (unsigned long long)fd, (unsigned long long)dd);
      SR_CHECK(fd == 0x7706771d161b6111ull);
      SR_CHECK(dd == 0xeda0522fea2ab5d1ull);
    }
    int pairs = 0, across = 0;
    for (int blk = 0; blk < c->grid * c->grid; blk++) {
      const SrBlock *b = &c->blocks[blk];
      for (int i = 0; i < b->building_count; i++) {
        SrFacade fi;
        sr_facade_for(c, b->first_building + i, &fi);
        SR_CHECK(fi.winmod < SR_WIN_COUNT && fi.base < SR_BASE_COUNT && fi.use <= SR_FUSE_VACANT);
        SR_CHECK(fi.floor_h >= 3.2f && fi.floor_h <= 4.2f);
        for (int j = 0; j < i; j++) { /* same block: never both module and ground floor */
          SrFacade fj;
          sr_facade_for(c, b->first_building + j, &fj);
          SR_CHECK(!(fi.winmod == fj.winmod && fi.base == fj.base));
          pairs++;
        }
        /* across the street to the south: the building facing this one differs */
        const SrBuilding *bd = &c->buildings[b->first_building + i];
        int south = blk + c->grid;
        if (south >= c->grid * c->grid || c->blocks[south].water) continue;
        const SrBlock *sb = &c->blocks[south];
        for (int k = 0; k < sb->building_count; k++) {
          const SrBuilding *o = &c->buildings[sb->first_building + k];
          if (o->x1 <= bd->x0 || o->x0 >= bd->x1) continue;
          SrFacade fo;
          sr_facade_for(c, sb->first_building + k, &fo);
          bool same_tint = fo.tint[0] == fi.tint[0] && fo.tint[1] == fi.tint[1] && fo.tint[2] == fi.tint[2];
          SR_CHECK(fo.winmod != fi.winmod || !same_tint || fo.crown != fi.crown);
          across++;
        }
      }
    }
    SR_CHECK(pairs > 50 && across > 50);
    /* close neighbours get blank walls on the facing sides */
    int blanks = 0;
    for (int bi = 0; bi < c->building_count; bi++) {
      SrFacade f;
      sr_facade_for(c, bi, &f);
      blanks += f.blank != 0;
    }
    SR_CHECK(blanks > 0);
    sr_city_destroy(c);
  }
}

static void test_shapes_and_roofs(void) {
  SR_SPEC("city-generation", "Shapes stay inside the footprint");
  SR_SPEC("retro-rendering", "Roofs vary");
  int shapes[SR_SHAPE_COUNT] = {0};
  for (int seed = 1; seed <= 5; seed++) {
    SrCity *c = sr_city_create((uint64_t)seed * 7919u, NULL);
    int roofs[SR_D_COUNT][SR_ROOF_COUNT], count[SR_D_COUNT];
    memset(roofs, 0, sizeof roofs);
    memset(count, 0, sizeof count);
    for (int bi = 0; bi < c->building_count; bi++) {
      const SrBuilding *b = &c->buildings[bi];
      SrFacade f;
      sr_facade_for(c, bi, &f);
      SrVolume v[SR_MAX_VOLUMES];
      int n = sr_building_volumes(c, bi, &f, v);
      SR_CHECK(n >= 1 && n <= SR_MAX_VOLUMES);
      float x0 = (float)b->x0 / 100.0f, z0 = (float)b->y0 / 100.0f, x1 = (float)b->x1 / 100.0f,
            z1 = (float)b->y1 / 100.0f;
      bool full_base = false;
      for (int i = 0; i < n; i++) {
        SR_CHECK(v[i].x0 >= x0 - 1e-3f && v[i].x1 <= x1 + 1e-3f && v[i].z0 >= z0 - 1e-3f && v[i].z1 <= z1 + 1e-3f);
        SR_CHECK(v[i].y1 <= (float)b->height_cm / 100.0f + 1e-3f && v[i].y0 >= 0.0f);
        if (v[i].y0 < 3.0f) /* below 3 m only the full footprint */
          SR_CHECK(fabsf(v[i].x0 - x0) < 1e-3f && fabsf(v[i].x1 - x1) < 1e-3f && fabsf(v[i].z0 - z0) < 1e-3f &&
                   fabsf(v[i].z1 - z1) < 1e-3f);
        full_base = full_base || (v[i].flags & SR_VOL_BASE);
      }
      SR_CHECK(full_base);
      shapes[f.shape]++;
      SrDistrictType t = c->districts[c->blocks[b->block].district].type;
      roofs[t][f.roof]++;
      count[t]++;
    }
    for (int t = 0; t < SR_D_COUNT; t++) {
      if (count[t] < 20 || t == SR_D_OLDQUARTER) continue; /* the old quarter is all tiled roofs */
      int kinds = 0;
      for (int r = 0; r < SR_ROOF_COUNT; r++) kinds += roofs[t][r] > 0;
      SR_CHECK(kinds >= 3);
    }
    sr_city_destroy(c);
  }
  for (int s = 0; s < SR_SHAPE_COUNT; s++) SR_CHECK(shapes[s] > 0); /* every shape occurs */
  /* a headquarters gets a crown above its roof; a building mesh has its roof kit on top */
  SrGame *g = g_game;
  const SrCity *c = g->city;
  SrMesh m;
  sr_mesh_init(&m);
  int hq = c->factions.f[0].hq_building;
  sr_build_building(c, hq, (SrColor){255, 0, 255, 255}, &m);
  float top = 0;
  for (int i = 0; i < m.nv; i++) top = SR_MAX(top, m.v[i].y);
  SR_CHECK(top > (float)c->buildings[hq].height_cm / 100.0f + 5.0f);
  sr_mesh_free(&m);
}

/* Distance of a point from the nearest kerb of its block and which kerb (-1 if not on the lot). */
static float kerb_dist(const SrCity *c, int blk, float x, float z, int *which) {
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float lx0 = (float)(ox + SR_STREET_CM) / 100.0f, lz0 = (float)(oy + SR_STREET_CM) / 100.0f;
  float lx1 = (float)(ox + SR_BLOCK_CM) / 100.0f, lz1 = (float)(oy + SR_BLOCK_CM) / 100.0f;
  float d[4] = {x - lx0, z - lz0, lx1 - x, lz1 - z};
  *which = -1;
  float best = 1e9f;
  for (int k = 0; k < 4; k++)
    if (d[k] >= 0 && d[k] < best) {
      best = d[k];
      *which = k;
    }
  if (d[0] < 0 || d[1] < 0 || d[2] < 0 || d[3] < 0) *which = -1;
  return best;
}

static void test_street_dressing(void) {
  SR_SPEC("city-generation", "Street furniture by district");
  SR_SPEC("city-generation", "Lamps along every street");
  SR_SPEC("city-generation", "The walking lane stays clear");
  SR_SPEC("city-generation", "Alleys");
  SR_SPEC("retro-rendering", "The pavement has a kerb");
  for (int seed = 1; seed <= 5; seed++) {
    SrCity *c = sr_city_create((uint64_t)seed * 7919u, NULL);
    float props_per_type[SR_D_COUNT] = {0}, kerb_m[SR_D_COUNT] = {0};
    int alleys = 0, alleys_ok = 0;
    SrStreetProp p[SR_MAX_BLOCK_PROPS];
    for (int blk = 0; blk < c->grid * c->grid; blk++) {
      if (c->blocks[blk].water) {
        SR_CHECK_EQ(sr_dressing_block(c, blk, p, SR_MAX_BLOCK_PROPS), 0);
        continue;
      }
      SrDistrictType t = c->districts[c->blocks[blk].district].type;
      int n = sr_dressing_block(c, blk, p, SR_MAX_BLOCK_PROPS);
      float pav[4];
      sr_block_pavements(c, blk, pav);
      kerb_m[t] += 4.0f * (float)(SR_BLOCK_CM - SR_STREET_CM) / 100.0f;
      /* lamps: along each kerb no gap over 24 m */
      for (int k = 0; k < 4; k++) {
        float pos[8];
        int np = 0;
        for (int i = 0; i < n; i++) {
          if (p[i].kind != SR_SP_LAMP || p[i].band != SR_BAND_KERB) continue;
          int w;
          float kd = kerb_dist(c, blk, p[i].x, p[i].z, &w);
          if (w != k || kd > 1.0f || np >= 8) continue;
          pos[np++] = (k == 0 || k == 2) ? p[i].z : p[i].x;
        }
        SR_CHECK(np >= 3);
        for (int a = 1; a < np; a++) SR_CHECK(fabsf(pos[a] - pos[a - 1]) <= 24.01f);
      }
      for (int i = 0; i < n; i++) {
        if (p[i].band == SR_BAND_KERB || p[i].band == SR_BAND_BUILDING) props_per_type[t] += 1.0f;
        if (!(p[i].flags & SR_SPF_SOLID) || p[i].band == SR_BAND_ROAD || p[i].band == SR_BAND_ALLEY) continue;
        /* solid props: never in the clear walking lane */
        int w;
        float kd = kerb_dist(c, blk, p[i].x, p[i].z, &w);
        if (w < 0) continue;
        float lane = sr_walk_lane(pav[w]);
        SR_CHECK(fabsf(kd - lane) > 0.5f);
      }
      /* alleys: a light and at least two props per gap of 4 m or more */
      const SrBlock *b = &c->blocks[blk];
      for (int i = 0; i < b->building_count; i++)
        for (int j = 0; j < b->building_count; j++) {
          const SrBuilding *pa = &c->buildings[b->first_building + i], *qa = &c->buildings[b->first_building + j];
          if (qa->x0 - pa->x1 < 400 || qa->y0 >= pa->y1 || pa->y0 >= qa->y1) continue;
          alleys++;
          float mid = (float)(pa->x1 + qa->x0) / 200.0f;
          int lights = 0, things = 0;
          for (int k = 0; k < n; k++) {
            if (fabsf(p[k].x - mid) > (float)(qa->x0 - pa->x1) / 200.0f + 0.1f || !(p[k].flags & SR_SPF_ALLEY))
              continue;
            lights += p[k].kind == SR_SP_LAMP;
            things += p[k].kind != SR_SP_LAMP && p[k].kind != SR_SP_FIRE_ESCAPE;
          }
          alleys_ok += lights >= 1 && things >= 2;
        }
    }
    for (int t = 0; t < SR_D_COUNT; t++) {
      if (kerb_m[t] < 400.0f || t == SR_D_BARRENS) continue;
      float per100 = props_per_type[t] / kerb_m[t] * 100.0f;
      SR_CHECK(per100 >= 6.0f);
    }
    SR_CHECK(alleys > 0 && alleys_ok == alleys);
    sr_city_destroy(c);
  }
  /* the street mesh: kerb faces down to the road and crosswalks around the corner */
  SrGame *g = g_game;
  const SrCity *c = g->city;
  int blk = c->buildings[c->home_building].block;
  SrMesh m;
  sr_mesh_init(&m);
  sr_build_block_props(c, blk, 2, &m);
  int vertical = 0, crosswalks = 0;
  SrAtlasCell cw = sr_atlas_cell(SR_LAYER_DECALS, SR_DECAL_CROSSWALK);
  for (int q = 0; q + 3 < m.nv; q += 4) {
    bool kerb = fabsf(m.v[q].y) < 1e-4f && fabsf(m.v[q + 2].y - 0.12f) < 1e-4f;
    vertical += kerb;
    crosswalks += m.v[q].layer == cw.layer && fabsf(m.v[q].u - cw.u0) < 1e-4f;
  }
  SR_CHECK(vertical >= 4);
  SR_CHECK(crosswalks >= 16); /* four tiles on each of the four sides */
  int full = m.nv;
  sr_build_block_props(c, blk, 0, &m);
  SR_CHECK(m.nv < full); /* low detail drops props */
  sr_build_block_props(c, -1, 2, &m);
  SR_CHECK_EQ(m.nv, 0);
  sr_mesh_free(&m);
}

static void test_props_collide_and_shops(void) {
  SR_SPEC("city-generation", "Solid props block movement");
  SR_SPEC("retro-rendering", "Shops at street level");
  SR_SPEC("retro-rendering", "Closed shops are shuttered");
  SrGame *g = g_game;
  const SrCity *c = g->city;
  /* a solid prop stops the player; the walking lanes and the doors stay free */
  int solid_blk = -1, solid_i = -1;
  SrStreetProp p[SR_MAX_BLOCK_PROPS];
  for (int blk = 0; blk < c->grid * c->grid && solid_i < 0; blk++) {
    int n = sr_dressing_block(c, blk, p, SR_MAX_BLOCK_PROPS);
    for (int i = 0; i < n; i++)
      if ((p[i].flags & SR_SPF_SOLID) && p[i].band == SR_BAND_BUILDING) {
        solid_blk = blk;
        solid_i = i;
        break;
      }
  }
  SR_CHECK(solid_i >= 0);
  if (solid_i >= 0) {
    SR_CHECK(sr_game_blocked(g, p[solid_i].x, p[solid_i].z));
    SR_CHECK(sr_dressing_blocks(c, p[solid_i].x, p[solid_i].z));
  }
  (void)solid_blk;
  int checked = 0;
  for (int blk = 0; blk < c->grid * c->grid; blk++) {
    if (c->blocks[blk].water) continue;
    int32_t ox, oy;
    sr_city_block_origin(blk, c->grid, &ox, &oy);
    float pav[4];
    sr_block_pavements(c, blk, pav);
    /* walk the lane on the north kerb: never blocked by a prop */
    float lz = (float)(oy + SR_STREET_CM) / 100.0f + sr_walk_lane(pav[1]);
    for (float x = (float)(ox + SR_STREET_CM) / 100.0f + 3.0f; x < (float)(ox + SR_BLOCK_CM) / 100.0f - 3.0f;
         x += 0.5f) {
      SR_CHECK(!sr_dressing_blocks(c, x, lz));
      checked++;
    }
  }
  SR_CHECK(checked > 1000);
  for (int bi = 0; bi < c->building_count; bi++) { /* every place is reachable at its door */
    const SrBuilding *b = &c->buildings[bi];
    if (b->poi == SR_POI_NONE) continue;
    SR_CHECK(!sr_dressing_blocks(c, (float)(b->x0 + b->x1) / 200.0f, (float)b->y1 / 100.0f + 1.5f));
  }
  /* shops: lit fronts while open, shutters when closed */
  int shop = -1;
  for (int bi = 0; bi < c->building_count && shop < 0; bi++) {
    SrFacade f;
    sr_facade_for(c, bi, &f);
    if (c->buildings[bi].poi != SR_POI_NONE && f.base == SR_BASE_SHOPFRONT) shop = bi;
  }
  SR_CHECK(shop >= 0);
  if (shop < 0) return;
  const SrBuilding *b = &c->buildings[shop];
  SrVec3 eye = sr_v3((float)(b->x0 + b->x1) / 200.0f, 1.7f, (float)b->y1 / 100.0f + 5.0f);
  int64_t open_t = -1, closed_t = -1;
  for (int64_t t = 0; t < 7 * 86400 && (open_t < 0 || closed_t < 0); t += 1800) {
    bool o = sr_poi_open(c, shop, t);
    if (o && open_t < 0) open_t = t;
    if (!o && closed_t < 0) closed_t = t;
  }
  SrMesh m;
  sr_mesh_init(&m);
  sr_build_poi_fronts(c, &g->deltas, eye, open_t, &m);
  SR_CHECK(m.nv >= 4);
  bool lit_front = false;
  for (int v = 0; v < m.nv; v++)
    lit_front = lit_front || (m.v[v].layer == SR_LAYER_BASE_FIRST + SR_BASE_SHOPFRONT && m.v[v].emissive > 60 &&
                              fabsf(m.v[v].z - (float)b->y1 / 100.0f) < 0.1f);
  SR_CHECK(lit_front);
  if (closed_t >= 0) {
    sr_build_poi_fronts(c, &g->deltas, eye, closed_t, &m);
    bool still = false;
    for (int v = 0; v < m.nv; v++)
      still = still || (fabsf(m.v[v].x - eye.x) < 1.0f && fabsf(m.v[v].z - (float)b->y1 / 100.0f) < 0.1f);
    SR_CHECK(!still);
  }
  /* the chunk's own front is the shutter */
  SrFacade f;
  sr_facade_for(c, shop, &f);
  sr_mesh_clear(&m);
  sr_build_box_facade(&m, &f, (float)b->x0 / 100.0f, (float)b->y0 / 100.0f, (float)b->x1 / 100.0f,
                      (float)b->y1 / 100.0f, 0.0f, 10.0f, true, 0);
  bool shutter = false;
  for (int v = 0; v < m.nv; v++)
    shutter = shutter || (m.v[v].layer == SR_LAYER_BASE_FIRST + SR_BASE_SHUTTER &&
                          fabsf(m.v[v].z - (float)b->y1 / 100.0f) < 1e-3f);
  SR_CHECK(shutter);
  sr_mesh_free(&m);
}

void suite_atmos(void) {
  g_game = sr_game_create();
  sr_game_new(g_game, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Atmos");
  SR_RUN(test_layer_map);
  SR_RUN(test_sprites);
  SR_RUN(test_animation_and_glow);
  SR_RUN(test_window_share);
  SR_RUN(test_same_time_same_frame);
  SR_RUN(test_halos);
  SR_RUN(test_deck_and_sky);
  SR_RUN(test_wet_streets);
  SR_RUN(test_facades);
  SR_RUN(test_shapes_and_roofs);
  SR_RUN(test_street_dressing);
  SR_RUN(test_props_collide_and_shops);
  sr_game_destroy(g_game);
}
