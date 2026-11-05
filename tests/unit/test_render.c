#include "core/sr_rng.h"
#include "render/sr_render.h"
#include "ui/sr_ui.h"
#include "sr_test.h"

#include <math.h>
#include <stdlib.h>

static void test_mesh_basics(void) {
  SrMesh m;
  sr_mesh_init(&m);
  SrColor c = {10, 20, 30, 255};
  for (int i = 0; i < 300; i++)
    sr_mesh_box(&m, sr_v3(0, 0, 0), sr_v3(1, 2, 3), c, SR_LAYER_CONCRETE, (float)(i % 2), 0);
  SR_CHECK_EQ(m.nv, 300 * 20);
  SR_CHECK_EQ(m.ni, 300 * 30);
  SR_CHECK(m.dirty);
  uint64_t d = sr_mesh_digest(&m);
  sr_mesh_clear(&m);
  SR_CHECK_EQ(m.nv, 0);
  SR_CHECK(d != sr_mesh_digest(&m));
  m.gpu = 77;
  sr_mesh_free(&m);
  SR_CHECK_EQ(m.gpu, 77); /* backend handle survives for the backend to release */
}

static void test_textures_and_font(void) {
  SR_SPEC("retro-rendering", "Sign legibility");
  uint64_t a = sr_texgen_digest(), b = sr_texgen_digest();
  SR_CHECK(a == b);
  printf("  texture digest: %016llx\n", (unsigned long long)a); /* compared across targets in CI logs */
  uint8_t *px = (uint8_t *)malloc((size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4);
  sr_texgen_layer(SR_LAYER_FONT, px);
  int cx, cy;
  sr_font_cell('A', &cx, &cy);
  int on = 0;
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 5; x++) on += px[((size_t)(cy + y) * SR_TEX_SIZE + (size_t)(cx + x)) * 4 + 3] > 0;
  SR_CHECK(on > 10);
  volatile unsigned char high = 200u;
  sr_font_cell((char)high, &cx, &cy); /* out of range falls back to '?' */
  int qx, qy;
  sr_font_cell('?', &qx, &qy);
  SR_CHECK(cx == qx && cy == qy);
  SR_CHECK(sr_font_glyph('\n') == sr_font_glyph('?'));
  sr_texgen_layer(SR_LAYER_ICON_FIRST + SR_POI_FOOD, px);
  int lit = 0;
  for (int i = 0; i < SR_TEX_SIZE * SR_TEX_SIZE; i++) lit += px[i * 4 + 3] > 0;
  SR_CHECK(lit > 500);
  sr_texgen_layer(SR_TEX_LAYERS + 40, px); /* unused layer */
  free(px);
  /* a 2 m sign icon at 10 m covers >= 12 px at 270 px with 70 degrees vertical fov */
  float pixels = sr_projected_pixels(2.0f, 10.0f, 70.0f * SR_PI / 180.0f, 270);
  SR_CHECK(pixels >= 12.0f);
  SR_CHECK_EQ((int)sr_projected_pixels(1, 0, 1, 100), 100);
}

static void test_palettes_and_fog(void) {
  SR_SPEC("retro-rendering", "Palettes differ");
  SR_SPEC("retro-rendering", "Neon through fog");
  for (int a = 0; a < SR_D_COUNT; a++) {
    for (int b = 0; b < a; b++) {
      const SrPalette *pa = sr_palette((SrDistrictType)a), *pb = sr_palette((SrDistrictType)b);
      float dfog = sr_color_distance(pa->fog, pb->fog);
      float dneon = sr_color_distance(pa->neon[0], pb->neon[0]);
      SR_CHECK(dfog + dneon > 40.0f);
    }
  }
  SR_CHECK(sr_palette(SR_D_COUNT) == sr_palette(SR_D_RESIDENTIAL));
  /* corp colours: 15 degree hue steps, two apart are clearly distinct */
  for (int h = 0; h < 24; h++) {
    SrColor c = sr_corp_color(h), d = sr_corp_color(h + 2);
    SR_CHECK(sr_color_distance(c, d) > 60.0f);
  }
  /* emissive at the fog end still contrasts */
  float plain = sr_fog_factor(400, 100, 400, 0.0f);
  float neon = sr_fog_factor(400, 100, 400, 1.0f);
  SR_CHECK_NEAR(plain, 1.0, 1e-6);
  SR_CHECK(neon <= 0.31f);
  SR_CHECK_NEAR(sr_fog_factor(10, 100, 400, 0), 0, 0);
}

