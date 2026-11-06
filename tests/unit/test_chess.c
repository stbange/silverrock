/* The corporate game (M5): faction simulation, consequences, politics, market, news and story. */
#include "core/sr_rng.h"
#include "ui/sr_ui.h"
#include "sim/sr_chess.h"
#include "sim/sr_progress.h"
#include "game/sr_save.h"
#include "game/sr_script.h"
#include "render/sr_render.h"
#include "sr_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct World {
  SrCity *c;
  SrPopulation *pop;
  SrEventLog log;
  SrChess *ch;
} World;

static World world(uint64_t seed) {
  World w;
  memset(&w, 0, sizeof w);
  w.c = (SrCity *)calloc(1, sizeof *w.c);
  sr_city_generate(w.c, seed, NULL);
  w.pop = (SrPopulation *)calloc(1, sizeof *w.pop);
  sr_population_generate(w.pop, w.c);
  sr_events_init(&w.log);
  w.ch = (SrChess *)calloc(1, sizeof *w.ch);
  sr_chess_init(w.ch, w.c, seed, -1, 0);
  return w;
}

/* Skips game time minute by minute to an absolute time. */
static void sr_skip_to(SrGame *g, int64_t t) {
  if (t > sr_game_time(g)) sr_game_skip(g, t - sr_game_time(g), false);
}

static void world_free(World *w) {
  free(w->c);
  free(w->pop);
  free(w->ch);
}

static void test_soak_bounds(void) {
  SR_SPEC("faction-simulation", "Resources stay in bounds");
  SR_SPEC("faction-simulation", "Long soak keeps every megacorp");
  World w = world(7);
  int64_t end = 52LL * SR_WEEK_S;
  bool in_bounds = true;
  for (int64_t t = SR_TICK_S; t <= end; t += SR_TICK_S) {
    sr_chess_advance(w.ch, w.c, w.pop, &w.log, t);
    for (int f = 0; f < w.c->factions.count; f++)
      for (int r = 0; r < SR_RES_COUNT; r++) {
        int16_t lo, hi;
        sr_resource_bounds((SrResource)r, &lo, &hi);
        if (w.ch->f[f].res[r] < lo || w.ch->f[f].res[r] > hi) in_bounds = false;
      }
  }
  SR_CHECK(in_bounds);
  for (int f = 0; f < w.c->factions.corp_count; f++) {
    SR_CHECK(w.ch->f[f].res[SR_RES_CAPITAL] >= 25);
    SR_CHECK(w.c->buildings[w.c->factions.f[f].hq_building].owner == f);
  }
  int ops = 0, done = 0;
  for (int f = 0; f < w.c->factions.count; f++)
    for (int t = 0; t < SR_OP_COUNT; t++) ops += w.ch->f[f].ops_by_type[t];
  for (int i = 0; i < w.ch->op_count; i++) done += w.ch->ops[i].succeeded;
  SR_CHECK(done > 0);
  printf("  soak: %d ops, %d news, %d elections, act %d, ending %d, arcs %d/%d\n", ops, w.ch->news.count,
         w.ch->pol.elections_held, w.ch->story.act, w.ch->story.ending, w.ch->story.arcs_started,
         w.ch->story.arcs_finished);
  for (int i = 0; i < 12 && i < w.ch->history_count; i++)
    printf("    %s\n", w.ch->history[(w.ch->history_count - 1 - i) % SR_MAX_HISTORY]);
  SR_CHECK(ops > 50);
  world_free(&w);
}

static void test_campaign_bot(void) {
  SR_SPEC("story-campaign", "Bots complete the campaign");
  SrGame *g = sr_game_create();
  sr_game_new(g, 5, NULL, SR_BG_STREET_KID, "Bot");
  g->crowd_enabled = false;
  sr_game_leave(g);
  SrAutoplayStats st;
  int days = sr_campaign(g, 7 * 40, &st);
  printf("  campaign: %d days, %d jobs, act %d, ending %s, trust %d, cred %d, arcs %d\n", days, st.jobs_done,
         g->chess.story.act, sr_ending_name((SrEnding)g->chess.story.ending), g->chess.story.trust,
         g->player.prog.street_cred, g->chess.story.arcs_started);
  SR_CHECK(days > 0);
  SR_CHECK(!st.soft_lock);
  sr_game_destroy(g);
}

/* ---- helpers --------------------------------------------------------------------------------- */

static SrGame *play(uint64_t seed) {
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, SR_BG_STREET_KID, "Tester");
  g->crowd_enabled = false;
  sr_game_leave(g);
  return g;
}

static int facility(const SrCity *c, int f) {
  return c->factions.f[f].facility_count > 0 ? c->factions.f[f].facilities[0] : c->factions.f[f].hq_building;
}

static int district_of(const SrCity *c, int b) { return c->blocks[c->buildings[b].block].district; }

static SrContract *contract_of(SrChess *ch, int op) {
  SrOperation *o = sr_chess_op(ch, op);
  return o ? sr_chess_contract(ch, o->contract) : NULL;
}

/* ---- faction simulation ------------------------------------------------------------------- */

static void test_strategy(void) {
  SR_SPEC("faction-simulation", "Deterministic strategy");
  SR_SPEC("faction-simulation", "Doctrine shapes choices");
  World a = world(21), b = world(21);
  for (int64_t t = SR_TICK_S; t <= 8LL * SR_WEEK_S; t += SR_TICK_S) {
    sr_chess_advance(a.ch, a.c, a.pop, &a.log, t);
    sr_chess_advance(b.ch, b.c, b.pop, &b.log, t);
  }
  SR_CHECK(sr_chess_digest(a.ch) == sr_chess_digest(b.ch));
  SR_CHECK(a.ch->op_count > 0);
  world_free(&b);
  /* the same city with corp 0 a Shark and corp 1 a Fortress */
  World d = world(33);
  d.c->factions.f[0].doctrine = SR_DOC_SHARK;
  d.c->factions.f[1].doctrine = SR_DOC_FORTRESS;
  sr_chess_init(d.ch, d.c, 33, -1, 0);
  for (int64_t t = SR_TICK_S; t <= 52LL * SR_WEEK_S; t += SR_TICK_S) sr_chess_advance(d.ch, d.c, d.pop, &d.log, t);
  const uint16_t *shark = d.ch->f[0].ops_by_type, *fort = d.ch->f[1].ops_by_type;
  int shark_hostile = shark[SR_OP_SABOTAGE] + shark[SR_OP_TAKEOVER];
  int fort_hostile = fort[SR_OP_SABOTAGE] + fort[SR_OP_TAKEOVER];
  SR_CHECK(shark_hostile > fort_hostile);
  SR_CHECK(fort[SR_OP_FORTIFY] > shark[SR_OP_FORTIFY]);
  for (int t = 0; t <= SR_OP_COUNT; t++) SR_CHECK(sr_op_name((SrOpType)t) != NULL);
  world_free(&a);
  world_free(&d);
}

