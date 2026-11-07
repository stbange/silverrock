/* Playability pass after M5 (improve-playability): every scenario of that change. */
#include "core/sr_rng.h"
#include "game/sr_script.h"
#include "sr_test.h"
#include "ui/sr_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SrGame *game(uint64_t seed) {
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, SR_BG_STREET_KID, "Tester");
  g->crowd_enabled = false;
  sr_game_leave(g);
  return g;
}

static int facility_of(const SrCity *c, int f) {
  return c->factions.f[f].facility_count > 0 ? c->factions.f[f].facilities[0] : c->factions.f[f].hq_building;
}

/* ---- phase 1 --------------------------------------------------------------------------------- */

static void test_headlines(void) {
  SR_SPEC("news-media", "Headline from a job");
  SR_SPEC("news-media", "Outlet voice");
  SrGame *g = game(41);
  SrCity *c = g->city;
  int corp = 1, m = c->factions.media_first;
  SrWorldEvent e = {SR_EV_SABOTAGE, 5000, 2, facility_of(c, corp), (int8_t)corp, -1, true, "the Sabotage at X Site 4"};
  char a[64], b[64];
  for (int k = 0; k < 40; k++) {
    e.time = 5000 + k * 977;
    int owner = c->factions.f[m].owner;
    c->factions.f[m].owner = -1; /* an independent station */
    sr_news_event_headline(a, sizeof a, c, &e, m, -1);
    sr_news_event_headline(b, sizeof b, c, &e, -1, -1); /* the pirate feed */
    c->factions.f[m].owner = (int8_t)owner;
    SR_CHECK(strstr(a, "Site") == NULL && strstr(a, "the Sabotage") == NULL);
    SR_CHECK(strlen(a) <= 63 && strlen(b) <= 63);
    SR_CHECK(strcmp(a, b) != 0);
    SR_CHECK(a[strlen(a) - 1] != ' ');
  }
  /* the culprit as far as the evidence goes; long names never cut a word */
  e.blamed_faction = -2;
  sr_news_event_headline(a, sizeof a, c, &e, m, -2);
  SR_CHECK(strstr(a, "unner") != NULL);
  sr_news_event_headline(a, sizeof a, c, &e, m, 3);
  SR_CHECK(strstr(a, "suspected") != NULL || strstr(a, "behind") != NULL);
  char h[64];
  sr_headline(h, sizeof h, "%1 and %2 and %3 meet in a very long headline indeed", "Castellan-Adeyemi Holdings",
              "Vantor Orbital Systems", "Quintero-Delacroix");
  SR_CHECK(strlen(h) <= 63);
  SR_CHECK(h[strlen(h) - 1] != ' ' && h[strlen(h) - 1] != '-');
  sr_headline(h, sizeof h, "%1", NULL, NULL, NULL);
  SR_CHECK_STR(h, "");
  /* every event kind has a headline */
  static const SrEventType kinds[] = {SR_EV_THEFT,   SR_EV_JOB_DONE, SR_EV_JOB_FAILED,  SR_EV_GUNFIRE,
                                      SR_EV_KILLING, SR_EV_BLACKOUT, SR_EV_CRIME_REPORT};
  for (size_t k = 0; k < SR_ARRAY_LEN(kinds); k++) {
    SrWorldEvent x = e;
    x.type = kinds[k];
    for (int sub = -1; sub <= 1; sub++)
      for (int perp = -1; perp <= 1; perp++) {
        x.victim_faction = (int8_t)(sub < 0 ? -1 : c->factions.gang_first);
        x.building = sub > 0 ? -1 : facility_of(c, corp);
        sr_news_event_headline(a, sizeof a, c, &x, perp ? m : -1, perp ? c->factions.gang_first + 1 : -1);
        SR_CHECK(a[0] != '\0' && strlen(a) <= 63);
      }
  }
  sr_game_destroy(g);
}

