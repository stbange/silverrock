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
    } else {
      fprintf(stderr, "usage: %s [--script file.scn]... [--sweep N] [--balance SEEDS [--bands FILE]]\n", argv[0]);
      return 2;
    }
  }
  return 0;
}