static void test_contracts(void) {
  SR_SPEC("faction-simulation", "Operation step becomes a contract");
  SR_SPEC("faction-simulation", "Unclaimed contract resolved");
  World w = world(8);
  int b = facility(w.c, 1);
  int id = sr_chess_start_op(w.ch, w.c, SR_OP_SABOTAGE, 0, 1, b, -1, 100);
  SrContract *k = contract_of(w.ch, id);
  SR_CHECK(k != NULL);
  if (!k) return;
  SR_CHECK(k->open);
  SR_CHECK_EQ(k->job_type, SR_JT_SABOTAGE);
  SR_CHECK_EQ(k->channel, SR_CH_FIXER);
  SR_CHECK_EQ(k->building, b);
  SR_CHECK(k->deadline > 100);
  /* the patron's operations go through the handler, gangs through their contacts */
  w.ch->story.patron = 2;
  w.ch->story.retainer = true;
  SR_CHECK_EQ(contract_of(w.ch, sr_chess_start_op(w.ch, w.c, SR_OP_THEFT, 2, 1, b, -1, 100))->channel, SR_CH_HANDLER);
  int g0 = w.c->factions.gang_first, g1 = g0 + 1;
  SR_CHECK_EQ(contract_of(w.ch, sr_chess_start_op(w.ch, w.c, SR_OP_TURF, g0, g1, -1, 0, 100))->channel, SR_CH_GANG);
  int takeover = sr_chess_start_op(w.ch, w.c, SR_OP_TAKEOVER, 0, 1, b, -1, 100);
  SR_CHECK_EQ(contract_of(w.ch, takeover)->job_type, SR_JT_STEAL_DATA); /* plans first */
  /* nobody takes it: an NPC crew tries at the deadline */
  int events = w.log.count;
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, k->deadline + SR_TICK_S);
  SR_CHECK(!sr_chess_op(w.ch, id)->active);
  SR_CHECK(!sr_chess_contract(w.ch, sr_chess_op(w.ch, id)->contract)->open);
  if (sr_chess_op(w.ch, id)->succeeded) SR_CHECK(w.log.count > events);
  SR_CHECK(sr_chess_contract_for_job(w.ch, -1) == NULL);
  SR_CHECK(sr_chess_op(w.ch, 99999) == NULL);
  world_free(&w);
}

static void test_counterplay(void) {
  SR_SPEC("faction-simulation", "Security rises after an attack");
  SR_SPEC("faction-simulation", "Retaliation follows attribution");
  SR_SPEC("faction-simulation", "Framing redirects blame");
  World w = world(9);
  int b = facility(w.c, 1);
  int before = sr_chess_security(w.ch, b);
  int id = sr_chess_start_op(w.ch, w.c, SR_OP_SABOTAGE, 0, 1, b, -1, 100);
  SrContract *k = contract_of(w.ch, id);
  k->framed = 2; /* the player planted evidence pointing at corp 2 */
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, k, true, true, 200);
  SR_CHECK(sr_chess_security(w.ch, b) >= before + 2);
  SR_CHECK_EQ(sr_chess_op(w.ch, id)->blamed, 2);
  /* the victim strikes back at whoever the evidence points to (60% of attacks are answered) */
  int answered = 0;
  for (int n = 0; n < 12; n++) {
    int o = sr_chess_start_op(w.ch, w.c, SR_OP_SMEAR, 0, 3, -1, -1, 300);
    SrContract *kk = contract_of(w.ch, o);
    kk->framed = 4;
    sr_chess_resolve(w.ch, w.c, w.pop, &w.log, kk, true, true, 300);
    if (w.ch->f[3].retaliate_against == 4) answered++;
  }
  SR_CHECK(answered > 0);
  w.ch->f[3].retaliate_against = 4;
  w.ch->f[3].retaliate_by = 400 + 7 * 86400;
  w.ch->f[3].cooldown_until = 0;
  for (int i = 0; i < w.ch->op_count; i++) w.ch->ops[i].active = false;
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, SR_TICK_S * 2);
  bool struck = false;
  for (int i = 0; i < w.ch->op_count; i++)
    struck |= w.ch->ops[i].type == SR_OP_RETALIATE && w.ch->ops[i].client == 3 && w.ch->ops[i].target == 4;
  SR_CHECK(struck);
  /* no evidence at all: nobody to blame */
  int clean = sr_chess_start_op(w.ch, w.c, SR_OP_THEFT, 0, 1, b, -1, 500);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, clean), true, true, 500);
  SR_CHECK_EQ(sr_chess_op(w.ch, clean)->blamed, -1);
  int traced = sr_chess_start_op(w.ch, w.c, SR_OP_THEFT, 0, 1, b, -1, 500);
  contract_of(w.ch, traced)->player_evidence = true;
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, traced), false, true, 500);
  SR_CHECK_EQ(sr_chess_op(w.ch, traced)->blamed, -2);
  SR_CHECK(sr_chess_op(w.ch, traced)->failed);
  world_free(&w);
}

static void test_consequences(void) {
  SR_SPEC("faction-simulation", "Sabotage raises prices");
  SR_SPEC("faction-simulation", "Turf change is visible");
  SR_SPEC("faction-simulation", "Gang replaced");
  World w = world(10);
  int b = facility(w.c, 1), d = district_of(w.c, b);
  SR_CHECK_EQ(sr_chess_price_pct(w.ch, d), 100);
  int id = sr_chess_start_op(w.ch, w.c, SR_OP_SABOTAGE, 0, 1, b, -1, 100);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, id), true, false, 100);
  SR_CHECK(sr_chess_price_pct(w.ch, d) > 100);
  SR_CHECK_EQ(sr_chess_price_pct(w.ch, -1), 100);
  /* a takeover changes the owner (and is recorded for loading) */
  int tid = sr_chess_start_op(w.ch, w.c, SR_OP_TAKEOVER, 0, 1, b, -1, 100);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, tid), true, false, 100);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, tid), true, false, 100);
  SR_CHECK_EQ(w.c->buildings[b].owner, 0);
  SrCity *fresh = (SrCity *)calloc(1, sizeof *fresh);
  sr_city_generate(fresh, 10, NULL);
  sr_chess_apply_changes(w.ch, fresh);
  SR_CHECK_EQ(fresh->buildings[b].owner, 0);
  /* turf war: the district changes hands and the walls are repainted */
  int g0 = w.c->factions.gang_first, g1 = g0 + 1;
  int blk = -1;
  for (int k = 0; k < w.c->grid * w.c->grid && blk < 0; k++)
    if (!w.c->blocks[k].water && w.c->blocks[k].building_count > 2) blk = k;
  int dz = w.c->blocks[blk].district, dy = (dz + 1) % w.c->district_count;
  for (int g = g0; g < g0 + w.c->factions.gang_count; g++)
    w.c->factions.f[g].turf = (uint16_t)(w.c->factions.f[g].turf & ~((1u << dz) | (1u << dy)));
  w.c->factions.f[g1].turf = (uint16_t)(w.c->factions.f[g1].turf | (1u << dz) | (1u << dy));
  w.c->factions.f[g0].hue = (uint8_t)((w.c->factions.f[g1].hue + 12) % 24);
  int n = sr_chunk_count(w.c), cx = -1, cy = -1;
  for (int y = 0; y < n && cx < 0; y++)
    for (int x = 0; x < n && cx < 0; x++)
      for (int by = y * SR_CHUNK_BLOCKS; by < (y + 1) * SR_CHUNK_BLOCKS; by++)
        for (int bx = x * SR_CHUNK_BLOCKS; bx < (x + 1) * SR_CHUNK_BLOCKS; bx++)
          if (sr_city_block_index(w.c, bx, by) == blk) cx = x, cy = y;
  SrDeltas dl;
  memset(&dl, 0, sizeof dl);
  SrMesh m;
  memset(&m, 0, sizeof m);
  sr_chunk_build(w.c, &dl, cx, cy, &m);
  uint64_t before = sr_mesh_digest(&m);
  uint32_t ver = w.ch->turf_version;
  int turf = sr_chess_start_op(w.ch, w.c, SR_OP_TURF, g0, g1, -1, dz, 100);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, turf), true, false, 100);
  SR_CHECK_EQ(sr_chess_gang_of(w.c, dz), g0);
  SR_CHECK(w.ch->turf_version != ver);
  sr_chunk_build(w.c, &dl, cx, cy, &m);
  SR_CHECK(sr_mesh_digest(&m) != before); /* graffiti in the new gang's colour */
  sr_mesh_free(&m);
  /* the last district falls: the gang is finished and a new one appears weeks later */
  w.c->factions.f[g1].turf = (uint16_t)(1u << dy);
  int last = sr_chess_start_op(w.ch, w.c, SR_OP_TURF, g0, g1, -1, dy, 100);
  sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, last), true, false, 100);
  SR_CHECK(w.ch->f[g1].dissolved);
  char old_name[SR_NAME_LEN];
  snprintf(old_name, sizeof old_name, "%s", w.c->factions.f[g1].name);
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, 100 + SR_GANG_REBIRTH_S + SR_WEEK_S * 3);
  SR_CHECK(w.ch->f[g1].generation >= 1); /* reborn (and maybe already in trouble again) */
  SR_CHECK(strcmp(old_name, w.c->factions.f[g1].name) != 0);
  SR_CHECK(w.ch->f[g1].dissolved || w.c->factions.f[g1].turf != 0);
  sr_chess_apply_changes(w.ch, fresh); /* the new gang's identity survives a reload */
  SR_CHECK_STR(fresh->factions.f[g1].name, w.c->factions.f[g1].name);
  free(fresh);
  world_free(&w);
}

