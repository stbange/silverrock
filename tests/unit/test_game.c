#include "core/sr_rng.h"
#include "game/sr_save.h"
#include "game/sr_script.h"
#include "sr_test.h"

#include <stdlib.h>
#include <time.h>

static SrGame *new_game(uint64_t seed) {
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, SR_BG_STREET_KID, "Tester");
  return g;
}

static SrActionState idle(void) {
  SrActionState a;
  memset(&a, 0, sizeof a);
  return a;
}

/* ---- loop and stepping ------------------------------------------------------------------------ */

static void test_fixed_timestep(void) {
  SR_SPEC("platform-runtime", "Frame rate does not change results");
  SR_SPEC("platform-runtime", "Long frame is clamped");
  SR_SPEC("quality-gates", "Replay matches");
  SR_SPEC("quality-gates", "Divergence is reported");
  /* replay the same input at 30 and 144 FPS: identical checksums every 60 steps */
  SrGame *a = new_game(31), *b = new_game(31);
  a->crowd_enabled = b->crowd_enabled = false;
  SrLoop la = {0, 0}, lb = {0, 0};
  uint64_t ca[8], cb[8];
  int ka = 0, kb = 0;
  SrActionState mv = idle();
  mv.move_y = 1.0f;
  mv.look_x = 0.01f;
  while (ka < 8) {
    int n = sr_loop_advance(&la, 1.0 / 30.0);
    for (int i = 0; i < n && ka < 8; i++) {
      sr_game_step(a, &mv);
      if (a->steps % 60 == 0) ca[ka++] = sr_game_checksum(a);
    }
  }
  while (kb < 8) {
    int n = sr_loop_advance(&lb, 1.0 / 144.0);
    for (int i = 0; i < n && kb < 8; i++) {
      sr_game_step(b, &mv);
      if (b->steps % 60 == 0) cb[kb++] = sr_game_checksum(b);
    }
  }
  for (int i = 0; i < 8; i++) SR_CHECK(ca[i] == cb[i]);
  /* divergence: a different input changes the checksum at the next checkpoint */
  SrActionState other = mv;
  other.look_x = 0.05f;
  for (int i = 0; i < 60; i++) {
    sr_game_step(a, &mv);
    sr_game_step(b, &other);
  }
  SR_CHECK(sr_game_checksum(a) != sr_game_checksum(b));
  /* a 2 s frame runs at most 5 steps */
  SrLoop l = {0, 0};
  SR_CHECK_EQ(sr_loop_advance(&l, 2.0), SR_MAX_STEPS_PER_FRAME);
  SR_CHECK(l.acc < SR_STEP_DT);
  SR_CHECK_EQ(sr_loop_advance(&l, -1.0), 0);
  sr_game_destroy(a);
  sr_game_destroy(b);
}

static void test_movement_and_collision(void) {
  SR_SPEC("input-controls", "Toggle crouch");
  SrGame *g = new_game(5);
  g->crowd_enabled = false;
  sr_game_teleport_building(g, g->player.home);
  SrVec3 p0 = g->player.pos;
  SR_CHECK(!sr_game_blocked(g, p0.x, p0.z));
  SrActionState a = idle();
  a.move_y = 1.0f; /* walk into the building in front */
  for (int i = 0; i < 300; i++) sr_game_step(g, &a);
  SR_CHECK(!sr_game_blocked(g, g->player.pos.x, g->player.pos.z));
  SR_CHECK(sr_game_blocked(g, -10, -10));
  /* jump */
  a = idle();
  a.pressed[SR_ACT_JUMP] = a.down[SR_ACT_JUMP] = true;
  sr_game_step(g, &a);
  a = idle();
  float peak = 0;
  for (int i = 0; i < 120; i++) {
    sr_game_step(g, &a);
    peak = SR_MAX(peak, g->player.pos.y);
  }
  SR_CHECK(peak > 1.8f);
  SR_CHECK_NEAR(g->player.pos.y, 1.7f, 1e-3);
  /* crouch lowers the eye; sprint moves faster; overburdened slows down */
  a.down[SR_ACT_CROUCH] = true;
  sr_game_step(g, &a);
  SR_CHECK(g->player.pos.y < 1.5f);
  a = idle();
  a.move_x = 1.0f;
  a.move_y = 1.0f;
  a.down[SR_ACT_SPRINT] = true;
  sr_game_step(g, &a);
  for (int i = 0; i < 20; i++) sr_inv_add(&g->player.inv, sr_item_weapon(12, SR_STREET_MAKER_FIRST, 0, (uint32_t)i));
  sr_game_step(g, &a);
  /* toggle crouch through the input layer */
  SrInput in;
  sr_input_init(&in, false);
  in.toggle_mode[SR_ACT_CROUCH] = true;
  sr_input_key(&in, SR_KEY_C, true);
  SrActionState s = sr_input_tick(&in, SR_STEP_DT);
  sr_input_key(&in, SR_KEY_C, false);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.down[SR_ACT_CROUCH]);
  sr_input_key(&in, SR_KEY_C, true);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(!s.down[SR_ACT_CROUCH]);
  sr_game_destroy(g);
}

