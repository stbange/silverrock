/* Test hooks and scenario scripts. The command/query interface lets headless scenario scripts,
 * the autoplayer and browser tests drive the game; it is compiled only with SR_TEST_HOOKS.
 *
 * Script commands (one per line, '#' comments):
 *   seed <code|number|random(N)>   newgame [background=<name>] [corps=<n>]
 *   advance <n>[d|h|m|s]           steps <n>          press|release <action>
 *   hold <action> <steps>          move <x> <y> <steps>          look <dx> <steps>
 *   set <path> = <value>           expect <path> <op> <value>     (op: == != < <= > >=)
 *   teleport <poi>|home|building <i>  interact   sleep <hours> [home|capsule|street]
 *   eat   fire   focus <0|1>   autoplay strategy=<s> weeks=<n>   job <cmd> ...
 *   save <name>  load <name>  echo <text> */
#ifndef SR_SCRIPT_H
#define SR_SCRIPT_H

#include "game/sr_game.h"

typedef enum SrStrategy { SR_STRAT_GIG, SR_STRAT_GHOST, SR_STRAT_FACE, SR_STRAT_HAMMER, SR_STRAT_NETRUNNER, SR_STRAT_LIVE } SrStrategy;

typedef struct SrAutoplayStats {
  int days;
  int jobs_done, jobs_failed;
  int missed_rent;
  float starving_hours;
  int64_t earned, spent;
  int meals, sleeps, flatlines;
  bool soft_lock;
} SrAutoplayStats;

bool sr_game_query(SrGame *g, const char *path, char *out, size_t cap);
SrResult sr_game_set(SrGame *g, const char *path, const char *value);
/* Executes one command line; output (query results, errors) written to out. */
SrResult sr_game_command(SrGame *g, const char *line, char *out, size_t cap);

SrStrategy sr_strategy_parse(const char *s);
/* Plays whole days through the game's interaction layer. */
SrAutoplayStats sr_autoplay(SrGame *g, SrStrategy s, int days, int max_tier);

/* Runs a scenario script; returns 0 on success. On failure err holds "name:line: message". */
int sr_script_run(const char *text, const char *name, char *err, size_t cap);
/* Sweeps world-generation invariants over n seeds; returns failures. */
int sr_sweep_seeds(uint64_t first, int n, char *err, size_t cap);

#endif
