/* The game: owns the city, population, player, jobs, Heat and clock, advances them in fixed
 * steps, and exposes the interactions the UI, scenario scripts and the autoplayer use. */
#ifndef SR_GAME_H
#define SR_GAME_H

#include "game/sr_settings.h"
#include "game/sr_net.h"
#include "game/sr_site.h"
#include "game/sr_storage.h"
#include "input/sr_input.h"
#include "sim/sr_air.h"
#include "sim/sr_chess.h"
#include "sim/sr_combat.h"
#include "sim/sr_crowd.h"
#include "sim/sr_jobs.h"
#include "world/sr_dressing.h"
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
  /* the Matrix */
  SR_CTX_NODE,  /* a host node within reach (prop = node index) */
  SR_CTX_HOST,  /* a host construct on the grid (building) */
  SR_CTX_QUICK, /* Netsight quick action on an owned device (prop = device index) */
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
  bool housing;   /* rows are apartments for rent (item type = housing tier) */
  bool node_menu; /* rows are Matrix node actions */
  bool job_menu;  /* rows are a fixer's job offers */
  bool encounter; /* rows are the choices of a hunt encounter */
  int32_t job[SR_MENU_MAX];
  int16_t node;
  uint8_t action[SR_MENU_MAX];
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
  SrAirTraffic air;
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
  int8_t force_weather; /* -1: the seed's weather; else a fixed SrWeather (reference views) */
  /* solid street props of the block last walked in (collision cache, rebuilt on demand) */
  int dress_block;
  int dress_n;
  SrStreetProp dress[48];
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
  /* the Matrix */
  SrNet net;
  SrDive dive;
  /* the corporate game */
  SrChess chess;
  int8_t patron_choice; /* new game: chosen patron corp, -1 random */
  int64_t curfew_stop_at;
  int term_tab, term_sel, term_axis_x, term_axis_y;
  int32_t confirm_job; /* job whose patron warning was shown; accepting it again goes ahead */
  /* a hunt encounter waiting for the player's choice */
  struct {
    bool active;
    bool raid;      /* at home at night (else an ambush in the street) */
    int8_t faction; /* the hunters */
    int32_t payoff;
  } enc;
  int64_t last_raid;
  int flatlines; /* times the player flatlined */
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
/* The weather now: the seed's weather, or the forced one. */
SrWeatherState sr_game_weather(const SrGame *g);
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
#define SR_FIXER_OFFERS 6
/* A fixer's offers: steps of operations around the fixer's district (started on demand when there
 * is too little work). Returns the number of job ids written. */
int sr_game_fixer(SrGame *g, int fixer_building, int out_ids[SR_FIXER_OFFERS]);
SrResult sr_game_accept(SrGame *g, int32_t job);
SrMissionResult sr_game_execute(SrGame *g, int32_t job, SrApproach a);
/* Walks away from an accepted job: it fails and the client remembers. */
SrResult sr_game_abandon(SrGame *g, int32_t job);
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
/* Housing offers on the home terminal; moving keeps the stash and starts the new rent next week. */
void sr_game_housing_open(SrGame *g);
SrResult sr_game_move_home(SrGame *g, int tier);
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

/* The Matrix (sr_game_matrix.c). */
int sr_game_deck_grade(const SrGame *g);
SrPersona sr_game_persona(const SrGame *g);
bool sr_game_diving(const SrGame *g);
/* Jacks in from where the player is: home rig, booth, a port or spike in a site, or wireless. */
SrResult sr_game_jack_in(SrGame *g);
SrResult sr_game_jack_in_via(SrGame *g, SrAccess access, int building);
void sr_game_jack_out(SrGame *g, bool emergency);
/* From the grid into a host construct, in front of its gateway. */
SrResult sr_game_dive_enter_host(SrGame *g, int building);
int sr_game_node_near(const SrGame *g);
int sr_game_host_near(const SrGame *g);
bool sr_game_node_locked(const SrGame *g, int node);
int sr_game_node_actions(const SrGame *g, int node, SrNodeAction *out);
SrResult sr_game_node_act(SrGame *g, int node, SrNodeAction a);
SrResult sr_game_breach_start(SrGame *g, int node);
bool sr_game_program(SrGame *g, int slot);
void sr_game_dive_step(SrGame *g, const SrActionState *a, float dt);
void sr_game_net_tick(SrGame *g);
void sr_game_net_reset(SrGame *g);
bool sr_game_owns_device(const SrGame *g, uint32_t device);
bool sr_game_has_backdoor(const SrGame *g, int building);
bool sr_game_has_creds(const SrGame *g, int building);
int sr_game_host_alert(const SrGame *g, int building);
SrVec3 sr_game_device_pos(const SrGame *g, int device);
int sr_game_device_ahead(const SrGame *g);
SrNodeAction sr_game_quick_action(const SrGame *g, int device);
SrResult sr_game_quick(SrGame *g, int device);
void sr_game_netsight(SrGame *g);
void sr_game_node_menu(SrGame *g, int node);
/* Lights of a district are out (blackout). */
bool sr_game_blackout_at(const SrGame *g, int district);