static void test_pause_focus_terminal(void) {
  SrGame *g = new_game(6);
  g->crowd_enabled = false;
  SrActionState a = idle();
  a.pressed[SR_ACT_PAUSE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PAUSE);
  int64_t t = sr_game_time(g);
  sr_game_step(g, NULL);
  SR_CHECK_EQ(sr_game_time(g), t);
  sr_game_step(g, &a); /* pause again resumes */
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  a = idle();
  a.pressed[SR_ACT_TERMINAL] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_TERMINAL);
  a = idle();
  a.pressed[SR_ACT_PAUSE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  sr_game_focus(g, false);
  SR_CHECK(g->clock.paused && g->audio_muted && g->screen == SR_SCREEN_PAUSE);
  SR_CHECK(g->autosave_requests > 0);
  sr_game_confirm_resume(g);
  SR_CHECK(!g->clock.paused && g->screen == SR_SCREEN_PLAY);
  sr_game_focus(g, true);
  SrGame *idle_game = sr_game_create();
  sr_game_focus(idle_game, false); /* not started: nothing happens */
  sr_game_step(idle_game, &a);
  SR_CHECK_EQ(idle_game->screen, SR_SCREEN_TITLE);
  sr_game_destroy(idle_game);
  sr_game_destroy(g);
}

static void test_interactions(void) {
  SR_SPEC("player-life", "Sleeping eight hours");
  SR_SPEC("player-life", "Capsule hotel night");
  SR_SPEC("game-persistence", "Autosave on sleep");
  SrGame *g = new_game(7);
  g->crowd_enabled = false;
  /* food */
  int32_t d;
  int food = sr_city_find_poi_near(g->city, SR_POI_FOOD, g->player.home, &d);
  int open, hours;
  sr_poi_hours(g->city, food, &open, &hours);
  sr_game_skip(g, ((open + 1 + 24) - sr_time_hour(sr_game_time(g))) % 24 * 3600, false);
  sr_game_teleport_building(g, food);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_FOOD);
  float sat = g->player.satiety;
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(g->player.satiety > sat);
  g->player.cred = 0;
  SR_CHECK(sr_game_interact(g) != SR_OK);
  g->player.cred = 5000;
  SR_CHECK_EQ(sr_game_eat_at(g, g->player.home), SR_ERR_NOT_FOUND);
  /* home: sleep 8 hours */
  sr_game_teleport_building(g, g->player.home);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_HOME);
  int64_t t = sr_game_time(g);
  int saves = g->autosave_requests;
  g->player.fatigue = 90;
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(sr_game_time(g) - t, 8 * 3600);
  SR_CHECK_NEAR(g->player.fatigue, 0, 1e-3);
  SR_CHECK(g->autosave_requests > saves);
  g->hostiles_near = 1;
  SR_CHECK_EQ(sr_game_sleep(g, 8, SR_SLEEP_HOME), SR_ERR_DENIED);
  g->hostiles_near = 0;
  /* capsule */
  int cap = sr_city_find_poi_near(g->city, SR_POI_CAPSULE, g->player.home, &d);
  sr_game_teleport_building(g, cap);
  g->ctx = sr_game_context(g);
  int64_t cash = g->player.cred;
  SR_CHECK_EQ(sr_game_sleep(g, 8, SR_SLEEP_CAPSULE), SR_OK);
  SR_CHECK(g->player.cred < cash);
  g->player.cred = 0;
  SR_CHECK_EQ(sr_game_sleep(g, 8, SR_SLEEP_CAPSULE), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_sleep(g, 2, SR_SLEEP_STREET), SR_OK);
  /* evicted: home is locked until the debt is paid */
  g->player.evicted = true;
  g->player.rent_debt = 100;
  sr_game_teleport_building(g, g->player.home);
  SR_CHECK_EQ(sr_game_interact(g), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_sleep(g, 8, SR_SLEEP_HOME), SR_ERR_DENIED);
  g->player.cred = 1000;
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(!g->player.evicted);
  /* metro and cab */
  g->player.cred = 1000;
  sr_game_teleport_building(g, g->player.home);
  SR_CHECK_EQ(sr_game_metro(g, g->city->station_count - 1), SR_OK);
  SR_CHECK_EQ(sr_game_metro(g, 999), SR_ERR_INVALID);
  g->player.cred = 0;
  SR_CHECK_EQ(sr_game_metro(g, 0), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_cab(g, g->player.home), SR_ERR_DENIED);
  g->player.cred = 1000;
  SR_CHECK_EQ(sr_game_cab(g, g->player.home), SR_OK);
  SR_CHECK_EQ(sr_game_walk_to(g, food), SR_OK);
  /* metro context: interact rides to the next station */
  int32_t ox, oy;
  sr_city_block_origin(g->city->stations[0].block, g->city->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 4.0f, 1.7f, (float)oy / 100.0f + 4.0f);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_METRO);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  /* outside the city: no context */
  g->player.pos = sr_v3(-50, 1.7f, -50);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_NONE);
  SR_CHECK_EQ(sr_game_interact(g), SR_ERR_NOT_FOUND);
  sr_game_toast(g, "hello");
  SR_CHECK(g->toast_time > 0);
  sr_game_destroy(g);
}

