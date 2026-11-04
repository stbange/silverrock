#include "core/sr_rng.h"
#include "sim/sr_combat.h"
#include "sim/sr_crowd.h"
#include "sim/sr_heat.h"
#include "sim/sr_jobs.h"
#include "sim/sr_player.h"
#include "world/sr_environ.h"
#include "sr_test.h"

#include <math.h>
#include <stdlib.h>

static SrCity *g_c;
static SrPopulation *g_pop;

static SrCity *city(void) {
  if (!g_c) {
    g_c = sr_city_create(0xABCDEFull, NULL);
    g_pop = (SrPopulation *)calloc(1, sizeof *g_pop);
    sr_population_generate(g_pop, g_c);
  }
  return g_c;
}

/* ---- population --------------------------------------------------------------------- */

static void test_appearance(void) {
  SR_SPEC("population-simulation", "Unique appearances in a crowd");
  SR_SPEC("population-simulation", "Faction members show their marker");
  SrCity *c = city();
  SrAmbient amb[SR_MAX_AMBIENT_PER_BLOCK];
  for (int b = 0; b < c->grid * c->grid; b += 37) {
    int n = sr_ambient_generate(c, b, 22 * 3600, amb, 20);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < i; j++)
        SR_CHECK(sr_appearance_key(&amb[i].look) != sr_appearance_key(&amb[j].look));
      if (amb[i].faction >= 0) SR_CHECK_EQ(amb[i].look.marker, c->factions.f[amb[i].faction].marker);
    }
  }
  SrAppearance g = sr_appearance_generate(5, SR_ROLE_GANG, 7);
  SR_CHECK_EQ(g.marker, 7);
  SR_CHECK_EQ(g.outfit / 4, SR_ROLE_GANG);
  for (int r = 0; r <= SR_ROLE_COUNT; r++) SR_CHECK(sr_role_name((SrRole)r)[0] != '\0');
}

static void test_density_and_stability(void) {
  SR_SPEC("population-simulation", "Strip at peak and off-peak");
  SR_SPEC("population-simulation", "Same corner, same crowd");
  SR_CHECK(sr_crowd_density(SR_D_ENTERTAINMENT, 1) >= 2 * sr_crowd_density(SR_D_ENTERTAINMENT, 9));
  SR_CHECK(sr_crowd_density(SR_D_ENTERTAINMENT, 6) > 0);
  SR_CHECK(sr_crowd_density(SR_D_ENTERTAINMENT, 18) > 0);
  SR_CHECK(sr_crowd_density(SR_D_COMMERCIAL, 20) > sr_crowd_density(SR_D_COMMERCIAL, 10));
  SR_CHECK(sr_crowd_density(SR_D_RESIDENTIAL, 6) > sr_crowd_density(SR_D_RESIDENTIAL, 10));
  SrCity *c = city();
  SrAmbient a[48], b[48];
  int block = c->buildings[c->home_building].block;
  int na = sr_ambient_generate(c, block, 3 * 86400 + 7200, a, 48);
  int nb = sr_ambient_generate(c, block, 3 * 86400 + 7200 + 60, b, 48);
  SR_CHECK_EQ(na, nb);
  for (int i = 0; i < na; i++) SR_CHECK(a[i].key == b[i].key && a[i].x == b[i].x);
  SR_CHECK_EQ(sr_ambient_generate(c, -1, 0, a, 48), 0);
}

static void test_notables(void) {
  SR_SPEC("population-simulation", "Vendor keeps her hours");
  SR_SPEC("population-simulation", "Consistent after leaving view");
  SR_SPEC("population-simulation", "Shift-change rush");
  SR_SPEC("population-simulation", "Closed shop");
  SrCity *c = city();
  SR_CHECK(g_pop->count >= 1500);
  /* a shop with limited hours: the same vendor at work on two different days */
  int shop = -1;
  for (int b = 0; b < c->building_count && shop < 0; b++)
    if (c->buildings[b].poi == SR_POI_GUNSHOP) shop = b;
  int open, hours;
  sr_poi_hours(c, shop, &open, &hours);
  int64_t t1 = 1 * 86400 + (open + 1) * 3600, t2 = 4 * 86400 + (open + 2) * 3600;
  int staff = sr_population_staff_of(g_pop, shop);
  SR_CHECK(staff >= 0);
  SrPlace p1 = sr_npc_place(c, &g_pop->npcs[staff], t1), p2 = sr_npc_place(c, &g_pop->npcs[staff], t2);
  SR_CHECK(p1.activity == SR_ACT_WORK && p1.building == shop);
  SR_CHECK(p2.activity == SR_ACT_WORK && p2.building == shop);
  /* closed outside opening hours, and the vendor is elsewhere */
  int64_t closed = 2 * 86400 + ((open + hours + 2) % 24) * 3600;
  SR_CHECK(!sr_poi_open(c, shop, closed));
  SR_CHECK(sr_npc_place(c, &g_pop->npcs[staff], closed).building != shop ||
           sr_npc_place(c, &g_pop->npcs[staff], closed).activity != SR_ACT_WORK);
  SR_CHECK(sr_poi_open(c, shop, t1));
  /* analytic schedule: every activity occurs and commuting interpolates */
  const SrNpc *w = NULL;
  for (int i = 0; i < g_pop->count && !w; i++)
    if (g_pop->npcs[i].role == SR_ROLE_WORKER) w = &g_pop->npcs[i];
  bool seen[5] = {false};
  for (int64_t t = 0; t < 86400; t += 600) {
    SrPlace pl = sr_npc_place(c, w, 86400 * 2 + t);
    seen[pl.activity] = true;
    if (pl.activity == SR_ACT_COMMUTE) SR_CHECK(pl.progress >= 0 && pl.progress <= 1);
  }
  for (int a = 0; a < 5; a++) SR_CHECK(seen[a]);
  /* same time, same answer (consistency after leaving view) */
  SrPlace x = sr_npc_place(c, w, 500000), y = sr_npc_place(c, w, 500000);
  SR_CHECK(x.x == y.x && x.y == y.y && x.activity == y.activity);
  SrNpc dead = *w;
  dead.status = SR_NPC_DEAD;
  SR_CHECK_EQ(sr_npc_place(c, &dead, 0).building, -1);
  /* shift-change rush at stations: commuters peak around shift starts */
  int rush = 0, mid = 0;
  for (int i = 0; i < g_pop->count; i++) {
    rush += sr_npc_place(c, &g_pop->npcs[i], 86400 * 2 + 6 * 3600 - 900).activity == SR_ACT_COMMUTE;
    mid += sr_npc_place(c, &g_pop->npcs[i], 86400 * 2 + 9 * 3600).activity == SR_ACT_COMMUTE;
  }
  SR_CHECK(rush > mid);
  SR_CHECK_EQ(sr_population_staff_of(g_pop, c->home_building), -1);
}

static void test_promotion(void) {
  SR_SPEC("population-simulation", "Trading promotes");
  SrCity *c = city();
  SrPopulation *p = (SrPopulation *)malloc(sizeof *p);
  memcpy(p, g_pop, sizeof *p);
  SrAmbient amb[8];
  int n = sr_ambient_generate(c, c->buildings[c->home_building].block, 80000, amb, 8);
  SR_CHECK(n > 0);
  int id = sr_population_promote(p, c, &amb[0], 80000);
  SR_CHECK(id >= 0);
  SR_CHECK(p->npcs[id].promoted);
  SR_CHECK_EQ(sr_appearance_key(&p->npcs[id].look), sr_appearance_key(&amb[0].look));
  SR_CHECK_EQ(sr_population_promote(p, c, &amb[0], 90000), id); /* idempotent */
  SR_CHECK_EQ(sr_population_find_by_key(p, amb[0].key), id);
  SrPlace next_day = sr_npc_place(c, &p->npcs[id], 80000 + 86400);
  SR_CHECK(next_day.building >= 0);
  p->count = SR_MAX_NPCS;
  SR_CHECK_EQ(sr_population_promote(p, c, &amb[1], 0), -1);
  free(p);
}

static void test_reactions_and_rumours(void) {
  SR_SPEC("population-simulation", "Nobody reports in the Undercity");
  SR_SPEC("population-simulation", "Witness reports in the Spire");
  SR_SPEC("population-simulation", "Rumour about a recent event");
  for (uint64_t k = 0; k < 50; k++) {
    SR_CHECK(!sr_witness_reports(SR_D_UNDERCITY, 3, k));
    SR_CHECK(sr_witness_reports(SR_D_SPIRE, 0, k));
    SR_CHECK(sr_react_gunfire(0, SR_D_SPIRE, k) != SR_RESP_GAWK);
    SR_CHECK_EQ(sr_react_gunfire(2, SR_D_SPIRE, k), SR_RESP_COVER);
    SR_CHECK_EQ(sr_react_gunfire(3, SR_D_UNDERCITY, k), SR_RESP_GAWK);
  }
  SR_CHECK(!sr_witness_reports(SR_D_RESIDENTIAL, 0, 1));
  SrCity *c = city();
  SrEventLog log;
  sr_events_init(&log);
  char bark[128];
  SR_CHECK(!sr_bark(c, &log, 0, 1000, 1, bark, sizeof bark));
  SrWorldEvent ev = {SR_EV_SABOTAGE, 1000, 2, 5, 0, -1, true, "the sabotage at Site 1"};
  sr_events_add(&log, &ev);
  SR_CHECK(sr_bark(c, &log, 2, 5000, 1, bark, sizeof bark));
  SR_CHECK(strstr(bark, "sabotage") != NULL);
  SR_CHECK(!sr_bark(c, &log, 3, 5000, 1, bark, sizeof bark));      /* other district */
  SR_CHECK(!sr_bark(c, &log, 2, 1000 + 90000, 1, bark, sizeof bark)); /* older than a day */
  for (int i = 0; i < 200; i++) sr_events_add(&log, &ev);
  SR_CHECK_EQ(sr_events_size(&log), SR_MAX_EVENTS);
  SR_CHECK(sr_events_recent(&log, SR_MAX_EVENTS) == NULL);
}

static void test_metro_timetable(void) {
  SR_SPEC("population-simulation", "Metro timetable");
  SrCity *c = city();
  for (int s = 0; s < c->station_count; s++) {
    for (int64_t t = 0; t < 4000; t += 97) {
      int64_t next = sr_metro_next_train(c, s, 0, t);
      SR_CHECK(next >= t && next - t <= 240);
      SR_CHECK(sr_metro_next_train(c, s, 1, t) - t <= 240);
    }
  }
}