static void test_intel_board(void) {
  SR_SPEC("faction-simulation", "Intel reveals a link");
  SrGame *g = play(12);
  SrChess *ch = &g->chess;
  SR_CHECK(ch->link_count > 0);
  const SrLink *l = &ch->links[0];
  SR_CHECK(!sr_chess_link_known(ch, l->a, l->b, (SrLinkType)l->type));
  sr_game_chess_paydata(g, g->city->factions.f[l->a].hq_building); /* stolen files from its host */
  SR_CHECK(sr_chess_link_known(ch, l->a, l->b, (SrLinkType)l->type));
  SR_CHECK(!sr_chess_link_known(ch, 99, 98, SR_LINK_FEUD));
  sr_chess_reveal(ch, l->a, l->b, (SrLinkType)l->type);
  SR_CHECK_EQ(sr_chess_reveal_faction(ch, l->a), 0); /* nothing new */
  g->term_tab = SR_TAB_BOARD;
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  g->screen = SR_SCREEN_TERMINAL;
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  sr_game_destroy(g);
}

/* ---- politics -------------------------------------------------------------------------------- */

static void test_elections(void) {
  SR_SPEC("city-politics", "Election held on schedule");
  SR_SPEC("city-politics", "Scandal costs votes");
  SR_SPEC("city-politics", "Vote tampering");
  World w = world(14);
  SrPolitics *p = &w.ch->pol;
  SR_CHECK(p->seat_count >= 5);
  int held = p->elections_held, news = w.ch->news.count;
  int64_t due = p->next_council;
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, due - SR_TICK_S);
  SR_CHECK_EQ(p->elections_held, held);
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, due + SR_TICK_S);
  SR_CHECK(p->elections_held > held);
  SR_CHECK(p->next_council > due);
  bool announced = false;
  for (int i = 0; i < sr_news_size(&w.ch->news); i++)
    announced |= sr_news_latest(&w.ch->news, i)->kind == SR_NEWS_ELECTION;
  SR_CHECK(announced && w.ch->news.count > news);
  /* a scandal costs votes */
  SR_CHECK(sr_politics_share(w.ch, w.c, 0, 1, 20) < sr_politics_share(w.ch, w.c, 0, 1, 0));
  /* tampering: the count is altered for corp 2, which takes the seat */
  for (int s = 0; s < 5; s++) {
    w.ch->pol.tamper_for = 2;
    w.ch->pol.seats[s].backer = 2;
    w.ch->f[2].res[SR_RES_INFLUENCE] = 10;
    w.ch->f[2].res[SR_RES_IMAGE] = 10;
    SR_CHECK_EQ(sr_politics_elect(w.ch, w.c, s, due), 2);
  }
  w.ch->pol.tamper_for = 1; /* against the incumbent */
  w.ch->pol.seats[0].backer = 3;
  SR_CHECK(sr_politics_share(w.ch, w.c, 3, 1, 0) < 0.5f);
  sr_politics_elect(w.ch, w.c, SR_COUNCIL_SEATS, due); /* the mayor */
  SR_CHECK(w.ch->pol.mayor.name[0] != '\0');
  /* the mayoral election comes on its own schedule */
  int64_t mayor = w.ch->pol.next_mayor;
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, mayor + SR_TICK_S);
  SR_CHECK(w.ch->pol.next_mayor > mayor);
  world_free(&w);
}

static void test_policies(void) {
  SR_SPEC("city-politics", "Curfew in effect");
  SR_SPEC("city-politics", "Police contract privatised");
  SrGame *g = play(15);
  SrChess *ch = &g->chess;
  int d = district_of(g->city, g->player.home);
  /* violence in the district and a law-and-order council: the next vote declares a curfew */
  for (int s = 0; s < ch->pol.seat_count; s++) ch->pol.seats[s].lean = 2;
  for (int i = 0; i < 3; i++) {
    SrWorldEvent e = {SR_EV_GUNFIRE, sr_game_time(g), (int8_t)d, -1, -1, -1, true, "shots fired"};
    sr_events_add(&g->events, &e);
  }
  ch->pol.next_vote = sr_game_time(g);
  sr_politics_tick(ch, g->city, &g->events, sr_game_time(g));
  SR_CHECK(ch->pol.curfew & (1u << d));
  SR_CHECK_EQ(ch->pol.police_budget, 2);
  int64_t day = sr_game_time(g) / 86400 * 86400;
  SR_CHECK(sr_politics_curfew_now(&ch->pol, d, day + 23 * 3600));
  SR_CHECK(!sr_politics_curfew_now(&ch->pol, d, day + 12 * 3600));
  SR_CHECK(!sr_politics_curfew_now(&ch->pol, -1, day));
  /* at night most citizens stay indoors there */
  g->crowd_enabled = true;
  sr_game_teleport_building(g, g->player.home);
  g->cam.pos = g->player.pos;
  g->cam.forward = sr_v3(0, 0, -1);
  g->cam.cos_half_fov = 0.5f;
  SrCrowd open;
  sr_crowd_init(&open);
  open.fresh = true;
  sr_crowd_refresh(&open, g->city, g->pop, &g->cam, day + 23 * 3600, 400);
  sr_skip_to(g, day + 86400 + 23 * 3600);
  SR_CHECK(g->crowd.curfew & (1u << d));
  sr_crowd_init(&g->crowd);
  g->crowd.curfew = (uint16_t)(1u << d);
  sr_crowd_refresh(&g->crowd, g->city, g->pop, &g->cam, day + 23 * 3600, 400);
  SR_CHECK(g->crowd.count < open.count);
  /* an officer stops the player on the street */
  int64_t cash = g->player.cred;
  g->crowd.count = 1;
  memset(&g->crowd.agents[0], 0, sizeof(SrAgent));
  g->crowd.agents[0].role = SR_ROLE_COP;
  g->crowd.agents[0].pos = g->player.pos;
  g->curfew_stop_at = 0;
  sr_game_chess_minute(g, sr_game_time(g) + 60);
  SR_CHECK(g->player.cred < cash);
  /* a corp with a council majority gets the police contract */
  for (int s = 0; s < ch->pol.seat_count; s++) ch->pol.seats[s].backer = 1;
  ch->pol.next_vote = sr_game_time(g);
  sr_politics_tick(ch, g->city, &g->events, sr_game_time(g));
  SR_CHECK_EQ(ch->pol.police_contract, 1);
  SR_CHECK_EQ(sr_game_police(g), 1);
  SR_CHECK_EQ(sr_game_jurisdiction(g, g->player.home), 1);
  sr_game_chess_minute(g, (sr_game_time(g) / 3600 + 1) * 3600);
  SR_CHECK_EQ(g->crowd.police_outfit, g->city->factions.f[1].hue % 16);
  sr_crowd_init(&g->crowd);
  g->crowd.police_outfit = (int8_t)(g->city->factions.f[1].hue % 16);
  g->crowd.police_faction = 1;
  g->crowd.fresh = true;
  bool coloured = true;
  int cops = 0;
  for (int k = 0; k < 6 && cops == 0; k++) { /* walk around until officers are in view */
    sr_game_teleport_building(g, g->city->factions.precinct_building[k % g->city->factions.precinct_count]);
    g->cam.pos = g->player.pos;
    sr_crowd_refresh(&g->crowd, g->city, g->pop, &g->cam, day + 12 * 3600, 400);
    for (int i = 0; i < g->crowd.count; i++)
      if (g->crowd.agents[i].role == SR_ROLE_COP) {
        cops++;
        coloured &= g->crowd.agents[i].look.outfit_color == g->crowd.police_outfit && g->crowd.agents[i].faction == 1;
      }
  }
  SR_CHECK(coloured);
  /* the majority falls apart: the police go back to the city */
  for (int s = 0; s < ch->pol.seat_count; s++) ch->pol.seats[s].backer = -1, ch->pol.seats[s].lean = -2;
  ch->pol.next_vote = sr_game_time(g);
  sr_politics_tick(ch, g->city, &g->events, sr_game_time(g));
  SR_CHECK_EQ(ch->pol.police_contract, -1);
  SR_CHECK_EQ(ch->pol.police_budget, 0);
  SR_CHECK(!(ch->pol.curfew & (1u << d)));
  sr_game_destroy(g);
}