static void test_patron_warning(void) {
  SR_SPEC("job-system", "Job against the patron");
  SrGame *g = game(42);
  g->chess.story.offer_open = true;
  sr_game_accept_retainer(g);
  int patron = g->chess.story.patron;
  SrJob j;
  SR_CHECK(sr_job_for_contract(g->city, SR_JT_PLANT, facility_of(g->city, patron), (patron + 1) % 5, patron, 9,
                               sr_game_time(g), &j));
  j.tier = 0;
  SrJob *pj = sr_jobs_push(&g->jobs, &j);
  SR_CHECK(sr_game_job_against_patron(g, pj));
  int msgs = g->player.msg_count;
  SR_CHECK_EQ(sr_game_accept(g, pj->id), SR_ERR_CONFIRM); /* the warning first */
  SR_CHECK(g->player.msg_count > msgs);
  SR_CHECK_EQ(pj->state, SR_JS_OFFERED);
  SR_CHECK_EQ(sr_game_accept(g, pj->id), SR_OK); /* then it goes ahead */
  SR_CHECK_STR(sr_result_name(SR_ERR_CONFIRM), "confirm");
  /* the patron's own gangs count too */
  for (int i = 0; i < g->chess.link_count; i++)
    if (g->chess.links[i].type == SR_LINK_FUNDS) {
      SrJob k = j;
      g->chess.story.patron = g->chess.links[i].a;
      k.target_faction = g->chess.links[i].b;
      SR_CHECK(sr_game_job_against_patron(g, &k));
      break;
    }
  SrJob none = j;
  none.target_faction = -1;
  SR_CHECK(!sr_game_job_against_patron(g, &none));
  sr_game_destroy(g);
}

static void test_defection_limits(void) {
  SR_SPEC("story-campaign", "Defection");
  SR_SPEC("story-campaign", "No way back");
  SR_SPEC("story-campaign", "One defection");
  SrGame *g = game(43);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  int old = st->patron, rival = (old + 1) % g->city->factions.corp_count;
  st->rival_offer = (int8_t)rival;
  SR_CHECK_EQ(sr_game_accept_offer(g), SR_OK);
  SR_CHECK_EQ(st->patron, rival);
  SR_CHECK_EQ(st->trust, SR_TRUST_DEFECT);
  SR_CHECK_EQ(st->hostile, old);
  SR_CHECK(sr_story_blacklisted(&g->chess, old));
  SR_CHECK(!sr_story_defect(&g->chess, g->city, old, 0)); /* no way back */
  /* Act II with a loyal new patron: nobody else calls */
  st->act = SR_ACT_KNIGHT;
  st->next_counter = 0;
  for (int64_t t = sr_game_time(g); t < sr_game_time(g) + 8 * SR_WEEK_S; t += SR_TICK_S) {
    sr_story_tick(&g->chess, g->city, g->pop, t);
    SR_CHECK(st->rival_offer < 0);
  }
  /* the new patron turns on the player: offers come again, never from a betrayed corp */
  st->trust = 1;
  sr_story_discovered(&g->chess, g->city, sr_game_time(g));
  SR_CHECK(st->patron < 0);
  SR_CHECK(sr_story_blacklisted(&g->chess, rival));
  for (int64_t t = sr_game_time(g); t < sr_game_time(g) + 30 * SR_WEEK_S && st->rival_offer < 0; t += SR_TICK_S)
    sr_story_tick(&g->chess, g->city, g->pop, t);
  SR_CHECK(st->rival_offer >= 0);
  SR_CHECK(!sr_story_blacklisted(&g->chess, st->rival_offer));
  sr_game_destroy(g);
}

static void test_mission_risk(void) {
  SR_SPEC("job-system", "Resolved mission can flatline");
  SR_SPEC("job-system", "Story preset cap");
  int deaths = 0, story_ok = 1;
  for (int diff = 0; diff < 2; diff++)
    for (int k = 0; k < 60; k++) {
      SrGame *g = game(500 + (uint64_t)k);
      g->difficulty = diff == 0 ? SR_DIFF_STANDARD : SR_DIFF_STORY;
      g->player.prog.street_cred = 50;
      SrJob j;
      sr_job_for_contract(g->city, SR_JT_RAID, -1, -1, -1, (uint64_t)k, sr_game_time(g), &j);
      j.tier = 3;
      SrJob *pj = sr_jobs_push(&g->jobs, &j);
      sr_game_accept(g, pj->id);
      g->player.health = 15.0f;
      float max = g->player.max_health;
      int64_t cash = g->player.cred;
      SrMissionResult r = sr_game_execute(g, pj->id, SR_AP_HAMMER);
      if (diff == 0 && g->player.cred < cash && r.damage >= 15.0f) deaths++; /* woke up robbed or billed */
      if (diff == 1 && r.damage > 0.6f * max + 0.01f) story_ok = 0;
      sr_game_destroy(g);
    }
  SR_CHECK(deaths > 0);
  SR_CHECK(story_ok);
}