static void test_lights(void) {
  SrLight l[20];
  for (int i = 0; i < 20; i++) {
    l[i].pos = sr_v3((float)i * 3.0f, 0, 0);
    l[i].radius = 10;
    l[i].r = l[i].g = l[i].b = 1;
  }
  int idx[SR_MAX_LIGHTS_PER_DRAW];
  int n = sr_select_lights(l, 20, sr_v3(0, 0, 0), idx);
  SR_CHECK(n > 0 && n <= SR_MAX_LIGHTS_PER_DRAW);
  SR_CHECK_EQ(idx[0], 0); /* most influential first */
  for (int i = 1; i < n; i++) SR_CHECK(idx[i] > idx[i - 1]);
  SR_CHECK_EQ(sr_select_lights(l, 20, sr_v3(1000, 0, 0), idx), 0);
}

static SrGame *g_game;

static void test_chunks_and_streaming(void) {
  SR_SPEC("city-generation", "Generation budget respected");
  SR_SPEC("city-generation", "Returning to an area");
  SrGame *g = g_game;
  SrChunkCache *cc = (SrChunkCache *)calloc(1, sizeof *cc);
  sr_chunk_cache_init(cc, 4);
  int n = sr_chunk_count(g->city);
  SR_CHECK_EQ(n, 8);
  SrChunk *c0 = sr_chunk_get(cc, g->city, &g->deltas, 0, 0, 1);
  SR_CHECK(c0 && c0->mesh.nv > 0);
  uint64_t d0 = c0->digest;
  SR_CHECK(sr_chunk_get(cc, g->city, &g->deltas, 1, 0, 1) == NULL); /* budget: one build per frame */
  cc->built_this_frame = 0;
  for (int i = 1; i <= 6; i++) {
    cc->built_this_frame = 0;
    SR_CHECK(sr_chunk_get(cc, g->city, &g->deltas, i, 1, 1) != NULL);
  }
  SR_CHECK(cc->evictions > 0);
  cc->built_this_frame = 0;
  SrChunk *again = sr_chunk_get(cc, g->city, &g->deltas, 0, 0, 1);
  SR_CHECK(again && again->digest == d0);                            /* regenerated identically */
  SR_CHECK(sr_chunk_get(cc, g->city, &g->deltas, 0, 0, 0) == again); /* cached: no budget needed */
  SR_CHECK(sr_chunk_get(cc, g->city, &g->deltas, -1, 0, 1) == NULL);
  SR_CHECK(sr_chunk_get(cc, g->city, &g->deltas, n, 0, 1) == NULL);
  /* a delta (switched-off sign) changes the chunk; reverting restores the digest */
  int sign = -1;
  for (int b = 0; b < g->city->building_count && sign < 0; b++) {
    int blk = g->city->buildings[b].block;
    if (g->city->buildings[b].poi != SR_POI_NONE && blk % g->city->grid < 4 && blk / g->city->grid < 4) sign = b;
  }
  SrMesh m;
  sr_mesh_init(&m);
  if (sign >= 0) {
    SrDeltas d = g->deltas;
    sr_deltas_set(&d, g->city->buildings[sign].id, 0);
    sr_chunk_build(g->city, &d, 0, 0, &m);
    SR_CHECK(sr_mesh_digest(&m) != d0);
  }
  sr_mesh_free(&m);
  sr_chunk_cache_free(cc);
  free(cc);
  sr_chunk_cache_init(cc = (SrChunkCache *)calloc(1, sizeof *cc), 999);
  SR_CHECK_EQ(cc->capacity, SR_CHUNK_CACHE);
  free(cc);
}