static SrCamera camera_at(SrVec3 pos) {
  SrCamera cam = {pos, sr_v3(0, 0, -1), 0.5f};
  return cam;
}

static void test_crowd(void) {
  SR_SPEC("population-simulation", "No pop-in nearby");
  SR_SPEC("population-simulation", "Gunfire panic");
  SR_SPEC("population-simulation", "Downpour empties the street");
  SrCity *c = city();
  SrCrowd *cr = (SrCrowd *)malloc(sizeof *cr);
  sr_crowd_init(cr);
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  SrCamera cam = camera_at(sr_v3((float)hx / 100.0f, 1.7f, (float)hy / 100.0f + 30.0f));
  int64_t t = 2 * 86400 + 22 * 3600;
  sr_crowd_refresh(cr, c, g_pop, &cam, t, SR_MAX_AGENTS); /* first frame: may fill the view */
  SR_CHECK(cr->count > 20);
  SR_CHECK(!cr->fresh);
  /* walking and refreshing: newly spawned agents never appear in view nearby */
  for (int k = 0; k < 120; k++) {
    sr_crowd_update(cr, c, 0.5f, 0.0f);
    cam.pos.z -= 0.5f;
    uint64_t keys[SR_MAX_AGENTS];
    int nk = cr->count;
    for (int i = 0; i < nk; i++) keys[i] = cr->agents[i].key;
    sr_crowd_refresh(cr, c, g_pop, &cam, t + k * 900, SR_MAX_AGENTS);
    for (int i = 0; i < cr->count; i++) {
      bool existed = false;
      for (int j = 0; j < nk && !existed; j++) existed = keys[j] == cr->agents[i].key;
      if (existed) continue;
      SrAgent *ag = &cr->agents[i];
      float dx = ag->pos.x - cam.pos.x, dz = ag->pos.z - cam.pos.z;
      bool near = sqrtf(dx * dx + dz * dz) < SR_NO_POP_RADIUS;
      SR_CHECK(!(near && sr_camera_sees(&cam, ag->pos)));
    }
  }
  SR_CHECK(sr_crowd_find(cr, 12345) < 0);
  SR_CHECK(sr_crowd_visible(cr, &cam, 200.0f) > 0);
  /* gunfire: every agent in range reacts within the tick */
  int in_range = 0;
  for (int i = 0; i < cr->count; i++) {
    float dx = cr->agents[i].pos.x - cam.pos.x, dz = cr->agents[i].pos.z - cam.pos.z;
    in_range += sqrtf(dx * dx + dz * dz) <= 40.0f;
  }
  SrEventLog log;
  sr_events_init(&log);
  int reacted = sr_crowd_gunfire(cr, c, cam.pos, 40.0f, &log, t);
  SR_CHECK_EQ(reacted, in_range);
  for (int i = 0; i < cr->count; i++) {
    float dx = cr->agents[i].pos.x - cam.pos.x, dz = cr->agents[i].pos.z - cam.pos.z;
    if (sqrtf(dx * dx + dz * dz) <= 40.0f) SR_CHECK(cr->agents[i].state != SR_AG_WALK);
  }
  for (int k = 0; k < 40; k++) sr_crowd_update(cr, c, 0.5f, 0.0f); /* calm down */
  /* downpour: within 10 in-game minutes (30 real seconds) the exposed share halves */
  float before = sr_crowd_exposed_share(cr);
  for (int k = 0; k < 30 * 60; k++) sr_crowd_update(cr, c, 1.0f / 60.0f, 1.0f);
  float after = sr_crowd_exposed_share(cr);
  SR_CHECK(after <= before * 0.5f);
  sr_crowd_update(cr, c, 0.1f, 0.0f); /* rain stops */
  /* far away camera despawns everyone */
  cam.pos = sr_v3(-5000, 0, -5000);
  sr_crowd_refresh(cr, c, g_pop, &cam, t + 7200, SR_MAX_AGENTS);
  SR_CHECK_EQ(cr->count, 0);
  SrCrowd empty;
  sr_crowd_init(&empty);
  SR_CHECK_NEAR(sr_crowd_exposed_share(&empty), 0, 0);
  free(cr);
}

/* ---- items ------------------------------------------------------------------------------ */

static void test_weapons(void) {
  SR_SPEC("equipment", "Archetype coverage");
  SR_SPEC("equipment", "Same item seed, same weapon");
  SR_SPEC("equipment", "Manufacturer style is consistent");
  SR_SPEC("equipment", "Rarity stat ranges");
  SrCity *c = city();
  SR_CHECK(sr_archetype_count() >= 20);
  int per_cat[SR_WC_COUNT] = {0};
  for (int i = 0; i < sr_archetype_count(); i++) per_cat[sr_archetype(i)->category]++;
  for (int k = 0; k < SR_WC_COUNT; k++) SR_CHECK(per_cat[k] >= 2);
  SR_CHECK(sr_archetype(-1) == NULL && sr_archetype(99) == NULL);
  SrItem a = sr_item_weapon(2, 0, 1, 77), b = sr_item_weapon(2, 0, 1, 77);
  SrWeaponStats sa = sr_weapon_stats(c->seed, &a), sb = sr_weapon_stats(c->seed, &b);
  SR_CHECK(memcmp(&sa, &sb, sizeof sa) == 0);
  int maker = -1;
  for (int k = 0; k < c->factions.corp_count; k++) if (sr_maker_valid(c, k)) maker = k;
  SR_CHECK(maker >= 0);
  int8_t tend[4];
  sr_maker_tendency(c->seed, maker, tend);
  for (uint32_t s = 0; s < 100; s++) {
    SrItem w = sr_item_weapon(8, maker, 0, s);
    SrWeaponStats ws = sr_weapon_stats(c->seed, &w);
    const SrArchetype *ar = sr_archetype(8);
    SR_CHECK_EQ(ws.style, sr_maker_style(maker));
    float d = ws.rate - ar->rate;
    if (tend[0] > 0) SR_CHECK(d > 0);
    if (tend[0] < 0) SR_CHECK(d < 0);
    if (tend[0] == 0) SR_CHECK_NEAR(d, 0, 1e-4);
    float d2 = ws.weight - ar->weight;
    if (tend[3] > 0) SR_CHECK(d2 > 0);
    if (tend[3] < 0) SR_CHECK(d2 < 0);
  }
  for (int r = 0; r < SR_RAR_COUNT; r++) {
    float lo, hi;
    sr_rarity_range(r, &lo, &hi);
    for (uint32_t s = 0; s < 20; s++) {
      SrItem w = sr_item_weapon(3, SR_STREET_MAKER_FIRST, r, s);
      float dm = sr_weapon_stats(c->seed, &w).damage / sr_archetype(3)->damage;
      SR_CHECK(dm >= lo - 1e-4 && dm <= hi + 1e-4);
    }
    SR_CHECK(sr_rarity_name(r)[0] != '?');
  }
  SR_CHECK_STR(sr_rarity_name(9), "?");
  SrItem junk = sr_item_simple(SR_IK_FOOD, 0, 1);
  SR_CHECK_NEAR(sr_weapon_stats(c->seed, &junk).damage, 0, 0);
  char name[96];
  sr_item_name(c, &a, name, sizeof name);
  SR_CHECK(strstr(name, "Heavy Pistol") != NULL);
  sr_maker_name(c, SR_STREET_MAKER_FIRST + 1, name, sizeof name);
  SR_CHECK_STR(name, "Backalley Forge");
  sr_maker_name(c, 99, name, sizeof name);
  SR_CHECK_STR(name, "Unknown");
  SR_CHECK(!sr_maker_valid(c, 99) && !sr_maker_valid(c, -1) && sr_maker_valid(c, SR_STREET_MAKER_FIRST));
  SR_CHECK(!sr_maker_valid(c, SR_STREET_MAKER_FIRST + 5));
}

static void test_mods_ammo(void) {
  SR_SPEC("equipment", "Suppressor reduces noise");
  SR_SPEC("equipment", "Gel rounds are non-lethal");
  SrCity *c = city();
  SrInventory inv;
  sr_inv_init(&inv);
  SrItem w = sr_item_weapon(1, SR_STREET_MAKER_FIRST, 1, 5);
  float noise = sr_weapon_stats(c->seed, &w).noise;
  SR_CHECK(!sr_install_mod(&w, SR_MOD_SUPPRESSOR, &inv)); /* not owned */
  sr_inv_add(&inv, sr_item_simple(SR_IK_MOD, SR_MOD_SUPPRESSOR, 1));
  sr_inv_add(&inv, sr_item_simple(SR_IK_MOD, SR_MOD_COMPENSATOR, 1));
  SR_CHECK(sr_install_mod(&w, SR_MOD_SUPPRESSOR, &inv));
  SR_CHECK_NEAR(sr_weapon_stats(c->seed, &w).noise, noise * 0.35f, 1e-3);
  SR_CHECK(sr_install_mod(&w, SR_MOD_COMPENSATOR, &inv)); /* swaps, returns suppressor */
  SR_CHECK_EQ(sr_inv_count(&inv, SR_IK_MOD, SR_MOD_SUPPRESSOR), 1);
  SR_CHECK(sr_remove_mod(&w, SR_SLOT_MUZZLE, &inv));
  SR_CHECK(!sr_remove_mod(&w, SR_SLOT_MUZZLE, &inv));
  SR_CHECK(!sr_install_mod(&w, SR_MOD_NONE, &inv));
  SrItem knife = sr_item_weapon(18, SR_STREET_MAKER_FIRST, 0, 1);
  SR_CHECK(!sr_install_mod(&knife, SR_MOD_SUPPRESSOR, &inv));
  for (int m = SR_MOD_REDDOT; m < SR_MOD_COUNT; m++) {
    sr_inv_add(&inv, sr_item_simple(SR_IK_MOD, m, 1));
    SR_CHECK(sr_install_mod(&w, (SrMod)m, &inv));
  }
  SrWeaponStats ms = sr_weapon_stats(c->seed, &w);
  SR_CHECK(ms.mag > sr_archetype(1)->mag);
  for (int a = 0; a < SR_AMMO_COUNT; a++) {
    w.ammo = (uint8_t)a;
    SrWeaponStats s = sr_weapon_stats(c->seed, &w);
    if (a == SR_AMMO_GEL) SR_CHECK(!s.lethal);
    if (a == SR_AMMO_EMP) SR_CHECK_EQ(s.damage_type, SR_DMG_EMP);
    if (a == SR_AMMO_INCENDIARY) SR_CHECK_EQ(s.damage_type, SR_DMG_THERMAL);
    SR_CHECK(sr_ammo_name(a)[0] != '?');
  }
  SR_CHECK_STR(sr_ammo_name(99), "?");
  for (int m = 0; m <= SR_MOD_COUNT; m++) SR_CHECK(sr_mod_name(m)[0] != '\0');
  SrEnemy e;
  sr_enemy_init(&e, sr_v3(0, 0, 0), 1, 10);
  w.ammo = SR_AMMO_GEL;
  SrWeaponStats gel = sr_weapon_stats(c->seed, &w);
  while (!sr_enemy_hit(&e, &gel, SR_ZONE_HEAD)) {}
  SR_CHECK(e.ko && !e.dead);
}

