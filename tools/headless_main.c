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
      static const char *const strategies[] = {"gig", "ghost", "face", "hammer", "netrunner"};
      for (int s = 0; s < 5; s++) {
        int64_t earned = 0;
        int missed = 0, jobs = 0, failed = 0;
        for (int k = 0; k < seeds; k++) {
          SrGame *g = sr_game_create();
          sr_game_new(g, 1000u + (uint64_t)k, NULL, (SrBackground)(k % SR_BG_COUNT), "Bot");
          SrAutoplayStats st = sr_autoplay(g, sr_strategy_parse(strategies[s]), 28, s == 0 ? 0 : 3);
          earned += st.earned;
          missed += st.missed_rent;
          jobs += st.jobs_done;
          failed += st.jobs_failed;
          sr_game_destroy(g);
        }
        printf("%-10s avg earned %7lld  jobs %5.1f  failed %4.1f  missed rent %d\n", strategies[s],
               (long long)(earned / seeds), (double)jobs / seeds, (double)failed / seeds, missed);
      }
    } else {
      fprintf(stderr, "usage: %s [--script file.scn]... [--sweep N] [--balance SEEDS]\n", argv[0]);
      return 2;
    }
  }
  return 0;
}
