/* Jobs: generation from templates bound to the live world, tiers, sources, briefings and intel,
 * negotiation, approach validation, execution, constraints, payment, failure, consequences. */
#ifndef SR_JOBS_H
#define SR_JOBS_H

#include "sim/sr_heat.h"
#include "sim/sr_player.h"
#include "world/sr_host.h"

#define SR_MAX_JOBS 64

typedef enum SrJobType {
  SR_JT_DELIVER,
  SR_JT_COLLECT,
  SR_JT_FIND,
  SR_JT_PLANT,
  SR_JT_SURVEIL,
  SR_JT_THEFT,
  SR_JT_STEAL_DATA,
  SR_JT_SABOTAGE,
  SR_JT_RAID,
  SR_JT_EXTRACT,
  SR_JT_COUNT
} SrJobType;

typedef enum SrApproach { SR_AP_GHOST, SR_AP_FACE, SR_AP_RUNNER, SR_AP_HAMMER, SR_AP_COUNT } SrApproach;
typedef enum SrJobState { SR_JS_OFFERED, SR_JS_ACCEPTED, SR_JS_COMPLETED, SR_JS_FAILED, SR_JS_WITHDRAWN } SrJobState;
typedef enum SrJobSource { SR_SRC_BOARD, SR_SRC_FIXER, SR_SRC_CONTACT, SR_SRC_EVENT } SrJobSource;

enum SrJobConstraint { SR_JC_NO_KILLS = 1, SR_JC_NO_ALARMS = 2, SR_JC_ITEM_INTACT = 4 };
enum SrJobIntel { SR_INTEL_GUARDS = 1, SR_INTEL_CAMERAS = 2, SR_INTEL_ENTRANCE = 4, SR_INTEL_SECURITY = 8 };

typedef struct SrJob {
  int32_t id;
  uint8_t type, tier, state, source;
  int8_t client_faction; /* -1 private client */
  int8_t target_faction;
  int32_t client_building; /* where to report back (fixer haunt or client) */
  int32_t target_building;
  int32_t dest_building; /* deliveries */
  uint8_t constraints;
  bool constraint_fails; /* violating a constraint fails the job (else halves pay) */
  uint8_t approaches;    /* bitmask of validated approaches */
  uint8_t intel;         /* revealed facts */
  bool negotiated;
  bool executed; /* objective done, report back pending */
  bool has_bonus, bonus_done;
  int64_t offered, deadline;
  int32_t pay, upfront, bonus;
  int32_t paid; /* total paid so far */
  int kills, alarms;
  int8_t framed; /* faction the planted evidence points to, -1 none */
  bool booked;   /* the corporate game has seen the outcome */
  char title[64];
} SrJob;

typedef struct SrJobBoard {
  int count;
  SrJob jobs[SR_MAX_JOBS];
  int32_t next_id;
  int completed, failed;
} SrJobBoard;

typedef struct SrMissionResult {
  bool success;
  bool alarm;
  int kills, knockouts;
  float damage;
  bool witnessed, camera;
  int64_t seconds; /* in-game time spent */
} SrMissionResult;

void sr_jobs_init(SrJobBoard *b);
const char *sr_job_type_name(SrJobType t);
const char *sr_approach_name(SrApproach a);
void sr_tier_pay_range(int tier, int32_t *lo, int32_t *hi);
int sr_job_tier_of_type(SrJobType t);
int sr_max_tier_for_cred(int32_t street_cred);

/* Generates one job of a type bound to the world; returns false if no binding exists. */
bool sr_job_generate(const SrCity *c, SrJobType type, uint64_t key, int64_t t, SrJob *out);
/* Matrix approach planner: what a netrunner has to do in the target's host to clear the way. */
typedef struct SrMatrixPlan {
  bool feasible; /* networked, complete security chain, the target reachable once doors open */
  int security;  /* host security 1..10 */
  int cameras, doors, turrets;
  int steps; /* node actions in the plan (plus breaching the gateway and erasing logs) */
} SrMatrixPlan;
SrMatrixPlan sr_job_matrix_plan(const SrCity *c, const SrJob *j);

/* Approach bitmask the job's site allows (validated against the interior). */
uint8_t sr_job_validate_approaches(const SrCity *c, const SrJob *j);
/* Fixer offers for a day (1..4 jobs within unlocked tiers); adds them to the board. */
int sr_jobs_fixer_offers(SrJobBoard *b, const SrCity *c, int fixer_building, int64_t t, int32_t street_cred,
                         int out_ids[4]);
/* Public job board refresh (board jobs of unlocked tiers). */
int sr_jobs_board_refresh(SrJobBoard *b, const SrCity *c, int64_t t, int32_t street_cred);
SrJob *sr_jobs_get(SrJobBoard *b, int32_t id);
/* Adds a job to the board (dropping the oldest inactive one when full); NULL if the board is busy. */
SrJob *sr_jobs_push(SrJobBoard *b, const SrJob *j);
/* A job for a contract of the corporate game: the type bound to the contract's target building
 * and factions where the building fits. */
bool sr_job_for_contract(const SrCity *c, SrJobType type, int building, int client, int target, uint64_t key, int64_t t,
                         SrJob *out);

SrResult sr_job_accept(SrJob *j, SrPlayer *p, int64_t t);
int32_t sr_job_intel_price(const SrJob *j);
SrResult sr_job_buy_intel(SrJob *j, SrPlayer *p, int64_t t);
/* Persuasion check with rng; returns SR_OK (pay up), SR_ERR_DENIED (failed, lower or withdrawn). */
SrResult sr_job_negotiate(SrJob *j, const SrPlayer *p, uint64_t seed);
/* Simulates executing the objective with an approach at the target site. */
SrMissionResult sr_job_execute(const SrCity *c, SrJob *j, SrPlayer *p, SrApproach a, uint64_t seed);
/* Books a mission outcome (simulated or played in 3D): kill and alarm counts, objective state, job
 * items. Damage is not applied here. */
void sr_job_apply_result(SrJob *j, SrPlayer *p, const SrMissionResult *r);
/* Whether a job type takes place inside a site interior. */
bool sr_job_is_site(SrJobType t);
/* Reports back: pays remaining (+bonus), applies constraints; returns amount paid. */
int32_t sr_job_complete(SrJob *j, SrPlayer *p, int64_t t, SrEventLog *log, const SrCity *c);
/* Fails jobs past their deadline. Returns how many failed. */
int sr_jobs_tick(SrJobBoard *b, SrPlayer *p, int64_t t, SrEventLog *log);
SrResult sr_job_abandon(SrJob *j, SrPlayer *p, int64_t t, SrEventLog *log);
void sr_job_briefing(const SrCity *c, const SrJob *j, char *out, size_t cap);

#endif