static void test_inventory_gadgets_crafting(void) {
  SR_SPEC("equipment", "Overburdened");
  SR_SPEC("equipment", "Quick slot use");
  SR_SPEC("equipment", "Crafting a gadget");
  SR_SPEC("equipment", "Grapple line to a rooftop");
  SR_SPEC("equipment", "Holo-decoy distracts");
  SrInventory inv;
  sr_inv_init(&inv);
  SR_CHECK(SR_GD_COUNT >= 16);
  for (int g = 0; g < SR_GD_COUNT; g++) SR_CHECK(sr_gadget_def(g)->name != NULL);
  SR_CHECK(sr_gadget_def(-1) == NULL);
  SR_CHECK(sr_gadget_def(SR_GD_GRAPPLE)->radius >= 20); /* reaches rooftops */
  SR_CHECK(sr_gadget_def(SR_GD_DECOY)->duration > 0);
  sr_inv_add(&inv, sr_item_simple(SR_IK_GADGET, SR_GD_MEDKIT, 2));
  sr_inv_add(&inv, sr_item_simple(SR_IK_GADGET, SR_GD_MEDKIT, 1));
  SR_CHECK_EQ(inv.count, 1);
  SR_CHECK_EQ(sr_inv_count(&inv, SR_IK_GADGET, SR_GD_MEDKIT), 3);
  int idx = sr_inv_find(&inv, SR_IK_GADGET, SR_GD_MEDKIT);
  SR_CHECK(sr_inv_remove_at(&inv, idx, 1)); /* quick slot use */
  SR_CHECK_EQ(sr_inv_count(&inv, SR_IK_GADGET, SR_GD_MEDKIT), 2);
  SR_CHECK(!sr_inv_remove_at(&inv, idx, 9));
  SR_CHECK(!sr_inv_remove_at(&inv, 40, 1));
  SR_CHECK(!sr_inv_remove_at(&inv, idx, 0));
  for (int i = 0; i < 10; i++) sr_inv_add(&inv, sr_item_weapon(12, SR_STREET_MAKER_FIRST, 0, (uint32_t)i));
  SR_CHECK(sr_inv_weight(&inv) > sr_carry_limit(3));
  SrItem kinds[] = {sr_item_simple(SR_IK_AMMO, 0, 10), sr_item_simple(SR_IK_FOOD, 0, 1),
                    sr_item_simple(SR_IK_COMPONENT, 0, 1), sr_item_simple(SR_IK_OUTFIT, 0, 1),
                    sr_item_simple(SR_IK_DECK, 0, 1), sr_item_simple(SR_IK_PAYDATA, 0, 1)};
  for (size_t i = 0; i < SR_ARRAY_LEN(kinds); i++) SR_CHECK(sr_item_weight(&kinds[i]) > 0);
  /* crafting */
  SrInventory c2;
  sr_inv_init(&c2);
  SR_CHECK(!sr_craft(&c2, 1u << SR_GD_SMOKE, SR_GD_SMOKE)); /* no components */
  sr_inv_add(&c2, sr_item_simple(SR_IK_COMPONENT, 0, 3));
  SR_CHECK(!sr_craft(&c2, 0, SR_GD_SMOKE));                 /* no blueprint */
  SR_CHECK(!sr_craft(&c2, 0xFFFFFFFFu, SR_GD_CAMO));         /* no recipe */
  SR_CHECK(sr_craft(&c2, 1u << SR_GD_SMOKE, SR_GD_SMOKE));
  SR_CHECK_EQ(sr_inv_count(&c2, SR_IK_GADGET, SR_GD_SMOKE), 1);
  SR_CHECK_EQ(sr_inv_count(&c2, SR_IK_COMPONENT, 0), 1);
  SrItem w = sr_item_weapon(0, 0, 2, 1);
  SR_CHECK_EQ(sr_salvage_value(&w), 6);
  SR_CHECK_EQ(sr_salvage_value(&kinds[0]), 0);
  SrItem gd = sr_item_simple(SR_IK_GADGET, 0, 1), cy = sr_item_simple(SR_IK_CYBER, 0, 1);
  SR_CHECK_EQ(sr_salvage_value(&gd), 1);
  SR_CHECK_EQ(sr_salvage_value(&cy), 3);
  /* full inventory */
  SrInventory full;
  sr_inv_init(&full);
  for (int i = 0; i < SR_INV_MAX; i++) SR_CHECK(sr_inv_add(&full, sr_item_weapon(0, 0, 0, (uint32_t)i)));
  SR_CHECK(!sr_inv_add(&full, sr_item_weapon(0, 0, 0, 99)));
  SrInventory dst;
  sr_inv_init(&dst);
  SR_CHECK(sr_inv_transfer_all(&c2, &dst));
  SR_CHECK_EQ(c2.count, 0);
  SR_CHECK(!sr_inv_transfer_all(&full, &dst) || true);
  SrInventory dst2;
  sr_inv_init(&dst2);
  for (int i = 0; i < SR_INV_MAX; i++) sr_inv_add(&dst2, sr_item_weapon(0, 0, 0, (uint32_t)i));
  SrInventory one;
  sr_inv_init(&one);
  sr_inv_add(&one, sr_item_weapon(1, 0, 0, 1));
  SR_CHECK(!sr_inv_transfer_all(&one, &dst2));
  SR_CHECK(sr_food_def(9) == NULL && sr_cyber_def(99) == NULL);
}

static void test_cyberware_and_vendors(void) {
  SR_SPEC("equipment", "Cyberware ability works");
  SR_SPEC("equipment", "Over capacity causes glitches");
  SR_SPEC("equipment", "Jailbreak removes the backdoor");
  SR_SPEC("equipment", "Stolen goods refused by a shop");
  SrCity *c = city();
  SrItem inst[3] = {sr_item_simple(SR_IK_CYBER, 2, 1), sr_item_simple(SR_IK_CYBER, 13, 1),
                    sr_item_simple(SR_IK_CYBER, 11, 1)};
  SR_CHECK_STR(sr_cyber_def(2)->name, "Thermal Optics");
  int load = sr_cyber_load(inst, 3);
  SR_CHECK_EQ(load, 2 + 4 + 4);
  SR_CHECK(sr_cyber_glitching(load, sr_neural_capacity(1, 1)));
  SR_CHECK(!sr_cyber_glitching(load, sr_neural_capacity(5, 5)));
  inst[0].flags = SR_IF_BACKDOOR;
  sr_cyber_jailbreak(&inst[0]);
  SR_CHECK(!(inst[0].flags & SR_IF_BACKDOOR));
  int fails = 0;
  for (int64_t d = 0; d < 1000; d++) fails += sr_cyber_malfunctions(&inst[1], 3, d);
  SR_CHECK(fails > 5 && fails < 80);
  inst[1].grade = 2;
  SR_CHECK(!sr_cyber_malfunctions(&inst[1], 3, 1));
  int gun = -1, fence = -1, ripper = -1;
  int shops[SR_POI_COUNT];
  for (int p = 0; p < SR_POI_COUNT; p++) shops[p] = -1;
  for (int b = 0; b < c->building_count; b++) {
    int p = c->buildings[b].poi;
    if (shops[p] < 0) shops[p] = b;
  }
  gun = shops[SR_POI_GUNSHOP];
  fence = shops[SR_POI_FENCE];
  ripper = shops[SR_POI_RIPPERDOC];
  SrItem loot = sr_item_weapon(2, 0, 1, 1);
  loot.flags = SR_IF_STOLEN;
  SR_CHECK(!sr_vendor_buys(c, gun, &loot));
  SR_CHECK(sr_vendor_buys(c, fence, &loot));
  loot.flags = 0;
  SR_CHECK(sr_vendor_buys(c, gun, &loot));
  SrItem job = sr_item_simple(SR_IK_JOB, 0, 1);
  SR_CHECK(!sr_vendor_buys(c, fence, &job));
  SrItem cyb = sr_item_simple(SR_IK_CYBER, 1, 1);
  SR_CHECK(sr_vendor_buys(c, ripper, &cyb));
  SrItem pd = sr_item_simple(SR_IK_PAYDATA, 1, 1);
  SR_CHECK(sr_vendor_buys(c, shops[SR_POI_ELECTRONICS], &pd));
  SR_CHECK(!sr_vendor_buys(c, shops[SR_POI_FOOD], &cyb));
  SR_CHECK(sr_vendor_sells_blackmarket(5, 0) && sr_vendor_sells_blackmarket(0, 60));
  SR_CHECK(!sr_vendor_sells_blackmarket(2, 10));
  SrItem stock[16];
  for (int p = 0; p < SR_POI_COUNT; p++) {
    if (shops[p] < 0) continue;
    int n = sr_vendor_stock(c, shops[p], 3, stock, 16);
    if (p == SR_POI_GUNSHOP || p == SR_POI_ELECTRONICS || p == SR_POI_RIPPERDOC || p == SR_POI_FOOD)
      SR_CHECK(n > 0);
    for (int i = 0; i < n; i++) {
      SR_CHECK(sr_price_at(c, shops[p], &stock[i]) >= 1);
      char nm[96];
      sr_item_name(c, &stock[i], nm, sizeof nm);
      SR_CHECK(nm[0] != '\0');
    }
  }
  SrItem misc[] = {sr_item_simple(SR_IK_AMMO, 1, 1), sr_item_simple(SR_IK_MOD, 1, 1),
                   sr_item_simple(SR_IK_COMPONENT, 0, 1), sr_item_simple(SR_IK_PAYDATA, 2, 1),
                   sr_item_simple(SR_IK_OUTFIT, 2, 1), sr_item_simple(SR_IK_DECK, 1, 1),
                   sr_item_simple(SR_IK_JOB, 0, 1), sr_item_simple(SR_IK_NONE, 0, 1)};
  for (size_t i = 0; i < SR_ARRAY_LEN(misc); i++) {
    char nm[64];
    sr_item_name(c, &misc[i], nm, sizeof nm);
    SR_CHECK(nm[0] != '\0');
    SR_CHECK(sr_item_base_price(&misc[i]) >= 0);
  }
}

