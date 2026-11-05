/* The game: owns the city, population, player, jobs, Heat and clock, advances them in fixed
 * steps, and exposes the interactions the UI, scenario scripts and the autoplayer use. */
#ifndef SR_GAME_H
#define SR_GAME_H

#include "game/sr_settings.h"
#include "game/sr_site.h"
#include "game/sr_storage.h"
#include "input/sr_input.h"
#include "sim/sr_combat.h"
#include "sim/sr_crowd.h"
#include "sim/sr_jobs.h"
#include "world/sr_environ.h"

#define SR_STEP_DT             (1.0 / 60.0)
#define SR_MAX_STEPS_PER_FRAME 5

typedef struct SrLoop {
  double acc;
  double alpha; /* interpolation factor for rendering */
} SrLoop;

/* Returns the number of fixed steps to run for a frame of frame_dt seconds (max 5). */
int sr_loop_advance(SrLoop *l, double frame_dt);

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
  SR_CTX_CLOSED,
  /* inside interiors and on rooftops */
  SR_CTX_EXIT,
  SR_CTX_BED,
  SR_CTX_STASH,
  SR_CTX_TERMINAL,
  SR_CTX_WARDROBE,
  SR_CTX_OBJECTIVE,
  SR_CTX_CONSOLE,
  SR_CTX_LOCKED,
  SR_CTX_TAKEDOWN,
  SR_CTX_ENTER, /* a site that can be entered without a job */
  SR_CTX_ROOF_HATCH,
  SR_CTX_CLIMB_DOWN,
  SR_CTX_COUNT
} SrContextKind;

typedef struct SrContext {
  SrContextKind kind;
  int32_t building;
  int32_t station;
  int32_t job;
  float distance;
  int32_t prop; /* interior prop or device index, -1 */
} SrContext;

typedef enum SrScreen {
  SR_SCREEN_TITLE,
  SR_SCREEN_PLAY,
  SR_SCREEN_PAUSE,
  SR_SCREEN_TERMINAL,
  SR_SCREEN_DEAD,
  SR_SCREEN_CREATE,
  SR_SCREEN_SHOP
} SrScreen;

#define SR_MENU_MAX 12

/* Shop and ripperdoc menu. Rows are items for sale; a ripperdoc adds a jailbreak row. */
typedef struct SrMenu {
  int32_t building;
  bool ripperdoc;
  int count;
  int sel;
  SrItem items[SR_MENU_MAX];
  int32_t price[SR_MENU_MAX];
  int8_t jailbreak[SR_MENU_MAX]; /* installed cyberware index for jailbreak rows, else -1 */
} SrMenu;

#define SR_GLITCH_PER_MINUTE 1.0f /* configured glitch rate while over neural capacity */
#define SR_GLITCH_TIME       1.5f

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
  /* interiors, rooftops */
  SrSite site;
  SrInterior approach; /* interior generated on approach */
  int32_t approach_building;
  int32_t roof_building; /* -1 on the ground */
  float floor_y;
  SrVec3 site_return; /* where to stand after leaving the current site */
  /* gadgets and cyberware in play */
  float buff_camo, buff_reflex, buff_mask, buff_focus, buff_pain;
  bool thermal;
  float ability_cd;
  float glitch_time;      /* seconds of the current glitch left */
  int glitches;           /* glitches so far */
  uint32_t cyber_offline; /* bitmask of installed implants down for the day (malfunction) */
  int64_t cyber_day;
  float speed; /* measured player speed, m/s */
  SrMenu menu;
  int menu_axis;
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
/* Position for saving: the front door of the building the player is in or on; inside the
 * apartment the indoor position is kept and the apartment reopens on load. */
SrVec3 sr_game_outside_pos(const SrGame *g);

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

/* Interiors, rooftops, gadgets, cyberware, shops (sr_game_site.c). */
bool sr_game_enterable(const SrGame *g, int building);
/* Enters a building's interior through an entry type (front, service, roof...). */
SrResult sr_game_enter(SrGame *g, int building, SrEntryType via);
void sr_game_leave(SrGame *g);
bool sr_game_inside(const SrGame *g);
SrResult sr_game_climb_down(SrGame *g);
/* Grapple line: pulls the player onto the roof of the building aimed at, if in range. */
SrResult sr_game_grapple(SrGame *g);
/* Uses a gadget from the inventory (consumed unless it is a reusable tool). */
bool sr_game_use_gadget(SrGame *g, int gadget);
bool sr_game_has_cyber(const SrGame *g, int cyber_type);
int sr_game_cyber_load(const SrGame *g);
int sr_game_neural_capacity(const SrGame *g);
/* Installs cyberware from the inventory (ripperdoc service); false if the slot is taken. */
bool sr_game_install_cyber(SrGame *g, int inv_index);
/* Active cyberware: reflex booster, chameleon skin; toggles thermal optics. */
bool sr_game_ability(SrGame *g);
void sr_game_shop_open(SrGame *g, int building);
SrResult sr_game_shop_buy(SrGame *g, int row);
void sr_game_shop_close(SrGame *g);
/* Stash: stores everything but the equipped weapon, ammo and quick-slot gadgets; takes it all
 * back when nothing is left to store. */
SrResult sr_game_stash(SrGame *g);
void sr_game_change_outfit(SrGame *g);
/* Per-step upkeep of the systems above (called from sr_game_step). */
void sr_game_site_step(SrGame *g, float dt);
SrContext sr_game_site_context(SrGame *g);
SrResult sr_game_site_interact(SrGame *g, const SrContext *c);
/* Movement collision for the current place (street, roof or interior). */
bool sr_game_blocked_here(const SrGame *g, float x, float z);

/* Character creation: background choice and a randomisable appearance. */
void sr_game_begin_create(SrGame *g, uint64_t seed);
void sr_game_create_select(SrGame *g, int background);
void sr_game_create_randomize(SrGame *g);
SrAppearance sr_game_create_preview(const SrGame *g);
SrResult sr_game_create_confirm(SrGame *g);

#endif
