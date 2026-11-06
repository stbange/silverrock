/* The corporate game (M5): faction simulation, consequences, politics, market, news and story. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"
#include "sim/sr_progress.h"
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
  printf("  soak: %d ops, %d news, %d elections, act %d, ending %d, arcs %d/%d\n", ops, w.ch->news.count,
         w.ch->pol.elections_held, w.ch->story.act, w.ch->story.ending, w.ch->story.arcs_started,
         w.ch->story.arcs_finished);
  for (int i = 0; i < 12 && i < w.ch->history_count; i++)
    printf("    %s\n", w.ch->history[(w.ch->history_count - 1 - i) % SR_MAX_HISTORY]);
  SR_CHECK(ops > 50);
  world_free(&w);
}

void suite_chess(void) { SR_RUN(test_soak_bounds); }