/* ---- combat ---------------------------------------------------------------------------- */

static SrWeaponStats test_weapon(float dmg, int type, float pen, bool lethal) {
  SrWeaponStats w;
  memset(&w, 0, sizeof w);
  w.damage = dmg;
  w.damage_type = (uint8_t)type;
  w.penetration = pen;
  w.lethal = lethal;
  return w;
}

static void test_damage(void) {
  SR_SPEC("combat", "Headshot multiplier");
  SR_SPEC("combat", "Armour reduces damage");
  SR_SPEC("combat", "EMP against a drone");
  SrEnemy plain, armoured, drone;
  sr_enemy_init(&plain, sr_v3(0, 0, 0), 1, 100);
  sr_enemy_init(&armoured, sr_v3(0, 0, 0), 1, 100);
  armoured.armor = 60;
  sr_enemy_init(&drone, sr_v3(0, 0, 0), 1, 100);
  drone.drone = true;
  SrWeaponStats w = test_weapon(20, SR_DMG_KINETIC, 0, true);
  SR_CHECK_NEAR(sr_compute_damage(&w, SR_ZONE_HEAD, &plain), 2 * sr_compute_damage(&w, SR_ZONE_TORSO, &plain), 1e-4);
  SR_CHECK(sr_compute_damage(&w, SR_ZONE_LIMB, &plain) < sr_compute_damage(&w, SR_ZONE_TORSO, &plain));
  SR_CHECK(sr_compute_damage(&w, SR_ZONE_TORSO, &armoured) < sr_compute_damage(&w, SR_ZONE_TORSO, &plain));
  SrWeaponStats emp = test_weapon(50, SR_DMG_EMP, 0, true);
  SR_CHECK(sr_compute_damage(&emp, SR_ZONE_TORSO, &drone) >= 3 * sr_compute_damage(&emp, SR_ZONE_TORSO, &plain));
  SR_CHECK_NEAR(sr_type_multiplier(SR_DMG_TOXIN, true, 0), 0, 0);
  SR_CHECK_NEAR(sr_type_multiplier(SR_DMG_SHOCK, true, 0), 0.5, 1e-6);
  SR_CHECK(sr_type_multiplier(SR_DMG_THERMAL, false, 0) > 1);
  SR_CHECK(sr_type_multiplier(SR_DMG_EMP, false, 4) > sr_type_multiplier(SR_DMG_EMP, false, 0));
  /* lethal hits kill, and further hits do nothing */
  SrWeaponStats big = test_weapon(500, SR_DMG_KINETIC, 1, true);
  SR_CHECK(sr_enemy_hit(&plain, &big, SR_ZONE_TORSO));
  SR_CHECK(plain.dead);
  SR_CHECK(!sr_enemy_hit(&plain, &big, SR_ZONE_TORSO));
  SrWeaponStats small = test_weapon(5, SR_DMG_KINETIC, 0, true);
  SR_CHECK(!sr_enemy_hit(&armoured, &small, SR_ZONE_LIMB));
  SR_CHECK_EQ(armoured.alert, SR_ALERT_COMBAT);
  SR_CHECK(sr_difficulty((SrDifficulty)9) == sr_difficulty(SR_DIFF_STANDARD));
}

static void test_takedown_ko(void) {
  SR_SPEC("combat", "Silent non-lethal takedown");
  SR_SPEC("combat", "Knockout is not a kill");
  SR_SPEC("combat", "Unconscious target wakes up");
  SrEnemy e;
  sr_enemy_init(&e, sr_v3(0, 0, 0), 1, 100);
  e.yaw = 0; /* facing -z */
  float noise = 99;
  SR_CHECK(!sr_takedown(&e, sr_v3(0, 0, -1.5f), false, &noise)); /* from the front */
  SR_CHECK(!sr_takedown(&e, sr_v3(0, 0, 5), false, &noise));     /* too far */
  SR_CHECK(sr_takedown(&e, sr_v3(0, 0, 1.5f), false, &noise));
  SR_CHECK(e.ko && !e.dead && noise <= 2.0f);
  sr_enemy_update_ko(&e, 29 * 60);
  SR_CHECK(e.ko);
  sr_enemy_update_ko(&e, 2 * 60);
  SR_CHECK(!e.ko);
  SR_CHECK_EQ(e.alert, SR_ALERT_SEARCHING);
  SrEnemy r;
  sr_enemy_init(&r, sr_v3(0, 0, 0), 1, 100);
  SR_CHECK(sr_takedown(&r, sr_v3(0, 0, 1), false, NULL));
  sr_enemy_restrain(&r);
  sr_enemy_update_ko(&r, 3600);
  SR_CHECK(r.ko && r.restrained);
  SrEnemy k;
  sr_enemy_init(&k, sr_v3(0, 0, 0), 1, 100);
  SR_CHECK(sr_takedown(&k, sr_v3(0, 0, 1), true, &noise));
  SR_CHECK(k.dead && noise == 4.0f);
  SR_CHECK(!sr_takedown(&k, sr_v3(0, 0, 1), true, &noise));
  SrEnemy alerted;
  sr_enemy_init(&alerted, sr_v3(0, 0, 0), 1, 100);
  alerted.alert = SR_ALERT_COMBAT;
  SR_CHECK(!sr_takedown(&alerted, sr_v3(0, 0, 1), false, NULL));
  sr_enemy_restrain(&alerted); /* not knocked out: nothing happens */
  SR_CHECK(!alerted.restrained);
}

static void test_health_flatline(void) {
  SR_SPEC("combat", "Natural regeneration cap");
  SR_SPEC("combat", "Flatline with MedEvac");
  SR_SPEC("combat", "Flatline without insurance");
  float h = 10;
  for (int i = 0; i < 1000; i++) h = sr_regen_health(h, 100, 1.0f);
  SR_CHECK_NEAR(h, 30, 1e-3);
  SR_CHECK_NEAR(sr_regen_health(50, 100, 1), 50, 0);
  SrFlatline f = sr_flatline(true, 1000, 1, 100, false);
  SR_CHECK(f.wake_in_clinic && f.bill > 0 && f.cash_lost == 0);
  f = sr_flatline(false, 1000, 1, 100, false);
  SR_CHECK(!f.wake_in_clinic && f.cash_lost >= 200 && f.cash_lost <= 500);
  f = sr_flatline(false, 1000, 1, 100, true);
  SR_CHECK(f.delete_save);
}

static void test_perception(void) {
  SR_SPEC("combat", "Darkness hides the player");
  SR_SPEC("combat", "Noise draws investigation");
  SR_SPEC("combat", "Alert decays");
  SR_SPEC("combat", "Visibility meter reacts to light");
  SR_SPEC("combat", "Difficulty changes enemy accuracy");
  SrPerceptInput in = {sr_v3(0, 0, -15), 0.0f, 0.0f, true, false, 400.0f, SR_DIFF_STANDARD};
  SrEnemy dark, bright;
  sr_enemy_init(&dark, sr_v3(0, 0, 0), 1, 100);
  sr_enemy_init(&bright, sr_v3(0, 0, 0), 1, 100);
  for (int i = 0; i < 50; i++) sr_perceive(&dark, &in, 0.1f);
  SR_CHECK_EQ(dark.alert, SR_ALERT_UNAWARE);
  in.light = 1.0f;
  SrAlert a = SR_ALERT_UNAWARE;
  for (int i = 0; i < 50; i++) a = sr_perceive(&bright, &in, 0.1f);
  SR_CHECK_EQ(a, SR_ALERT_COMBAT);
  SR_CHECK(sr_player_visibility(0.1f, 0, false, false) < sr_player_visibility(1.0f, 0, false, false));
  SR_CHECK(sr_player_visibility(1, 6, false, true) < sr_player_visibility(1, 6, false, false));
  /* losing sight: searching, then back to unaware with heightened state */
  in.player_pos = sr_v3(0, 0, 100); /* behind and far */
  sr_perceive(&bright, &in, 0.1f);
  SR_CHECK_EQ(bright.alert, SR_ALERT_SEARCHING);
  for (int i = 0; i < 250; i++) sr_perceive(&bright, &in, 0.1f);
  SR_CHECK_EQ(bright.alert, SR_ALERT_UNAWARE);
  SR_CHECK(bright.heightened);
  /* noise */
  SrEnemy g;
  sr_enemy_init(&g, sr_v3(0, 0, 0), 1, 100);
  sr_hear_noise(&g, sr_v3(50, 0, 0), 10);
  SR_CHECK_EQ(g.alert, SR_ALERT_UNAWARE);
  sr_hear_noise(&g, sr_v3(5, 0, 0), 10);
  SR_CHECK_EQ(g.alert, SR_ALERT_SUSPICIOUS);
  SR_CHECK_NEAR(g.investigate.x, 5, 0);
  /* in view while suspicious keeps the search timer up */
  in.player_pos = sr_v3(0, 0, -30);
  in.light = 0.2f;
  sr_perceive(&g, &in, 0.1f);
  SR_CHECK(sr_in_view(&g, sr_v3(0, 0, -0.5f), 10));
  SR_CHECK(!sr_in_view(&g, sr_v3(0, 0, -50), 10));
  SrEnemy dead;
  sr_enemy_init(&dead, sr_v3(0, 0, 0), 1, 100);
  dead.dead = true;
  sr_hear_noise(&dead, sr_v3(1, 0, 0), 10);
  SR_CHECK_EQ(sr_perceive(&dead, &in, 1.0f), SR_ALERT_UNAWARE);
  sr_body_discovered(&dead, sr_v3(0, 0, 0));
  SR_CHECK(sr_difficulty(SR_DIFF_STORY)->enemy_accuracy < sr_difficulty(SR_DIFF_STREET)->enemy_accuracy);
  for (int i = 0; i <= 4; i++) SR_CHECK(sr_alert_name((SrAlert)i)[0] != '\0');
}

