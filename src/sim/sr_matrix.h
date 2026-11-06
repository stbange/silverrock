/* The Matrix rules: persona stats from the deck, programs with memory and cooldowns, the
 * ring-alignment breach puzzle (generator, input abstraction, solver), ICE on the shared alert
 * machine, traces and downloads. Pure functions over plain structs; the game layer owns the dive. */
#ifndef SR_MATRIX_H
#define SR_MATRIX_H

#include "sim/sr_combat.h"
#include "world/sr_host.h"

#define SR_BREACH_MAX_RINGS 6
#define SR_MX_MAX_ICE       12
#define SR_LOADOUT_MAX      8
#define SR_DECK_GRADES      5

typedef struct SrPersona {
  float integrity, max_integrity; /* the persona's health */
  float firewall;                 /* damage reduction 0..0.8 */
  float bandwidth;                /* MB per second */
  float stealth;                  /* 0..1, lowers detection */
  float signal;                   /* wireless range in metres */
} SrPersona;

/* deck_grade 0 (no deck: zero stats) .. 4; mind and netrunning from progression. */
SrPersona sr_persona_stats(int deck_grade, int mind, int netrunning);
int sr_deck_memory(int deck_grade);

typedef enum SrProgramKind { SR_PK_ATTACK, SR_PK_DEFENCE, SR_PK_STEALTH, SR_PK_UTILITY } SrProgramKind;

typedef enum SrProgram {
  SR_PRG_SPIKE, /* attack: damages ICE */
  SR_PRG_CRASH, /* attack: heavy damage, long cooldown */
  SR_PRG_SHIELD,
  SR_PRG_MEND,  /* restores integrity */
  SR_PRG_VEIL,  /* stealth: hard to detect for a while */
  SR_PRG_GHOST, /* stealth: drops the trace back */
  SR_PRG_WORM,  /* utility: leaves a backdoor */
  SR_PRG_SNIFF, /* utility: reveals ICE and data */
  SR_PRG_COUNT
} SrProgram;

typedef struct SrProgramDef {
  const char *name;
  uint8_t kind;
  uint8_t memory;
  float cooldown; /* seconds */
  float power;
} SrProgramDef;

typedef struct SrLoadout {
  int count;
  uint8_t prog[SR_LOADOUT_MAX];
  float cooldown[SR_LOADOUT_MAX];
} SrLoadout;

const SrProgramDef *sr_program_def(int p);
int sr_loadout_memory(const SrLoadout *l);
/* Loads a program if it fits the deck's free memory. */
bool sr_loadout_add(SrLoadout *l, int deck_grade, SrProgram p);
/* Uses the program in a slot: false while it cools down. */
bool sr_loadout_use(SrLoadout *l, int slot);
void sr_loadout_tick(SrLoadout *l, float dt);
int sr_loadout_find(const SrLoadout *l, SrProgram p);

/* Breach puzzle: rings turn on their own; each has one true gap and maybe decoy gaps. Turn the
 * selected ring until its true gap meets the lock mark (angle 0) and commit: the ring locks and
 * stops. A commit on a miss or on a decoy costs time and scrambles the ring. Lock every ring
 * before the time runs out. */
typedef struct SrBreach {
  int rings;
  float angle[SR_BREACH_MAX_RINGS]; /* where each ring's true gap is now */
  float speed[SR_BREACH_MAX_RINGS]; /* drift, rad/s */
  int decoys[SR_BREACH_MAX_RINGS];
  float decoy_at[SR_BREACH_MAX_RINGS][3]; /* decoy gap offsets from the true gap */
  bool locked[SR_BREACH_MAX_RINGS];
  int sel;
  float time_left, time_limit;
  int mistakes;
  bool done, failed;
} SrBreach;

#define SR_BREACH_TOLERANCE 0.18f /* radians */
#define SR_BREACH_TURN_RATE 2.4f  /* rad/s a player turns a ring at minimum skill */

void sr_breach_generate(SrBreach *b, int security, uint64_t seed);
void sr_breach_tick(SrBreach *b, float dt);
void sr_breach_select(SrBreach *b, int delta); /* next/previous ring (wraps) */
void sr_breach_select_ring(SrBreach *b, int ring);
void sr_breach_rotate(SrBreach *b, float radians); /* turns the selected ring */
/* Commits the selected ring. Returns true when the whole breach is done. */
bool sr_breach_commit(SrBreach *b);
#define SR_BREACH_PENALTY 3.0f
bool sr_breach_aligned(const SrBreach *b, int ring);
/* Seconds a minimum-skill player needs; the generator guarantees it fits the time limit. */
float sr_breach_solve_time(const SrBreach *b);
/* Plays the puzzle to the end with perfect inputs at the minimum turn rate (for tests and bots). */
bool sr_breach_autosolve(SrBreach *b, float dt);

typedef enum SrIceType {
  SR_ICE_WATCHER, /* patrols, raises the alert */
  SR_ICE_BARRIER, /* seals a node until broken */
  SR_ICE_TARPIT,  /* slows the persona */
  SR_ICE_TRACER,  /* traces the body */
  SR_ICE_KILLER,  /* attacks integrity */
  SR_ICE_BLACK,   /* attacks integrity and the body */
  SR_ICE_SYSOP,   /* an enemy netrunner on duty: hunts, hits hard and speeds up the trace */
  SR_ICE_COUNT
} SrIceType;

typedef struct SrIce {
  uint8_t type;
  bool active;
  int16_t node; /* the node it guards or patrols around */
  SrEnemy e;    /* position, alert state and health on the shared AI machine */
  float cd;
  float phase;
} SrIce;

typedef struct SrIceHit {
  float integrity; /* damage to the persona */
  float body;      /* damage to the body (Black ICE) */
  float trace;     /* trace progress added (0..1) */
  bool detected;   /* a Watcher or another ICE noticed the persona this tick */
  float slow;      /* 0..1 speed penalty from tar pits */
} SrIceHit;

const char *sr_ice_name(SrIceType t);
/* Initial ICE of a host: watchers, barriers on the core layer, more and harder with security. */
int sr_ice_populate(const SrHost *h, uint64_t seed, SrIce *out, int max);
/* ICE spawned when the alert rises (killers, tracers, black ICE at high security). */
int sr_ice_reinforce(const SrHost *h, int alert, uint64_t seed, SrIce *out, int max);
/* Advances one ICE for dt seconds against a persona at pos. */
void sr_ice_update(SrIce *ice, const SrHost *h, SrVec3 persona, const SrPersona *ps, bool veiled, int security,
                   float dt, SrIceHit *hit);
/* Damage of an attack program on ICE. */
bool sr_ice_damage(SrIce *ice, float amount);

/* Download time in seconds for a file of `megabytes` at the persona's bandwidth (x speed factor). */
float sr_download_time(int megabytes, const SrPersona *ps, float speed);
/* Seconds until host security finds a backdoor or a tampered device: shorter at high security. */
float sr_discovery_time(int security);
/* Trace speed per second for a host security level. */
float sr_trace_rate(int security);

#endif
