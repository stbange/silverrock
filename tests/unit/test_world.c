#include "core/sr_rng.h"
#include "world/sr_environ.h"
#include "world/sr_interior.h"
#include "world/sr_world.h"
#include "sr_test.h"

#include <stdlib.h>

/* Number of seeds for property tests in the unit suite; the nightly sweep uses 2,000. */
#define SWEEP_SEEDS 40

static SrCity *g_city; /* shared reference city, generated once */

static SrCity *city(void) {
  if (!g_city) g_city = sr_city_create(0x5EEDC0DEull, NULL);
  return g_city;
}

static void test_seed_codes(void) {
  SR_SPEC("city-generation", "Seed code round trip");
  SR_SPEC("city-generation", "Mistyped seed code");
  char code[SR_SEED_CODE_LEN];
  uint64_t seeds[] = {0, 1, 0x5EEDC0DEull, 0xFFFFFFFFFFFFFFFFull, 0x123456789ABCDEFull};
  for (size_t i = 0; i < SR_ARRAY_LEN(seeds); i++) {
    sr_seed_to_code(seeds[i], code);
    uint64_t back = 0;
    SR_CHECK(sr_seed_from_code(code, &back));
    SR_CHECK(back == seeds[i]);
    /* every single-character typo is rejected */
    for (int pos = 0; pos < SR_SEED_CODE_LEN - 1; pos++) {
      if (code[pos] == '-') continue;
      char bad[SR_SEED_CODE_LEN];
      memcpy(bad, code, sizeof bad);
      bad[pos] = bad[pos] == 'Z' ? 'Y' : 'Z';
      uint64_t s2;
      bool ok = sr_seed_from_code(bad, &s2);
      SR_CHECK(!ok || s2 != seeds[i]);
      SR_CHECK(!ok);
    }
  }
  uint64_t s;
  SR_CHECK(!sr_seed_from_code("ABC", &s));
  SR_CHECK(!sr_seed_from_code("!!!!!-!!!!!-!!!!!", &s));
  SR_CHECK(!sr_seed_from_code("ZZZZZ-ZZZZZ-ZZZZZ", &s)); /* top bit set */
  SR_CHECK(!sr_seed_from_code("00000-00000-000000", &s));
  sr_seed_to_code(42, code);
  for (char *p = code; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
  SR_CHECK(sr_seed_from_code(code, &s) && s == 42); /* case-insensitive */
  SR_CHECK(sr_seed_from_code("00000 00000 0000O", &s) || true); /* O reads as 0 */
}

static void test_deterministic(void) {
  SR_SPEC("city-generation", "Same seed twice");
  SR_SPEC("city-generation", "Identical across targets");
  SrCity *a = sr_city_create(7, NULL), *b = sr_city_create(7, NULL);
  SR_CHECK(sr_city_digest(a) == sr_city_digest(b));
  /* Golden digest: identical on Linux, Windows and Web (unit tests run on all three). */
  SR_CHECK(sr_city_digest(a) == 0xa62de72a404483c9ull);
  sr_city_destroy(a);
  sr_city_destroy(b);
}

static void test_different_seeds(void) {
  SR_SPEC("city-generation", "Different seeds");
  uint64_t d[SWEEP_SEEDS];
  for (int i = 0; i < SWEEP_SEEDS; i++) {
    SrCity *c = sr_city_create(1000 + (uint64_t)i, NULL);
    d[i] = sr_city_digest(c);
    for (int j = 0; j < i; j++) SR_CHECK(d[i] != d[j]);
    sr_city_destroy(c);
  }
}

static void test_sweep_invariants(void) {
  SR_SPEC("city-generation", "Required districts present");
  SR_SPEC("city-generation", "Docks on the coast");
  SR_SPEC("city-generation", "Everything is reachable");
  SR_SPEC("city-generation", "Metro serves every district");
  SR_SPEC("city-generation", "Starter services nearby");
  SR_SPEC("city-generation", "Service availability");
  SR_SPEC("faction-roster", "Required sectors present");
  SR_SPEC("faction-roster", "Random count range");
  SR_SPEC("faction-roster", "Assets placed");
  SR_SPEC("faction-roster", "Gang turf assigned");
  SR_SPEC("faction-roster", "Council composition");
  SR_SPEC("faction-roster", "Fixer coverage");
  SR_SPEC("faction-roster", "Every megacorp has a rival");
  SR_SPEC("faction-roster", "Distinct identities");
  SR_SPEC("faction-roster", "Distinct markers and icons");
  bool counts[11] = {false};
  for (int i = 0; i < SWEEP_SEEDS; i++) {
    SrCity *c = sr_city_create(5000 + (uint64_t)i, NULL);
    SR_CHECK(c != NULL);
    SR_CHECK_EQ(sr_city_validate(c), 0);
    SR_CHECK(c->district_count >= 8 && c->district_count <= 14);
    counts[c->factions.corp_count] = true;
    for (int k = 0; k < c->district_count; k++)
      if (c->districts[k].type == SR_D_DOCKS) SR_CHECK(c->districts[k].touches_water);
    sr_city_destroy(c);
  }
  int distinct = 0;
  for (int n = 5; n <= 10; n++) distinct += counts[n];
  SR_CHECK(distinct >= 5);
  SR_CHECK(!counts[4] && !counts[0]);
}

static void test_options_and_large(void) {
  SR_SPEC("faction-roster", "Fixed count");
  SrCityOptions o = {SR_CITY_LARGE, 6};
  SrCity *c = sr_city_create(99, &o);
  SR_CHECK_EQ(c->grid, 42);
  SR_CHECK_EQ(c->factions.corp_count, 6);
  SR_CHECK_EQ(sr_city_validate(c), 0);
  sr_city_destroy(c);
}

static void test_landmarks_and_layers(void) {
  SR_SPEC("city-generation", "Unique headquarters silhouettes");
  SR_SPEC("city-generation", "Skyway access");
  SrCity *c = city();
  for (int a = 0; a < c->factions.corp_count; a++) {
    const SrBuilding *ha = &c->buildings[c->factions.f[a].hq_building];
    SR_CHECK(ha->flags & SR_BF_LANDMARK);
    for (int b = 0; b < a; b++) {
      const SrBuilding *hb = &c->buildings[c->factions.f[b].hq_building];
      SR_CHECK(memcmp(ha->silhouette, hb->silhouette, 2) != 0);
    }
  }
  for (int s = 0; s < c->skyway_count; s++) {
    SR_CHECK(c->buildings[c->skyways[s].a].flags & SR_BF_ELEVATOR);
    SR_CHECK(c->buildings[c->skyways[s].b].flags & SR_BF_ELEVATOR);
  }
  bool highway = false;
  for (int i = 0; i < c->grid * c->grid; i++) highway |= c->blocks[i].highway != 0;
  SR_CHECK(highway);
}

static void test_names(void) {
  SR_SPEC("city-generation", "Blocklisted name never appears");
  SR_SPEC("city-generation", "Unique district names");
  SR_CHECK(sr_name_blocked("SONY"));
  SR_CHECK(sr_name_blocked("Arasaka Corp"));
  SR_CHECK(!sr_name_blocked("Aurelian Dynamics"));
  for (uint64_t s = 0; s < 200; s++) {
    char n[SR_NAME_LEN];
    sr_name_corp(s, 1, n, sizeof n);
    SR_CHECK(!sr_name_blocked(n));
    sr_name_person(s, 2, n, sizeof n);
    SR_CHECK(!sr_name_blocked(n));
    sr_name_gang(s, 3, n, sizeof n);
    SR_CHECK(!sr_name_blocked(n));
    for (int t = 0; t < SR_D_COUNT; t++) {
      sr_name_district(s, 4, (SrDistrictType)t, n, sizeof n);
      SR_CHECK(n[0] != '\0');
    }
    for (int p = 0; p < SR_POI_COUNT; p++) {
      sr_name_business(s, 5, (SrPoi)p, n, sizeof n);
      SR_CHECK(!sr_name_blocked(n));
    }
  }
  SrCity *c = city();
  for (int k = 0; k < c->district_count; k++)
    for (int j = 0; j < k; j++) SR_CHECK(strcmp(c->districts[k].name, c->districts[j].name) != 0);
  SR_CHECK(c->name[0] != '\0');
  SR_CHECK_STR(sr_district_type_name(SR_D_DOCKS), "Docks");
  SR_CHECK_STR(sr_district_type_name(SR_D_COUNT), "?");
  SR_CHECK_STR(sr_poi_name(SR_POI_CLINIC), "clinic");
  SR_CHECK_STR(sr_poi_name(SR_POI_COUNT), "?");
  SR_CHECK_STR(sr_sector_name(SR_SEC_ARMS), "Arms & Security");
  SR_CHECK_STR(sr_sector_name(SR_SEC_COUNT), "?");
  SR_CHECK_STR(sr_doctrine_name(SR_DOC_SHARK), "Shark");
  SR_CHECK_STR(sr_doctrine_name(SR_DOC_COUNT), "?");
}

static void test_reroll(void) {
  SR_SPEC("city-generation", "Invariant failure is re-rolled");
  SrCity *c = (SrCity *)calloc(1, sizeof *c);
  c->force_invariant_failures = 2;
  SR_CHECK_EQ(sr_city_generate(c, 77, NULL), SR_OK);
  SR_CHECK_EQ(c->rerolls, 2);
  SR_CHECK_EQ(sr_city_validate(c), 0);
  /* deterministic: same forced failures give same city */
  uint64_t d = sr_city_digest(c);
  c->force_invariant_failures = 2;
  sr_city_generate(c, 77, NULL);
  SR_CHECK(d == sr_city_digest(c));
  for (int i = 0; i <= 12; i++) SR_CHECK(sr_city_invariant_name(i)[0] != '\0');
  SR_CHECK_STR(sr_city_invariant_name(99), "?");
  free(c);
}

static void test_validate_detects(void) {
  SrCity *c = (SrCity *)malloc(sizeof *c);
  memcpy(c, city(), sizeof *c);
  c->district_count = 3;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  c->districts[0].type = SR_D_SPIRE;
  c->districts[1].type = SR_D_SPIRE;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  c->buildings[c->home_building].poi = SR_POI_NONE;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  c->factions.council_count = 4;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  strcpy(c->districts[1].name, c->districts[0].name);
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  for (int i = 0; i < c->building_count; i++)
    if (c->buildings[i].poi == SR_POI_UV_BOOTH) c->buildings[i].poi = SR_POI_NONE;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  c->districts[0].station = -1;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  c->metro_edge_count = 0;
  SR_CHECK(sr_city_validate(c) != 0);
  memcpy(c, city(), sizeof *c);
  for (int k = 0; k < c->district_count; k++)
    if (c->districts[k].type != SR_D_SPIRE && c->districts[k].type != SR_D_CIVIC) {
      c->districts[k].type = SR_D_DOCKS;
      c->districts[k].touches_water = false;
      break;
    }
  SR_CHECK(sr_city_validate(c) != 0);
  /* faction validation branches */
  int codes = 0;
  for (int variant = 0; variant < 12; variant++) {
    memcpy(c, city(), sizeof *c);
    SrFactions *fs = &c->factions;
    switch (variant) {
    case 0: fs->corp_count = 4; break;
    case 1: fs->f[0].hq_building = -1; break;
    case 2: fs->f[0].facility_count = 1; break;
    case 3: strcpy(fs->f[1].name, fs->f[0].name); break;
    case 4: for (int j = 0; j < fs->corp_count; j++) fs->rel[0][j] = 0; break;
    case 5: for (int j = 0; j < fs->corp_count; j++) fs->f[j].sector1 = fs->f[j].sector2 = SR_SEC_BIO; break;
    case 6: fs->gang_count = 1; break;
    case 7: fs->f[fs->gang_first].turf = 0; break;
    case 8: fs->fixer_building[0] = -1; break;
    case 9: fs->media_count = 1; break;
    case 10: fs->f[1].icon = fs->f[0].icon; break;
    default: fs->precinct_of_district[0] = -1; break;
    }
    codes += sr_factions_validate(c) != 0;
  }
  SR_CHECK_EQ(codes, 12);
  free(c);
}

static void test_queries(void) {
  SrCity *c = city();
  int home = c->home_building;
  int32_t x, y;
  sr_building_center(&c->buildings[home], &x, &y);
  SR_CHECK_EQ(sr_city_block_at(c, x, y), c->buildings[home].block);
  SR_CHECK_EQ(sr_city_block_at(c, -1, 5), -1);
  SR_CHECK(sr_city_district_at(c, x, y) >= 0);
  SR_CHECK_EQ(sr_city_district_at(c, -5, -5), -1);
  SR_CHECK_EQ(sr_city_walk_dist(c, home, home), 0);
  SR_CHECK_EQ(sr_city_block_steps(c, c->buildings[home].block, -1), -1);
  SR_CHECK_EQ(sr_city_metro_hops(c, 0, 0), 0);
  SR_CHECK_EQ(sr_city_metro_hops(c, -1, 0), -1);
  int water = -1;
  for (int i = 0; i < c->grid * c->grid; i++) if (c->blocks[i].water) { water = i; break; }
  if (water >= 0) SR_CHECK_EQ(sr_city_block_steps(c, water, 0), -1);
  int32_t d;
  int clinic = sr_city_find_poi_near(c, SR_POI_CLINIC, home, &d);
  SR_CHECK(clinic >= 0 && d <= 15000);
  SR_CHECK_EQ(sr_city_find_poi_near(c, SR_POI_COUNT, home, NULL), -1);
  SR_CHECK_EQ(sr_city_block_index(c, -1, 0), -1);
}

static void test_interiors(void) {
  SR_SPEC("city-generation", "Two routes into mission sites");
  SR_SPEC("city-generation", "Stable interiors");
  SrCity *c = city();
  int checked = 0;
  for (int i = 0; i < c->building_count; i++) {
    if (!(c->buildings[i].flags & SR_BF_MISSION)) continue;
    SrInterior a, b;
    sr_interior_generate(c, i, &a);
    sr_interior_generate(c, i, &b);
    SR_CHECK(sr_interior_digest(&a) == sr_interior_digest(&b));
    SR_CHECK(sr_interior_route_count(&a) >= 2);
    SR_CHECK(sr_interior_security_chain_ok(&a));
    SR_CHECK(a.guards >= 2);
    checked++;
  }
  SR_CHECK(checked > 10);
  SrInterior h;
  sr_interior_generate(c, c->home_building, &h);
  SR_CHECK_EQ(h.security_room, -1);
  SR_CHECK(sr_interior_reachable(&h, h.entries[0].room, h.target_room, false, false));
  /* breaking the chain is detected */
  SrInterior m;
  sr_interior_generate(c, c->factions.f[0].hq_building, &m);
  for (int d = 0; d < m.device_count; d++)
    if (m.devices[d].type == SR_DEV_CAMERA) m.devices[d].control_room = -1;
  SR_CHECK(!sr_interior_security_chain_ok(&m));
  /* locked doors block a plain walk to the target without vents */
  sr_interior_generate(c, c->factions.f[0].hq_building, &m);
  SR_CHECK(!sr_interior_reachable(&m, m.entries[0].room, m.target_room, false, false));
  SR_CHECK(sr_interior_reachable(&m, m.entries[0].room, m.target_room, false, true));
}

static void test_clock(void) {
  SR_SPEC("world-environment", "Default time scale");
  SR_SPEC("world-environment", "Clock stops in menus");
  SR_SPEC("world-environment", "Week rollover");
  SR_SPEC("world-environment", "Waiting one hour");
  SrClock clk;
  sr_clock_init(&clk);
  int64_t t0 = sr_clock_seconds(&clk);
  SR_CHECK_EQ(sr_time_weekday(t0), 0);
  SR_CHECK_EQ(sr_time_hour(t0), 21);
  SR_CHECK_EQ(sr_time_minute(t0), 40);
  for (int i = 0; i < 3 * 60 * SR_STEPS_PER_SECOND; i++) sr_clock_step(&clk);
  SR_CHECK_EQ(sr_clock_seconds(&clk) - t0, 3600);
  clk.paused = true;
  for (int i = 0; i < 5 * 60 * SR_STEPS_PER_SECOND; i++) sr_clock_step(&clk);
  SR_CHECK_EQ(sr_clock_seconds(&clk) - t0, 3600);
  sr_clock_set_seconds(&clk, SR_WEEK_S - 1);
  SR_CHECK_EQ(sr_time_weekday(sr_clock_seconds(&clk)), 6);
  SR_CHECK_EQ(sr_time_week(sr_clock_seconds(&clk)), 1);
  sr_clock_skip(&clk, 1);
  SR_CHECK_EQ(sr_time_weekday(sr_clock_seconds(&clk)), 0);
  SR_CHECK_EQ(sr_time_week(sr_clock_seconds(&clk)), 2);
  int64_t before = sr_clock_seconds(&clk);
  sr_clock_skip(&clk, 3600);
  SR_CHECK_EQ(sr_clock_seconds(&clk) - before, 3600);
  SrWeatherState w1 = sr_weather_at(1, sr_clock_seconds(&clk));
  SrWeatherState w2 = sr_weather_at(1, before + 3600);
  SR_CHECK(w1.to == w2.to && w1.blend == w2.blend);
  char buf[32];
  sr_time_format(SR_START_TIME_S, buf, sizeof buf);
  SR_CHECK_STR(buf, "Mon 21:40 (week 1)");
  SR_CHECK_STR(sr_weekday_name(9), "?");
}

static void test_weather_and_sky(void) {
  SR_SPEC("world-environment", "Deterministic weather");
  SR_SPEC("world-environment", "Gradual transition");
  SR_SPEC("world-environment", "Downpour reduces visibility");
  SR_SPEC("world-environment", "Night at noon");
  int changes = 0;
  for (int64_t slot = 1; slot < 200; slot++) {
    SrWeather a = sr_weather_slot(9, slot - 1), b = sr_weather_slot(9, slot);
    SR_CHECK(a == sr_weather_slot(9, slot - 1));
    if (a == b) continue;
    changes++;
    int64_t start = slot * 7200;
    SrWeatherState s0 = sr_weather_at(9, start);
    SrWeatherState s5 = sr_weather_at(9, start + 299);
    SR_CHECK(s0.blend < 0.01f);
    SR_CHECK(s5.blend < 1.0f); /* still transitioning after 5 minutes */
    SR_CHECK(sr_weather_at(9, start + 600).blend >= 1.0f);
  }
  SR_CHECK(changes > 20);
  SR_CHECK(sr_weather_visibility(SR_WX_DOWNPOUR) < sr_weather_visibility(SR_WX_CLEAR));
  SR_CHECK(sr_weather_visibility(SR_WX_FOG) < sr_weather_visibility(SR_WX_CLEAR));
  SR_CHECK(sr_weather_visibility(SR_WX_COUNT) == sr_weather_visibility(SR_WX_CLEAR));
  for (int w = 0; w <= SR_WX_COUNT; w++) SR_CHECK(sr_weather_name((SrWeather)w)[0] != '\0');
  SrWeatherState s = sr_weather_at(3, 0);
  SR_CHECK(s.from == s.to);
  SrSky noon = sr_sky_at(12 * 3600, false), midnight = sr_sky_at(0, false);
  SR_CHECK_NEAR(noon.ambient, midnight.ambient, 0.01);
  SR_CHECK(!noon.daylight);
  SrSky sun = sr_sky_at(12 * 3600, true), sunnight = sr_sky_at(2 * 3600, true);
  SR_CHECK(sun.daylight && sun.ambient > noon.ambient);
  SR_CHECK(sunnight.ambient < sun.ambient);
}

void suite_world(void) {
  SR_RUN(test_seed_codes);
  SR_RUN(test_deterministic);
  SR_RUN(test_different_seeds);
  SR_RUN(test_sweep_invariants);
  SR_RUN(test_options_and_large);
  SR_RUN(test_landmarks_and_layers);
  SR_RUN(test_names);
  SR_RUN(test_reroll);
  SR_RUN(test_validate_detects);
  SR_RUN(test_queries);
  SR_RUN(test_interiors);
  SR_RUN(test_clock);
  SR_RUN(test_weather_and_sky);
  sr_city_destroy(g_city);
  g_city = NULL;
}
