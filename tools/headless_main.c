/* Headless runner: scenario scripts, seed sweeps and autoplayer balance runs without a window,
 * GPU or audio device. Exit status 0 means every expectation held. */
#include "core/sr_log.h"
#include "game/sr_script.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1);
  if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    buf = NULL;
  }
  if (buf) buf[n] = '\0';
  fclose(f);
  return buf;
}

static void sink(void *user, SrLogLevel level, const char *sub, const char *msg) {
  (void)user;
  if (level <= SR_LOG_WARN) fprintf(stderr, "[%s] %s: %s\n", sr_log_level_name(level), sub, msg);
}

static const char *const k_strategies[] = {"gig", "ghost", "face", "hammer", "netrunner"};
#define N_STRAT 5

typedef struct Report {
  double earned[N_STRAT], jobs[N_STRAT], failed[N_STRAT], missed_share[N_STRAT];
  double dominance;
} Report;

static double metric(const Report *r, int s, const char *name) {
  if (strcmp(name, "earned") == 0) return r->earned[s];
  if (strcmp(name, "jobs") == 0) return r->jobs[s];
  if (strcmp(name, "failed") == 0) return r->failed[s];
  if (strcmp(name, "missed_share") == 0) return r->missed_share[s];
  return r->dominance;
}

/* Checks every band line; prints each violation. Returns the number of violations, -1 on error. */
static int check_bands(const Report *r, const char *path) {
  char *text = read_file(path);
  if (!text) {
    fprintf(stderr, "cannot read bands %s\n", path);
    return -1;
  }
  int bad = 0, lines = 0;
  for (char *line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
    char strat[32], name[32];
    double lo, hi;
    if (line[0] == '#' || sscanf(line, "%31s %31s %lf %lf", strat, name, &lo, &hi) != 4) continue;
    lines++;
    for (int s = 0; s < N_STRAT; s++) {
      if (strcmp(strat, "*") != 0 && strcmp(strat, k_strategies[s]) != 0) continue;
      double v = metric(r, s, name);
      if (v < lo || v > hi) {
        printf("BAND FAIL %s %s = %.2f outside [%.2f, %.2f]\n", k_strategies[s], name, v, lo, hi);
        bad++;
      }
      if (strcmp(name, "dominance") == 0) break; /* one value for all strategies */
    }
  }
  free(text);
  printf("bands: %d lines checked, %d violations\n", lines, bad);
  return bad;
}

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

/* Autoplayer balance report over `seeds` seeds and all strategies, optionally gated by bands. */
static int balance(int seeds, const char *bands) {
  Report r;
  memset(&r, 0, sizeof r);
  for (int s = 0; s < N_STRAT; s++) {
    int64_t earned = 0;
    int missed_seeds = 0, jobs = 0, failed = 0, missed = 0;
    for (int k = 0; k < seeds; k++) {
      SrGame *g = sr_game_create();
      sr_game_new(g, 1000u + (uint64_t)k, NULL, (SrBackground)(k % SR_BG_COUNT), "Bot");
      SrAutoplayStats st = sr_autoplay(g, sr_strategy_parse(k_strategies[s]), 28, s == 0 ? 0 : 3);
      earned += st.earned;
      missed += st.missed_rent;
      missed_seeds += st.missed_rent > 0;
      jobs += st.jobs_done;
      failed += st.jobs_failed;
      sr_game_destroy(g);
    }
    r.earned[s] = (double)earned / seeds;
    r.jobs[s] = (double)jobs / seeds;
    r.failed[s] = (double)failed / seeds;
    r.missed_share[s] = (double)missed_seeds / seeds;
    printf("%-10s avg earned %7.0f  jobs %5.1f  failed %4.1f  missed rent %d (%.0f%% of seeds)\n", k_strategies[s],
           r.earned[s], r.jobs[s], r.failed[s], missed, r.missed_share[s] * 100);
  }
  double approach[4] = {r.earned[1], r.earned[2], r.earned[3], r.earned[4]};
  qsort(approach, 4, sizeof approach[0], cmp_double);
  double median = (approach[1] + approach[2]) / 2;
  r.dominance = median > 0 ? approach[3] / median : 0;
  printf("dominance (best approach / median) %.2f\n", r.dominance);
  if (!bands) return 0;
  int bad = check_bands(&r, bands);
  return bad != 0;
}