/* ---- phase 2 --------------------------------------------------------------------------------- */

static int home_fixer(const SrGame *g) {
  return g->city->factions.fixer_building[g->city->blocks[g->city->buildings[g->player.home].block].district];
}

static void test_offers_from_contracts(void) {
  SR_SPEC("faction-simulation", "Fixer offers come from contracts");
  SR_SPEC("faction-simulation", "Operations on demand");
  SrGame *g = game(44);
  int ops = g->chess.op_count;
  int ids[SR_FIXER_OFFERS];
  int n = sr_game_fixer(g, home_fixer(g), ids);
  SR_CHECK(n >= 1 && n <= SR_FIXER_OFFERS);
  SR_CHECK(g->chess.op_count > ops); /* nothing around yet: operations started on demand */
  for (int i = 0; i < n; i++) {
    SrContract *k = sr_chess_contract_for_job(&g->chess, ids[i]);
    SR_CHECK(k != NULL);
    if (k) SR_CHECK(sr_chess_op(&g->chess, k->op) != NULL);
  }
  /* at most one operation on demand per district and day */
  int d = g->city->blocks[g->city->buildings[g->player.home].block].district;
  SR_CHECK_EQ(sr_chess_demand_op(&g->chess, g->city, d, 0, sr_game_time(g)), -1);
  SR_CHECK(sr_chess_demand_op(&g->chess, g->city, d, 0, sr_game_time(g) + 86400) >= 0);
  SR_CHECK_EQ(sr_chess_demand_op(&g->chess, g->city, -1, 0, 0), -1);
  for (int tier = 1; tier <= 3; tier++)
    SR_CHECK(sr_chess_demand_op(&g->chess, g->city, (d + tier) % g->city->district_count, tier, sr_game_time(g)) >= 0);
  /* completing an offer advances its operation */
  SrJob *j = sr_jobs_get(&g->jobs, ids[0]);
  SrContract *k = sr_chess_contract_for_job(&g->chess, ids[0]);
  int32_t kid = k->id;
  SR_CHECK_EQ(sr_game_accept(g, ids[0]), SR_OK);
  j->executed = true;
  sr_game_report(g, ids[0]);
  sr_game_skip(g, 120, false);
  SR_CHECK(!sr_chess_contract(&g->chess, kid)->open);
  SR_CHECK(sr_chess_contract(&g->chess, kid)->by_player);
  SR_CHECK_STR(sr_op_name(SR_OP_ERRAND), "errand");
  sr_game_destroy(g);
}

static void test_terminal_jobs(void) {
  SR_SPEC("job-system", "Job board listings");
  SR_SPEC("job-system", "Accepting from the terminal");
  SrGame *g = game(45);
  g->chess.story.offer_open = true;
  sr_game_accept_retainer(g);
  int patron = g->chess.story.patron;
  int op = sr_chess_start_op(&g->chess, g->city, SR_OP_THEFT, patron, (patron + 1) % g->city->factions.corp_count,
                             facility_of(g->city, (patron + 1) % g->city->factions.corp_count), -1, sr_game_time(g));
  SR_CHECK(op > 0);
  g->player.prog.street_cred = 30;
  sr_game_skip(g, 3600, false); /* contracts are posted on the hour */
  int32_t ids[32];
  int n = sr_game_terminal_jobs(g, ids, 32);
  SR_CHECK(n >= 1);
  SrJob *j = sr_jobs_get(&g->jobs, ids[0]);
  SR_CHECK(j && j->client_building == g->player.home); /* the handler's work first */
  /* accept on the Jobs tab, without visiting anyone */
  g->screen = SR_SCREEN_TERMINAL;
  g->term_tab = SR_TAB_JOBS;
  g->term_sel = 0;
  SR_CHECK(sr_game_term_rows(g) >= 1);
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(j->state, SR_JS_ACCEPTED);
  n = sr_game_terminal_jobs(g, ids, 32);
  bool listed = false;
  for (int i = 0; i < n; i++) listed |= ids[i] == j->id;
  SR_CHECK(listed); /* accepted jobs stay on the list */
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  g->chess.camp.hunter = (int8_t)patron;
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  /* the fixer menu draws its rows and the briefing */
  g->screen = SR_SCREEN_PLAY;
  SR_CHECK(sr_game_jobs_open(g, home_fixer(g)));
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  sr_game_shop_close(g);
  sr_game_destroy(g);
}