static void test_scene_frame(void) {
  SR_SPEC("retro-rendering", "Logo beyond fog");
  SR_SPEC("retro-rendering", "No void at the horizon");
  SR_SPEC("retro-rendering", "No rain under cover");
  SR_SPEC("world-environment", "Night at noon");
  SrGame *g = g_game;
  SrSceneCache *s = (SrSceneCache *)calloc(1, sizeof *s);
  sr_scene_init(s, false);
  sr_game_teleport_building(g, g->player.home);
  /* run a few frames so chunks stream in */
  for (int i = 0; i < 30; i++) {
    sr_game_step(g, NULL);
    SrFrame *f = (SrFrame *)malloc(sizeof *f);
    sr_scene_build(s, g, 16.0f / 9.0f, f);
    free(f);
  }
  SrFrame *f = (SrFrame *)malloc(sizeof *f);
  sr_scene_build(s, g, 16.0f / 9.0f, f);
  SR_CHECK(f->ndraws > 10);
  SR_CHECK(f->nlights > 0);
  SR_CHECK(f->fog_end > f->fog_start);
  SR_CHECK(!f->daylight);
  int nofog = 0;
  for (int i = 0; i < f->ndraws; i++) {
    if (f->draws[i].pass == SR_PASS_NOFOG) nofog++;
    SR_CHECK(f->draws[i].nlights <= SR_MAX_LIGHTS_PER_DRAW);
  }
  SR_CHECK_EQ(nofog, 2); /* skyline and landmark logos skip the fog */
  SR_CHECK(s->landmarks.nv == g->city->factions.corp_count * 8);
  /* skyline: a closed ring of geometry around the city */
  SR_CHECK(s->skyline.nv == 180 * 4);
  float r0 = 0;
  for (int i = 0; i < s->skyline.nv; i++) {
    float dx = s->skyline.v[i].x - (float)g->city->grid * SR_BLOCK_CM / 200.0f;
    float dz = s->skyline.v[i].z - (float)g->city->grid * SR_BLOCK_CM / 200.0f;
    r0 = SR_MAX(r0, sqrtf(dx * dx + dz * dz));
  }
  SR_CHECK(r0 > f->fog_end);
  /* daylight only with the Pall removed */
  g->pall_removed = true;
  sr_scene_build(s, g, 1.0f, f);
  SR_CHECK(f->daylight);
  g->pall_removed = false;
  /* rain is never generated under cover */
  SrMesh rain;
  sr_mesh_init(&rain);
  for (uint64_t k = 0; k < 5; k++) {
    sr_build_rain(g->player.pos, 1.0f, k, g->city, &rain);
    for (int i = 0; i < rain.nv; i += 4)
      SR_CHECK(!sr_point_covered(g->city, sr_v3(rain.v[i].x, rain.v[i].y, rain.v[i].z)));
  }
  sr_mesh_free(&rain);
  const SrBuilding *b = &g->city->buildings[g->player.home];
  SR_CHECK(sr_point_covered(g->city, sr_v3((float)(b->x0 + b->x1) / 200.0f, 2, (float)(b->y0 + b->y1) / 200.0f)));
  SR_CHECK(!sr_point_covered(g->city, sr_v3(-5, 1, -5)));
  for (int blk = 0; blk < g->city->grid * g->city->grid; blk++) {
    if (!g->city->blocks[blk].highway) continue;
    int32_t ox, oy;
    sr_city_block_origin(blk, g->city->grid, &ox, &oy);
    SR_CHECK(sr_point_covered(g->city, sr_v3((float)ox / 100.0f + 5, 2, (float)oy / 100.0f + 8)));
    break;
  }
  /* rain in the frame when it rains: find a rainy hour */
  for (int64_t t = 0; t < 30 * 86400; t += 7200) {
    if (sr_weather_at(g->seed, t).rain > 0.5f) {
      sr_clock_set_seconds(&g->clock, t + 3600);
      break;
    }
  }
  sr_scene_build(s, g, 1.0f, f);
  SR_CHECK(s->rain.nv > 0);
  free(f);
  sr_scene_free(s);
  free(s);
}