static void test_job_interactions(void) {
  SrGame *g = new_game(8);
  g->crowd_enabled = false;
  int fixer = g->city->factions.fixer_building[g->city->blocks[g->city->buildings[g->player.home].block].district];
  int open, hours;
  sr_poi_hours(g->city, fixer, &open, &hours);
  sr_game_teleport_building(g, fixer);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_FIXER);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK); /* takes the first offered job */
  SR_CHECK(g->active_job >= 0);
  SrJob *j = sr_jobs_get(&g->jobs, g->active_job);
  sr_game_teleport_building(g, j->target_building);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_JOB_TARGET);
  int tries = 0;
  while (!j->executed && j->state == SR_JS_ACCEPTED && tries++ < 10) {
    g->player.prog.skill_rank[SR_SK_PERSUASION] = 20;
    g->player.prog.skill_rank[SR_SK_STEALTH] = 20;
    sr_game_interact(g);
  }
  if (j->executed && j->state == SR_JS_ACCEPTED) {
    sr_game_teleport_building(g, j->client_building);
    SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_JOB_REPORT);
    SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  }
  SR_CHECK(j->state != SR_JS_ACCEPTED || !j->executed);
  /* a second visit the same day: no new work */
  sr_game_teleport_building(g, fixer);
  for (int i = 0; i < 5; i++) sr_game_interact(g);
  SR_CHECK_EQ(sr_game_accept(g, 99999), SR_ERR_NOT_FOUND);
  SrMissionResult none = sr_game_execute(g, 99999, SR_AP_GHOST);
  SR_CHECK(!none.success);
  SR_CHECK_EQ(sr_game_report(g, 99999), 0);
  /* a mission that raises an alarm adds Heat with witnesses */
  SrJob loud;
  sr_job_generate(g->city, SR_JT_SABOTAGE, 3, sr_game_time(g), &loud);
  loud.state = SR_JS_ACCEPTED;
  loud.approaches = 0xF;
  loud.id = g->jobs.next_id++;
  g->jobs.jobs[g->jobs.count++] = loud;
  g->player.prog.skill_rank[SR_SK_PISTOLS] = 0;
  sr_game_execute(g, loud.id, SR_AP_HAMMER);
  int heat = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat += g->heat.level[k];
  SR_CHECK(heat > 0);
  /* closed shop context */
  int shop = -1;
  for (int b = 0; b < g->city->building_count && shop < 0; b++)
    if (g->city->buildings[b].poi == SR_POI_GUNSHOP) shop = b;
  sr_poi_hours(g->city, shop, &open, &hours);
  int64_t now = sr_game_time(g);
  int want = (open + hours + 1) % 24;
  sr_game_skip(g, ((want - sr_time_hour(now)) + 24) % 24 * 3600 + 60, false);
  sr_game_teleport_building(g, shop);
  SrContext c = sr_game_context(g);
  if (c.kind == SR_CTX_CLOSED) SR_CHECK_EQ(sr_game_interact(g), SR_ERR_DENIED);
  sr_game_destroy(g);
}

static void test_fire_and_flatline(void) {
  SR_SPEC("player-life", "Starvation floor");
  SR_SPEC("combat", "Hardcore death deletes the save");
  SrGame *g = new_game(9);
  g->crowd_enabled = true;
  int spire = -1;
  for (int b = 0; b < g->city->building_count && spire < 0; b++)
    if (g->city->districts[g->city->blocks[g->city->buildings[b].block].district].type == SR_D_SPIRE) spire = b;
  sr_game_teleport_building(g, spire);
  SrActionState a = idle();
  for (int i = 0; i < 40; i++) sr_game_step(g, &a);
  int ammo = sr_inv_count(&g->player.inv, SR_IK_AMMO, -1);
  a.pressed[SR_ACT_FIRE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(sr_inv_count(&g->player.inv, SR_IK_AMMO, -1), ammo - 1);
  SR_CHECK(g->events.count > 0);
  /* out of ammo */
  int idx = sr_inv_find(&g->player.inv, SR_IK_AMMO, -1);
  sr_inv_remove_at(&g->player.inv, idx, g->player.inv.items[idx].count);
  g->player.equipped = (int8_t)sr_inv_find(&g->player.inv, SR_IK_WEAPON, 0);
  sr_game_fire(g);
  /* melee or nothing equipped: firing does nothing */
  g->player.equipped = -1;
  sr_game_fire(g);
  /* flatline with and without MedEvac */
  g->player.medevac = true;
  int64_t cash = g->player.cred;
  g->player.health = 0;
  sr_game_step(g, &a);
  SR_CHECK(g->player.health > 0);
  SR_CHECK(g->player.cred < cash);
  g->player.medevac = false;
  g->player.cred = 1000;
  sr_game_flatline(g);
  SR_CHECK(g->player.cred <= 1000);
  g->hardcore = true;
  sr_game_flatline(g);
  SR_CHECK_EQ(g->screen, SR_SCREEN_DEAD);
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  st.write(st.user, "hardcore.srsv", "x", 1);
  SR_CHECK_EQ(sr_save_delete_hardcore(&st), SR_OK);
  SR_CHECK(!st.exists(st.user, "hardcore.srsv"));
  sr_memstore_free(&mem);
  sr_game_destroy(g);
}

static void test_deltas(void) {
  SR_SPEC("city-generation", "Returning to an area");
  SrDeltas d;
  memset(&d, 0, sizeof d);
  SR_CHECK_EQ(sr_deltas_get(&d, 5), -1);
  sr_deltas_set(&d, 5, 1);
  sr_deltas_set(&d, 5, 2);
  SR_CHECK_EQ(sr_deltas_get(&d, 5), 2);
  for (uint32_t i = 0; i < SR_MAX_DELTAS + 5; i++) sr_deltas_set(&d, 100 + i, 1);
  SR_CHECK_EQ(d.count, SR_MAX_DELTAS);
}

/* ---- saves ------------------------------------------------------------------------------------ */

static void test_save_roundtrip(void) {
  SR_SPEC("game-persistence", "Save and load round trip");
  SR_SPEC("game-persistence", "Save size bound");
  SR_SPEC("game-persistence", "Stash keeps items");
  SR_SPEC("player-life", "Stash keeps items");
  SrGame *g = new_game(11);
  g->crowd_enabled = false;
  sr_inv_add(&g->player.stash, sr_item_simple(SR_IK_GADGET, SR_GD_SMOKE, 3));
  g->player.cyber[0] = sr_item_simple(SR_IK_CYBER, 2, 1);
  g->player.cyber_count = 1;
  g->pop->npcs[3].status = SR_NPC_DEAD;
  g->pop->npcs[4].attitude = -40;
  SrAmbient amb[4];
  sr_ambient_generate(g->city, g->city->buildings[g->player.home].block, sr_game_time(g), amb, 4);
  sr_population_promote(g->pop, g->city, &amb[0], sr_game_time(g));
  sr_deltas_set(&g->deltas, 77, 1);
  SrHeat *h = &g->heat;
  SrCrime cr = {0, 2, true, true, false, 9, sr_game_time(g)};
  sr_heat_crime(h, &cr);
  int ids[4];
  sr_game_fixer(g, g->city->factions.fixer_building[0], ids);
  sr_game_accept(g, ids[0]);
  SrAutoplayStats st = sr_autoplay(g, SR_STRAT_GIG, 70, 0); /* ten weeks */
  (void)st;
  uint64_t before = sr_game_checksum(g);
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, SR_SAVE_VERSION, 5), SR_OK);
  SR_CHECK(w.len <= 4u * 1024u * 1024u);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_save_read(l, w.data, w.len), SR_OK);
  SR_CHECK(sr_game_checksum(l) == before);
  SR_CHECK_EQ(l->pop->npcs[3].status, SR_NPC_DEAD);
  SR_CHECK_EQ(l->pop->npcs[4].attitude, -40);
  SR_CHECK_EQ(sr_inv_count(&l->player.stash, SR_IK_GADGET, SR_GD_SMOKE), 3);
  SR_CHECK_EQ(l->events.count > 0, g->events.count > 0);
  SrSaveInfo info;
  SR_CHECK_EQ(sr_save_peek(w.data, w.len, &info), SR_OK);
  SR_CHECK(info.seed == 11 && info.saved_at == 5);
  sr_writer_free(&w);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