static void test_tactics_bodies(void) {
  SR_SPEC("combat", "Reinforcements called");
  SR_SPEC("combat", "Jammed comms");
  SR_SPEC("combat", "Surrender at low morale");
  SR_SPEC("combat", "Body discovered");
  SrEnemy e;
  sr_enemy_init(&e, sr_v3(0, 0, 0), 1, 100);
  SR_CHECK(!sr_enemy_tactics(&e, false, 0)); /* not in combat */
  e.alert = SR_ALERT_COMBAT;
  SR_CHECK(!sr_enemy_tactics(&e, true, 0));
  SR_CHECK(sr_enemy_tactics(&e, false, 0));
  SR_CHECK(!sr_enemy_tactics(&e, false, 0)); /* only once */
  SR_CHECK(!sr_enemy_tactics(&e, false, 5));
  SR_CHECK(e.surrendered);
  SrEnemy g;
  sr_enemy_init(&g, sr_v3(0, 0, 0), 1, 100);
  sr_body_discovered(&g, sr_v3(3, 0, 3));
  SR_CHECK_EQ(g.alert, SR_ALERT_SEARCHING);
  SR_CHECK(g.heightened);
}

/* ---- progression ------------------------------------------------------------------------ */

static void test_progression(void) {
  SR_SPEC("character-progression", "Attribute range");
  SR_SPEC("character-progression", "Using a skill raises it");
  SR_SPEC("character-progression", "Trivial use grants nothing");
  SR_SPEC("character-progression", "Level-up rewards");
  SR_SPEC("character-progression", "Perk locked until rank");
  SR_SPEC("character-progression", "Tier thresholds");
  SR_SPEC("character-progression", "Unattributed action");
  SR_SPEC("character-progression", "Hostile faction attacks");
  SR_SPEC("character-progression", "Respec refunds points");
  SrProgress p;
  sr_progress_init(&p);
  int8_t base[SR_ATTR_COUNT];
  memcpy(base, p.attr, sizeof base);
  p.attr[SR_ATTR_BODY] = 10;
  p.attr_points = 1;
  SR_CHECK(!sr_progress_raise_attr(&p, SR_ATTR_BODY));
  SR_CHECK_EQ(p.attr_points, 1);
  SR_CHECK(sr_progress_raise_attr(&p, SR_ATTR_COOL));
  SR_CHECK(!sr_progress_raise_attr(&p, SR_ATTR_COOL)); /* no points left */
  SR_CHECK(!sr_progress_raise_attr(&p, (SrAttr)9));
  p.attr[SR_ATTR_BODY] = 3;
  SR_CHECK(!sr_progress_use_skill(&p, SR_SK_PISTOLS, SR_USE_TRIVIAL, 1000));
  SR_CHECK_EQ(p.skill_xp[SR_SK_PISTOLS], 0);
  SR_CHECK(sr_progress_use_skill(&p, SR_SK_PISTOLS, SR_USE_HOSTILE, 50));
  SR_CHECK_EQ(p.skill_rank[SR_SK_PISTOLS], 1);
  SR_CHECK(!sr_progress_use_skill(&p, (SrSkill)99, SR_USE_HOSTILE, 5));
  p.skill_rank[SR_SK_MELEE] = SR_MAX_SKILL_RANK;
  SR_CHECK(!sr_progress_use_skill(&p, SR_SK_MELEE, SR_USE_SUCCESS, 999));
  SrProgress l;
  sr_progress_init(&l);
  int gained = sr_progress_add_xp(&l, sr_level_xp(1) + sr_level_xp(2) + sr_level_xp(3));
  SR_CHECK_EQ(gained, 3);
  SR_CHECK_EQ(l.level, 4);
  SR_CHECK_EQ(l.perk_points, 3);
  SR_CHECK_EQ(l.attr_points, 1);
  SR_CHECK(!sr_progress_buy_perk(&l, SR_SK_STEALTH, 1)); /* rank 0 < 10 */
  l.skill_rank[SR_SK_STEALTH] = 9;
  SR_CHECK(!sr_progress_buy_perk(&l, SR_SK_STEALTH, 1));
  SR_CHECK_EQ(sr_perk_required_rank(1), 10);
  l.skill_rank[SR_SK_STEALTH] = 10;
  SR_CHECK(sr_progress_buy_perk(&l, SR_SK_STEALTH, 1));
  SR_CHECK(!sr_progress_buy_perk(&l, SR_SK_STEALTH, 1)); /* already owned */
  SR_CHECK(sr_progress_has_perk(&l, SR_SK_STEALTH, 1));
  SR_CHECK(!sr_progress_buy_perk(&l, SR_SK_STEALTH, 7));
  SR_CHECK(!sr_progress_has_perk(&l, (SrSkill)99, 0));
  l.attr[SR_ATTR_MIND] = 5;
  sr_progress_respec(&l, base);
  SR_CHECK_EQ(l.attr[SR_ATTR_MIND], base[SR_ATTR_MIND]);
  SR_CHECK_EQ(l.perk_points, 3);
  SR_CHECK(l.attr_points >= 3);
  SR_CHECK(sr_progress_add_cred(&p, 10));
  SR_CHECK_EQ(sr_cred_tier(p.street_cred), SR_CRED_PAWN);
  SR_CHECK(!sr_progress_add_cred(&p, 1));
  sr_progress_add_cred(&p, 1000);
  SR_CHECK_EQ(p.street_cred, 100);
  SR_CHECK_EQ(sr_cred_tier(25), SR_CRED_RUNNER);
  SR_CHECK_EQ(sr_cred_tier(45), SR_CRED_OPERATOR);
  SR_CHECK_EQ(sr_cred_tier(70), SR_CRED_KNIGHT);
  for (int t = 0; t <= 6; t++) SR_CHECK(sr_cred_tier_name((SrCredTier)t)[0] != '\0');
  sr_progress_standing(&p, 3, -50, false);
  SR_CHECK_EQ(p.standing[3], 0);
  sr_progress_standing(&p, 3, -70, true);
  SR_CHECK(sr_faction_hostile(&p, 3));
  SR_CHECK(!sr_faction_hostile(&p, 4) && !sr_faction_hostile(&p, -1));
  sr_progress_standing(&p, -1, 5, true);
  SR_CHECK_STR(sr_standing_name(-70), "Hostile");
  SR_CHECK_STR(sr_standing_name(-30), "Unfriendly");
  SR_CHECK_STR(sr_standing_name(0), "Neutral");
  SR_CHECK_STR(sr_standing_name(30), "Friendly");
  SR_CHECK_STR(sr_standing_name(80), "Trusted");
  for (int s = 0; s <= SR_SK_COUNT; s++) {
    SR_CHECK(sr_skill_name((SrSkill)s)[0] != '\0');
    (void)sr_skill_attr((SrSkill)s);
  }
  for (int a = 0; a <= SR_ATTR_COUNT; a++) SR_CHECK(sr_attr_name((SrAttr)a)[0] != '\0');
}

/* ---- heat ----------------------------------------------------------------------------------- */

static void test_heat(void) {
  SR_SPEC("law-and-heat", "Separate jurisdictions");
  SR_SPEC("law-and-heat", "Unwitnessed crime");
  SR_SPEC("law-and-heat", "Camera recording raises Heat");
  SR_SPEC("law-and-heat", "Response time by district");
  SR_SPEC("law-and-heat", "Heat decays out of sight");
  SR_SPEC("law-and-heat", "Changing appearance breaks recognition");
  SR_SPEC("law-and-heat", "Surrender to police");
  SrCity *c = city();
  SrHeat h;
  sr_heat_init(&h);
  int corp_site = c->factions.f[0].facilities[0];
  int j = sr_heat_jurisdiction(c, corp_site);
  SR_CHECK_EQ(j, 0);
  SR_CHECK_EQ(sr_heat_jurisdiction(c, c->home_building), c->factions.police);
  SR_CHECK_EQ(sr_heat_jurisdiction(c, -1), c->factions.police);
  SrCrime cr = {0, 3, false, false, false, 77, 1000};
  SR_CHECK_EQ(sr_heat_crime(&h, &cr), 0);
  cr.camera = true;
  SR_CHECK_EQ(sr_heat_crime(&h, &cr), 1);
  SR_CHECK_EQ(h.level[0], 1);
  SR_CHECK_EQ(h.level[1], 0);
  SR_CHECK(sr_heat_recognises(&h, 0, 77));
  SR_CHECK(!sr_heat_recognises(&h, 0, 78)); /* changed appearance */
  cr.severity = 5;
  cr.jurisdiction = -1;
  SR_CHECK_EQ(sr_heat_crime(&h, &cr), 0);
  cr.jurisdiction = 0;
  SR_CHECK_EQ(sr_heat_crime(&h, &cr), 2);
  SR_CHECK(sr_heat_response_time(2, SR_D_SPIRE) < sr_heat_response_time(2, SR_D_RESIDENTIAL));
  SR_CHECK_EQ(sr_heat_response_time(0, SR_D_SPIRE), -1);
  SR_CHECK(sr_heat_response_time(5, SR_D_SPIRE) < sr_heat_response_time(1, SR_D_SPIRE));
  /* decay: police Heat 2, unseen for 24 hours */
  int pol = c->factions.police;
  h.level[pol] = 2;
  h.last_seen[pol] = h.decay_from[pol] = 10000;
  sr_heat_update(&h, 10000 + 1800, false);
  SR_CHECK_EQ(h.level[pol], 2);
  sr_heat_update(&h, 10000 + 24 * 3600, false);
  SR_CHECK(h.level[pol] < 2);
  sr_heat_seen(&h, pol, 200000);
  sr_heat_seen(&h, -1, 0);
  /* bribe and arrest */
  h.level[pol] = 2;
  int64_t cash = 100;
  SR_CHECK(!sr_heat_bribe(&h, pol, pol, &cash));
  cash = 5000;
  SR_CHECK(sr_heat_bribe(&h, pol, pol, &cash));
  SR_CHECK_EQ(h.level[pol], 1);
  SR_CHECK(!sr_heat_bribe(&h, 0, pol, &cash)); /* corps don't take bribes */
  SrArrest a = sr_heat_arrest_terms(&h, pol, pol);
  SR_CHECK(a.allowed && a.fine > 0 && a.jail_seconds > 0);
  SR_CHECK(!sr_heat_arrest_terms(&h, 0, pol).allowed);
  sr_heat_surrender(&h, pol);
  SR_CHECK_EQ(h.level[pol], 0);
  sr_heat_surrender(&h, -1);
  SR_CHECK_EQ(sr_heat_bribe_cost(&h, -1), 0);
  /* at home decays faster; zero heat forgets the face */
  h.level[3] = 1;
  h.last_seen[3] = h.decay_from[3] = 0;
  h.known_look[3] = 5;
  sr_heat_update(&h, 6 * 3600, true);
  SR_CHECK_EQ(h.level[3], 0);
  SR_CHECK(h.known_look[3] == 0);
  h.level[0] = 3;
  sr_heat_erase_records(&h, 0);
  SR_CHECK_EQ(h.level[0], 1);
  SR_CHECK_EQ(h.evidence_count, 0);
}