static void test_exposure(void) {
  SR_SPEC("city-politics", "Exposed councillor resigns");
  SrGame *g = play(16);
  SrChess *ch = &g->chess;
  int seat = 0;
  for (int s = 0; s < ch->pol.seat_count; s++) ch->pol.seats[s].backer = ch->pol.seats[s].bribed = -1;
  ch->pol.seats[seat].bribed = 2;
  int hq = g->city->factions.f[2].hq_building;
  SrNodeAction acts[8];
  int n = sr_game_chess_node_actions(g, hq, acts);
  bool leak = false;
  for (int i = 0; i < n; i++) leak |= acts[i] == SR_NA_LEAK_BRIBES;
  SR_CHECK(leak);
  int64_t t = sr_game_time(g);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hq, SR_NA_LEAK_BRIBES), SR_OK);
  SR_CHECK(sr_chess_link_known(ch, 2, seat, SR_LINK_BRIBES));
  sr_skip_to(g, t + 2 * 3600); /* the story runs */
  SR_CHECK(ch->pol.seats[seat].exposed);
  sr_skip_to(g, t + 3 * 86400);
  SR_CHECK(ch->pol.seats[seat].vacant);
  SR_CHECK(ch->pol.by_election > 0);
  sr_skip_to(g, ch->pol.by_election + SR_TICK_S + 3600);
  SR_CHECK(!ch->pol.seats[seat].vacant);
  SR_CHECK_EQ(ch->pol.by_election, 0);
  sr_politics_expose(ch, g->city, -1, t);
  sr_game_destroy(g);
}

/* ---- market ---------------------------------------------------------------------------------- */

static void test_market(void) {
  SR_SPEC("corporate-market", "Price reacts to an outcome");
  SR_SPEC("corporate-market", "Buying shares");
  SR_SPEC("corporate-market", "Suspicious trade flagged");
  SrGame *g = play(17);
  SrChess *ch = &g->chess;
  int32_t before = ch->market.price[1];
  int id = sr_chess_start_op(ch, g->city, SR_OP_SABOTAGE, 0, 1, facility(g->city, 1), -1, sr_game_time(g));
  sr_chess_resolve(ch, g->city, g->pop, &g->events, contract_of(ch, id), true, false, sr_game_time(g));
  sr_market_tick(ch, g->city);
  SR_CHECK(ch->market.price[1] < before);
  /* buying 10 shares */
  g->player.cred = 100000;
  int32_t price = ch->market.price[2];
  int64_t cash = g->player.cred;
  SR_CHECK_EQ(sr_game_trade(g, 2, 10), SR_OK);
  SR_CHECK_EQ(g->player.cred, cash - 10LL * price - sr_market_fee(10 * price));
  SR_CHECK_EQ(ch->market.shares[2], 10);
  SR_CHECK(sr_market_portfolio(&ch->market, g->city) >= 10LL * price);
  SR_CHECK_EQ(sr_game_trade(g, 2, -10), SR_OK);
  SR_CHECK_EQ(sr_game_trade(g, 2, -200), SR_ERR_DENIED); /* past the margin */
  g->player.cred = 0;
  SR_CHECK_EQ(sr_game_trade(g, 2, 10), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_trade(g, 2, 0), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_market_fee(-1000), 20);
  /* a large short just before a sabotage against the corp: the banks notice */
  int bank = 3, target = 4;
  g->city->factions.f[bank].sector1 = SR_SEC_FINANCE;
  g->player.cred = 100000;
  SR_CHECK_EQ(sr_game_trade(g, target, -60), SR_OK);
  SrJob j;
  SR_CHECK(sr_job_for_contract(g->city, SR_JT_SABOTAGE, facility(g->city, target), 0, target, 5, sr_game_time(g), &j));
  j.tier = 0;
  SrJob *pj = sr_jobs_push(&g->jobs, &j);
  int standing = g->player.prog.standing[bank];
  SR_CHECK_EQ(sr_game_accept(g, pj->id), SR_OK);
  bool flagged = false;
  for (int i = 0; i < ch->market.suspicion_count; i++)
    flagged |= ch->market.suspicion[i].corp == target && ch->market.suspicion[i].by == bank;
  SR_CHECK(flagged);
  SR_CHECK(g->player.prog.standing[bank] < standing);
  /* small or old trades draw nothing */
  SR_CHECK_EQ(sr_market_check_insider(ch, g->city, target, sr_game_time(g) + 3 * 86400), 0);
  for (int k = 0; k < 20; k++) sr_market_trade(ch, 5, 1, 1000000, sr_game_time(g), &flagged);
  SR_CHECK_EQ(ch->market.trade_count, SR_MAX_TRADES);
  /* no bank in town: the regulator notes it */
  for (int f = 0; f < g->city->factions.corp_count; f++)
    g->city->factions.f[f].sector1 = g->city->factions.f[f].sector2 = SR_SEC_ARMS;
  sr_market_trade(ch, 5, 80, 1000000, sr_game_time(g), &flagged);
  for (int k = 0; k < 10; k++) sr_market_check_insider(ch, g->city, 5, sr_game_time(g));
  SR_CHECK_EQ(ch->market.suspicion[ch->market.suspicion_count - 1].by, -1);
  sr_game_destroy(g);
}

/* ---- news ------------------------------------------------------------------------------------ */