static void test_characters(void) {
  SrMesh m;
  sr_mesh_init(&m);
  for (int k = 0; k < 64; k++) {
    SrAppearance a = sr_appearance_generate((uint64_t)k, (SrRole)(k % SR_ROLE_COUNT), k % 4);
    a.cyber = (uint8_t)(k % 2);
    sr_build_character(&a, sr_v3((float)k, 0, 0), sr_v3(1, 0, 0), k % 3 == 0, &m);
  }
  SR_CHECK(m.nv > 64 * 60);
  /* two different appearances give different geometry or colours */
  SrMesh a1, a2;
  sr_mesh_init(&a1);
  sr_mesh_init(&a2);
  SrAppearance x = sr_appearance_generate(1, SR_ROLE_WORKER, 0), y = x;
  y.hair_color = (uint8_t)((x.hair_color + 1) % 12);
  y.hair = 2;
  x.hair = 2;
  sr_build_character(&x, sr_v3(0, 0, 0), sr_v3(1, 0, 0), false, &a1);
  sr_build_character(&y, sr_v3(0, 0, 0), sr_v3(1, 0, 0), false, &a2);
  SR_CHECK(sr_mesh_digest(&a1) != sr_mesh_digest(&a2));
  sr_mesh_free(&a1);
  sr_mesh_free(&a2);
  sr_mesh_free(&m);
}

static void test_view_math(void) {
  SR_SPEC("retro-rendering", "Integer scaling");
  SR_SPEC("platform-runtime", "Resize keeps the aspect ratio");
  SrViewport v = sr_letterbox(1920, 1080, 480, 270, true);
  SR_CHECK(v.w == 1920 && v.h == 1080 && v.x == 0);
  v = sr_letterbox(1900, 1080, 480, 270, true);
  SR_CHECK_EQ(v.w, 1440); /* integer factor 3 */
  v = sr_letterbox(1000, 1000, 480, 270, false);
  SR_CHECK(v.y > 0 && v.x == 0 && v.w == 1000);
  SR_CHECK_NEAR((float)v.w / v.h, 480.0f / 270.0f, 0.01);
  v = sr_letterbox(0, 0, 480, 270, false);
  SR_CHECK_EQ(v.w, 0);
  v = sr_letterbox(200, 100, 480, 270, true); /* smaller than 1x: no integer clamp */
  SR_CHECK(v.w > 0);
  SrDynRes d;
  memset(&d, 0, sizeof d);
  SR_CHECK(!sr_dynres_update(&d, 30, 16, 1.0f));
  SR_CHECK(sr_dynres_update(&d, 30, 16, 1.5f));
  SR_CHECK_EQ(d.level, 1);
  d.level = 3;
  for (int i = 0; i < 10; i++) sr_dynres_update(&d, 30, 16, 3.0f);
  SR_CHECK_EQ(d.level, 3);
}