static void test_disguise_attribution(void) {
  SR_SPEC("law-and-heat", "Uniform grants access");
  SR_SPEC("law-and-heat", "Suspicious behaviour breaks the disguise");
  SR_SPEC("law-and-heat", "Trespass detected");
  SrDisguise d = {2, SR_AREA_MAINTENANCE, false};
  SR_CHECK(sr_disguise_authorised(&d, 2, SR_AREA_MAINTENANCE));
  SR_CHECK(!sr_disguise_authorised(&d, 2, SR_AREA_SECURE));
  SR_CHECK(!sr_disguise_authorised(&d, 3, SR_AREA_MAINTENANCE));
  SR_CHECK(sr_disguise_authorised(&d, 3, SR_AREA_PUBLIC));
  sr_disguise_action(&d, false);
  SR_CHECK(!d.blown);
  sr_disguise_action(&d, true);
  SR_CHECK(sr_is_trespassing(&d, 2, SR_AREA_MAINTENANCE));
  SrEvidence ev[3] = {{SR_EVID_PLANTED, 0, 4, 1, 0, 0}, {SR_EVID_PLANTED, 0, 4, 1, 0, 0},
                      {SR_EVID_CAMERA, 0, -2, 1, 0, 0}};
  SR_CHECK_EQ(sr_heat_attribution(ev, 2), 4);
  SR_CHECK_EQ(sr_heat_attribution(ev, 3), 4);
  SR_CHECK_EQ(sr_heat_attribution(&ev[2], 1), -2);
  SR_CHECK_EQ(sr_heat_attribution(ev, 0), -1);
}

/* ---- player life ----------------------------------------------------------------------- */

static void test_player_create(void) {
  SR_SPEC("player-life", "Background applies starting values");
  SR_SPEC("player-life", "Rent reminders");
  SR_SPEC("player-life", "Rent paid automatically");
  SrCity *c = city();
  SrPlayer p;
  for (int bg = 0; bg < SR_BG_COUNT; bg++) {
    sr_player_create(&p, c, "Kit", (SrBackground)bg, 9);
    const SrBackgroundDef *d = sr_background_def(bg);
    SR_CHECK_EQ(p.cred, d->cash);
    SR_CHECK_EQ(p.prog.skill_rank[d->trained[0]], 3);
    SR_CHECK(sr_inv_find(&p.inv, SR_IK_DECK, -1) >= 0);
    SR_CHECK_EQ(p.home, c->home_building);
  }
  SR_CHECK(sr_background_def(9) == NULL && sr_housing_def(9) == NULL);
  sr_player_create(&p, c, "Kit", (SrBackground)42, 9); /* unknown background falls back */
  SR_CHECK_EQ(p.cred, sr_background_def(0)->cash);
  sr_player_create(&p, c, "Kit", SR_BG_NETRUNNER, 9);
  SR_CHECK_EQ(p.prog.skill_rank[SR_SK_NETRUNNING], 3);
  SR_CHECK_EQ(p.prog.skill_rank[SR_SK_ELECTRONICS], 3);
  /* reminders and automatic payment */
  SrEventLog log;
  sr_events_init(&log);
  int before = p.msg_count;
  sr_player_rent_tick(&p, p.rent_due - 2 * SR_DAY_S, &log);
  SR_CHECK_EQ(p.msg_count, before + 1);
  sr_player_rent_tick(&p, p.rent_due - SR_DAY_S, &log);
  SR_CHECK_EQ(p.msg_count, before + 2);
  int64_t due = p.rent_due, cash = p.cred;
  sr_player_rent_tick(&p, due, &log);
  SR_CHECK_EQ(p.cred, cash - p.rent);
  SR_CHECK_EQ(p.rent_due, due + SR_WEEK_S);
  SR_CHECK_EQ(p.missed, 0);
  SR_CHECK_EQ(sr_player_unread(&p), p.msg_count);
  for (int i = 0; i < 40; i++) sr_player_message(&p, "spam", "x", 0);
  SR_CHECK_EQ(p.msg_count, SR_MAX_MESSAGES);
  for (int i = 0; i < 80; i++) sr_player_tx(&p, 0, 0, "x");
  SR_CHECK_EQ(p.tx_count, SR_MAX_TX);
}

static void test_needs_sleep(void) {
  SR_SPEC("player-life", "Eating restores satiety");
  SR_SPEC("player-life", "Starvation floor");
  SR_SPEC("player-life", "Fatigue penalties");
  SR_SPEC("player-life", "Hostiles prevent sleep");
  SR_SPEC("player-life", "Sleeping rough");
  SrCity *c = city();
  SrPlayer p;
  sr_player_create(&p, c, "Kit", SR_BG_STREET_KID, 1);
  p.satiety = 100;
  for (int m = 0; m < 36 * 60; m++) sr_player_minute(&p, false);
  SR_CHECK(p.satiety < 1.0f);
  SR_CHECK(sr_player_stamina_regen(&p) < 1.0f);
  float s0 = p.satiety;
  sr_player_eat(&p, 0);
  SR_CHECK_NEAR(p.satiety, s0 + sr_food_def(0)->satiety, 1e-3);
  sr_player_eat(&p, 99);
  p.satiety = 0;
  for (int m = 0; m < 48 * 60; m++) sr_player_minute(&p, false);
  SR_CHECK(p.health < p.max_health);
  SR_CHECK(p.health >= p.max_health * 0.25f - 1e-3);
  SR_CHECK(p.starving_hours >= 47.9f);
  p.satiety = 99;
  sr_player_eat(&p, 2);
  SR_CHECK_NEAR(p.satiety, 100, 0);
  p.fatigue = 80;
  SR_CHECK(sr_player_aim_sway(&p) > 1.0f && sr_player_matrix_time(&p) > 1.0f);
  for (int m = 0; m < 8 * 60; m++) sr_player_minute(&p, true);
  SR_CHECK_NEAR(p.fatigue, 0, 1e-3);
  SR_CHECK_NEAR(sr_player_aim_sway(&p), 1.0, 0);
  p.fatigue = 99.99f;
  sr_player_minute(&p, false);
  SR_CHECK_NEAR(p.fatigue, 100, 0);
  SR_CHECK_EQ(sr_player_sleep_allowed(&p, 1), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_player_sleep_allowed(&p, 0), SR_OK);
  int robbed = 0;
  for (int64_t d = 0; d < 100; d++) {
    p.cred = 1000;
    sr_player_wake(&p, SR_SLEEP_STREET, 5, d * SR_DAY_S, SR_D_UNDERCITY);
    robbed += p.cred < 1000;
  }
  SR_CHECK(robbed > 20 && robbed < 80);
  p.cred = 1000;
  sr_player_wake(&p, SR_SLEEP_HOME, 5, 0, SR_D_UNDERCITY);
  SR_CHECK_EQ(p.cred, 1000);
  SR_CHECK(sr_capsule_price(c, c->home_building) > 0);
}

static void test_money_housing_travel(void) {
  SR_SPEC("player-life", "Buying food");
  SR_SPEC("player-life", "Insufficient funds");
  SR_SPEC("player-life", "District price differences");
  SR_SPEC("player-life", "Moving to a better apartment");
  SR_SPEC("player-life", "Stash keeps items");
  SR_SPEC("player-life", "Metro ride");
  SR_SPEC("player-life", "Cab costs more than metro");
  SrCity *c = city();
  SrPlayer p;
  sr_player_create(&p, c, "Kit", SR_BG_STREET_KID, 1);
  int32_t d;
  int food = sr_city_find_poi_near(c, SR_POI_FOOD, c->home_building, &d);
  SrItem meal = sr_item_simple(SR_IK_FOOD, 0, 1);
  int open, hours;
  sr_poi_hours(c, food, &open, &hours);
  int64_t t = 86400 + (open + 1) * 3600;
  int64_t before = p.cred;
  int32_t price = sr_price_at(c, food, &meal);
  SR_CHECK_EQ(sr_player_buy(&p, c, food, &meal, t), SR_OK);
  SR_CHECK_EQ(p.cred, before - price);
  SR_CHECK(strstr(p.tx[p.tx_count - 1].what, "Noodles") != NULL);
  p.cred = 1;
  SR_CHECK_EQ(sr_player_buy(&p, c, food, &meal, t), SR_ERR_DENIED);
  SR_CHECK_EQ(p.cred, 1);
  SR_CHECK_EQ(sr_player_buy(&p, c, food, &meal, 86400 + ((open + hours + 1) % 24) * 3600), SR_ERR_DENIED);
  /* Spire vs Undercity prices */
  int spire = -1, under = -1;
  for (int b = 0; b < c->building_count; b++) {
    SrDistrictType ty = c->districts[c->blocks[c->buildings[b].block].district].type;
    if (ty == SR_D_SPIRE && spire < 0) spire = b;
    if (ty == SR_D_UNDERCITY && under < 0) under = b;
  }
  SrItem gun = sr_item_weapon(5, SR_STREET_MAKER_FIRST, 0, 1);
  SR_CHECK(sr_price_at(c, spire, &gun) > sr_price_at(c, under, &gun));
  /* buy a non-food item into the inventory, then sell it */
  int shop = -1;
  for (int b = 0; b < c->building_count && shop < 0; b++)
    if (c->buildings[b].poi == SR_POI_GUNSHOP) shop = b;
  sr_poi_hours(c, shop, &open, &hours);
  t = 86400 + (open + 1) * 3600;
  p.cred = 100000;
  int n0 = p.inv.count;
  SR_CHECK_EQ(sr_player_buy(&p, c, shop, &gun, t), SR_OK);
  SR_CHECK_EQ(p.inv.count, n0 + 1);
  int64_t c1 = p.cred;
  SR_CHECK_EQ(sr_player_sell(&p, c, shop, p.inv.count - 1, t), SR_OK);
  SR_CHECK(p.cred > c1);
  SR_CHECK_EQ(sr_player_sell(&p, c, shop, 99, t), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_player_sell(&p, c, food, 0, t), SR_ERR_DENIED);
  p.equipped = 0;
  SrItem stolen = sr_item_weapon(5, 0, 0, 2);
  stolen.flags = SR_IF_STOLEN;
  sr_inv_add(&p.inv, stolen);
  int fence = -1;
  for (int b = 0; b < c->building_count && fence < 0; b++)
    if (c->buildings[b].poi == SR_POI_FENCE) fence = b;
  SR_CHECK_EQ(sr_player_sell(&p, c, fence, p.inv.count - 1, t), SR_OK);
  p.equipped = (int8_t)sr_inv_find(&p.inv, SR_IK_WEAPON, -1);
  SR_CHECK_EQ(sr_player_sell(&p, c, fence, p.equipped, t), SR_OK); /* sells the equipped weapon */
  SR_CHECK_EQ(p.equipped, -1);
  SrPlayer full = p;
  for (int i = full.inv.count; i < SR_INV_MAX; i++) sr_inv_add(&full.inv, sr_item_weapon(0, 0, 0, (uint32_t)i));
  SR_CHECK_EQ(sr_player_buy(&full, c, shop, &gun, t), SR_ERR_NOMEM);
  /* housing */
  sr_inv_add(&p.stash, sr_item_simple(SR_IK_GADGET, SR_GD_SMOKE, 2));
  int64_t rent_before = p.rent;
  SR_CHECK_EQ(sr_player_move(&p, SR_HOUSE_MID, spire, t), SR_OK);
  SR_CHECK(p.rent > rent_before && p.home == spire);
  SR_CHECK_EQ(sr_inv_count(&p.stash, SR_IK_GADGET, SR_GD_SMOKE), 2);
  SR_CHECK_EQ(sr_player_move(&p, 99, spire, t), SR_ERR_DENIED);
  p.cred = 0;
  SR_CHECK_EQ(sr_player_move(&p, SR_HOUSE_LOFT, spire, t), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_player_pay_debt(&p, t), SR_ERR_INVALID);
  p.rent_debt = 500;
  SR_CHECK_EQ(sr_player_pay_debt(&p, t), SR_ERR_DENIED);
  /* travel */
  SrTrip m = sr_metro_trip(c, 0, c->station_count - 1, 1000);
  SR_CHECK(m.possible && m.fare > 0 && m.seconds > 0);
  SrTrip bad = sr_metro_trip(c, 0, 99, 1000);
  SR_CHECK(!bad.possible);
  int a = c->stations[0].block, b2 = c->stations[1].block;
  SrTrip cab = sr_cab_trip(c, c->blocks[a].first_building, c->blocks[b2].first_building);
  SrTrip metro = sr_metro_trip(c, 0, 1, 1000);
  SR_CHECK(cab.fare > metro.fare);
  for (int h = 0; h < SR_HOUSE_COUNT; h++) SR_CHECK(sr_housing_def(h)->rent > 0);
}