static void test_news(void) {
  SR_SPEC("news-media", "Event becomes news");
  SR_SPEC("news-media", "Owner protection");
  SR_SPEC("news-media", "Billboards show headlines");
  SR_SPEC("news-media", "Anonymous reporting");
  World w = world(18);
  SrChess *ch = w.ch;
  int d = 2;
  SrWorldEvent e = {SR_EV_SABOTAGE, 1000, (int8_t)d, facility(w.c, 1), 1, -1, true, "Raid on a plant"};
  sr_events_add(&w.log, &e);
  sr_news_tick(ch, w.c, &w.log, 1000 + 3600); /* within 2 hours */
  const SrNews *it = sr_news_latest(&ch->news, 0);
  SR_CHECK(it != NULL);
  SR_CHECK_EQ(it->subject, 1);
  SR_CHECK(it->time - 1000 <= 2 * 3600);
  SR_CHECK_EQ(it->perpetrator, -1); /* no evidence: no culprit named */
  SR_CHECK(strstr(it->headline, "blamed") == NULL && strstr(it->headline, "suspected") == NULL);
  /* the billboards of that district run it */
  bool shown = false;
  for (int64_t t = 0; t < 200; t += 20) shown |= sr_news_billboard(&ch->news, d, t) == it->headline;
  SR_CHECK(shown);
  SR_CHECK(sr_news_billboard(&(SrNewsDesk){0}, d, 0) == NULL);
  /* the player was seen: the press says so; an outlet never names its owner */
  SrWorldEvent seen = {SR_EV_THEFT, 2000, (int8_t)d, -1, 2, -2, true, "Data heist"};
  sr_events_add(&w.log, &seen);
  int m = w.c->factions.media_first;
  int owner = w.c->factions.f[m].owner >= 0 ? w.c->factions.f[m].owner : 0;
  w.c->factions.f[m].owner = (int8_t)owner;
  SrWorldEvent corp = {SR_EV_SABOTAGE, 2000, (int8_t)d, -1, (int8_t)(owner == 3 ? 4 : 3), (int8_t)owner, true, "Fire"};
  sr_events_add(&w.log, &corp);
  SrWorldEvent sym = {SR_EV_SABOTAGE, 2000, (int8_t)d, -1, (int8_t)owner, 5, true, "Blast"};
  sr_events_add(&w.log, &sym);
  sr_news_tick(ch, w.c, &w.log, 2500);
  SR_CHECK(sr_news_reported(&ch->news, 2, false));
  /* owner protection: no scandal about the owner on its own channel until an independent ran one */
  SR_CHECK(!sr_news_publish(ch, w.c, m, owner, -1, -1, SR_NEWS_SCANDAL, true, "Owner scandal", 3000));
  SR_CHECK(sr_news_publish(ch, w.c, m, owner, -1, -1, SR_NEWS_STORY, false, "Owner opens a park", 3000));
  SR_CHECK(sr_news_publish(ch, w.c, -1, owner, -1, -1, SR_NEWS_SCANDAL, true, "Pirate feed: the truth", 3000));
  SR_CHECK(sr_news_independent_scandal(&ch->news, w.c, owner));
  SR_CHECK(sr_news_publish(ch, w.c, m, owner, -1, -1, SR_NEWS_SCANDAL, true, "Owner scandal", 3100));
  SR_CHECK(sr_news_reported(&ch->news, owner, true));
  SR_CHECK(sr_news_latest(&ch->news, 999) == NULL);
  /* the investigation into corporate violence can be killed before it runs */
  bool queued = ch->news.pending_count > 0;
  SR_CHECK(queued);
  SR_CHECK(sr_news_kill(ch, -1));
  for (int k = 0; k < SR_MAX_PENDING + 2; k++) sr_news_queue(ch, -1, 1, -1, false, false, "Filler", 1 << 30);
  SR_CHECK_EQ(ch->news.pending_count, SR_MAX_PENDING);
  SR_CHECK(!sr_news_kill(ch, 77));
  for (int k = 0; k < SR_MAX_NEWS + 5; k++) sr_news_publish(ch, w.c, -1, -1, -1, -1, SR_NEWS_STORY, false, "x", 4000);
  SR_CHECK_EQ(sr_news_size(&ch->news), SR_MAX_NEWS);
  world_free(&w);
}

static void test_planted_story(void) {
  SR_SPEC("news-media", "Planted story lowers public image");
  SrGame *g = play(19);
  SrChess *ch = &g->chess;
  int m = g->city->factions.media_first, hq = g->city->factions.f[m].hq_building;
  SrNodeAction acts[8];
  int n = sr_game_chess_node_actions(g, hq, acts);
  SR_CHECK(n >= 1 && acts[0] == SR_NA_PLANT_STORY);
  int target = -1;
  for (int f = 0; f < g->city->factions.corp_count; f++)
    if (f != ch->story.patron && (target < 0 || ch->f[f].res[SR_RES_CAPITAL] > ch->f[target].res[SR_RES_CAPITAL]))
      target = f;
  int image = ch->f[target].res[SR_RES_IMAGE];
  int64_t t = sr_game_time(g);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hq, SR_NA_PLANT_STORY), SR_OK);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hq, SR_NA_KILL_STORY), SR_OK); /* changed our mind */
  SR_CHECK_EQ(sr_game_chess_node_act(g, hq, SR_NA_KILL_STORY), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hq, SR_NA_PLANT_STORY), SR_OK);
  sr_skip_to(g, t + 2 * 3600);
  SR_CHECK(sr_news_reported(&ch->news, target, true));
  SR_CHECK(ch->f[target].res[SR_RES_IMAGE] < image);
  SR_CHECK_EQ(ch->story.leaks, 1);
  /* the vote host at city hall */
  int hall = sr_chess_city_hall(g->city);
  n = sr_game_chess_node_actions(g, hall, acts);
  bool vote = false;
  for (int i = 0; i < n; i++) vote |= acts[i] == SR_NA_TAMPER_VOTE;
  SR_CHECK(vote);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hall, SR_NA_TAMPER_VOTE), SR_OK);
  SR_CHECK(ch->pol.tamper_for >= 0);
  SR_CHECK_EQ(sr_game_chess_node_act(g, hall, SR_NA_LEAK_BRIBES), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_chess_node_actions(g, -1, acts), 0);
  for (int a = SR_NA_PLANT_STORY; a <= SR_NA_LEAK_BRIBES; a++) SR_CHECK(sr_node_action_name((SrNodeAction)a)[0]);
  sr_game_destroy(g);
}

/* ---- story ----------------------------------------------------------------------------------- */

static void test_patron(void) {
  SR_SPEC("story-campaign", "Random patron");
  SR_SPEC("story-campaign", "Chosen patron");
  SR_SPEC("story-campaign", "Retainer offer");
  SrGame *g = play(20);
  int p = g->chess.story.patron;
  SR_CHECK(p >= 0 && p < g->city->factions.corp_count);
  SrGame *h = play(20);
  SR_CHECK_EQ(h->chess.story.patron, p); /* chosen from the seed */
  sr_game_destroy(h);
  h = sr_game_create();
  h->patron_choice = (int8_t)((p + 1) % g->city->factions.corp_count);
  sr_game_new(h, 20, NULL, SR_BG_STREET_KID, "Chooser");
  SR_CHECK_EQ(h->chess.story.patron, (p + 1) % g->city->factions.corp_count);
  sr_game_destroy(h);
  /* the first gig is the patron's test: completing it brings the handler's offer within 2 days */
  SrStory *st = &g->chess.story;
  int ids[4];
  int fixer = g->city->factions.fixer_building[district_of(g->city, g->player.home)];
  int k = sr_game_fixer(g, fixer, ids);
  SR_CHECK(k > 0);
  SR_CHECK_EQ(sr_game_accept(g, ids[0]), SR_OK);
  SR_CHECK_EQ(st->test_job, ids[0]);
  SrJob *j = sr_jobs_get(&g->jobs, ids[0]);
  j->executed = true;
  SR_CHECK(sr_game_report(g, ids[0]) > 0 || j->state == SR_JS_COMPLETED);
  int64_t done = sr_game_time(g);
  int msgs = g->player.msg_count;
  SR_CHECK_EQ(sr_game_accept_retainer(g), SR_ERR_INVALID); /* not yet */
  sr_skip_to(g, done + 2 * 86400);
  SR_CHECK(st->offer_open);
  SR_CHECK(st->offer_at - done <= 2 * 86400);
  SR_CHECK(g->player.msg_count > msgs);
  SR_CHECK_EQ(sr_game_accept_retainer(g), SR_OK);
  SR_CHECK(st->retainer);
  SR_CHECK_EQ(st->act, SR_ACT_PAWN);
  /* without a test, the end of the first week brings the offer anyway */
  SrGame *late = play(22);
  sr_skip_to(late, sr_game_time(late) + 8 * 86400);
  SR_CHECK(late->chess.story.offer_open);
  sr_game_destroy(late);
  sr_game_destroy(g);
}

