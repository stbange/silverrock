/* The game: owns the city, population, player, jobs, Heat and clock, advances them in fixed
 * steps, and exposes the interactions the UI, scenario scripts and the autoplayer use. */
#ifndef SR_GAME_H
#define SR_GAME_H

#include "game/sr_settings.h"
#include "game/sr_storage.h"
#include "input/sr_input.h"
#include "sim/sr_combat.h"
#include "sim/sr_crowd.h"
#include "sim/sr_jobs.h"
#include "world/sr_environ.h"

#define SR_STEP_DT             (1.0 / 60.0)
#define SR_MAX_STEPS_PER_FRAME 5
#define SR_MAX_DELTAS          256

typedef struct SrLoop {
  double acc;
  double alpha; /* interpolation factor for rendering */
} SrLoop;

/* Returns the number of fixed steps to run for a frame of frame_dt seconds (max 5). */
int sr_loop_advance(SrLoop *l, double frame_dt);

typedef struct SrDeltas {
  int count;
  uint32_t id[SR_MAX_DELTAS];
  uint8_t value[SR_MAX_DELTAS];
} SrDeltas;

void sr_deltas_set(SrDeltas *d, uint32_t id, uint8_t value);
int sr_deltas_get(const SrDeltas *d, uint32_t id); /* -1 if unset */

typedef enum SrContextKind {
  SR_CTX_NONE,
  SR_CTX_FOOD,
  SR_CTX_SHOP,
  SR_CTX_FIXER,
  SR_CTX_HOME,
  SR_CTX_METRO,
  SR_CTX_CAPSULE,
  SR_CTX_JOB_TARGET,
  SR_CTX_JOB_REPORT,
  SR_CTX_CLINIC,
  SR_CTX_LANDLORD,
  SR_CTX_CLOSED
} SrContextKind;

typedef struct SrContext {
  SrContextKind kind;
  int32_t building;
  int32_t station;
  int32_t job;
  float distance;
} SrContext;

typedef enum SrScreen {
  SR_SCREEN_TITLE,
  SR_SCREEN_PLAY,
  SR_SCREEN_PAUSE,
  SR_SCREEN_TERMINAL,
  SR_SCREEN_DEAD,
  SR_SCREEN_CREATE
} SrScreen;

typedef struct SrGame {
  SrSettings settings;
  SrCity *city;
  SrPopulation *pop;
  SrCrowd crowd;
  SrClock clock;
  SrPlayer player;
  SrJobBoard jobs;
  SrHeat heat;
  SrEventLog events;
  SrDeltas deltas;
  uint64_t seed;
  SrCityOptions opt;
  SrDifficulty difficulty;
  bool hardcore;
  bool pall_removed;
  bool started;
  SrScreen screen;
  bool resume_needs_confirm;
  int64_t last_minute;
  uint64_t steps;
  int combat_count;  /* enemies currently in combat with the player */
  int hostiles_near; /* alerted hostiles within 50 m */
  bool crowd_enabled;
  SrCamera cam;
  float vel_y;
  bool on_ground;
  SrContext ctx;
  char toast[128];
  float toast_time;
  int autosave_requests; /* incremented when an autosave should be written */
  int32_t active_job;
  int max_agents;
  bool audio_muted;
  /* character creation */
  uint64_t create_seed;
  uint64_t create_look;
  int create_bg;
} SrGame;

SrGame *sr_game_create(void);
void sr_game_destroy(SrGame *g);
SrResult sr_game_new(SrGame *g, uint64_t seed, const SrCityOptions *opt, SrBackground bg, const char *name);
/* Regenerates city and population for a seed (used by new game and load). */
SrResult sr_game_build_world(SrGame *g, uint64_t seed, const SrCityOptions *opt);
void sr_game_step(SrGame *g, const SrActionState *a);
/* Skips in-game time minute by minute (sleep, travel, waiting). */
void sr_game_skip(SrGame *g, int64_t seconds, bool sleeping);
int64_t sr_game_time(const SrGame *g);
uint64_t sr_game_checksum(const SrGame *g);
void sr_game_toast(SrGame *g, const char *msg);
void sr_game_focus(SrGame *g, bool focused);
void sr_game_confirm_resume(SrGame *g);

/* Movement helpers */
bool sr_game_blocked(const SrGame *g, float x, float z);
void sr_game_teleport_building(SrGame *g, int building);

/* Interactions */
SrContext sr_game_context(SrGame *g);
SrResult sr_game_interact(SrGame *g);
SrResult sr_game_eat_at(SrGame *g, int building);
SrResult sr_game_sleep(SrGame *g, int hours, SrSleepPlace place);
SrResult sr_game_metro(SrGame *g, int to_station);
SrResult sr_game_cab(SrGame *g, int to_building);
SrResult sr_game_walk_to(SrGame *g, int building);
int sr_game_fixer(SrGame *g, int fixer_building, int out_ids[4]);
SrResult sr_game_accept(SrGame *g, int32_t job);
SrMissionResult sr_game_execute(SrGame *g, int32_t job, SrApproach a);
int32_t sr_game_report(SrGame *g, int32_t job);
void sr_game_fire(SrGame *g);
/* Uses the gadget bound to a quick slot (0..3); false if empty or none carried. */
bool sr_game_use_quick(SrGame *g, int slot);
void sr_game_flatline(SrGame *g);

/* Character creation: background choice and a randomisable appearance. */
void sr_game_begin_create(SrGame *g, uint64_t seed);
void sr_game_create_select(SrGame *g, int background);
void sr_game_create_randomize(SrGame *g);
SrAppearance sr_game_create_preview(const SrGame *g);
SrResult sr_game_create_confirm(SrGame *g);

#endif
