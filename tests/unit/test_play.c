/* Playability pass after M5 (improve-playability): every scenario of that change. */
#include "core/sr_rng.h"
#include "game/sr_save.h"
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
  int corps = g->city->factions.corp_count; /* the city is too big for a copy on the (web) stack */
  g->city->factions.corp_count = 10;
  SR_CHECK(sr_chess_dominance_threshold(g->city) > 0.249f && sr_chess_dominance_threshold(g->city) < 0.251f);
  g->city->factions.corp_count = 5;
  SR_CHECK(sr_chess_dominance_threshold(g->city) > 0.399f && sr_chess_dominance_threshold(g->city) < 0.401f);
  g->city->factions.corp_count = corps;
  sr_game_destroy(g);
}

/* ---- phase 4 --------------------------------------------------------------------------------- */

static bool inbox_has(const SrGame *g, const char *needle) {
  for (int i = 0; i < g->player.msg_count; i++)
    if (strstr(g->player.msgs[i].text, needle)) return true;
  return false;
}

static void test_staged_acts(void) {
  SR_SPEC("story-campaign", "Staged transition");
  SR_SPEC("story-campaign", "Beat announced");
  SrGame *g = game(51);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  int news = g->chess.news.count, log = g->chess.camp.log_count;
  st->act_started -= SR_ACT1_MIN_S;
  g->player.prog.street_cred = 30;
  st->trust = 45;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK_EQ(st->act, SR_ACT_KNIGHT);
  SR_CHECK(g->chess.news.count >= news + 3);                             /* the board in the news */
  SR_CHECK(inbox_has(g, "Real operations"));                             /* the handler's meeting */
  SR_CHECK(g->chess.pol.curfew != 0 || g->chess.pol.police_budget == 2); /* the streets tighten */
  SR_CHECK(g->chess.camp.log_count > log);
  /* an arc beat is announced by a cast member and logged */
  int msgs = g->player.msg_count;
  int idx = sr_story_arc_start(&g->chess, g->city, g->pop, SR_ARC_SCANDAL, sr_game_time(g));
  SR_CHECK(idx >= 0);
  sr_game_skip(g, 120, false);
  SR_CHECK(g->player.msg_count > msgs);
  const SrMessage *m = &g->player.msgs[g->player.msg_count - 1];
  SR_CHECK(m->pinned);
  SR_CHECK_STR(m->from, st->cast[SR_CAST_JOURNALIST].name);
  SR_CHECK(strstr(sr_story_log_entry(&g->chess, 0)->text, "Scandal") != NULL);
  SR_CHECK(sr_story_log_entry(&g->chess, 999) == NULL);
  /* every arc type has lines for every stage */
  char line[112];
  for (int t = 0; t < SR_ARC_COUNT; t++)
    for (int b = 0; b < 3; b++) {
      sr_story_beat_text(line, sizeof line, (SrArcType)t, b, b & 1, "A Corp", "B Corp", "Somewhere", (uint64_t)t);
      SR_CHECK(line[0] && strchr(line, '%') == NULL);
    }
  for (int e = 0; e <= SR_END_SUNRISE + 1; e++)
    for (int l = -1; l <= 3; l++) sr_story_ending_text(line, sizeof line, (SrEnding)e, l, "X");
  for (int a = SR_ACT_PROLOGUE; a <= SR_ACT_EPILOGUE; a++) SR_CHECK(sr_story_act_text((SrAct)a)[0]);
  g->chess.camp.msg_count = 0;
  for (int k = 0; k < SR_STORY_QUEUE + 3; k++) {
    char q[16];
    snprintf(q, sizeof q, "queued %d", k);
    sr_story_say(&g->chess, SR_CAST_FIXER, q);
  }
  SR_CHECK_EQ(g->chess.camp.msg_count, SR_STORY_QUEUE);
  /* nobody says the same thing twice in a row */
  SR_SPEC("story-campaign", "No repeated lines");
  g->chess.camp.msg_count = 0;
  for (int k = 0; k < 4; k++) sr_story_say(&g->chess, SR_CAST_FIXER, "the same line");
  SR_CHECK_EQ(g->chess.camp.msg_count, 1);
  /* Act III: the scenario corp's message on billboards everywhere */
  int news3 = g->chess.news.count;
  st->act_started -= SR_ACT2_MAX_S;
  sr_game_skip(g, SR_TICK_S, false);
  SR_CHECK_EQ(st->act, SR_ACT_PROMOTION);
  SR_CHECK(g->chess.news.count >= news3 + 5);
  SR_CHECK(sr_news_billboard(&g->chess.news, 0, 0) != NULL);
  sr_game_destroy(g);
}