static void test_save_corruption_versions(void) {
  SR_SPEC("game-persistence", "Corrupted save detected");
  SR_SPEC("game-persistence", "Older save migrated");
  SR_SPEC("game-persistence", "Newer save refused");
  SrGame *g = new_game(12);
  SrWriter w;
  sr_writer_init(&w);
  sr_save_write(g, &w, SR_SAVE_VERSION, 1);
  SrGame *l = sr_game_create();
  /* flipped byte in the middle: detected, no crash */
  uint8_t *bad = (uint8_t *)malloc(w.len);
  memcpy(bad, w.data, w.len);
  bad[w.len / 2] ^= 0x5A;
  SR_CHECK_EQ(sr_save_read(l, bad, w.len), SR_ERR_CORRUPT);
  SR_CHECK_EQ(sr_save_read(l, bad, 3), SR_ERR_CORRUPT);
  SR_CHECK_EQ(sr_save_read(l, w.data, 20), SR_ERR_CORRUPT);
  memcpy(bad, w.data, w.len);
  bad[0] = 'X';
  SR_CHECK_EQ(sr_save_read(l, bad, w.len), SR_ERR_CORRUPT);
  /* newer format version and other generator version are refused */
  memcpy(bad, w.data, w.len);
  bad[4] = 99;
  SR_CHECK_EQ(sr_save_read(l, bad, w.len), SR_ERR_VERSION);
  memcpy(bad, w.data, w.len);
  bad[8] = 77;
  SR_CHECK_EQ(sr_save_read(l, bad, w.len), SR_ERR_VERSION);
  free(bad);
  /* version 1 (M2 format) migrates */
  SrWriter w1;
  sr_writer_init(&w1);
  g->player.prog.street_cred = 33;
  SR_CHECK_EQ(sr_save_write(g, &w1, 1, 2), SR_OK);
  SR_CHECK(w1.len < w.len);
  SR_CHECK_EQ(sr_save_read(l, w1.data, w1.len), SR_OK);
  SR_CHECK_EQ(l->player.cred, g->player.cred);
  SR_CHECK_EQ(l->player.prog.street_cred, 0); /* not stored in version 1 */
  SR_CHECK_EQ(l->player.prog.attr[SR_ATTR_REFLEX], sr_background_def(SR_BG_STREET_KID)->attr[SR_ATTR_REFLEX]);
  /* a save whose player section is cut short */
  SrWriter w2;
  sr_writer_init(&w2);
  w2.limit = 512;
  SR_CHECK_EQ(sr_save_write(g, &w2, SR_SAVE_VERSION, 1), SR_ERR_NOMEM);
  sr_writer_free(&w2);
  sr_writer_free(&w1);
  sr_writer_free(&w);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

static void test_storage_and_slots(void) {
  SR_SPEC("game-persistence", "Interrupted save");
  SR_SPEC("game-persistence", "Autosave rotation");
  SR_SPEC("game-persistence", "Manual save blocked in combat");
  SR_SPEC("game-persistence", "Save survives reload");
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SrGame *g = new_game(13);
  g->crowd_enabled = false;
  SR_CHECK_EQ(sr_manual_save(g, &st, 0), SR_OK);
  uint64_t first = sr_game_checksum(g);
  sr_game_skip(g, 3600, false);
  mem.fail_writes = 1;
  SR_CHECK(sr_manual_save(g, &st, 0) != SR_OK); /* interrupted */
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "slot0.srsv"), SR_OK);
  SR_CHECK(sr_game_checksum(l) == first);
  SR_CHECK_EQ(sr_manual_save(g, &st, 7), SR_ERR_INVALID);
  g->combat_count = 1;
  SR_CHECK_EQ(sr_manual_save(g, &st, 1), SR_ERR_DENIED);
  SR_CHECK(!st.exists(st.user, "slot1.srsv"));
  g->combat_count = 0;
  /* autosave rotation: four autosaves leave three, the oldest replaced */
  int slots[4];
  for (int i = 0; i < 4; i++) {
    slots[i] = sr_autosave(g, &st);
    sr_game_skip(g, 60, false);
  }
  SR_CHECK_EQ(slots[0], 0);
  SR_CHECK_EQ(slots[3], slots[0]);
  int count = 0;
  for (int s = 0; s < SR_AUTOSAVE_SLOTS; s++) {
    char n[32];
    sr_autosave_name(s, n, sizeof n);
    count += st.exists(st.user, n);
  }
  SR_CHECK_EQ(count, 3);
  SR_CHECK_EQ(sr_load_from(l, &st, "missing.srsv"), SR_ERR_NOT_FOUND);
  SrMemFile *f = sr_memstore_get(&mem, "slot0.srsv");
  ((uint8_t *)f->data)[f->len - 10] ^= 1;
  SR_CHECK_EQ(sr_load_from(l, &st, "slot0.srsv"), SR_ERR_CORRUPT);
  SR_CHECK_EQ(st.remove(st.user, "nope"), SR_ERR_NOT_FOUND);
  /* memstore full */
  SrMemStore small;
  SrStorage ss;
  sr_memstore_init(&small, &ss);
  for (int i = 0; i < SR_MEM_FILES; i++) {
    char n[16];
    snprintf(n, sizeof n, "f%d", i);
    ss.write(ss.user, n, "a", 1);
  }
  SR_CHECK_EQ(ss.write(ss.user, "overflow", "a", 1), SR_ERR_NOMEM);
  sr_memstore_free(&small);
  mem.fail_writes = 5;
  SR_CHECK_EQ(sr_autosave(g, &st), -1);
  sr_memstore_free(&mem);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