static void test_discovery(void) {
  SR_SPEC("story-campaign", "Betrayal discovered");
  SR_SPEC("story-campaign", "Framed betrayal stays hidden");
  int found = 0;
  for (int k = 0; k < 12; k++) {
    SrGame *g = game(600 + (uint64_t)k);
    SrStory *st = &g->chess.story;
    st->offer_open = true;
    sr_game_accept_retainer(g);
    int trust = st->trust;
    int64_t t = sr_game_time(g);
    sr_story_job_done(&g->chess, g->city, -1, st->patron, true, 3, false, t); /* loud: certain */
    SR_CHECK_EQ(st->trust, trust);                                            /* not yet */
    sr_story_tick(&g->chess, g->city, g->pop, t + 3 * 86400 + 1);
    found += st->trust <= trust - SR_BETRAYAL;
    /* framed: never found out */
    int before = st->trust;
    sr_story_job_done(&g->chess, g->city, -1, st->patron, true, 3, true, t);
    sr_story_tick(&g->chess, g->city, g->pop, t + 6 * 86400);
    SR_CHECK_EQ(st->trust, before);
    sr_game_destroy(g);
  }
  SR_CHECK_EQ(found, 12);
}

static void test_hunters(void) {
  SR_SPEC("law-and-heat", "Hostile patron sends hunters");
  SR_SPEC("law-and-heat", "Paying off the hunters");
  SR_SPEC("law-and-heat", "Heat brings raids");
  SR_SPEC("law-and-heat", "Bounty listing");
  SrGame *g = game(46);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  int patron = st->patron;
  st->trust = 10;
  sr_story_discovered(&g->chess, g->city, sr_game_time(g));
  SR_CHECK_EQ(st->hostile, patron);
  /* the hunt operation completes: hunters strike the next time the player is out */
  int64_t t0 = sr_game_time(g);
  for (int h = 0; h < 48 && !g->enc.active; h++) sr_game_skip(g, 3600, false);
  SR_CHECK(g->enc.active);
  SR_CHECK(sr_game_time(g) - t0 <= 2 * 86400);
  SR_CHECK_EQ(g->enc.faction, patron);
  SR_CHECK_EQ(g->chess.camp.hunter, patron); /* listed as a bounty */
  g->player.cred = 0;
  SR_CHECK_EQ(sr_game_encounter(g, SR_ENC_PAY), SR_ERR_DENIED);
  g->player.cred = 100000;
  int64_t cash = g->player.cred;
  float hp = g->player.health;
  SR_CHECK_EQ(sr_game_encounter(g, SR_ENC_PAY), SR_OK);
  SR_CHECK_EQ(g->player.cred, cash - g->enc.payoff);
  SR_CHECK(g->player.health == hp);
  SR_CHECK(!g->enc.active);
  SR_CHECK(g->chess.camp.hunter < 0);
  SR_CHECK_EQ(sr_game_encounter(g, SR_ENC_FIGHT), SR_ERR_INVALID);
  /* fight and flight */
  for (int c = SR_ENC_FIGHT; c <= SR_ENC_FLEE; c++)
    for (int k = 0; k < 6; k++) {
      g->player.health = g->player.max_health;
      sr_game_encounter_start(g, (patron + 1) % g->city->factions.corp_count, false);
      SR_CHECK(g->enc.active);
      sr_game_skip(g, 600, false);
      SR_CHECK_EQ(sr_game_encounter(g, (SrEncChoice)c), SR_OK);
    }
  /* Heat 4: a raid on the apartment at night */
  int pol = sr_game_police(g);
  sr_game_teleport_building(g, g->player.home);
  sr_game_enter(g, g->player.home, SR_ENTRY_FRONT);
  int64_t night = (sr_game_time(g) / 86400 + 1) * 86400 + 23 * 3600;
  g->last_raid = -SR_WEEK_S;
  sr_game_skip(g, night - 60 - sr_game_time(g), true);
  g->heat.level[pol] = 4; /* wanted, asleep at home */
  g->heat.decay_from[pol] = sr_game_time(g) + 86400;
  sr_game_skip(g, 120, true);
  SR_CHECK(g->enc.active && g->enc.raid);
  /* the menu shows the choices when play resumes */
  SrActionState a;
  memset(&a, 0, sizeof a);
  g->screen = SR_SCREEN_PLAY;
  sr_game_step(g, &a);
  SR_CHECK(g->screen == SR_SCREEN_SHOP && g->menu.encounter);
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  SR_CHECK_EQ(sr_game_shop_buy(g, SR_ENC_PAY), SR_OK);
  sr_game_destroy(g);
}