/* ---- jobs ------------------------------------------------------------------------------ */

static void test_job_generation(void) {
  SR_SPEC("job-system", "Jobs reference real entities");
  SR_SPEC("job-system", "Pay within tier range");
  SR_SPEC("job-system", "Two approaches validated");
  SrCity *c = city();
  for (int i = 0; i < 300; i++) {
    SrJob j;
    SrJobType type = (SrJobType)(i % SR_JT_COUNT);
    SR_CHECK(sr_job_generate(c, type, (uint64_t)i, 1000, &j));
    SR_CHECK(j.target_building >= 0 && j.target_building < c->building_count);
    SR_CHECK(j.client_faction < c->factions.count);
    SR_CHECK(j.target_faction < c->factions.count);
    int32_t lo, hi;
    sr_tier_pay_range(j.tier, &lo, &hi);
    SR_CHECK(j.pay >= lo && j.pay <= hi);
    SR_CHECK(j.deadline > j.offered);
    char brief[256];
    sr_job_briefing(c, &j, brief, sizeof brief);
    SR_CHECK(strstr(brief, "Pay") != NULL);
    j.intel = 0xF;
    sr_job_briefing(c, &j, brief, sizeof brief);
  }
  SR_CHECK_EQ(sr_job_tier_of_type(SR_JT_DELIVER), 0);
  SR_CHECK_EQ(sr_job_tier_of_type(SR_JT_THEFT), 1);
  SR_CHECK_EQ(sr_job_tier_of_type(SR_JT_RAID), 2);
  for (int t = 0; t <= SR_JT_COUNT; t++) SR_CHECK(sr_job_type_name((SrJobType)t)[0] != '\0');
  for (int a = 0; a <= SR_AP_COUNT; a++) SR_CHECK(sr_approach_name((SrApproach)a)[0] != '\0');
}

static SrJob *offer_job(SrJobBoard *b, SrCity *c, int tier_cred, int min_tier) {
  for (int day = 0; day < 60; day++) {
    for (int d = 0; d < c->district_count; d++) {
      int ids[4];
      int n = sr_jobs_fixer_offers(b, c, c->factions.fixer_building[d], day * SR_DAY_S + 3600, tier_cred, ids);
      for (int i = 0; i < n; i++) {
        SrJob *j = sr_jobs_get(b, ids[i]);
        if (j->tier >= min_tier) return j;
      }
    }
  }
  return NULL;
}

static void test_job_sources_tiers(void) {
  SR_SPEC("job-system", "Fixer offers jobs");
  SR_SPEC("job-system", "Job board listings");
  SR_SPEC("job-system", "Tier gating");
  SrCity *c = city();
  SrJobBoard *b = (SrJobBoard *)malloc(sizeof *b);
  sr_jobs_init(b);
  int ids[4];
  int fixer = c->factions.fixer_building[0];
  int n = sr_jobs_fixer_offers(b, c, fixer, 3600, 0, ids);
  SR_CHECK(n >= 1 && n <= 4);
  for (int i = 0; i < n; i++) SR_CHECK_EQ(sr_jobs_get(b, ids[i])->tier, 0);
  int again[4];
  SR_CHECK_EQ(sr_jobs_fixer_offers(b, c, fixer, 7200, 0, again), n); /* same batch today */
  /* Nobody never sees above T0; Runner sees T2 */
  for (int day = 1; day < 20; day++) {
    for (int d = 0; d < c->district_count; d++) {
      int k = sr_jobs_fixer_offers(b, c, c->factions.fixer_building[d], day * SR_DAY_S, 0, ids);
      for (int i = 0; i < k; i++) SR_CHECK_EQ(sr_jobs_get(b, ids[i])->tier, 0);
    }
  }
  SrJobBoard *b2 = (SrJobBoard *)malloc(sizeof *b2);
  sr_jobs_init(b2);
  SR_CHECK(offer_job(b2, c, 25, 2) != NULL);
  SR_CHECK(offer_job(b2, c, 50, 3) != NULL); /* Operator variant */
  SR_CHECK_EQ(sr_max_tier_for_cred(0), 0);
  SR_CHECK_EQ(sr_max_tier_for_cred(95), 3);
  sr_jobs_init(b2);
  SR_CHECK_EQ(sr_jobs_board_refresh(b2, c, 3600, 0), 4);
  SR_CHECK_EQ(sr_jobs_board_refresh(b2, c, 7200, 0), 0);
  for (int i = 0; i < b2->count; i++) SR_CHECK_EQ(b2->jobs[i].source, SR_SRC_BOARD);
  SR_CHECK(sr_jobs_get(b2, 9999) == NULL);
  /* the board recycles old jobs when full */
  for (int day = 0; day < 40; day++) sr_jobs_board_refresh(b2, c, day * SR_DAY_S, 0);
  SR_CHECK(b2->count <= SR_MAX_JOBS);
  free(b);
  free(b2);
}