static void test_set_piece(void) {
  SR_SPEC("story-campaign", "The set piece waits for the player");
  SR_SPEC("story-campaign", "Choosing a king");
  SrGame *g = game(52);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 75;
  st->act = SR_ACT_KNIGHT;
  st->act_started = sr_game_time(g) - SR_ACT2_MAX_S;
  sr_game_skip(g, SR_TICK_S, false);
  SR_CHECK_EQ(st->act, SR_ACT_PROMOTION);
  SR_CHECK_EQ(st->arcs[0].beats, 4);
  /* the first three beats go by off-screen; the fourth is the set piece */
  for (int k = 0; k < 40 && g->chess.camp.set_piece == 0; k++) sr_game_skip(g, 86400, false);
  SR_CHECK(g->chess.camp.set_piece != 0);
  SrOperation *spo = sr_chess_op(&g->chess, g->chess.camp.set_piece);
  SrContract *k = spo ? sr_chess_contract(&g->chess, spo->contract) : NULL;
  SR_CHECK(k != NULL);
  if (!k) return;
  sr_game_skip(g, 3600, false);
  SrJob *j = sr_jobs_get(&g->jobs, k->job_id);
  SR_CHECK(j != NULL && strstr(j->title, "FINAL JOB") != NULL);
  SR_CHECK(j->client_building == g->player.home); /* on the terminal, from the handler */
  /* nobody takes it: it does not resolve off-screen and does not expire */
  sr_game_skip(g, 20 * 86400, false);
  spo = sr_chess_op(&g->chess, g->chess.camp.set_piece);
  k = spo ? sr_chess_contract(&g->chess, spo->contract) : NULL;
  SR_CHECK(k && k->open);
  if (!k) return;
  j = sr_jobs_get(&g->jobs, k->job_id);
  SR_CHECK(j != NULL && j->state == SR_JS_OFFERED);
  if (!j) return;
  SR_CHECK(!st->finale_ready);
  /* the player does it (every step of it): the choice opens, and Kingmaker lists every megacorp */
  int32_t sp_id = g->chess.camp.set_piece; /* look it up by id: the operation list moves */
  for (int step = 0; step < 3; step++) {
    spo = sr_chess_op(&g->chess, sp_id);
    if (!spo || !spo->active) break;
    k = sr_chess_contract(&g->chess, spo->contract);
    sr_game_skip(g, 3600, false);
    j = k ? sr_jobs_get(&g->jobs, k->job_id) : NULL;
    if (!j) break;
    SR_CHECK_EQ(sr_game_accept(g, j->id), SR_OK);
    j->executed = true;
    sr_game_report(g, j->id);
    sr_game_skip(g, 120, false);
  }
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK(g->chess.camp.set_piece_done);
  SR_CHECK(st->finale_ready);
  g->screen = SR_SCREEN_TERMINAL;
  g->term_tab = SR_TAB_STORY;
  int crown = -1, kind = 0;
  for (int i = 0; sr_game_story_option(g, i, &kind); i++)
    if (kind == 100 + 2) crown = i;
  SR_CHECK(crown >= 0);
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  g->term_sel = crown;
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(st->ending, SR_END_KINGMAKER);
  SR_CHECK_EQ(g->chess.camp.king, 2);
  SR_CHECK_EQ(g->chess.pol.police_contract, 2); /* the king's colours on the streets */
  SR_CHECK(inbox_has(g, "picked a king"));
  sr_game_destroy(g);
  /* walking away from the set piece crowns the rival (R on the Jobs tab) */
  g = game(53);
  st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 75;
  st->act = SR_ACT_KNIGHT;
  st->act_started = sr_game_time(g) - SR_ACT2_MAX_S;
  sr_game_skip(g, SR_TICK_S, false);
  for (int d = 0; d < 40 && g->chess.camp.set_piece == 0; d++) sr_game_skip(g, 86400, false);
  sr_game_skip(g, 3600, false);
  spo = sr_chess_op(&g->chess, g->chess.camp.set_piece);
  k = spo ? sr_chess_contract(&g->chess, spo->contract) : NULL;
  j = k ? sr_jobs_get(&g->jobs, k->job_id) : NULL;
  SR_CHECK(j != NULL);
  if (j) {
    SR_CHECK_EQ(sr_game_accept(g, j->id), SR_OK);
    g->screen = SR_SCREEN_TERMINAL;
    g->term_tab = SR_TAB_JOBS;
    int32_t ids[32];
    int nj = sr_game_terminal_jobs(g, ids, 32);
    for (int i = 0; i < nj; i++)
      if (ids[i] == j->id) g->term_sel = i;
    memset(&a, 0, sizeof a);
    a.pressed[SR_ACT_RELOAD] = true;
    sr_game_step(g, &a);
    SR_CHECK(j->abandoned);
    sr_game_skip(g, 120, false);
    SR_CHECK_EQ(st->ending, SR_END_KINGMAKER);
    SR_CHECK_EQ(g->chess.camp.king, st->scenario_other);
  }
  SR_CHECK_EQ(sr_game_abandon(g, -7), SR_ERR_NOT_FOUND);
  sr_story_set_piece_abandoned(&g->chess, g->city, sr_game_time(g)); /* already over */
  sr_game_destroy(g);
}

static void test_nudge_and_epilogue(void) {
  SR_SPEC("story-campaign", "Stalled at Pawn");
  SR_SPEC("story-campaign", "Continue after the ending");
  SrGame *g = game(54);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 12; /* Pawn */
  int ops = g->chess.op_count;
  sr_game_skip(g, 2 * SR_WEEK_S + SR_TICK_S, false);
  SR_CHECK(g->chess.camp.nudged);
  SR_CHECK(inbox_has(g, "Runner-level cred"));
  SR_CHECK(g->chess.op_count > ops); /* a starter job from the handler */
  /* after an ending: two weeks later, a new cycle */
  st->act = SR_ACT_PROMOTION;
  st->finale_ready = true;
  st->leaks = 2; /* the player leaked enough to burn it all */
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_BURN), SR_OK);
  SR_CHECK(st->sandbox);
  SR_CHECK_EQ(g->chess.pol.curfew, 0); /* the old order is gone */
  SR_CHECK_EQ(g->chess.saga.season, 0);
  sr_game_skip(g, 2 * SR_WEEK_S + SR_TICK_S, false);
  SR_CHECK_EQ(g->chess.saga.season, 1); /* the next season: the corps rebuild, the gangs move */
  SR_CHECK(inbox_has(g, "the gangs smell it"));
  SR_CHECK_EQ(st->ending, SR_END_NONE);
  SR_CHECK(!st->sandbox);
  sr_game_skip(g, SR_WEEK_S, false);
  int active = 0;
  for (int i = 0; i < SR_MAX_ARCS; i++) active += st->arcs[i].active;
  SR_CHECK(active >= 1); /* the city's stories go on */
  sr_game_destroy(g);
  /* the Ghost writes to a runner who goes deep */
  g = game(55);
  g->chess.story.act = SR_ACT_KNIGHT;
  g->chess.story.matrix_depth = 12;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK(inbox_has(g, "The Pall is a machine"));
  sr_game_destroy(g);
}