/* Story bots play the campaign on n seeds; every run must reach an ending. */
static int campaign(int n) {
  int failures = 0;
  for (int k = 0; k < n; k++) {
    uint64_t seed = 1000u + (uint64_t)k * 7919u;
    SrGame *g = sr_game_create();
    sr_game_new(g, seed, NULL, (SrBackground)(k % SR_BG_COUNT), "Bot");
    g->crowd_enabled = false;
    sr_game_leave(g);
    SrAutoplayStats st;
    int days = sr_campaign(g, 7 * 52, &st);
    printf("seed %llu: %s after %d days, %d jobs, act %d, arcs %d/%d, %s\n", (unsigned long long)seed,
           days > 0 ? sr_ending_name((SrEnding)g->chess.story.ending) : "NO ENDING", st.days, st.jobs_done,
           g->chess.story.act, g->chess.story.arcs_finished, g->chess.story.arcs_started,
           st.soft_lock ? "SOFT-LOCK" : "ok");
    failures += days <= 0 || st.soft_lock;
    sr_game_destroy(g);
  }
  printf("campaign: %d seeds, %d without an ending\n", n, failures);
  return failures;
}

/* The corporate game without the player for a year: the full history of notable outcomes. */
static int soak(int n) {
  for (int k = 0; k < n; k++) {
    uint64_t seed = 500u + (uint64_t)k * 104729u;
    SrCity *c = (SrCity *)calloc(1, sizeof *c);
    SrPopulation *pop = (SrPopulation *)calloc(1, sizeof *pop);
    SrChess *ch = (SrChess *)calloc(1, sizeof *ch);
    SrEventLog log;
    sr_city_generate(c, seed, NULL);
    sr_population_generate(pop, c);
    sr_events_init(&log);
    sr_chess_init(ch, c, seed, -1, 0);
    printf("== soak seed %llu: %s, %d megacorps, %d gangs\n", (unsigned long long)seed, c->name, c->factions.corp_count,
           c->factions.gang_count);
    int printed = 0;
    for (int w = 1; w <= 52; w++) {
      sr_chess_advance(ch, c, pop, &log, (int64_t)w * SR_WEEK_S);
      if (ch->history_count - printed > SR_MAX_HISTORY) printed = ch->history_count - SR_MAX_HISTORY;
      for (; printed < ch->history_count; printed++) printf("  %s\n", ch->history[printed % SR_MAX_HISTORY]);
    }
    int leader = 0;
    float dom = sr_chess_dominance(ch, c, &leader);
    printf("  -> leader %s (%.0f%%), %d elections, %d news\n", c->factions.f[leader].name, (double)(dom * 100.0f),
           ch->pol.elections_held, ch->news.count);
    free(c);
    free(pop);
    free(ch);
  }
  return 0;
}

/* ---- long playtests ----------------------------------------------------------------------------
 * Plays a character day by day for `weeks` with a strategy and writes a daily timeline (CSV) and an
 * event log to dir. Story choices on the terminal (retainer, defection, endings) are taken as a
 * player would; strategy "story" also takes the patron's contract jobs (not reachable in the UI). */
typedef struct Playtest {
  int first_tier_day[6];
  int retainer_day, act2_day, act3_day, ending_day, evicted_day, first_flatline_day;
  int idle_days, jobs, failed, contract_jobs, news_player, msgs;
  int64_t cash_min, cash_max, cash_end;
  int ending, act;
} Playtest;

static void pt_event(FILE *ev, int day, const char *what) {
  if (ev) fprintf(ev, "d%03d %s\n", day, what);
}

