/* Playability pass after M5 (improve-playability): every scenario of that change. */
#include "core/sr_rng.h"
#include "game/sr_script.h"
#include "sr_test.h"

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
  sr_story_job_done(&g->chess, g->city, -1, rival, true, true, sr_game_time(g));
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

void suite_play(void) {
  SR_RUN(test_headlines);
  SR_RUN(test_patron_warning);
  SR_RUN(test_defection_limits);
  SR_RUN(test_mission_risk);
}