static void test_notoriety(void) {
  SR_SPEC("news-media", "A name on the street");
  SrGame *g = game(56);
  g->player.prog.street_cred = 44;
  int news = g->chess.news.count;
  SrJob j;
  sr_job_for_contract(g->city, SR_JT_SABOTAGE, facility_of(g->city, 2), 1, 2, 77, sr_game_time(g), &j);
  j.tier = 2;
  j.state = SR_JS_COMPLETED;
  SrJob *pj = sr_jobs_push(&g->jobs, &j);
  (void)pj;
  sr_game_skip(g, 120, false); /* Operator */
  SR_CHECK(sr_cred_tier(g->player.prog.street_cred) == SR_CRED_OPERATOR);
  bool story = false;
  for (int i = 0; i < sr_news_size(&g->chess.news) && i < g->chess.news.count - news; i++)
    story |= sr_news_latest(&g->chess.news, i)->perpetrator == -2;
  SR_CHECK(story);
  /* three reports in a week bring another story the next week */
  sr_game_skip(g, SR_WEEK_S, false);
  int before = g->chess.news.count;
  for (int k = 0; k < 3; k++)
    sr_news_publish(&g->chess, g->city, -1, 1, -1, -2, SR_EV_SABOTAGE, false, "Runner hits a site", sr_game_time(g));
  sr_game_skip(g, 3600, false);
  SR_CHECK(g->chess.news.count >= before + 4);
  sr_game_destroy(g);
}

/* ---- phase 5 --------------------------------------------------------------------------------- */

static void test_save_v5(void) {
  SrGame *g = game(57);
  SrChess *ch = &g->chess;
  ch->camp.blacklist = 0x5;
  ch->camp.defections = 1;
  ch->camp.medical_debt = 321;
  ch->camp.king = 2;
  sr_story_log(ch, sr_game_time(g), "Saved line");
  sr_player_story_message(&g->player, "Handler", "Pinned before saving", sr_game_time(g));
  sr_player_message(&g->player, "Spam", "Twice", sr_game_time(g));
  sr_player_message(&g->player, "Spam", "Twice", sr_game_time(g));
  int ids[SR_FIXER_OFFERS];
  int n = sr_game_fixer(g, g->city->factions.fixer_building[0], ids);
  SR_CHECK(n > 0);
  SrJob *j = sr_jobs_get(&g->jobs, ids[0]);
  j->approach = SR_AP_FACE;
  g->flatlines = 2;
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SR_CHECK_EQ(sr_save_to(g, &st, "v5.srsv", 0), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "v5.srsv"), SR_OK);
  SR_CHECK_EQ(l->chess.camp.blacklist, 0x5);
  SR_CHECK_EQ(l->chess.camp.defections, 1);
  SR_CHECK_EQ(l->chess.camp.medical_debt, 321);
  SR_CHECK_EQ(l->chess.camp.king, 2);
  SR_CHECK_STR(sr_story_log_entry(&l->chess, 0)->text, "Saved line");
  SR_CHECK_EQ(l->flatlines, 2);
  SR_CHECK_EQ(sr_jobs_get(&l->jobs, ids[0])->approach, SR_AP_FACE);
  const SrMessage *last = &l->player.msgs[l->player.msg_count - 1];
  SR_CHECK_EQ(last->repeat, 2);
  SR_CHECK(l->player.msgs[l->player.msg_count - 2].pinned);
  /* a version 4 save: the campaign state starts fresh */
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, 4, 0), SR_OK);
  SrGame *m = sr_game_create();
  SR_CHECK_EQ(sr_save_read(m, w.data, w.len), SR_OK);
  SR_CHECK_EQ(m->chess.camp.blacklist, 0);
  SR_CHECK_EQ(m->chess.camp.king, -1);
  SR_CHECK_EQ(m->chess.camp.hunter, -1);
  SR_CHECK_EQ(m->player.msgs[m->player.msg_count - 1].repeat, 1);
  SR_CHECK_EQ(sr_jobs_get(&m->jobs, ids[0])->approach, -1);
  sr_writer_free(&w);
  sr_memstore_free(&mem);
  sr_game_destroy(m);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

/* ---- the saga: seasons and people -------------------------------------------------------------- */

/* A game in Act II on a retainer, rich enough to help people. */
static SrGame *saga_game(uint64_t seed) {
  SrGame *g = game(seed);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  st->act = SR_ACT_KNIGHT;
  st->trust = 60;
  g->player.cred = 50000;
  g->player.prog.street_cred = 50;
  g->chess.saga.cash = g->player.cred;
  return g;
}

static void force_ask(SrGame *g, SrAskType t, SrCastRole r, int corp, int32_t cost) {
  g->chess.saga.ask = (SrAsk){(uint8_t)t, (uint8_t)r, (int8_t)corp, true, cost, sr_game_time(g) + SR_ASK_OPEN_S};
}