/* The corporate game (sr_game_chess.c). */
typedef enum SrTermTab {
  SR_TAB_HOME,
  SR_TAB_JOBS,
  SR_TAB_NEWS,
  SR_TAB_MARKET,
  SR_TAB_BOARD,
  SR_TAB_STORY,
  SR_TAB_COUNT
} SrTermTab;
typedef enum SrEncChoice { SR_ENC_FIGHT, SR_ENC_FLEE, SR_ENC_PAY } SrEncChoice;
/* Starts a hunt encounter (ambush or raid) by a faction. */
void sr_game_encounter_start(SrGame *g, int faction, bool raid);
/* Shows the waiting encounter's choices when the player is in play. */
void sr_game_encounter_menu(SrGame *g);
/* Resolves the waiting encounter with a choice; SR_ERR_DENIED when the payoff cannot be paid. */
SrResult sr_game_encounter(SrGame *g, SrEncChoice c);
#define SR_CRED_WEEK_CAP    6
#define SR_JOB_HEAT_CLEANUP (-100) /* a fixer menu row that buys a Heat clean-up */
/* Weekly upkeep: 2 % of installed cyberware and the equipped weapon. */
int32_t sr_game_weekly_upkeep(const SrGame *g);
/* Rent of the current home: +10 % per Street Cred tier above Runner. */
int32_t sr_game_rent_for(const SrGame *g);
#define SR_MED_SLEEP_PER_HP  5  /* bandages and meds for healing in bed */
#define SR_MED_CLINIC_PER_HP 15 /* a clinic's treatment */
/* A medical bill per healed health point; what cannot be paid is owed. */
void sr_game_medical_bill(SrGame *g, float healed_hp, int32_t per_hp);
/* A fixer lowers the worst Heat by one level for 500 cred per level. */
SrResult sr_game_heat_cleanup(SrGame *g);
/* Opens a fixer's offers as a menu; false when there is no work. */
bool sr_game_jobs_open(SrGame *g, int fixer);
/* Jobs on the terminal: the handler's work, gang contacts, the public board, then accepted jobs. */
int sr_game_terminal_jobs(const SrGame *g, int32_t *out, int max);
void sr_game_chess_init(SrGame *g);
/* Every in-game minute: the board, news, contracts as jobs, job outcomes, world effects. */
void sr_game_chess_minute(SrGame *g, int64_t t);
/* After loading: replays recorded city changes and refreshes derived state. */
void sr_game_chess_loaded(SrGame *g);
int sr_game_police(const SrGame *g);
/* The job hits the patron or a faction the patron funds or owns. */
bool sr_game_job_against_patron(const SrGame *g, const SrJob *j);
int sr_game_jurisdiction(const SrGame *g, int building);
/* District price index in percent (shortages after sabotage). */
int sr_game_price_pct(const SrGame *g, int building);
SrResult sr_game_accept_retainer(SrGame *g);
SrResult sr_game_accept_offer(SrGame *g);
SrResult sr_game_trade(SrGame *g, int corp, int qty);
SrResult sr_game_choose_ending(SrGame *g, SrEnding e);
/* Buys fake evidence pointing at a faction for an accepted job (framing). */
SrResult sr_game_frame(SrGame *g, int32_t job, int faction);
/* Black-op (T4) jobs from the handler when trust and Street Cred allow; returns jobs offered. */
int sr_game_handler_offers(SrGame *g);
int sr_game_chess_node_actions(const SrGame *g, int building, SrNodeAction *out);
SrResult sr_game_chess_node_act(SrGame *g, int building, SrNodeAction a);
void sr_game_chess_paydata(SrGame *g, int building);
/* Terminal tabs and rows. */
int sr_game_term_rows(const SrGame *g);
void sr_game_term_input(SrGame *g, const SrActionState *a);
/* Story options on the terminal's story tab (label for row i, or NULL). */
const char *sr_game_story_option(const SrGame *g, int i, int *kind);

/* Character creation: background choice and a randomisable appearance. */
void sr_game_begin_create(SrGame *g, uint64_t seed);
void sr_game_create_select(SrGame *g, int background);
void sr_game_create_randomize(SrGame *g);
SrAppearance sr_game_create_preview(const SrGame *g);
SrResult sr_game_create_confirm(SrGame *g);

#endif