static void test_trust(void) {
  SR_SPEC("story-campaign", "Trust gates black ops");
  SR_SPEC("story-campaign", "Betrayal discovered");
  SrGame *g = play(23);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 72; /* Knight */
  st->trust = 59;
  SR_CHECK_EQ(sr_game_handler_offers(g), 0);
  SR_CHECK(!sr_story_t4_allowed(st, SR_CRED_KNIGHT));
  st->trust = 60;
  SR_CHECK(sr_story_t4_allowed(st, SR_CRED_KNIGHT));
  SR_CHECK(!sr_story_t4_allowed(st, SR_CRED_OPERATOR));
  SR_CHECK_EQ(sr_game_handler_offers(g), 1);
  SR_CHECK_EQ(sr_game_handler_offers(g), 0); /* one at a time */
  SrJob *bo = &g->jobs.jobs[g->jobs.count - 1];
  SR_CHECK_EQ(bo->tier, 4);
  SR_CHECK_EQ(bo->client_faction, st->patron);
  SR_CHECK_EQ(sr_game_accept(g, bo->id), SR_OK);
  /* deniable work for the patron builds trust; failures cost it */
  sr_story_job_done(&g->chess, g->city, st->patron, 1, true, false, 0);
  SR_CHECK_EQ(st->trust, 66);
  sr_story_job_done(&g->chess, g->city, st->patron, 1, false, false, 0);
  SR_CHECK_EQ(st->trust, 58);
  /* a job against the patron that it pins on the player */
  int patron = st->patron;
  int rival = (patron + 1) % g->city->factions.corp_count;
  sr_story_job_done(&g->chess, g->city, rival, patron, true, false, 0); /* unseen: nothing */
  SR_CHECK_EQ(st->trust, 58);
  sr_story_job_done(&g->chess, g->city, rival, patron, true, true, 0);
  SR_CHECK_EQ(st->trust, 58 - SR_BETRAYAL);
  st->trust = 20;
  sr_story_job_done(&g->chess, g->city, rival, patron, true, true, sr_game_time(g));
  SR_CHECK_EQ(st->trust, 0);
  SR_CHECK_EQ(st->hostile, patron);
  SR_CHECK(st->patron < 0);
  sr_skip_to(g, sr_game_time(g) + SR_TICK_S + 60);
  bool hunt = false;
  for (int i = 0; i < g->chess.op_count; i++)
    hunt |= g->chess.ops[i].type == SR_OP_HUNT && g->chess.ops[i].client == patron;
  SR_CHECK(hunt);
  SR_CHECK(st->hunts >= 1);
  sr_game_destroy(g);
}

static void test_acts_and_defection(void) {
  SR_SPEC("story-campaign", "Act progression");
  SR_SPEC("story-campaign", "Defection");
  SrGame *g = play(24);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  g->player.prog.street_cred = 20; /* Pawn */
  st->trust = 45;
  sr_skip_to(g, sr_game_time(g) + SR_TICK_S + 60);
  SR_CHECK_EQ(st->act, SR_ACT_PAWN);
  g->player.prog.street_cred = 26; /* Runner */
  st->trust = 39;
  sr_skip_to(g, sr_game_time(g) + SR_TICK_S);
  SR_CHECK_EQ(st->act, SR_ACT_PAWN);
  st->trust = 40;
  sr_skip_to(g, sr_game_time(g) + SR_TICK_S);
  SR_CHECK_EQ(st->act, SR_ACT_KNIGHT);
  /* a rival division calls during Act II */
  sr_skip_to(g, sr_game_time(g) + 2 * SR_WEEK_S);
  SR_CHECK(st->rival_offer >= 0);
  int old = st->patron, rival = st->rival_offer;
  char handler[SR_NAME_LEN];
  snprintf(handler, sizeof handler, "%s", st->cast[SR_CAST_HANDLER].name);
  g->term_tab = SR_TAB_STORY;
  int kind = 0;
  SR_CHECK(sr_game_story_option(g, 0, &kind) != NULL);
  SR_CHECK_EQ(kind, 2);
  SR_CHECK_EQ(sr_game_accept_offer(g), SR_OK);
  SR_CHECK_EQ(st->patron, rival);
  SR_CHECK_EQ(st->hostile, old);
  SR_CHECK(strcmp(handler, st->cast[SR_CAST_HANDLER].name) != 0);
  SR_CHECK_EQ(sr_game_accept_offer(g), SR_ERR_INVALID);
  sr_story_defect(st, g->city, -1, 0); /* ignored */
  SR_CHECK_EQ(st->patron, rival);
  sr_game_destroy(g);
}

static void test_arcs(void) {
  SR_SPEC("story-campaign", "Arc instantiation");
  SR_SPEC("story-campaign", "Branching on outcome");
  SR_SPEC("story-campaign", "Skipped beat");
  for (int t = 0; t < SR_ARC_COUNT; t++) {
    World w = world(25);
    int idx = sr_story_arc_start(w.ch, w.c, w.pop, (SrArcType)t, 100);
    SR_CHECK(idx >= 0);
    const SrArc *a = &w.ch->story.arcs[idx];
    SR_CHECK(a->a >= 0 && a->a < w.c->factions.count);
    SR_CHECK(a->b >= 0 && a->b < w.c->factions.count && a->b != a->a);
    SR_CHECK(a->building >= 0 && a->building < w.c->building_count);
    bool person = a->person < 0;
    for (int i = 0; i < w.pop->count && !person; i++) person = (int32_t)w.pop->npcs[i].id == a->person;
    SR_CHECK(person);
    SR_CHECK(sr_chess_op(w.ch, a->op) != NULL);
    SR_CHECK(sr_arc_name((SrArcType)t)[0] != '?');
    world_free(&w);
  }
  /* the same seed, beat 1 won in one run and lost in the other */
  SrOpType next[2];
  for (int run = 0; run < 2; run++) {
    World w = world(26);
    int idx = sr_story_arc_start(w.ch, w.c, w.pop, SR_ARC_TAKEOVER, 100);
    SrArc *a = &w.ch->story.arcs[idx];
    a->building = facility(w.c, a->b);
    sr_chess_resolve(w.ch, w.c, w.pop, &w.log, contract_of(w.ch, a->op), run == 0, true, 200);
    SR_CHECK_EQ(a->beat, 1);
    next[run] = (SrOpType)sr_chess_op(w.ch, a->op)->type;
    world_free(&w);
  }
  SR_CHECK(next[0] != next[1]);
  /* the player lets a beat's job lapse: an NPC crew resolves it and the arc moves on */
  World w = world(27);
  int idx = sr_story_arc_start(w.ch, w.c, w.pop, SR_ARC_SCANDAL, 100);
  SrContract *k = contract_of(w.ch, w.ch->story.arcs[idx].op);
  sr_chess_advance(w.ch, w.c, w.pop, &w.log, k->deadline + SR_TICK_S);
  SR_CHECK(w.ch->story.arcs[idx].beat >= 1);
  SR_CHECK(w.ch->story.beats_offscreen >= 1);
  /* all three beats done: the arc ends; the slots fill up to three */
  for (int b = 0; b < 3 && w.ch->story.arcs[idx].active; b++) sr_story_beat_done(w.ch, w.c, idx, true, 300);
  SR_CHECK(!w.ch->story.arcs[idx].active);
  for (int i = 0; i < SR_MAX_ARCS; i++) sr_story_arc_start(w.ch, w.c, w.pop, SR_ARC_MOLE, 400);
  SR_CHECK_EQ(sr_story_arc_start(w.ch, w.c, w.pop, SR_ARC_MOLE, 400), -1);
  sr_story_beat_done(w.ch, w.c, -1, true, 0);
  world_free(&w);
}