static void test_seasons(void) {
  SR_SPEC("story-campaign", "The game goes on in seasons");
  SrGame *g = saga_game(61);
  SrStory *st = &g->chess.story;
  SrSaga *sg = &g->chess.saga;
  SR_CHECK_STR(sr_saga_season_name(sg), "The Climb");
  int patron = st->patron;
  /* a loyal ending: two weeks later the next season, with the winner as everybody's target */
  st->act = SR_ACT_PROMOTION;
  st->finale_ready = true;
  st->scenario = SR_SC_MERGER;
  st->scenario_corp = (int8_t)patron;
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_LOYAL), SR_OK);
  SR_CHECK_EQ(st->act, SR_ACT_EPILOGUE);
  SR_CHECK(sg->bond[SR_CAST_HANDLER] >= 15); /* the handler takes it personally, in a good way */
  int64_t ended = sr_game_time(g);
  sr_game_skip(g, SR_INTERLUDE_S - SR_TICK_S, false);
  SR_CHECK_EQ(sg->season, 0);
  sr_game_skip(g, 2 * SR_TICK_S, false);
  SR_CHECK_EQ(sg->season, 1);
  SR_CHECK_STR(sr_saga_season_name(sg), "The Reckoning");
  SR_CHECK_EQ(sg->endings[0], SR_END_LOYAL);
  SR_CHECK_EQ(sg->scenarios[0], SR_SC_MERGER);
  SR_CHECK_EQ(sg->kings[0], patron);
  SR_CHECK(sg->season_started >= ended + SR_INTERLUDE_S);
  SR_CHECK_EQ(st->ending, SR_END_NONE);
  SR_CHECK_EQ(st->act, SR_ACT_KNIGHT); /* still on the retainer: straight back to real operations */
  SR_CHECK(st->retainer && st->patron == patron && st->first_patron == patron);
  SR_CHECK(!st->finale_ready && !st->sandbox && g->chess.camp.ending_at == 0);
  SR_CHECK(inbox_has(g, "every knife in the city"));
  bool titled = false;
  for (int i = 0; i < 4; i++) {
    const SrLogEntry *le = sr_story_log_entry(&g->chess, i);
    titled |= le && strcmp(le->text, "Season 2: The Reckoning") == 0;
  }
  SR_CHECK(titled);
  int active = 0;
  for (int i = 0; i < SR_MAX_ARCS; i++) active += st->arcs[i].active;
  SR_CHECK(active >= 1); /* the season opens with an arc */
  /* the next climax does not repeat the last scenario, and comes again */
  st->act_started -= SR_ACT2_MAX_S;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK_EQ(st->act, SR_ACT_PROMOTION);
  SR_CHECK(st->scenario != SR_SC_MERGER && st->scenario != SR_SC_NONE);
  /* Street Cred above Knight stays open after the first ending */
  /* a second ending, burned: the handler turns, the hunt stays personal into season three */
  st->finale_ready = true;
  st->leaks = 2;
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_BURN), SR_OK);
  SR_CHECK(sg->bond[SR_CAST_HANDLER] <= SR_BOND_ENEMY);
  SR_CHECK(sg->bond[SR_CAST_JOURNALIST] >= 15);
  st->hostile = (int8_t)patron;
  st->patron = -1;
  st->retainer = false;
  sr_game_skip(g, SR_INTERLUDE_S + SR_TICK_S, false);
  SR_CHECK_EQ(sg->season, 2);
  SR_CHECK_STR(sr_saga_season_name(sg), "Ashes");
  SR_CHECK_EQ(st->hostile, patron); /* personal: the grudge survives the reset */
  SR_CHECK(sr_story_blacklisted(&g->chess, patron));
  SR_CHECK(st->patron >= 0 && st->patron != patron && st->offer_open && !st->retainer);
  SR_CHECK_EQ(st->act, SR_ACT_PAWN);
  SR_CHECK_EQ(st->leaks, 0);
  /* accepting the new retainer: Act II after a week in a later season, not three */
  SR_CHECK_EQ(sr_game_accept_retainer(g), SR_OK);
  st->trust = 60;
  sr_game_skip(g, SR_WEEK_S + SR_TICK_S, false);
  SR_CHECK_EQ(st->act, SR_ACT_KNIGHT);
  /* without a personal grudge the old hunt is called off at the reset */
  sg->bond[SR_CAST_HANDLER] = 0;
  st->act = SR_ACT_PROMOTION;
  st->finale_ready = true;
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_KINGMAKER), SR_OK);
  sr_game_skip(g, SR_INTERLUDE_S + SR_TICK_S, false);
  SR_CHECK_EQ(sg->season, 3);
  SR_CHECK_STR(sr_saga_season_name(sg), "The Crown");
  SR_CHECK_EQ(st->hostile, -1);
  SR_CHECK(inbox_has(g, "called off the hunt"));
  /* Sunrise: the sun stays out, and that ending is not offered twice */
  sg->bond[SR_CAST_GHOST] = SR_BOND_FRIEND; /* a Ghost who listens opens it */
  st->act = SR_ACT_PROMOTION;
  st->finale_ready = true;
  SR_CHECK(sr_saga_endings(&g->chess) & (1u << SR_END_SUNRISE));
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_SUNRISE), SR_OK);
  SR_CHECK(sg->sun_seen && g->pall_removed);
  sr_game_skip(g, SR_INTERLUDE_S + SR_TICK_S, false);
  SR_CHECK_STR(sr_saga_season_name(sg), "Daylight");
  SR_CHECK(g->pall_removed);
  SR_CHECK(inbox_has(g, "rebuilding the machine"));
  SR_CHECK(!(sr_saga_endings(&g->chess) & (1u << SR_END_SUNRISE)));
  /* a reporter who trusts the runner opens Burn the Board without leaks */
  st->leaks = 0;
  st->hostile = -1;
  sg->bond[SR_CAST_JOURNALIST] = 0;
  SR_CHECK(!(sr_saga_endings(&g->chess) & (1u << SR_END_BURN)));
  sg->bond[SR_CAST_JOURNALIST] = SR_BOND_FRIEND;
  SR_CHECK(sr_saga_endings(&g->chess) & (1u << SR_END_BURN));
  /* the other season titles */
  sg->endings[(sg->season - 1) % SR_SAGA_HISTORY] = SR_END_DEFECTOR;
  SR_CHECK_STR(sr_saga_season_name(sg), "Old Debts");
  sg->endings[(sg->season - 1) % SR_SAGA_HISTORY] = SR_END_NONE;
  SR_CHECK_STR(sr_saga_season_name(sg), "The Long Night");
  sr_game_destroy(g);
  /* a defector's ending opens a season too */
  g = saga_game(62);
  g->chess.story.act = SR_ACT_PROMOTION;
  g->chess.story.finale_ready = true;
  g->chess.story.first_patron = (int8_t)((g->chess.story.patron + 1) % g->city->factions.corp_count);
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_DEFECTOR), SR_OK);
  sr_game_skip(g, SR_INTERLUDE_S + SR_TICK_S, false);
  SR_CHECK_STR(sr_saga_season_name(&g->chess.saga), "Old Debts");
  sr_game_destroy(g);
}