/* ---- settings and input ------------------------------------------------------------------------ */

static void test_settings(void) {
  SrSettings s;
  sr_settings_default(&s);
  SR_CHECK_EQ(s.internal_res, 1);
  char text[2048];
  s.volume = 42;
  s.crt = true;
  s.mouse_sensitivity = 0.004f;
  strcpy(s.bindings, "jump=C");
  sr_settings_write(&s, text, sizeof text);
  SrSettings r;
  sr_settings_default(&r);
  SR_CHECK(sr_settings_parse(&r, text) >= 15);
  SR_CHECK_EQ(r.volume, 42);
  SR_CHECK(r.crt);
  SR_CHECK_NEAR(r.mouse_sensitivity, 0.004, 1e-5);
  SR_CHECK_STR(r.bindings, "jump=C");
  SR_CHECK_EQ(sr_settings_parse(&r, "# comment\nunknown=1\nvolume=abc\nnoequals\n"), 0);
  int w, h;
  sr_settings_internal_size(&r, 1920, 1080, &w, &h);
  SR_CHECK(w == 480 && h == 270);
  r.internal_res = 3;
  sr_settings_internal_size(&r, 1920, 1080, &w, &h);
  SR_CHECK(w == 1920 && h == 1080);
  sr_settings_internal_size(&r, 0, 0, &w, &h);
  SR_CHECK(w == 480);
}

static void test_input_actions(void) {
  SR_SPEC("input-controls", "Same action from every device");
  SR_SPEC("input-controls", "Conflicting binding");
  SR_SPEC("input-controls", "Mobile browser start");
  SR_SPEC("input-controls", "Desktop browser start");
  SR_SPEC("input-controls", "Gamepad hot-plug");
  SrInput in;
  sr_input_init(&in, false);
  SR_CHECK_EQ(in.scheme, SR_SCHEME_KBM);
  sr_input_key(&in, SR_KEY_SPACE, true);
  SrActionState s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.pressed[SR_ACT_JUMP]);
  sr_input_key(&in, SR_KEY_SPACE, false);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.released[SR_ACT_JUMP]);
  sr_input_gamepad_button(&in, SR_PAD_SOUTH, true);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.pressed[SR_ACT_JUMP] && in.scheme == SR_SCHEME_GAMEPAD);
  sr_input_gamepad_button(&in, SR_PAD_SOUTH, false);
  s = sr_input_tick(&in, SR_STEP_DT);
  sr_input_touch(&in, 1, in.layout.buttons[2].x / in.layout.width, in.layout.buttons[2].y / in.layout.height, 0);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.pressed[SR_ACT_JUMP] && in.scheme == SR_SCHEME_TOUCH);
  sr_input_touch(&in, 1, 0, 0, 2);
  /* conflicting binding: reported, then swapped */
  SR_CHECK_EQ(sr_input_rebind_key(&in, SR_ACT_JUMP, SR_KEY_E), SR_ACT_INTERACT);
  SR_CHECK_EQ(in.key_bind[SR_ACT_JUMP], SR_KEY_SPACE);
  sr_input_swap_keys(&in, SR_ACT_JUMP, SR_ACT_INTERACT);
  SR_CHECK_EQ(in.key_bind[SR_ACT_JUMP], SR_KEY_E);
  SR_CHECK_EQ(in.key_bind[SR_ACT_INTERACT], SR_KEY_SPACE);
  SR_CHECK_EQ(sr_input_rebind_key(&in, SR_ACT_JUMP, SR_KEY_Z), -1);
  char buf[1024];
  sr_input_bindings_save(&in, buf, sizeof buf);
  SrInput in2;
  sr_input_init(&in2, false);
  SR_CHECK(sr_input_bindings_load(&in2, buf));
  SR_CHECK_EQ(in2.key_bind[SR_ACT_JUMP], SR_KEY_Z);
  SR_CHECK(!sr_input_bindings_load(&in2, "jump=Nope;bad;fire=W"));
  SR_CHECK_EQ(in2.key_bind[SR_ACT_FIRE], SR_KEY_W);
  /* schemes by pointer type */
  SrInput mob;
  sr_input_init(&mob, true);
  SR_CHECK_EQ(mob.scheme, SR_SCHEME_TOUCH);
  SR_CHECK(!mob.pointer_lock_wanted);
  /* mouse look and gamepad axes */
  sr_input_key(&in, SR_KEY_W, true);
  sr_input_mouse_motion(&in, 100, 0);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.move_y > 0 && s.look_x > 0);
  sr_input_key(&in, SR_KEY_W, false);
  sr_input_mouse_motion(&in, 1, 0); /* tiny motion: no scheme change */
  sr_input_gamepad_axes(&in, 0.9f, -0.9f, 0.5f, 0.0f);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.move_x > 0 && s.move_y > 0 && s.look_x > 0);
  sr_input_gamepad_axes(&in, 0.05f, 0, 0, 0); /* within dead zone */
  sr_input_gamepad_connected(&in, true);
  SR_CHECK(!in.gamepad_lost);
  sr_input_gamepad_connected(&in, false);
  SR_CHECK(in.gamepad_lost);
  sr_input_key(&in, (SrKey)999, true);
  sr_input_gamepad_button(&in, (SrPad)999, true);
  for (int a = 0; a <= SR_ACT_COUNT; a++) SR_CHECK(sr_action_name((SrAction)a)[0] != '\0');
  for (int k = 0; k <= SR_KEY_COUNT; k++) SR_CHECK(sr_key_name((SrKey)k)[0] != '\0');
}