/* ---- phase 3 --------------------------------------------------------------------------------- */

/* A finished job of the player's top tier against a megacorp, booked by the corporate game. */
static SrJob *done_job(SrGame *g, int tier) {
  SrJob j;
  sr_job_for_contract(g->city, SR_JT_SURVEIL, facility_of(g->city, 2), 1, 2, (uint64_t)g->jobs.next_id, sr_game_time(g),
                      &j);
  j.tier = (uint8_t)tier;
  j.state = SR_JS_COMPLETED;
  j.client_building = g->player.home;
  return sr_jobs_push(&g->jobs, &j);
}

static void test_street_cred_curve(void) {
  SR_SPEC("character-progression", "Weekly cap");
  SR_SPEC("character-progression", "Idle decay");
  SR_SPEC("character-progression", "Story gates");
  SrGame *g = game(47);
  SrProgress *pr = &g->player.prog;
  pr->street_cred = 30;
  int64_t week = (sr_game_time(g) / SR_WEEK_S + 1) * SR_WEEK_S + 3600;
  sr_game_skip(g, week - sr_game_time(g), false);
  int before = pr->street_cred;
  for (int k = 0; k < 10; k++) done_job(g, sr_max_tier_for_cred(pr->street_cred));
  sr_game_skip(g, 120, false);
  SR_CHECK(pr->street_cred > before);
  SR_CHECK(pr->street_cred - before <= SR_CRED_WEEK_CAP);
  /* a failure costs */
  SrJob *f = done_job(g, 2);
  f->state = SR_JS_FAILED;
  int now = pr->street_cred;
  sr_game_skip(g, 120, false);
  SR_CHECK_EQ(pr->street_cred, now - 1);
  /* idle above Runner: one point a week */
  pr->street_cred = 50;
  int64_t t = sr_game_time(g);
  sr_game_skip(g, SR_WEEK_S + 3 * 3600, false);
  SR_CHECK_EQ(pr->street_cred, 49);
  (void)t;
  /* story gates: 69 until Act II, 89 until Act III */
  pr->street_cred = 68;
  g->chess.camp.cred_gained = 0;
  sr_game_skip(g, SR_WEEK_S, false); /* a new week */
  for (int k = 0; k < 6; k++) done_job(g, 3);
  sr_game_skip(g, 120, false);
  SR_CHECK(pr->street_cred <= 69);
  g->chess.story.act = SR_ACT_KNIGHT;
  for (int k = 0; k < 6; k++) done_job(g, 3);
  sr_game_skip(g, 120, false);
  SR_CHECK(pr->street_cred >= 70);
  pr->street_cred = 88;
  sr_game_skip(g, SR_WEEK_S, false);
  for (int k = 0; k < 6; k++) done_job(g, 4);
  sr_game_skip(g, 120, false);
  SR_CHECK(pr->street_cred <= 89);
  sr_game_destroy(g);
}