static void test_people_asks(void) {
  SR_SPEC("story-campaign", "People ask for help");
  SrGame *g = saga_game(63);
  SrChess *ch = &g->chess;
  SrStory *st = &ch->story;
  SrSaga *sg = &ch->saga;
  char label[96];
  sr_saga_ask_label(ch, label, sizeof label);
  SR_CHECK(label[0] == '\0');
  SR_CHECK_EQ(sr_game_answer_ask(g, true), SR_ERR_INVALID);
  SR_CHECK(!sr_saga_answer(ch, g->city, g->pop, true, sr_game_time(g)));
  /* a request arrives within a week and waits on the terminal */
  sr_game_skip(g, 6 * 86400, false);
  SR_CHECK(sg->ask.open);
  SR_CHECK(sg->asks == 1);
  sr_saga_ask_label(ch, label, sizeof label);
  SR_CHECK(label[0] != '\0');
  g->term_tab = SR_TAB_PEOPLE;
  SR_CHECK_EQ(sr_game_term_rows(g), 2);
  char goal[160];
  g->settings.guidance = true;
  g->active_job = -1;
  sr_guide_objective(g, goal, sizeof goal);
  SR_CHECK(strstr(goal, "IS WAITING FOR AN ANSWER") != NULL);
  /* silence is an answer: it expires as a no */
  int refused = sg->refused;
  sr_game_skip(g, SR_ASK_OPEN_S + SR_TICK_S, false);
  SR_CHECK(sg->refused == refused + 1);
  /* money: a yes costs, moves the bond and is remembered; too little cred is refused */
  int64_t cash = g->player.cred;
  force_ask(g, SR_ASK_DEBT, SR_CAST_NEIGHBOUR, -1, 1200);
  sg->bond[SR_CAST_NEIGHBOUR] = 0;
  sr_saga_ask_label(ch, label, sizeof label);
  SR_CHECK(strstr(label, "1200 cred") != NULL);
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_INTERACT] = true;
  g->term_sel = 0;
  sr_game_term_input(g, &a); /* the first row: help */
  SR_CHECK_EQ(g->player.cred, cash - 1200);
  SR_CHECK_EQ(sg->bond[SR_CAST_NEIGHBOUR], 25);
  SR_CHECK(sg->flags[SR_CAST_NEIGHBOUR] & SR_TIE_OWES);
  SR_CHECK(!sg->ask.open);
  SR_CHECK(inbox_has(g, "I won't forget it"));
  force_ask(g, SR_ASK_BAIL, SR_CAST_FIXER, -1, 999999);
  SR_CHECK_EQ(sr_game_answer_ask(g, true), SR_ERR_DENIED);
  SR_CHECK(sg->ask.open);
  g->term_sel = 1;
  sr_game_term_input(g, &a); /* the second row: refuse */
  SR_CHECK(!sg->ask.open);
  SR_CHECK_EQ(sg->bond[SR_CAST_FIXER], -25);
  force_ask(g, SR_ASK_BAIL, SR_CAST_FIXER, -1, 500);
  SR_CHECK_EQ(sr_game_answer_ask(g, true), SR_OK);
  SR_CHECK_EQ(sg->bond[SR_CAST_FIXER], 0);
  /* a leak for the reporter: the patron trusts less, Burn the Board comes closer */
  int trust = st->trust, leaks = st->leaks, news = ch->news.count + ch->news.pending_count;
  force_ask(g, SR_ASK_SOURCE, SR_CAST_JOURNALIST, st->patron, 0);
  SR_CHECK_EQ(sr_game_answer_ask(g, true), SR_OK);
  SR_CHECK_EQ(st->trust, trust - 8);
  SR_CHECK_EQ(st->leaks, leaks + 1);
  SR_CHECK(ch->news.count + ch->news.pending_count > news);
  SR_CHECK_EQ(sg->bond[SR_CAST_JOURNALIST], 25);
  force_ask(g, SR_ASK_SOURCE, SR_CAST_JOURNALIST, st->patron, 0);
  sr_game_answer_ask(g, false);
  SR_CHECK_EQ(sg->bond[SR_CAST_JOURNALIST], 10);
  /* the handler wants the reporter's name: refusing costs trust and wins a friend */
  SR_SPEC("story-campaign", "Loyalty against friendship");
  trust = st->trust;
  force_ask(g, SR_ASK_NAME, SR_CAST_HANDLER, st->patron, 0);
  sr_game_answer_ask(g, false);
  SR_CHECK_EQ(st->trust, trust - 10);
  SR_CHECK_EQ(sg->bond[SR_CAST_JOURNALIST], 30);
  SR_CHECK(inbox_has(g, "what you didn't do"));
  /* giving the name: trust, and the reporter is dead */
  char reporter[SR_NAME_LEN];
  snprintf(reporter, sizeof reporter, "%s", st->cast[SR_CAST_JOURNALIST].name);
  trust = st->trust;
  int lost = sg->lost;
  force_ask(g, SR_ASK_NAME, SR_CAST_HANDLER, st->patron, 0);
  sr_game_answer_ask(g, true);
  SR_CHECK_EQ(st->trust, trust + 15);
  SR_CHECK(!st->cast[SR_CAST_JOURNALIST].alive);
  SR_CHECK_EQ(sg->lost, lost + 1);
  SR_CHECK(inbox_has(g, "the canal"));
  /* a successor steps in two days later, a stranger */
  sg->next_ask = sr_game_time(g) + 30 * 86400;
  sr_game_skip(g, 2 * 86400 + SR_TICK_S, false);
  SR_CHECK(st->cast[SR_CAST_JOURNALIST].alive);
  SR_CHECK(strcmp(reporter, st->cast[SR_CAST_JOURNALIST].name) != 0);
  SR_CHECK_EQ(sg->bond[SR_CAST_JOURNALIST], 0);
  /* a truce pays the player's half; the Ghost's packet shows the Board */
  cash = g->player.cred;
  sg->bond[SR_CAST_RIVAL] = 0;
  force_ask(g, SR_ASK_TRUCE, SR_CAST_RIVAL, -1, 0);
  sr_game_answer_ask(g, true);
  SR_CHECK(sg->refund > 0);
  sr_game_skip(g, 120, false);
  SR_CHECK(g->player.cred > cash);
  SR_CHECK_EQ(sg->refund, 0);
  force_ask(g, SR_ASK_TRUCE, SR_CAST_RIVAL, -1, 0);
  sr_game_answer_ask(g, false);
  SR_CHECK_EQ(sg->bond[SR_CAST_RIVAL], 8);
  int depth = st->matrix_depth;
  force_ask(g, SR_ASK_DEEP, SR_CAST_GHOST, -1, 0);
  sr_game_answer_ask(g, true);
  SR_CHECK_EQ(st->matrix_depth, depth + 3);
  force_ask(g, SR_ASK_DEEP, SR_CAST_GHOST, -1, 0);
  sr_game_answer_ask(g, false);
  /* the landlord's lease, a place to hide */
  force_ask(g, SR_ASK_LEASE, SR_CAST_LANDLORD, 1, 2000);
  sr_game_answer_ask(g, true);
  SR_CHECK(sg->flags[SR_CAST_LANDLORD] & SR_TIE_OWES);
  force_ask(g, SR_ASK_LEASE, SR_CAST_LANDLORD, 1, 2000);
  sr_game_answer_ask(g, false);
  sg->bond[SR_CAST_RIVAL] = 0;
  force_ask(g, SR_ASK_HIDE, SR_CAST_RIVAL, 1, 0);
  sr_game_answer_ask(g, true);
  SR_CHECK_EQ(sg->bond[SR_CAST_RIVAL], 30);
  /* turned away, some do not make it */
  int gone = 0;
  for (int k = 0; k < 6; k++) {
    sg->asks++;
    if (!st->cast[SR_CAST_NEIGHBOUR].alive) break;
    sg->bond[SR_CAST_NEIGHBOUR] = 40;
    force_ask(g, SR_ASK_HIDE, SR_CAST_NEIGHBOUR, 1, 0);
    sr_game_answer_ask(g, false);
    gone += !st->cast[SR_CAST_NEIGHBOUR].alive;
  }
  SR_CHECK(gone == 1);
  /* names of the requests, the standings */
  for (int t = 0; t <= SR_ASK_COUNT; t++) SR_CHECK(sr_saga_ask_name((SrAskType)t)[0] != '\0');
  SR_CHECK_STR(sr_saga_standing(-60), "ENEMY");
  SR_CHECK_STR(sr_saga_standing(-20), "COLD");
  SR_CHECK_STR(sr_saga_standing(0), "NEUTRAL");
  SR_CHECK_STR(sr_saga_standing(20), "WARM");
  SR_CHECK_STR(sr_saga_standing(40), "FRIEND");
  SR_CHECK_STR(sr_saga_standing(70), "CLOSE");
  sr_saga_bond(ch, SR_CAST_COUNT, 5); /* out of range: ignored */
  sr_game_destroy(g);
  /* over a season every kind of request comes up, and the test player answers them */
  g = saga_game(64);
  g->chess.story.matrix_depth = 30;
  bool seen[SR_ASK_COUNT] = {false};
  for (int day = 0; day < 200; day++) {
    sr_game_skip(g, 86400, false);
    g->player.cred = 50000;
    if (g->chess.saga.ask.open) {
      seen[g->chess.saga.ask.type] = true;
      sr_bot_answer_ask(g, day % 2 == 0);
      SR_CHECK(!g->chess.saga.ask.open);
    }
  }
  int kinds = 0;
  for (int t = 1; t < SR_ASK_COUNT; t++) kinds += seen[t];
  printf("  request kinds seen in 200 days: %d, helped %d, refused %d, sold out by %d, lost %d\n", kinds,
         g->chess.saga.helped, g->chess.saga.refused, g->chess.saga.betrayals, g->chess.saga.lost);
  SR_CHECK(kinds >= 6);
  SR_CHECK(g->chess.saga.asks >= 20);
  sr_bot_answer_ask(g, true); /* nothing open: nothing happens */
  sr_game_destroy(g);
}