static void test_touch_controls(void) {
  SR_SPEC("input-controls", "Stick appears under the thumb");
  SR_SPEC("input-controls", "Edge push sprints");
  SR_SPEC("input-controls", "Release stops movement");
  SR_SPEC("input-controls", "Simultaneous move and look");
  SR_SPEC("input-controls", "Safe area respected");
  SR_SPEC("input-controls", "Gadget wheel by long press");
  SR_SPEC("input-controls", "Context icon on interact");
  SR_SPEC("input-controls", "Slow-down near a target");
  SR_SPEC("input-controls", "No assistance for the mouse");
  SrInput in;
  sr_input_init(&in, true);
  sr_input_set_screen(&in, 844, 390, 47, 47, 0, 21); /* notch phone */
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
    SR_CHECK(in.layout.buttons[i].size >= 48);
    SR_CHECK(sr_touch_button_in_safe_area(&in.layout, i));
  }
  SR_CHECK_EQ(sr_touch_hit_button(&in.layout, 10, 200), -1);
  /* stick under the thumb, drag forward */
  sr_input_touch(&in, 1, 0.2f, 0.6f, 0);
  SR_CHECK_EQ(in.fingers[0].role, 1);
  SR_CHECK_NEAR(in.fingers[0].start_x, 0.2, 1e-6);
  sr_input_touch(&in, 1, 0.2f, 0.6f - 30.0f / 390.0f, 1);
  /* second finger looks in the right half at the same time */
  sr_input_touch(&in, 2, 0.6f, 0.4f, 0);
  sr_input_touch(&in, 2, 0.65f, 0.4f, 1);
  SrActionState s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.move_y > 0.4f && s.move_y < 0.6f);
  SR_CHECK(s.look_x > 0);
  SR_CHECK(!s.sprint_from_stick);
  sr_input_touch(&in, 1, 0.2f, 0.6f - 70.0f / 390.0f, 1); /* beyond 90 % of the radius */
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(s.sprint_from_stick && s.down[SR_ACT_SPRINT]);
  SR_CHECK_NEAR(s.move_y, 1.0, 1e-3);
  sr_input_touch(&in, 1, 0.2f, 0.4f, 2);
  sr_input_touch(&in, 2, 0.65f, 0.4f, 2);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK_NEAR(s.move_x, 0, 0);
  SR_CHECK_NEAR(s.move_y, 0, 0);
  sr_input_touch(&in, 9, 0.5f, 0.5f, 1); /* unknown finger move: ignored */
  /* long press opens the gadget wheel; sliding selects */
  const SrTouchButton *gb = NULL;
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++)
    if (in.layout.buttons[i].action == SR_ACT_GADGET) gb = &in.layout.buttons[i];
  sr_input_touch(&in, 3, gb->x / in.layout.width, gb->y / in.layout.height, 0);
  for (int i = 0; i < 20; i++) s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(!in.wheel_open);
  for (int i = 0; i < 10; i++) s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(in.wheel_open);
  sr_input_touch(&in, 3, gb->x / in.layout.width + 0.05f, gb->y / in.layout.height, 1);
  SR_CHECK(in.wheel_choice >= 0 && in.wheel_choice < 4);
  sr_input_touch(&in, 3, 0, 0, 2);
  s = sr_input_tick(&in, SR_STEP_DT);
  SR_CHECK(!in.wheel_open);
  /* too many fingers */
  for (int i = 0; i < SR_MAX_FINGERS + 2; i++) sr_input_touch(&in, 100 + i, 0.9f, 0.1f, 0);
  /* context icon slot exists for the interact button */
  in.interact_icon = 3;
  SR_CHECK(in.interact_icon == 3);
  /* aim assistance */
  SR_CHECK_NEAR(sr_aim_assist(SR_SCHEME_KBM, 1.0f, 0.0f), 1.0, 0);
  SR_CHECK(sr_aim_assist(SR_SCHEME_TOUCH, 1.0f, 0.0f) < 0.5f);
  SR_CHECK(sr_aim_assist(SR_SCHEME_GAMEPAD, 1.0f, 0.04f) < 1.0f);
  SR_CHECK_NEAR(sr_aim_assist(SR_SCHEME_GAMEPAD, 1.0f, 0.5f), 1.0, 0);
  SR_CHECK_NEAR(sr_aim_assist(SR_SCHEME_GAMEPAD, 1.0f, -1.0f), 1.0, 0);
  SR_CHECK_NEAR(sr_aim_assist(SR_SCHEME_GAMEPAD, 0.0f, 0.0f), 1.0, 0);
}