static void pt_story_choices(SrGame *g, FILE *ev, int day) {
  SrStory *st = &g->chess.story;
  char buf[160];
  if (st->offer_open && !st->retainer && st->patron >= 0) {
    sr_game_accept_retainer(g);
    snprintf(buf, sizeof buf, "accepted the retainer from %s", g->city->factions.f[st->patron].name);
    pt_event(ev, day, buf);
  }
  if (st->rival_offer >= 0 && (st->hostile >= 0 || st->trust < 25)) {
    snprintf(buf, sizeof buf, "defected to %s (trust %d)", g->city->factions.f[st->rival_offer].name, st->trust);
    sr_game_accept_offer(g);
    pt_event(ev, day, buf);
  }
  if (st->finale_ready && st->ending == SR_END_NONE) {
    unsigned m = sr_story_endings(st);
    int n = 0;
    for (int e = SR_END_LOYAL; e <= SR_END_SUNRISE; e++) n += (m >> e) & 1u;
    int pick = (int)(g->seed % (uint64_t)(n > 0 ? n : 1));
    for (int e = SR_END_LOYAL; e <= SR_END_SUNRISE; e++)
      if (((m >> e) & 1u) && pick-- == 0) {
        sr_game_choose_ending(g, (SrEnding)e);
        snprintf(buf, sizeof buf, "chose the ending %s (offered mask %x)", sr_ending_name((SrEnding)e), m);
        pt_event(ev, day, buf);
        break;
      }
  }
}