static void test_living_costs(void) {
  SR_SPEC("player-life", "Upkeep with rent");
  SR_SPEC("player-life", "Medical bill");
  SR_SPEC("player-life", "Notoriety raises the rent");
  SrGame *g = game(48);
  SrPlayer *p = &g->player;
  p->cred = 100000;
  /* 10,000 cred of cyberware: 200 upkeep with the rent */
  p->cyber_count = 0;
  int type = 0;
  while (sr_item_base_price(&(SrItem){.kind = SR_IK_CYBER, .type = (uint8_t)type}) <= 0) type++;
  int32_t value = 0;
  while (value < 10000 && p->cyber_count < SR_MAX_CYBER) {
    p->cyber[p->cyber_count++] = sr_item_simple(SR_IK_CYBER, type, 1);
    value = 0;
    for (int i = 0; i < p->cyber_count; i++) value += sr_item_base_price(&p->cyber[i]);
  }
  p->equipped = -1;
  SR_CHECK_EQ(sr_game_weekly_upkeep(g), value * 2 / 100);
  sr_game_skip(g, 3600, false);
  int64_t due = p->rent_due, cash = p->cred;
  int32_t rent = p->rent;
  sr_game_skip(g, due - sr_game_time(g) + 3600, false);
  SR_CHECK_EQ(cash - p->cred, rent + sr_game_weekly_upkeep(g));
  /* notoriety: Operator pays 10 % more */
  p->prog.street_cred = 50;
  sr_game_skip(g, 3600, false);
  SR_CHECK_EQ(p->rent, sr_housing_def(p->tier)->rent * 110 / 100);
  /* a night at home heals 60 % of the damage at 5 cred a point */
  p->health = p->max_health - 40.0f;
  p->satiety = 30.0f; /* hungry enough not to mend while awake */
  sr_game_teleport_building(g, p->home);
  cash = p->cred;
  float before = p->health;
  p->satiety = 100.0f; /* fed: no starvation, no extra mending during the night */
  SR_CHECK_EQ(sr_game_sleep(g, 8, SR_SLEEP_HOME), SR_OK);
  float healed = p->health - before;
  SR_CHECK(healed >= 24.0f - 0.5f);
  SR_CHECK(cash - p->cred >= (int64_t)(24 * SR_MED_SLEEP_PER_HP) - 5);
  /* no cash: the bill is owed */
  p->cred = 0;
  sr_game_medical_bill(g, 10.0f, SR_MED_CLINIC_PER_HP);
  SR_CHECK_EQ(g->chess.camp.medical_debt, 150);
  sr_game_medical_bill(g, 0.0f, SR_MED_CLINIC_PER_HP);
  /* the clinic patches up the rest */
  int clinic = -1;
  for (int b = 0; b < g->city->building_count && clinic < 0; b++)
    if (g->city->buildings[b].poi == SR_POI_CLINIC) clinic = b;
  p->cred = 5000;
  p->health = p->max_health - 10.0f;
  sr_clock_set_seconds(&g->clock, (sr_game_time(g) / 86400 + 1) * 86400 + 12 * 3600);
  g->last_minute = sr_game_time(g) / 60;
  sr_game_teleport_building(g, clinic);
  g->ctx = sr_game_context(g);
  p->satiety = 100.0f;
  if (g->ctx.kind == SR_CTX_CLINIC) {
    SR_CHECK_EQ(sr_game_interact(g), SR_OK);
    SR_CHECK(p->health >= p->max_health - 0.01f);
    SR_CHECK_EQ(sr_game_interact(g), SR_ERR_INVALID);
  }
  /* heat clean-up at a fixer */
  g->heat.level[3] = 2;
  p->cred = 0;
  SR_CHECK_EQ(sr_game_heat_cleanup(g), SR_ERR_DENIED);
  p->cred = 5000;
  SR_CHECK_EQ(sr_game_heat_cleanup(g), SR_OK);
  SR_CHECK_EQ(g->heat.level[3], 1);
  memset(g->heat.level, 0, sizeof g->heat.level);
  SR_CHECK_EQ(sr_game_heat_cleanup(g), SR_ERR_INVALID);
  sr_game_destroy(g);
}