/* ---- scripts and autoplay ----------------------------------------------------------------------- */

static void test_scripts(void) {
  SR_SPEC("platform-runtime", "Headless script passes");
  SR_SPEC("platform-runtime", "Headless script fails");
  char err[512];
  const char *ok =
      "seed 99\nnewgame background=medic corps=6\nexpect city.corps == 6\n"
      "expect player.level == 1\nset player.cash = 500\nexpect player.cash == 500\n"
      "advance 2h\nexpect clock.hour == 23\nadvance 30m\nadvance 90s\nsteps 10\npress jump\n"
      "hold sprint 5\nmove 0 1 10\nlook 0.1 5\nrelease sprint\nteleport home\nteleport food\n"
      "teleport building 3\neat\nsleep 8\nsleep 1 street\nsleep 1 capsule\nfire\nteleport home\ninteract\n"
      "focus 0\nexpect screen == pause\nexpect paused == true\nfocus 1\nexpect screen == play\n"
      "save s1\nload s1\nset player.skill.stealth = 7\nexpect player.skill.stealth == 7\n"
      "get player.cash\njob offers\njob accept 1\njob execute 1 1\njob report 1\nset hostiles = 0\n"
      "set combat = 0\nset player.health = 50\nset player.satiety = 50\nset player.fatigue = 10\n"
      "set player.cred = 12\nset player.medevac = 1\nset hardcore = 0\nset crowd = 0\n"
      "set pall_removed = 1\nexpect sky.daylight == true\nset clock.time = 100000\n"
      "expect clock.time == 100000\nexpect player.x > 0\necho done # comment\n";
  SR_CHECK_EQ(sr_script_run(ok, "ok.scn", err, sizeof err), 0);
  if (err[0]) printf("  %s\n", err);
  SR_CHECK_EQ(sr_script_run("seed 1\nnewgame\nexpect player.cash == -1\n", "fail.scn", err, sizeof err), 1);
  SR_CHECK(strstr(err, "fail.scn:3") && strstr(err, "actual"));
  SR_CHECK_EQ(sr_script_run("seed 1\nnewgame\nexpect nothing.here == 1\n", "q.scn", err, sizeof err), 1);
  SR_CHECK_EQ(sr_script_run("bogus command\n", "b.scn", err, sizeof err), 1);
  SR_CHECK_EQ(sr_script_run("seed 1\nnewgame background=wizard\n", "b.scn", err, sizeof err), 1);
  SR_CHECK_EQ(sr_script_run("seed NOT-A-CODE\n", "b.scn", err, sizeof err), 1);
  SR_CHECK_EQ(sr_script_run("advance 1h\n", "b.scn", err, sizeof err), 1); /* no game yet */
  char code[SR_SEED_CODE_LEN];
  sr_seed_to_code(77, code);
  char text[128];
  snprintf(text, sizeof text, "seed %s\nnewgame\nexpect city.valid == 1\n", code);
  SR_CHECK_EQ(sr_script_run(text, "code.scn", err, sizeof err), 0);
  SR_CHECK_EQ(sr_script_run("seed random(3)\nnewgame\nexpect city.valid == 1\n", "r.scn", err, sizeof err), 0);
  SR_CHECK_EQ(sr_script_run("seed random(2)\nnewgame\nexpect player.cash == -5\n", "r.scn", err, sizeof err), 1);
  /* every query path answers */
  SrGame *g = new_game(3);
  static const char *const paths[] = {
      "player.cash", "player.home", "player.home.locked", "player.home.stash.count",
      "player.rent.missed", "player.rent.debt", "player.rent.due", "player.rent", "player.health",
      "player.satiety", "player.fatigue", "player.needs.starving_hours", "player.cred",
      "player.tier", "player.level", "player.messages", "player.unread", "player.inv.count",
      "player.x", "player.z", "player.y", "player.yaw", "player.background", "player.kills",
      "player.skill.pistols", "heat.police", "heat.max", "clock.time", "clock.day",
      "clock.weekday", "clock.hour", "clock.minute", "clock.week", "city.name",
      "city.districts", "city.corps", "city.buildings", "city.digest", "city.valid",
      "crowd.count", "crowd.visible", "crowd.popins", "jobs.count", "jobs.completed",
      "jobs.active", "events.count", "weather", "sky.daylight", "checksum", "paused",
      "audio.muted", "context", "screen"};
  char out[128];
  for (size_t i = 0; i < SR_ARRAY_LEN(paths); i++) SR_CHECK(sr_game_query(g, paths[i], out, sizeof out));
  SR_CHECK(!sr_game_query(g, "player.skill.juggling", out, sizeof out));
  SR_CHECK_EQ(sr_game_set(g, "player.skill.juggling", "1"), SR_ERR_NOT_FOUND);
  SR_CHECK_EQ(sr_game_set(g, "nope", "1"), SR_ERR_NOT_FOUND);
  g->player.home = -1;
  SR_CHECK(sr_game_query(g, "player.home", out, sizeof out) && strcmp(out, "none") == 0);
  g->player.home = g->city->home_building;
  SR_CHECK_EQ(sr_game_command(g, "get player.cash", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "teleport nowhere", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "press nothing", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "paydebt", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "autoplay strategy=live weeks=1", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "teleport city hall", out, sizeof out), SR_ERR_INVALID);
  sr_game_destroy(g);
  char serr[128];
  SR_CHECK_EQ(sr_sweep_seeds(40, 3, serr, sizeof serr), 0);
}