static Playtest playtest(uint64_t seed, const char *strategy, int bg, int weeks, const char *dir, const char *name) {
  Playtest r;
  memset(&r, 0, sizeof r);
  for (int i = 0; i < 6; i++) r.first_tier_day[i] = -1;
  r.retainer_day = r.act2_day = r.act3_day = r.ending_day = r.evicted_day = r.first_flatline_day = -1;
  char path[512];
  snprintf(path, sizeof path, "%s/%s.csv", dir, name);
  FILE *csv = fopen(path, "w");
  snprintf(path, sizeof path, "%s/%s.log", dir, name);
  FILE *ev = fopen(path, "w");
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, (SrBackground)bg, "Playtester");
  g->crowd_enabled = false;
  sr_game_leave(g);
  SrStrategy s = sr_strategy_parse(strategy);
  const SrCity *c = g->city;
  if (ev)
    fprintf(ev, "seed %llu, %s, background %s, strategy %s, patron %s, %d megacorps, %d gangs, %d districts\n",
            (unsigned long long)seed, c->name, sr_background_def(bg)->name, strategy,
            c->factions.f[g->chess.story.patron].name, c->factions.corp_count, c->factions.gang_count,
            c->district_count);
  if (csv)
    fprintf(csv, "day,cash,street_cred,tier,act,trust,retainer,hostile,jobs,failed,contract_jobs,heat_max,health,"
                 "satiety,missed_rent,evicted,messages,news,ops,arcs_started,arcs_finished,ending,flatlines,"
                 "portfolio\n");
  r.cash_min = r.cash_max = g->player.cred;
  int last_tier = -1, last_act = 0, last_jobs = 0, last_flat = 0, msgs0 = g->player.msg_count;
  bool was_evicted = false;
  int hostile_seen = -1;
  for (int day = 1; day <= weeks * 7; day++) {
    int tier = sr_max_tier_for_cred(g->player.prog.street_cred);
    if (sr_story_t4_allowed(&g->chess.story, sr_cred_tier(g->player.prog.street_cred))) tier = 4;
    SrAutoplayStats st = sr_autoplay(g, s, 1, s == SR_STRAT_GIG ? 0 : SR_MIN(tier, 3));
    r.jobs += st.jobs_done;
    r.failed += st.jobs_failed;
    if (st.jobs_done == 0) r.idle_days++;
    pt_story_choices(g, ev, day);
    if (g->player.cred < 0) g->player.cred = 0;
    /* events */
    SrStory *sy = &g->chess.story;
    int ct = (int)sr_cred_tier(g->player.prog.street_cred);
    if (ct != last_tier) {
      if (r.first_tier_day[ct] < 0) r.first_tier_day[ct] = day;
      char b[64];
      snprintf(b, sizeof b, "Street Cred tier %s (%d)", sr_cred_tier_name((SrCredTier)ct), g->player.prog.street_cred);
      pt_event(ev, day, b);
      last_tier = ct;
    }
    if (sy->retainer && r.retainer_day < 0) r.retainer_day = day;
    if (sy->act != last_act) {
      char b[64];
      snprintf(b, sizeof b, "act %d begins (trust %d)", sy->act, sy->trust);
      pt_event(ev, day, b);
      if (sy->act == SR_ACT_KNIGHT) r.act2_day = day;
      if (sy->act == SR_ACT_PROMOTION) {
        r.act3_day = day;
        snprintf(b, sizeof b, "scenario %s around %s", sr_scenario_name((SrScenario)sy->scenario),
                 sy->scenario_corp >= 0 ? c->factions.f[sy->scenario_corp].name : "-");
        pt_event(ev, day, b);
      }
      if (sy->act == SR_ACT_EPILOGUE) r.ending_day = day;
      last_act = sy->act;
    }
    if (sy->hostile != hostile_seen && sy->hostile >= 0) {
      char b[96];
      snprintf(b, sizeof b, "%s is hostile and hunting the player", c->factions.f[sy->hostile].name);
      pt_event(ev, day, b);
      hostile_seen = sy->hostile;
    }
    if (g->player.evicted && !was_evicted) {
      pt_event(ev, day, "evicted");
      if (r.evicted_day < 0) r.evicted_day = day;
    }
    was_evicted = g->player.evicted;
    if (st.flatlines > last_flat || st.flatlines > 0) {
      pt_event(ev, day, "flatlined");
      if (r.first_flatline_day < 0) r.first_flatline_day = day;
    }
    if (r.jobs - last_jobs >= 1 && day % 7 == 0) {
      char b[96];
      snprintf(b, sizeof b, "week %d: %d jobs so far, cash %lld", day / 7, r.jobs, (long long)g->player.cred);
      pt_event(ev, day, b);
    }
    if (g->player.cred < r.cash_min) r.cash_min = g->player.cred;
    if (g->player.cred > r.cash_max) r.cash_max = g->player.cred;
    int contract = 0;
    for (int i = 0; i < g->chess.contract_count; i++) contract += g->chess.contracts[i].by_player;
    if (contract > r.contract_jobs) r.contract_jobs = contract;
    int heat = 0;
    for (int f = 0; f < SR_MAX_FACTIONS; f++) heat = heat > g->heat.level[f] ? heat : g->heat.level[f];
    if (csv)
      fprintf(csv, "%d,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.0f,%.0f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lld\n", day,
              (long long)g->player.cred, g->player.prog.street_cred, ct, sy->act, sy->trust, sy->retainer, sy->hostile,
              r.jobs, r.failed, r.contract_jobs, heat, (double)g->player.health, (double)g->player.satiety,
              g->player.missed, g->player.evicted, g->player.msg_count, g->chess.news.count, g->chess.op_count,
              sy->arcs_started, sy->arcs_finished, sy->ending, st.flatlines,
              (long long)sr_market_portfolio(&g->chess.market, c));
    last_jobs = r.jobs;
    last_flat = st.flatlines;
  }
  for (int i = 0; i < sr_news_size(&g->chess.news); i++)
    r.news_player += strstr(sr_news_latest(&g->chess.news, i)->headline, "shadowrunner") != NULL;
  r.msgs = g->player.msg_count - msgs0;
  r.cash_end = g->player.cred;
  r.ending = g->chess.story.ending;
  r.act = g->chess.story.act;
  if (ev) {
    fprintf(ev, "-- story history (last %d notable outcomes)\n", SR_MAX_HISTORY);
    int n = g->chess.history_count < SR_MAX_HISTORY ? g->chess.history_count : SR_MAX_HISTORY;
    for (int i = n - 1; i >= 0; i--)
      fprintf(ev, "  %s\n", g->chess.history[(g->chess.history_count - 1 - i) % SR_MAX_HISTORY]);
    fprintf(ev, "-- last messages\n");
    for (int i = 0; i < g->player.msg_count; i++)
      fprintf(ev, "  %s: %s\n", g->player.msgs[i].from, g->player.msgs[i].text);
    fprintf(ev, "-- latest news\n");
    for (int i = 0; i < 20 && i < sr_news_size(&g->chess.news); i++)
      fprintf(ev, "  %s\n", sr_news_latest(&g->chess.news, i)->headline);
  }
  if (csv) fclose(csv);
  if (ev) fclose(ev);
  sr_game_destroy(g);
  return r;
}