static void test_ui(void) {
  SR_SPEC("retro-rendering", "Text on a phone");
  SR_SPEC("input-controls", "Portrait orientation");
  SrUi ui;
  sr_ui_init(&ui);
  int s = sr_ui_auto_scale(390.0f);
  SR_CHECK(sr_ui_glyph_height(s) >= 12);
  SR_CHECK(sr_ui_auto_scale(1080.0f) >= 3);
  sr_ui_begin(&ui, 844, 390, s);
  sr_ui_text(&ui, 0, 0, "HELLO world", (SrColor){255, 255, 255, 255}, s);
  SR_CHECK_EQ(ui.mesh.nv, 10 * 4); /* spaces are skipped */
  SR_CHECK_EQ(sr_ui_text_width("abc", 2), 36);
  sr_ui_icon(&ui, 0, 0, 16, SR_POI_FOOD, (SrColor){255, 255, 255, 255});
  SrGame *g = g_game;
  SrInput in;
  sr_input_init(&in, true);
  /* every screen builds */
  static const SrScreen screens[] = {SR_SCREEN_TITLE, SR_SCREEN_PLAY, SR_SCREEN_PAUSE, SR_SCREEN_TERMINAL,
                                     SR_SCREEN_DEAD};
  sr_game_toast(g, "toast");
  int ids[4];
  int k = sr_game_fixer(g, g->city->factions.fixer_building[0], ids);
  if (k) sr_game_accept(g, ids[0]);
  g->ctx.kind = SR_CTX_FOOD;
  g->heat.level[0] = 3;
  for (size_t i = 0; i < SR_ARRAY_LEN(screens); i++) {
    g->screen = screens[i];
    sr_ui_begin(&ui, 844, 390, s);
    sr_ui_build(&ui, g, &in, false);
    SR_CHECK(ui.mesh.nv > 0);
    sr_ui_begin(&ui, 844, 390, s);
    sr_ui_build(&ui, g, NULL, false);
  }
  g->screen = SR_SCREEN_PLAY;
  /* touch overlay with an active stick and an open wheel */
  sr_input_touch(&in, 1, 0.2f, 0.6f, 0);
  sr_input_touch(&in, 1, 0.25f, 0.55f, 1);
  sr_input_tick(&in, SR_STEP_DT);
  in.wheel_open = true;
  sr_ui_begin(&ui, 844, 390, s);
  sr_ui_build(&ui, g, &in, false);
  int with_overlay = ui.mesh.nv;
  in.scheme = SR_SCHEME_KBM;
  sr_ui_begin(&ui, 844, 390, s);
  sr_ui_build(&ui, g, &in, false);
  SR_CHECK(with_overlay > ui.mesh.nv);
  /* portrait on touch shows only the rotate prompt */
  in.scheme = SR_SCHEME_TOUCH;
  sr_ui_begin(&ui, 390, 844, s);
  sr_ui_build(&ui, g, &in, true);
  SR_CHECK(ui.mesh.nv < 120);
  for (int c = 0; c <= SR_CTX_CLOSED + 1; c++) (void)sr_ui_context_label((SrContextKind)c);
  SR_CHECK_STR(sr_ui_context_label(SR_CTX_HOME), "SLEEP");
  /* player outside the city: HUD still builds */
  SrVec3 keep = g->player.pos;
  g->player.pos = sr_v3(-100, 1.7f, -100);
  sr_ui_begin(&ui, 844, 390, s);
  sr_ui_build(&ui, g, &in, false);
  g->player.pos = keep;
  sr_ui_begin(&ui, 100, 100, 0);
  SR_CHECK_EQ(ui.scale, 2);
  sr_ui_free(&ui);
}

void suite_render(void) {
  g_game = sr_game_create();
  sr_game_new(g_game, 0x5EEDC0DEull, NULL, SR_BG_STREET_KID, "Render");
  g_game->crowd_enabled = true;
  SR_RUN(test_mesh_basics);
  SR_RUN(test_textures_and_font);
  SR_RUN(test_palettes_and_fog);
  SR_RUN(test_lights);
  SR_RUN(test_chunks_and_streaming);
  SR_RUN(test_scene_frame);
  SR_RUN(test_characters);
  SR_RUN(test_view_math);
  SR_RUN(test_ui);
  sr_game_destroy(g_game);
}