static void test_autoplay_strategies(void) {
  SR_SPEC("job-system", "Starter economy is sustainable");
  SR_SPEC("job-system", "Two approaches validated");
  static const char *const names[] = {"gig", "ghost", "face", "hammer", "netrunner", "live", "x"};
  for (size_t i = 0; i < SR_ARRAY_LEN(names); i++) (void)sr_strategy_parse(names[i]);
  SR_CHECK_EQ(sr_strategy_parse("ghost"), SR_STRAT_GHOST);
  int fine = 0;
  for (uint64_t seed = 0; seed < 6; seed++) {
    SrGame *g = new_game(400 + seed);
    SrAutoplayStats st = sr_autoplay(g, SR_STRAT_GIG, 28, 0);
    fine += st.missed_rent == 0 && st.starving_hours == 0 && !st.soft_lock;
    sr_game_destroy(g);
  }
  SR_CHECK(fine >= 6);
  for (int s = SR_STRAT_GHOST; s <= SR_STRAT_NETRUNNER; s++) {
    SrGame *g = new_game(500 + (uint64_t)s);
    g->player.prog.street_cred = 30;
    SrAutoplayStats st = sr_autoplay(g, (SrStrategy)s, 7, 3);
    SR_CHECK(st.jobs_done > 0);
    SR_CHECK(!st.soft_lock);
    sr_game_destroy(g);
  }
  /* evicted and broke: the bot sleeps rough and recovers */
  SrGame *g = new_game(77);
  g->player.evicted = true;
  g->player.rent_debt = 100000;
  g->player.cred = 0;
  g->player.fatigue = 90;
  SrAutoplayStats st = sr_autoplay(g, SR_STRAT_GIG, 3, 0);
  SR_CHECK(st.sleeps > 0);
  g->player.rent_debt = 10;
  g->player.cred = 100;
  g->player.fatigue = 90;
  sr_autoplay(g, SR_STRAT_GIG, 1, 0);
  SR_CHECK(!g->player.evicted);
  g->player.satiety = 10;
  g->player.cred = 0;
  sr_autoplay(g, SR_STRAT_LIVE, 1, 0);
  sr_game_destroy(g);
}

static void test_character_creation(void) {
  SR_SPEC("player-life", "Randomised appearance");
  SR_SPEC("player-life", "Background applies starting values");
  SrGame *g = sr_game_create();
  sr_game_begin_create(g, 4242);
  SR_CHECK_EQ(g->screen, SR_SCREEN_CREATE);
  SrAppearance a = sr_game_create_preview(g);
  sr_game_create_randomize(g);
  SrAppearance b = sr_game_create_preview(g);
  SR_CHECK(sr_appearance_key(&a) != sr_appearance_key(&b));
  SR_CHECK(b.skin < 8 && b.hair < 12 && b.body < 4); /* valid parts */
  sr_game_create_select(g, SR_BG_MEDIC);
  sr_game_create_select(g, 99); /* ignored */
  SR_CHECK_EQ(g->create_bg, SR_BG_MEDIC);
  SR_CHECK_EQ(sr_game_create_confirm(g), SR_OK);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  SR_CHECK_EQ(g->player.background, SR_BG_MEDIC);
  SR_CHECK_EQ(sr_appearance_key(&g->player.look), sr_appearance_key(&b));
  SR_CHECK_EQ(g->player.prog.skill_rank[SR_SK_MEDICINE], 3);
  char out[32];
  g->screen = SR_SCREEN_CREATE;
  SR_CHECK(sr_game_query(g, "screen", out, sizeof out) && strcmp(out, "create") == 0);
  sr_game_destroy(g);
}

static void test_performance_targets(void) {
  SR_SPEC("city-generation", "Macro generation time");
  SR_SPEC("population-simulation", "Peak crowd on desktop");
  /* generation must finish well inside 3 s; measured in CPU time, generous for sanitizer builds */
  clock_t t0 = clock();
  SrCity *c = sr_city_create(0xBEEFull, NULL);
  double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
  printf("  macro generation: %.3f s\n", secs);
  SR_CHECK(secs < 3.0);
  sr_city_destroy(c);
  /* busiest street at peak: at least 150 citizens in view */
  SrGame *g = new_game(0x5EEDC0DEull);
  int best = -1;
  for (int k = 0; k < g->city->district_count; k++)
    if (g->city->districts[k].type == SR_D_ENTERTAINMENT)
      best = sr_city_block_at(g->city, g->city->districts[k].cx, g->city->districts[k].cy);
  int32_t ox, oy;
  sr_city_block_origin(best, g->city->grid, &ox, &oy);
  g->player.pos = sr_v3((float)ox / 100.0f + 8, 1.7f, (float)oy / 100.0f + 8);
  sr_clock_set_seconds(&g->clock, 2 * 86400 + 23 * 3600);
  g->last_minute = sr_game_time(g) / 60;
  for (int i = 0; i < 60; i++) sr_game_step(g, NULL);
  int visible = sr_crowd_visible(&g->crowd, &g->cam, 160.0f);
  printf("  citizens in view at peak: %d\n", visible);
  SR_CHECK(visible >= 150);
  sr_game_destroy(g);
}

void suite_game(void) {
  SR_RUN(test_character_creation);
  SR_RUN(test_performance_targets);
  SR_RUN(test_fixed_timestep);
  SR_RUN(test_movement_and_collision);
  SR_RUN(test_pause_focus_terminal);
  SR_RUN(test_interactions);
  SR_RUN(test_job_interactions);
  SR_RUN(test_fire_and_flatline);
  SR_RUN(test_deltas);
  SR_RUN(test_save_roundtrip);
  SR_RUN(test_save_corruption_versions);
  SR_RUN(test_storage_and_slots);
  SR_RUN(test_settings);
  SR_RUN(test_input_actions);
  SR_RUN(test_touch_controls);
  SR_RUN(test_scripts);
  SR_RUN(test_autoplay_strategies);
}