static int playtests(int n, const char *dir) {
  static const char *const strategies[] = {"story", "ghost", "face", "hammer", "netrunner", "story", "gig", "story"};
  printf("run,seed,strategy,background,tier_pawn,tier_runner,tier_operator,tier_knight,retainer,act2,act3,ending_day,"
         "ending,jobs,failed,idle_days,contract_jobs,cash_min,cash_max,cash_end,evicted_day,flatline_day,"
         "news_about_player,messages\n");
  for (int k = 0; k < n; k++) {
    uint64_t seed = 3000u + (uint64_t)k * 7717u;
    const char *strat = strategies[k % 8];
    int bg = k % SR_BG_COUNT;
    char name[64];
    snprintf(name, sizeof name, "run%02d_%s", k + 1, strat);
    Playtest r = playtest(seed, strat, bg, 26, dir, name);
    printf("%d,%llu,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%s,%d,%d,%d,%d,%lld,%lld,%lld,%d,%d,%d,%d\n", k + 1,
           (unsigned long long)seed, strat, sr_background_def(bg)->name, r.first_tier_day[1], r.first_tier_day[2],
           r.first_tier_day[3], r.first_tier_day[4], r.retainer_day, r.act2_day, r.act3_day, r.ending_day,
           sr_ending_name((SrEnding)r.ending), r.jobs, r.failed, r.idle_days, r.contract_jobs, (long long)r.cash_min,
           (long long)r.cash_max, (long long)r.cash_end, r.evicted_day, r.first_flatline_day, r.news_player, r.msgs);
    fflush(stdout);
  }
  return 0;
}

int main(int argc, char **argv) {
  sr_log_set_sink(sink, NULL);
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
      const char *path = argv[++i];
      char *text = read_file(path);
      if (!text) {
        fprintf(stderr, "cannot read %s\n", path);
        return 2;
      }
      char err[512] = "";
      int rc = sr_script_run(text, path, err, sizeof err);
      free(text);
      if (rc) {
        fprintf(stderr, "FAIL %s\n", err);
        return 1;
      }
      printf("PASS %s\n", path);
    } else if (strcmp(argv[i], "--sweep") == 0 && i + 1 < argc) {
      int n = atoi(argv[++i]);
      char err[256] = "";
      int failures = sr_sweep_seeds(1, n, err, sizeof err);
      printf("sweep: %d seeds, %d failures %s\n", n, failures, err);
      if (failures) return 1;
    } else if (strcmp(argv[i], "--balance") == 0 && i + 1 < argc) {
      int seeds = atoi(argv[++i]);
      const char *bands = NULL;
      if (i + 2 < argc && strcmp(argv[i + 1], "--bands") == 0) {
        bands = argv[i + 2];
        i += 2;
      }
      if (balance(seeds, bands) != 0) return 1;
    } else if (strcmp(argv[i], "--campaign") == 0 && i + 1 < argc) {
      if (campaign(atoi(argv[++i])) != 0) return 1;
    } else if (strcmp(argv[i], "--playtest") == 0 && i + 2 < argc) {
      int n = atoi(argv[++i]);
      playtests(n, argv[++i]);
    } else if (strcmp(argv[i], "--soak") == 0 && i + 1 < argc) {
      soak(atoi(argv[++i]));
    } else {
      fprintf(stderr,
              "usage: %s [--script file.scn]... [--sweep N] [--balance SEEDS [--bands FILE]] [--campaign SEEDS] "
              "[--soak SEEDS] [--playtest RUNS DIR]\n",
              argv[0]);
      return 2;
    }
  }
  return 0;
}