static void test_people_drama(void) {
  SR_SPEC("story-campaign", "Betrayal by someone let down");
  SrGame *g = saga_game(65);
  SrChess *ch = &g->chess;
  SrStory *st = &ch->story;
  SrSaga *sg = &ch->saga;
  sg->next_ask = sr_game_time(g) + 400 * 86400; /* no requests in the way */
  sg->next_perk = sr_game_time(g) + 400 * 86400;
  /* a fixer let down twice sells the player out and skips town; hunters come */
  char fixer[SR_NAME_LEN];
  snprintf(fixer, sizeof fixer, "%s", st->cast[SR_CAST_FIXER].name);
  sg->bond[SR_CAST_FIXER] = SR_BOND_ENEMY;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK_EQ(sg->betrayals, 1);
  SR_CHECK(inbox_has(g, "They pay better than you do"));
  SR_CHECK(ch->camp.hunter >= 0 || g->enc.active);
  SR_CHECK(!st->cast[SR_CAST_FIXER].alive);
  char sold[72];
  snprintf(sold, sizeof sold, "%s sold you out", fixer);
  bool logged = false;
  for (int i = 0; i < 8; i++) {
    const SrLogEntry *e = sr_story_log_entry(ch, i);
    logged |= e && strcmp(e->text, sold) == 0;
  }
  SR_CHECK(logged);
  /* the others: the neighbour, the reporter, the handler, the rival, and the quiet ones */
  ch->camp.hunter = -1;
  g->enc.active = false;
  int trust = st->trust, news = ch->news.count;
  sg->bond[SR_CAST_NEIGHBOUR] = sg->bond[SR_CAST_JOURNALIST] = sg->bond[SR_CAST_HANDLER] = SR_BOND_ENEMY;
  sg->bond[SR_CAST_RIVAL] = sg->bond[SR_CAST_LANDLORD] = sg->bond[SR_CAST_GHOST] = SR_BOND_ENEMY;
  st->matrix_depth = 0;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK(sg->betrayals >= 6);
  SR_CHECK_EQ(st->trust, trust - 10 - 20 - 8);
  SR_CHECK(ch->news.count > news); /* the reporter's story about the runner */
  SR_CHECK(inbox_has(g, "which door is yours") && inbox_has(g, "You're the story now"));
  SR_CHECK(inbox_has(g, "That stops today") && inbox_has(g, "Mine now"));
  SR_CHECK(!st->cast[SR_CAST_NEIGHBOUR].alive);
  SR_CHECK(sg->flags[SR_CAST_HANDLER] & SR_TIE_BETRAYED);
  int before = sg->betrayals;
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK_EQ(sg->betrayals, before); /* once each */
  sr_game_destroy(g);

  /* friends: told once, then favours week by week */
  SR_SPEC("story-campaign", "Friends stand up for the player");
  g = saga_game(66);
  ch = &g->chess;
  st = &ch->story;
  sg = &ch->saga;
  sg->next_ask = sr_game_time(g) + 400 * 86400;
  for (int r = 0; r < SR_CAST_COUNT; r++) sg->bond[r] = SR_BOND_CLOSE;
  st->matrix_depth = 200;
  sr_game_skip(g, SR_TICK_S + 60, false);
  for (int r = 0; r < SR_CAST_COUNT; r++) SR_CHECK(sg->flags[r] & SR_TIE_CLOSE);
  SR_CHECK(inbox_has(g, "You're the exception"));
  SR_CHECK(inbox_has(g, "my notes go to you"));
  /* a hunt put off, a bill lost, a story buried, a word upstairs, a debt repaid */
  st->hostile = (int8_t)((st->patron + 1) % g->city->factions.corp_count);
  st->next_hunt = sr_game_time(g) + 8 * 86400;
  ch->camp.medical_debt = 700;
  st->trust = 30;
  sg->flags[SR_CAST_LANDLORD] |= SR_TIE_OWES;
  sg->flags[SR_CAST_NEIGHBOUR] |= SR_TIE_OWES;
  int64_t cash = g->player.cred;
  for (int w = 0; w < 16; w++) {
    sr_news_queue(ch, -1, st->patron, -1, false, false, "A pending story", sr_game_time(g) + 30 * 86400);
    if (st->next_hunt < sr_game_time(g) + 2 * 86400) st->next_hunt = sr_game_time(g) + 2 * 86400;
    g->player.cred = cash;
    if (w < 8) ch->camp.medical_debt = 700;
    g->player.health = g->player.max_health;
    g->enc.active = false;
    ch->camp.hunter = -1;
    sr_game_skip(g, SR_WEEK_S, false);
    for (int r = 0; r < SR_CAST_COUNT; r++) sg->bond[r] = SR_BOND_CLOSE;
  }
  SR_CHECK(inbox_has(g, "wrong district"));
  SR_CHECK(inbox_has(g, "lost your bill") || inbox_has(g, "some of what I owe you"));
  SR_CHECK(inbox_has(g, "died on my desk"));
  SR_CHECK(inbox_has(g, "I vouched for you"));
  SR_CHECK(inbox_has(g, "First instalment"));
  /* the neighbour makes a clinic bill disappear */
  for (int r = 0; r < SR_CAST_COUNT; r++) sg->bond[r] = 0, sg->flags[r] = 0;
  sg->bond[SR_CAST_NEIGHBOUR] = SR_BOND_CLOSE;
  ch->camp.medical_debt = 700;
  sg->next_perk = sr_game_time(g);
  sr_game_skip(g, SR_TICK_S + 60, false);
  SR_CHECK(inbox_has(g, "lost your bill"));
  SR_CHECK_EQ(ch->camp.medical_debt, 0);
  sr_game_destroy(g);

  /* someone close is taken: pay, or lose them */
  SR_SPEC("story-campaign", "Someone close is taken");
  g = saga_game(67);
  ch = &g->chess;
  st = &ch->story;
  sg = &ch->saga;
  st->hostile = (int8_t)((st->patron + 1) % g->city->factions.corp_count);
  st->next_hunt = sr_game_time(g) + 400 * 86400;
  sg->bond[SR_CAST_NEIGHBOUR] = 80;
  sg->next_perk = sr_game_time(g) + 400 * 86400;
  sr_game_skip(g, 6 * 86400, false);
  SR_CHECK(sg->ask.open && sg->ask.type == SR_ASK_RANSOM && sg->ask.role == SR_CAST_NEIGHBOUR);
  SR_CHECK(sg->flags[SR_CAST_NEIGHBOUR] & SR_TIE_TAKEN);
  SR_CHECK(sg->ask.cost >= 2000 && sg->ask.cost <= g->player.cred / 2 + 1);
  SR_CHECK(inbox_has(g, "or they send pieces"));
  cash = g->player.cred;
  SR_CHECK_EQ(sr_game_answer_ask(g, true), SR_OK);
  SR_CHECK(g->player.cred < cash);
  SR_CHECK(st->cast[SR_CAST_NEIGHBOUR].alive && !(sg->flags[SR_CAST_NEIGHBOUR] & SR_TIE_TAKEN));
  SR_CHECK(inbox_has(g, "I know what that cost you"));
  /* not paid: they die, and someone wants the corporation to pay for it */
  sg->flags[SR_CAST_NEIGHBOUR] |= SR_TIE_TAKEN;
  force_ask(g, SR_ASK_RANSOM, SR_CAST_NEIGHBOUR, st->hostile, 5000);
  int arcs = st->arcs_started, lost = sg->lost;
  for (int i = 0; i < SR_MAX_ARCS; i++) st->arcs[i].active = false;
  sr_game_skip(g, SR_ASK_OPEN_S + SR_TICK_S, false); /* no answer */
  SR_CHECK(sg->lost == lost + 1);
  SR_CHECK(inbox_has(g, "They found"));
  SR_CHECK(st->arcs_started > arcs);
  sr_game_destroy(g);
}