static void test_blackops_and_rewards(void) {
  SR_SPEC("story-campaign", "Black ops are rare");
  SR_SPEC("job-system", "Clean bonus");
  SrGame *g = game(49);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 75;
  st->trust = 70;
  SR_CHECK_EQ(sr_game_handler_offers(g), 1);
  SrJob *bo = &g->jobs.jobs[g->jobs.count - 1];
  int32_t lo, hi;
  sr_tier_pay_range(4, &lo, &hi);
  SR_CHECK(bo->pay <= hi / 2); /* half of it in favours */
  bo->state = SR_JS_WITHDRAWN;
  SR_CHECK_EQ(sr_game_handler_offers(g), 0); /* not again this week */
  sr_game_skip(g, 7 * 86400, false);
  int n = 0;
  for (int i = 0; i < g->jobs.count; i++) n += g->jobs.jobs[i].tier == 4 && g->jobs.jobs[i].state == SR_JS_OFFERED;
  SR_CHECK(n >= 1 || sr_game_handler_offers(g) == 1);
  /* a black op done: the favour arrives (Board, Heat or a voucher) */
  for (int k = 0; k < 3; k++) {
    SrJob *j = done_job(g, 4);
    j->id = 9000 + k; /* vary the favour */
    sr_game_skip(g, 120, false);
  }
  /* clean work: +20 % and a level of Heat off; loud work: none */
  SrJob *clean = done_job(g, 2);
  clean->approach = SR_AP_GHOST;
  clean->pay = 1000;
  int jur = sr_game_jurisdiction(g, clean->target_building);
  g->heat.level[jur] = 2;
  int64_t cash = g->player.cred;
  g->heat.last_seen[jur] = sr_game_time(g);
  g->heat.decay_from[jur] = sr_game_time(g) + 86400;
  sr_game_skip(g, 60, false);
  SR_CHECK(g->player.cred >= cash + 200 - 5);
  SR_CHECK_EQ(g->heat.level[jur], 1);
  SrJob *loud = done_job(g, 2);
  loud->alarms = 1;
  loud->pay = 1000;
  cash = g->player.cred;
  sr_game_skip(g, 60, false);
  SR_CHECK(g->player.cred < cash + 200);
  /* paydata sells high */
  SrItem pd = sr_item_simple(SR_IK_PAYDATA, 2, 1);
  sr_inv_add(&g->player.inv, pd);
  int fence = -1;
  for (int b = 0; b < g->city->building_count && fence < 0; b++)
    if (sr_vendor_buys(g->city, b, &pd)) fence = b;
  if (fence >= 0) {
    cash = g->player.cred;
    sr_player_sell(&g->player, g->city, fence, g->player.inv.count - 1, sr_game_time(g));
    SR_CHECK(g->player.cred - cash >= sr_price_at(g->city, fence, &pd) * 4 / 10 * 3 / 2 - 1);
  }
  sr_game_destroy(g);
}

static void test_reachable_dominance(void) {
  SR_SPEC("faction-simulation", "Takeovers accumulate");
  SR_SPEC("faction-simulation", "Threshold scales with the board");
  SrGame *g = game(50);
  SrChess *ch = &g->chess;
  for (int k = 0; k < 3; k++) {
    int target = 1 + k % 3, b = facility_of(g->city, target);
    int id = sr_chess_start_op(ch, g->city, SR_OP_TAKEOVER, 0, target, b, -1, sr_game_time(g));
    for (int step = 0; step < 2; step++) {
      SrOperation *op = sr_chess_op(ch, id);
      sr_chess_resolve(ch, g->city, g->pop, &g->events, sr_chess_contract(ch, op->contract), true, false,
                       sr_game_time(g));
    }
  }
  SR_CHECK_EQ(ch->camp.capital_bonus[0], 24);
  SrCity c = *g->city;
  c.factions.corp_count = 10;
  SR_CHECK(sr_chess_dominance_threshold(&c) > 0.249f && sr_chess_dominance_threshold(&c) < 0.251f);
  c.factions.corp_count = 5;
  SR_CHECK(sr_chess_dominance_threshold(&c) > 0.399f && sr_chess_dominance_threshold(&c) < 0.401f);
  sr_game_destroy(g);
}

void suite_play(void) {
  SR_RUN(test_headlines);
  SR_RUN(test_patron_warning);
  SR_RUN(test_defection_limits);
  SR_RUN(test_mission_risk);
  SR_RUN(test_offers_from_contracts);
  SR_RUN(test_terminal_jobs);
  SR_RUN(test_discovery);
  SR_RUN(test_hunters);
  SR_RUN(test_street_cred_curve);
  SR_RUN(test_living_costs);
  SR_RUN(test_blackops_and_rewards);
  SR_RUN(test_reachable_dominance);
}