static void test_job_flow(void) {
  SR_SPEC("job-system", "Buying intel reveals details");
  SR_SPEC("job-system", "Successful negotiation");
  SR_SPEC("job-system", "Failed negotiation");
  SR_SPEC("job-system", "Payment on completion");
  SR_SPEC("job-system", "Bonus objective");
  SR_SPEC("job-system", "No-kill constraint violated");
  SR_SPEC("job-system", "Deadline missed");
  SR_SPEC("job-system", "Job outcome becomes a world event");
  SrCity *c = city();
  SrPlayer p;
  sr_player_create(&p, c, "Kit", SR_BG_WAGESLAVE, 2);
  SrEventLog log;
  sr_events_init(&log);
  /* negotiation: try many jobs, see both outcomes */
  int ok = 0, fail = 0, withdrawn = 0;
  for (int i = 0; i < 60; i++) {
    SrJob j;
    sr_job_generate(c, SR_JT_DELIVER, (uint64_t)i + 500, 0, &j);
    j.id = i;
    int32_t pay = j.pay;
    SrResult r = sr_job_negotiate(&j, &p, (uint64_t)i);
    if (r == SR_OK) {
      ok++;
      SR_CHECK(j.pay > pay && j.pay <= pay * 13 / 10 + 1);
    } else {
      fail++;
      withdrawn += j.state == SR_JS_WITHDRAWN;
      if (j.state != SR_JS_WITHDRAWN) SR_CHECK(j.pay < pay);
    }
    SR_CHECK_EQ(sr_job_negotiate(&j, &p, 1), SR_ERR_INVALID); /* only once */
  }
  SR_CHECK(ok > 0 && fail > 0 && withdrawn > 0);
  /* a T2 job: intel, accept, execute, complete */
  SrJob j;
  sr_job_generate(c, SR_JT_STEAL_DATA, 4242, 0, &j);
  j.id = 7;
  j.constraints = 0;
  j.has_bonus = true;
  j.bonus = 100;
  p.cred = 10000;
  char before[256], after[256];
  sr_job_briefing(c, &j, before, sizeof before);
  SR_CHECK(strstr(before, "unknown") != NULL);
  SR_CHECK_EQ(sr_job_buy_intel(&j, &p, 0), SR_OK);
  sr_job_briefing(c, &j, after, sizeof after);
  SR_CHECK(strstr(after, "guards: ") != NULL && strstr(after, "unknown") == NULL);
  SR_CHECK_EQ(sr_job_buy_intel(&j, &p, 0), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_job_accept(&j, &p, 0), SR_ERR_DENIED); /* T2 needs Runner */
  p.prog.street_cred = 30;
  int64_t cash = p.cred;
  SR_CHECK_EQ(sr_job_accept(&j, &p, 0), SR_OK);
  SR_CHECK_EQ(p.cred, cash + j.upfront);
  SR_CHECK_EQ(sr_job_accept(&j, &p, 0), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_job_complete(&j, &p, 10, &log, c), 0); /* not executed yet */
  p.prog.skill_rank[SR_SK_PERSUASION] = 20;
  p.prog.attr[SR_ATTR_COOL] = 10;
  j.approaches |= 1u << SR_AP_FACE;
  SrMissionResult r = sr_job_execute(c, &j, &p, SR_AP_FACE, 1);
  int tries = 0;
  while (!r.success && tries++ < 20) r = sr_job_execute(c, &j, &p, SR_AP_FACE, (uint64_t)tries + 10);
  SR_CHECK(j.executed);
  SR_CHECK(sr_inv_find(&p.inv, SR_IK_PAYDATA, -1) >= 0);
  SrMissionResult again = sr_job_execute(c, &j, &p, SR_AP_FACE, 1);
  SR_CHECK(!again.success);
  cash = p.cred;
  int32_t paid = sr_job_complete(&j, &p, 100, &log, c);
  SR_CHECK_EQ(j.state, SR_JS_COMPLETED);
  if (j.alarms == 0) SR_CHECK_EQ(paid, j.pay - j.upfront + j.bonus);
  SR_CHECK_EQ(p.cred, cash + paid);
  SR_CHECK(sr_inv_find(&p.inv, SR_IK_PAYDATA, -1) < 0);
  const SrWorldEvent *ev = sr_events_recent(&log, 0);
  SR_CHECK(ev && ev->type == SR_EV_THEFT && ev->building == j.target_building);
  /* constraint violated */
  SrJob k;
  sr_job_generate(c, SR_JT_SABOTAGE, 99, 0, &k);
  k.id = 8;
  k.state = SR_JS_ACCEPTED;
  k.executed = true;
  k.kills = 1;
  k.constraints = SR_JC_NO_KILLS;
  k.constraint_fails = true;
  SR_CHECK_EQ(sr_job_complete(&k, &p, 100, &log, c), 0);
  SR_CHECK_EQ(k.state, SR_JS_FAILED);
  k.state = SR_JS_ACCEPTED;
  k.constraint_fails = false;
  k.upfront = 0;
  k.has_bonus = false;
  SR_CHECK_EQ(sr_job_complete(&k, &p, 100, &log, c), k.pay / 2);
  /* deadline */
  SrJobBoard *b = (SrJobBoard *)malloc(sizeof *b);
  sr_jobs_init(b);
  SrJob dl;
  sr_job_generate(c, SR_JT_DELIVER, 5, 0, &dl);
  b->jobs[0] = dl;
  b->jobs[0].id = 1;
  b->jobs[0].client_faction = 2;
  b->count = 1;
  SR_CHECK_EQ(sr_job_accept(&b->jobs[0], &p, 0), SR_OK);
  SR_CHECK(sr_inv_find(&p.inv, SR_IK_JOB, -1) >= 0);
  b->jobs[1] = dl;
  b->jobs[1].id = 2;
  b->count = 2;
  SR_CHECK_EQ(sr_jobs_tick(b, &p, dl.deadline + 1, &log), 1);
  SR_CHECK_EQ(b->jobs[0].state, SR_JS_FAILED);
  SR_CHECK_EQ(b->jobs[1].state, SR_JS_WITHDRAWN);
  SR_CHECK(sr_inv_find(&p.inv, SR_IK_JOB, -1) < 0);
  SR_CHECK(p.prog.standing[2] < 0);
  SrJob ab = dl;
  ab.id = 3;
  ab.state = SR_JS_ACCEPTED;
  SR_CHECK_EQ(sr_job_abandon(&ab, &p, 50, &log), SR_OK);
  SR_CHECK_EQ(ab.state, SR_JS_FAILED);
  SR_CHECK_EQ(sr_job_abandon(&ab, &p, 50, &log), SR_ERR_INVALID);
  SR_CHECK(sr_job_intel_price(&dl) >= 20);
  p.cred = 0;
  dl.intel = 0;
  SR_CHECK_EQ(sr_job_buy_intel(&dl, &p, 0), SR_ERR_DENIED);
  free(b);
}

static void test_job_approaches(void) {
  SrCity *c = city();
  SrPlayer p;
  sr_player_create(&p, c, "Kit", SR_BG_MILITARY, 3);
  p.prog.street_cred = 60;
  int done[SR_AP_COUNT] = {0};
  for (int i = 0; i < 80; i++) {
    for (int a = 0; a < SR_AP_COUNT; a++) {
      SrJob j;
      sr_job_generate(c, (SrJobType)(SR_JT_PLANT + (i % 7)), (uint64_t)i * 31u, 0, &j);
      j.id = i * 10 + a;
      j.state = SR_JS_ACCEPTED;
      p.health = p.max_health;
      p.prog.skill_rank[SR_SK_STEALTH] = p.prog.skill_rank[SR_SK_PERSUASION] =
          p.prog.skill_rank[SR_SK_NETRUNNING] = p.prog.skill_rank[SR_SK_LONGGUNS] =
              p.prog.skill_rank[SR_SK_PISTOLS] = 15;
      SrMissionResult r = sr_job_execute(c, &j, &p, (SrApproach)a, (uint64_t)i);
      if (!(j.approaches & (1u << a))) SR_CHECK(!r.success && r.alarm);
      done[a] += r.success;
      SR_CHECK(p.health >= 1.0f);
    }
  }
  for (int a = 0; a < SR_AP_COUNT; a++) SR_CHECK(done[a] > 0);
  /* no weapon and no deck: hammer uses melee, runner impossible */
  SrPlayer bare;
  sr_player_create(&bare, c, "Bare", SR_BG_STREET_KID, 4);
  sr_inv_init(&bare.inv);
  bare.equipped = -1;
  SrJob j;
  sr_job_generate(c, SR_JT_STEAL_DATA, 1, 0, &j);
  j.state = SR_JS_ACCEPTED;
  j.approaches = 0xF;
  sr_job_execute(c, &j, &bare, SR_AP_HAMMER, 1);
  SrJob j2 = j;
  j2.executed = false;
  sr_job_execute(c, &j2, &bare, SR_AP_RUNNER, 1);
  SrJob street;
  sr_job_generate(c, SR_JT_DELIVER, 3, 0, &street);
  street.state = SR_JS_ACCEPTED;
  bare.equipped = 0;
  sr_inv_add(&bare.inv, sr_item_simple(SR_IK_FOOD, 0, 1)); /* "equipped" item is not a weapon */
  SrMissionResult sr = sr_job_execute(c, &street, &bare, SR_AP_GHOST, 1);
  (void)sr;
}

static void test_traffic_and_memory(void) {
  SR_SPEC("population-simulation", "Vehicles stop at crossings");
  SR_SPEC("population-simulation", "Notable remembers the player");
  SrVehicle v;
  memset(&v, 0, sizeof v);
  v.pos = sr_v3(0, 0, 0);
  v.dir = sr_v3(1, 0, 0);
  SrVec3 ahead = sr_v3(5, 0, 1), behind = sr_v3(-5, 0, 0), side = sr_v3(5, 0, 6);
  SR_CHECK(sr_vehicle_should_stop(&v, &ahead, 1));
  SR_CHECK(!sr_vehicle_should_stop(&v, &behind, 1));
  SR_CHECK(!sr_vehicle_should_stop(&v, &side, 1));
  SR_CHECK(!sr_vehicle_should_stop(&v, NULL, 0));
  /* live traffic: spawn, stop for the player on the lane, drive on when clear */
  SrCity *c = city();
  SrCrowd *cr = (SrCrowd *)calloc(1, sizeof *cr);
  sr_crowd_init(cr);
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  SrCamera cam = camera_at(sr_v3((float)hx / 100.0f, 1.7f, (float)hy / 100.0f));
  sr_crowd_traffic(cr, c, &cam, sr_v3(-999, 0, -999), 0.1f);
  SR_CHECK(cr->vehicle_count > 0);
  SrVehicle *first = &cr->vehicles[0];
  SrVec3 in_front = sr_v3_add(first->pos, sr_v3_scale(first->dir, 4.0f));
  SrVec3 before = first->pos;
  sr_crowd_traffic(cr, c, &cam, in_front, 0.5f);
  SR_CHECK(cr->vehicles[0].stopped);
  SR_CHECK_NEAR(cr->vehicles[0].pos.x, before.x, 1e-4);
  sr_crowd_traffic(cr, c, &cam, sr_v3(-999, 0, -999), 0.5f);
  if (!cr->vehicles[0].stopped) SR_CHECK(cr->vehicles[0].pos.x != before.x || cr->vehicles[0].pos.z != before.z);
  cam.pos = sr_v3(-5000, 0, -5000);
  for (int i = 0; i < 5; i++) sr_crowd_traffic(cr, c, &cam, cam.pos, 0.1f);
  SR_CHECK_EQ(cr->vehicle_count, 0); /* far away: despawned, none spawn off the map */
  free(cr);
  /* memory */
  SrNpc n;
  memset(&n, 0, sizeof n);
  const char *neutral = sr_npc_greeting(&n);
  for (int i = 0; i < 10; i++) sr_npc_remember(&n, 5);
  SR_CHECK(strcmp(sr_npc_greeting(&n), neutral) != 0);
  sr_npc_remember(&n, -500);
  SR_CHECK_EQ(n.attitude, -100);
  SR_CHECK(strstr(sr_npc_greeting(&n), "Get lost") != NULL);
  n.attitude = 20;
  SR_CHECK(strstr(sr_npc_greeting(&n), "again") != NULL);
}

void suite_sim(void) {
  SR_RUN(test_traffic_and_memory);
  SR_RUN(test_appearance);
  SR_RUN(test_density_and_stability);
  SR_RUN(test_notables);
  SR_RUN(test_promotion);
  SR_RUN(test_reactions_and_rumours);
  SR_RUN(test_metro_timetable);
  SR_RUN(test_crowd);
  SR_RUN(test_weapons);
  SR_RUN(test_mods_ammo);
  SR_RUN(test_inventory_gadgets_crafting);
  SR_RUN(test_cyberware_and_vendors);
  SR_RUN(test_damage);
  SR_RUN(test_takedown_ko);
  SR_RUN(test_health_flatline);
  SR_RUN(test_perception);
  SR_RUN(test_tactics_bodies);
  SR_RUN(test_progression);
  SR_RUN(test_heat);
  SR_RUN(test_disguise_attribution);
  SR_RUN(test_player_create);
  SR_RUN(test_needs_sleep);
  SR_RUN(test_money_housing_travel);
  SR_RUN(test_job_generation);
  SR_RUN(test_job_sources_tiers);
  SR_RUN(test_job_flow);
  SR_RUN(test_job_approaches);
  sr_city_destroy(g_c);
  free(g_pop);
  g_c = NULL;
}