static void test_story_fixes(void) {
  /* another employer's suspicions do not follow the player, and nobody hires a burned runner overnight */
  SR_SPEC("story-campaign", "No chain of betrayals");
  SrGame *g = saga_game(68);
  SrChess *ch = &g->chess;
  SrStory *st = &ch->story;
  int old = st->patron;
  ch->camp.suspects[0] = (SrSuspect){sr_game_time(g) + 3600, 3, true};
  st->trust = SR_BETRAYAL; /* one discovery drops them */
  sr_story_discovered(ch, g->city, sr_game_time(g));
  SR_CHECK_EQ(st->hostile, old);
  SR_CHECK_EQ(st->patron, -1);
  SR_CHECK(!ch->camp.suspects[0].active);
  SR_CHECK(st->next_counter >= sr_game_time(g) + 2 * SR_WEEK_S); /* the first switch: two weeks of cold shoulders */
  sr_game_skip(g, SR_WEEK_S, false);
  SR_CHECK_EQ(st->rival_offer, -1);
  sr_game_skip(g, SR_WEEK_S + 2 * SR_TICK_S, false);
  SR_CHECK(st->rival_offer >= 0);
  SR_CHECK_EQ(sr_game_accept_offer(g), SR_OK);
  SR_CHECK_EQ(st->trust, SR_TRUST_DEFECT);
  SR_CHECK(st->next_counter >= sr_game_time(g) + 3 * SR_WEEK_S); /* and longer every time */
  SR_CHECK_EQ(ch->saga.bond[SR_CAST_HANDLER], 0);
  sr_game_destroy(g);
  /* the handler only briefs the patron's own operations */
  SR_SPEC("story-campaign", "Briefings come from the right person");
  g = saga_game(69);
  ch = &g->chess;
  st = &ch->story;
  ch->saga.next_ask = sr_game_time(g) + 400 * 86400;
  int other = (st->patron + 1) % g->city->factions.corp_count, third = (st->patron + 2) % g->city->factions.corp_count;
  for (int i = 0; i < SR_MAX_ARCS; i++) st->arcs[i].active = false;
  ch->camp.msg_count = 0;
  SR_CHECK(sr_story_arc_between(ch, g->city, g->pop, SR_ARC_TAKEOVER, other, third, sr_game_time(g)) >= 0);
  SR_CHECK_EQ(ch->camp.msg_count, 1);
  SR_CHECK_EQ(ch->camp.msgs[0].role, SR_CAST_FIXER);
  ch->camp.msg_count = 0;
  SR_CHECK(sr_story_arc_between(ch, g->city, g->pop, SR_ARC_TAKEOVER, st->patron, third, sr_game_time(g)) >= 0);
  SR_CHECK_EQ(ch->camp.msgs[0].role, SR_CAST_HANDLER);
  /* two arcs of a kind in a row are not announced in the same words */
  SR_CHECK(strcmp(ch->camp.msgs[0].text, "") != 0);
  SR_CHECK(sr_saga_was_said(ch, ch->camp.msgs[0].text));
  SR_CHECK(!sr_saga_was_said(ch, "never said"));
  sr_game_destroy(g);
  /* headlines about a site nobody owns read as sentences */
  SR_SPEC("news-media", "Headlines without an owner");
  g = game(70);
  static const SrEventType kinds[4] = {SR_EV_SABOTAGE, SR_EV_THEFT, SR_EV_JOB_DONE, SR_EV_JOB_FAILED};
  for (int k = 0; k < 4; k++)
    for (int v = 0; v < 12; v++) {
      SrWorldEvent e;
      memset(&e, 0, sizeof e);
      e.type = kinds[k];
      e.victim_faction = -1;
      e.district = 0;
      e.building = v;
      e.time = 1000 + v * 77;
      char head[64];
      for (int perp = -2; perp <= 0; perp++) {
        sr_news_event_headline(head, sizeof head, g->city, &e, -1, perp);
        SR_CHECK(strstr(head, "a firm") == NULL);
        SR_CHECK(head[0] >= 'A' && head[0] <= 'Z');
        SR_CHECK(strlen(head) <= 63);
      }
    }
  sr_game_destroy(g);
}