static void test_cast_succession(void) {
  SR_SPEC("story-campaign", "Handler succession");
  SrGame *g = play(28);
  SrStory *st = &g->chess.story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  char old[SR_NAME_LEN];
  snprintf(old, sizeof old, "%s", st->cast[SR_CAST_HANDLER].name);
  int64_t t = sr_game_time(g);
  sr_story_cast_died(st, SR_CAST_HANDLER, t);
  sr_story_cast_died(st, SR_CAST_HANDLER, t); /* already gone */
  SR_CHECK(!st->cast[SR_CAST_HANDLER].alive);
  int msgs = g->player.msg_count;
  sr_skip_to(g, t + 3 * 86400);
  SR_CHECK(st->cast[SR_CAST_HANDLER].alive);
  SR_CHECK_EQ(st->cast[SR_CAST_HANDLER].generation, 1);
  SR_CHECK(strcmp(old, st->cast[SR_CAST_HANDLER].name) != 0);
  SR_CHECK(g->player.msg_count > msgs); /* the successor makes contact */
  for (int r = 0; r <= SR_CAST_COUNT; r++) SR_CHECK(sr_cast_role_name((SrCastRole)r) != NULL);
  sr_game_destroy(g);
}

static void test_endgame(void) {
  SR_SPEC("story-campaign", "Dominance triggers the endgame");
  SR_SPEC("story-campaign", "Sunrise shows daylight");
  SR_SPEC("story-campaign", "Continue after the ending");
  SrGame *g = play(29);
  SrChess *ch = &g->chess;
  SrStory *st = &ch->story;
  st->offer_open = true;
  sr_game_accept_retainer(g);
  st->act = SR_ACT_KNIGHT;
  st->act_started = sr_game_time(g);
  int leader = (st->patron + 1) % g->city->factions.corp_count;
  for (int f = 0; f < g->city->factions.corp_count; f++) {
    ch->f[f].res[SR_RES_CAPITAL] = f == leader ? 200 : 25;
    ch->f[f].res[SR_RES_INFLUENCE] = ch->f[f].res[SR_RES_FORCE] = f == leader ? 100 : 10;
  }
  int who = -1;
  SR_CHECK(sr_chess_dominance(ch, g->city, &who) >= SR_DOMINANCE);
  SR_CHECK_EQ(who, leader);
  sr_story_tick(ch, g->city, g->pop, sr_game_time(g));
  SR_CHECK_EQ(st->act, SR_ACT_PROMOTION);
  SR_CHECK(st->scenario != SR_SC_NONE);
  SR_CHECK(st->scenario_corp == leader || st->scenario_other == leader);
  SR_CHECK(st->arcs[0].active);
  SR_CHECK(sr_scenario_name((SrScenario)st->scenario)[0] != '?');
  /* the climax: Sunrise needs the Pall Protocol and the Deep */
  st->finale_ready = true;
  SR_CHECK(!(sr_story_endings(st) & (1u << SR_END_SUNRISE)));
  st->scenario = SR_SC_PALL;
  st->matrix_depth = 25;
  SR_CHECK(sr_story_endings(st) & (1u << SR_END_SUNRISE));
  SR_CHECK(!sr_sky_at(sr_game_time(g), g->pall_removed).daylight);
  g->term_tab = SR_TAB_STORY;
  SR_CHECK(sr_game_term_rows(g) >= 3);
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_DEFECTOR), SR_ERR_INVALID);
  int16_t cap = ch->f[st->scenario_corp].res[SR_RES_CAPITAL];
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_SUNRISE), SR_OK);
  SR_CHECK(g->pall_removed);
  SR_CHECK(sr_sky_at(sr_game_time(g), g->pall_removed).daylight);
  SR_CHECK(ch->f[st->scenario_corp].res[SR_RES_CAPITAL] < cap);
  /* the city goes on */
  SR_CHECK(st->sandbox);
  SR_CHECK_EQ(st->act, SR_ACT_EPILOGUE);
  int ticks = ch->ticks;
  SrActionState a;
  memset(&a, 0, sizeof a);
  for (int i = 0; i < 30; i++) sr_game_step(g, &a);
  sr_skip_to(g, sr_game_time(g) + 2 * 86400);
  SR_CHECK(ch->ticks > ticks);
  SR_CHECK_EQ(st->ending, SR_END_SUNRISE);
  SR_CHECK(g->pall_removed);
  SR_CHECK_EQ(sr_game_choose_ending(g, SR_END_LOYAL), SR_ERR_INVALID);
  for (int e = 0; e <= SR_END_SUNRISE + 1; e++) SR_CHECK(sr_ending_name((SrEnding)e) != NULL);
  /* every other ending, on fresh copies of the board */
  for (int e = SR_END_LOYAL; e <= SR_END_KINGMAKER; e++) {
    World w = world(30);
    w.ch->story.patron = 1;
    w.ch->story.scenario_corp = 0;
    w.ch->story.scenario_other = 2;
    sr_story_end(w.ch, w.c, (SrEnding)e, 100);
    SR_CHECK_EQ(w.ch->story.ending, e);
    world_free(&w);
  }
  /* Act II running too long ends it too; an undecided climax resolves itself */
  World w = world(31);
  w.ch->story.act = SR_ACT_KNIGHT;
  w.ch->story.retainer = true;
  sr_story_tick(w.ch, w.c, w.pop, SR_ACT2_MAX_S + 1);
  SR_CHECK_EQ(w.ch->story.act, SR_ACT_PROMOTION);
  sr_story_tick(w.ch, w.c, w.pop, w.ch->story.finale_at + 1);
  SR_CHECK(w.ch->story.finale_ready);
  sr_story_tick(w.ch, w.c, w.pop, w.ch->story.finale_at + SR_FINALE_S + 1);
  SR_CHECK(w.ch->story.ending != SR_END_NONE);
  world_free(&w);
  sr_game_destroy(g);
}

static void test_scenarios(void) {
  World w = world(32);
  int other = -1;
  w.ch->story.matrix_depth = 80;
  SR_CHECK_EQ(sr_story_pick_scenario(w.ch, w.c, 0, &other), SR_SC_GHOST);
  w.ch->story.matrix_depth = 0;
  for (int s = 0; s < w.ch->pol.seat_count; s++) w.ch->pol.seats[s].backer = 0;
  SR_CHECK_EQ(sr_story_pick_scenario(w.ch, w.c, 0, &other), SR_SC_COUP);
  for (int s = 0; s < w.ch->pol.seat_count; s++) w.ch->pol.seats[s].backer = -1, w.ch->pol.seats[s].bribed = -1;
  for (int f = 0; f < w.c->factions.corp_count; f++)
    w.c->factions.f[f].sector1 = w.c->factions.f[f].sector2 = SR_SEC_ARMS;
  SR_CHECK_EQ(sr_story_pick_scenario(w.ch, w.c, 0, &other), SR_SC_MERGER);
  w.c->factions.f[other].sector1 = SR_SEC_ENERGY;
  SR_CHECK_EQ(sr_story_pick_scenario(w.ch, w.c, 0, &other), SR_SC_PALL);
  world_free(&w);
}