static void test_save_v6(void) {
  SR_SPEC("game-persistence", "The saga is saved");
  SrGame *g = saga_game(71);
  SrSaga *sg = &g->chess.saga;
  sg->season = 2;
  sg->endings[0] = SR_END_LOYAL;
  sg->endings[1] = SR_END_BURN;
  sg->bond[SR_CAST_FIXER] = 44;
  sg->flags[SR_CAST_FIXER] = SR_TIE_OWES;
  sg->helped = 7;
  sg->sun_seen = true;
  force_ask(g, SR_ASK_BAIL, SR_CAST_FIXER, -1, 900);
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, SR_SAVE_VERSION, 1), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_save_read(l, w.data, w.len), SR_OK);
  SR_CHECK_EQ(l->chess.saga.season, 2);
  SR_CHECK_EQ(l->chess.saga.endings[1], SR_END_BURN);
  SR_CHECK_EQ(l->chess.saga.bond[SR_CAST_FIXER], 44);
  SR_CHECK_EQ(l->chess.saga.helped, 7);
  SR_CHECK(l->chess.saga.ask.open && l->chess.saga.ask.cost == 900);
  SR_CHECK(l->pall_removed); /* the sun stays out after loading */
  SR_CHECK_STR(sr_saga_season_name(&l->chess.saga), "Ashes");
  sr_writer_free(&w);
  /* a version 5 save: no saga section, it starts empty */
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, 5, 1), SR_OK);
  SrGame *m = sr_game_create();
  SR_CHECK_EQ(sr_save_read(m, w.data, w.len), SR_OK);
  SR_CHECK_EQ(m->chess.saga.season, 0);
  SR_CHECK_EQ(m->chess.saga.bond[SR_CAST_FIXER], 0);
  SR_CHECK(!m->chess.saga.ask.open);
  SR_CHECK_EQ(m->chess.camp.blacklist, g->chess.camp.blacklist);
  sr_writer_free(&w);
  /* the terminal shows the season and the people */
  SrUi ui;
  sr_ui_init(&ui);
  g->screen = SR_SCREEN_TERMINAL;
  g->term_tab = SR_TAB_PEOPLE;
  sg->flags[SR_CAST_NEIGHBOUR] = SR_TIE_TAKEN;
  sg->flags[SR_CAST_RIVAL] = SR_TIE_BETRAYED;
  sg->bond[SR_CAST_RIVAL] = -60;
  g->chess.story.cast[SR_CAST_LANDLORD].alive = false;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  int with = ui.mesh.nv;
  sg->ask.open = false;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  SR_CHECK(ui.mesh.nv > 0 && ui.mesh.nv != with);
  g->term_tab = SR_TAB_STORY;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  SR_CHECK(ui.mesh.nv > 0);
  sr_ui_free(&ui);
  sr_game_destroy(m);
  sr_game_destroy(l);
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
  SR_RUN(test_staged_acts);
  SR_RUN(test_set_piece);
  SR_RUN(test_nudge_and_epilogue);
  SR_RUN(test_notoriety);
  SR_RUN(test_save_v5);
  SR_RUN(test_seasons);
  SR_RUN(test_people_asks);
  SR_RUN(test_people_drama);
  SR_RUN(test_story_fixes);
  SR_RUN(test_save_v6);
}