/* ---- persistence and the terminal ----------------------------------------------------------- */

static void test_chess_persistence(void) {
  SrGame *g = play(34);
  g->chess.story.offer_open = true;
  sr_game_accept_retainer(g);
  sr_skip_to(g, sr_game_time(g) + 3 * SR_WEEK_S);
  int g1 = g->city->factions.gang_first;
  g->city->factions.f[g1].turf = 0x5;
  g->chess.market.shares[1] = 7;
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SR_CHECK_EQ(sr_save_to(g, &st, "chess.srsv", 0), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "chess.srsv"), SR_OK);
  SR_CHECK(sr_chess_digest(&l->chess) == sr_chess_digest(&g->chess));
  SR_CHECK_EQ(l->chess.story.patron, g->chess.story.patron);
  SR_CHECK(l->chess.story.retainer);
  SR_CHECK_EQ(l->chess.pol.elections_held, g->chess.pol.elections_held);
  SR_CHECK_EQ(l->chess.market.shares[1], 7);
  SR_CHECK_EQ(l->chess.news.count, g->chess.news.count);
  SR_CHECK_EQ(l->city->factions.f[g1].turf, 0x5);
  for (int i = 0; i < g->jobs.count; i++) SR_CHECK_EQ(l->jobs.jobs[i].booked, g->jobs.jobs[i].booked);
  /* a version 3 save: the corporate game starts fresh at the loaded time */
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, 3, 0), SR_OK);
  SrGame *m = sr_game_create();
  SR_CHECK_EQ(sr_save_read(m, w.data, w.len), SR_OK);
  SR_CHECK(m->chess.story.patron >= 0);
  SR_CHECK_EQ(m->chess.ticks, 0);
  SR_CHECK(m->chess.next_tick > sr_game_time(m));
  sr_writer_free(&w);
  sr_memstore_free(&mem);
  sr_game_destroy(m);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

static void test_terminal_and_jobs(void) {
  SrGame *g = play(35);
  g->screen = SR_SCREEN_TERMINAL;
  SrActionState a;
  memset(&a, 0, sizeof a);
  SrUi ui;
  for (int tab = 0; tab < SR_TAB_COUNT; tab++) {
    sr_ui_init(&ui);
    sr_ui_begin(&ui, 1280, 720, 2);
    sr_ui_build(&ui, g, NULL, false);
    sr_ui_free(&ui);
    a.move_x = 1.0f;
    sr_game_step(g, &a);
    a.move_x = 0;
    sr_game_step(g, &a);
  }
  SR_CHECK_EQ(g->term_tab, SR_TAB_HOME);
  g->term_tab = SR_TAB_MARKET;
  g->player.cred = 100000;
  a.move_y = -1.0f;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->term_sel, 1);
  a.move_y = 0;
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->chess.market.shares[1], 10);
  a.pressed[SR_ACT_INTERACT] = false;
  a.pressed[SR_ACT_RELOAD] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->chess.market.shares[1], 0);
  a.pressed[SR_ACT_RELOAD] = false;
  g->term_tab = SR_TAB_STORY;
  g->chess.story.offer_open = true;
  g->term_sel = 0;
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK(g->chess.story.retainer);
  g->chess.story.finale_ready = true;
  sr_game_step(g, &a); /* picks the first ending offered */
  SR_CHECK(g->chess.story.ending != SR_END_NONE);
  g->term_tab = SR_TAB_NEWS;
  sr_news_publish(&g->chess, g->city, -1, 1, -1, -1, SR_NEWS_SCANDAL, true, "Headline", sr_game_time(g));
  g->chess.story.hostile = 2;
  for (int tab = 0; tab < SR_TAB_COUNT; tab++) {
    g->term_tab = tab;
    sr_ui_init(&ui);
    sr_ui_begin(&ui, 1280, 720, 2);
    sr_ui_build(&ui, g, NULL, false);
    sr_ui_free(&ui);
  }
  /* the street HUD shows the district's billboard */
  g->screen = SR_SCREEN_PLAY;
  int d = sr_city_district_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  sr_news_publish(&g->chess, g->city, -1, 1, d, -1, SR_NEWS_STORY, false, "Local news", sr_game_time(g));
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  /* contracts appear as jobs; the player's results resolve them */
  SrGame *h = play(36);
  h->player.prog.street_cred = 50;
  int op = sr_chess_start_op(&h->chess, h->city, SR_OP_SABOTAGE, 0, 1, facility(h->city, 1), -1, sr_game_time(h));
  SrContract *k = contract_of(&h->chess, op);
  sr_skip_to(h, (sr_game_time(h) / 3600 + 1) * 3600 + 60);
  SR_CHECK(k->job_id >= 0);
  SrJob *j = sr_jobs_get(&h->jobs, k->job_id);
  SR_CHECK(j != NULL);
  SR_CHECK_EQ(j->target_building, k->building);
  SR_CHECK_EQ(sr_game_accept(h, j->id), SR_OK);
  h->player.cred = 0;
  SR_CHECK_EQ(sr_game_frame(h, j->id, 3), SR_ERR_DENIED);
  h->player.cred = 1000;
  SR_CHECK_EQ(sr_game_frame(h, j->id, 3), SR_OK);
  SR_CHECK_EQ(sr_game_frame(h, -5, 3), SR_ERR_INVALID);
  j->executed = true;
  sr_game_report(h, j->id);
  sr_skip_to(h, sr_game_time(h) + 120);
  SR_CHECK(!k->open);
  SR_CHECK(k->by_player);
  SR_CHECK_EQ(sr_chess_op(&h->chess, op)->blamed, 3);
  /* prices in the hit district rise in the shops */
  SR_CHECK(sr_game_price_pct(h, facility(h->city, 1)) >= 100);
  SR_CHECK_EQ(sr_game_price_pct(h, -1), 100);
  /* a job left on the board: the crews take the contract */
  int op2 = sr_chess_start_op(&h->chess, h->city, SR_OP_THEFT, 0, 2, facility(h->city, 2), -1, sr_game_time(h));
  SrContract *k2 = contract_of(&h->chess, op2);
  sr_skip_to(h, k2->deadline + SR_TICK_S + 3600);
  SR_CHECK(!k2->open);
  SR_CHECK(!k2->by_player);
  /* saving and loading keep the shop markups and the police */
  sr_game_destroy(h);
  sr_game_destroy(g);
}

void suite_chess(void) {
  SR_RUN(test_soak_bounds);
  SR_RUN(test_strategy);
  SR_RUN(test_contracts);
  SR_RUN(test_counterplay);
  SR_RUN(test_consequences);
  SR_RUN(test_intel_board);
  SR_RUN(test_elections);
  SR_RUN(test_policies);
  SR_RUN(test_exposure);
  SR_RUN(test_market);
  SR_RUN(test_news);
  SR_RUN(test_planted_story);
  SR_RUN(test_patron);
  SR_RUN(test_trust);
  SR_RUN(test_acts_and_defection);
  SR_RUN(test_arcs);
  SR_RUN(test_cast_succession);
  SR_RUN(test_endgame);
  SR_RUN(test_scenarios);
  SR_RUN(test_chess_persistence);
  SR_RUN(test_terminal_and_jobs);
  SR_RUN(test_campaign_bot);
}
