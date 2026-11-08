/* Near-tier simulation: citizens around the player as full agents. Spawning respects the
 * no-pop-in rule (never inside the view within 30 m), agents walk sidewalks, react to danger
 * and rain, and witnesses emit crime reports. */
#ifndef SR_CROWD_H
#define SR_CROWD_H

#include "core/sr_math.h"
#include "sim/sr_population.h"

#define SR_MAX_AGENTS    400
#define SR_CROWD_RADIUS  160.0f
#define SR_NO_POP_RADIUS 30.0f

typedef enum SrAgentState { SR_AG_WALK, SR_AG_FLEE, SR_AG_COVER, SR_AG_GAWK, SR_AG_SHELTER, SR_AG_IDLE } SrAgentState;

/* What a citizen does while standing (chosen from the place, the district and the key). */
typedef enum SrStreetAct {
  SR_SA_NONE,
  SR_SA_IDLE,
  SR_SA_SIT,
  SR_SA_SLEEP,
  SR_SA_EAT,
  SR_SA_BUY,
  SR_SA_LEAN,
  SR_SA_SMOKE,
  SR_SA_PHONE,
  SR_SA_CALL,
  SR_SA_WINDOW,
  SR_SA_WAIT,
  SR_SA_QUEUE,
  SR_SA_DANCE,
  SR_SA_WATCH,
  SR_SA_POINT,
  SR_SA_WARM,
  SR_SA_BEG,
  SR_SA_TALK,
  SR_SA_FILM,
  SR_SA_COWER,
  SR_SA_COUNT
} SrStreetAct;

#define SR_AGENT_ACCEL 3.0f /* m/s^2 when starting */
#define SR_AGENT_BRAKE 4.5f /* m/s^2 when stopping */
#define SR_FLEE_SPEED  4.5f

typedef struct SrAgent {
  uint64_t key;
  int32_t npc; /* notable index or -1 for ambient */
  SrAppearance look;
  uint8_t role;
  int8_t faction;
  uint8_t bravery;
  bool umbrella, covered;
  SrAgentState state;
  float timer;
  SrVec3 pos, dir, target;
  float speed;
  int block;
  float walk;               /* metres walked, drives the walk cycle */
  float vel;                /* current speed m/s: agents accelerate and brake */
  float state_time;         /* seconds since the state (or shelter) began */
  uint8_t activity;         /* SrStreetAct while standing */
  uint8_t pending;          /* activity to start on reaching the target */
  uint8_t group_n, group_i; /* talking group size and this member's index */
  uint64_t group;           /* key of the group's first member, 0 none */
  SrVec3 face;              /* direction to face while standing */
} SrAgent;

typedef struct SrCamera {
  SrVec3 pos;
  SrVec3 forward;
  float cos_half_fov;
} SrCamera;

#define SR_MAX_VEHICLES 16

#define SR_CAR_ACCEL 3.0f /* m/s^2 */
#define SR_CAR_BRAKE 6.0f /* m/s^2 */

typedef struct SrVehicle {
  uint64_t key;
  SrVec3 pos;
  SrVec3 dir;   /* unit vector of travel (along the lane, or along the arc while turning) */
  float speed;  /* cruise speed m/s */
  bool stopped; /* braking for a pedestrian ahead */
  uint8_t color;
  float vel, accel; /* current speed and acceleration */
  float odo;        /* metres driven: turns the wheels */
  int8_t indicator; /* -1 left, 1 right, 0 off */
  bool turning;
  float turn_r, turn_a; /* arc radius and angle travelled 0..pi/2 */
  SrVec3 turn_c, turn_d0, turn_d1;
} SrVehicle;

typedef struct SrCrowd {
  int count;
  SrAgent agents[SR_MAX_AGENTS];
  int vehicle_count;
  SrVehicle vehicles[SR_MAX_VEHICLES];
  uint64_t vehicle_serial;
  int64_t last_slot;
  bool fresh;               /* nothing has been shown yet: spawning in view is invisible */
  int spawned_in_view_near; /* diagnostics: must stay 0 */
  int despawned_in_view_near;
  int reports;
  /* set by the game from city politics */
  int8_t police_outfit;  /* outfit colour of officers under a corporate police contract, -1 city blue */
  int8_t police_faction; /* faction officers serve */
  uint16_t curfew;       /* districts under curfew right now: most citizens stay indoors */
} SrCrowd;

void sr_crowd_init(SrCrowd *cr);
bool sr_camera_sees(const SrCamera *cam, SrVec3 p);
/* Rebuilds the desired crowd around the camera and spawns or despawns agents. */
void sr_crowd_refresh(SrCrowd *cr, const SrCity *c, const SrPopulation *pop, const SrCamera *cam, int64_t t,
                      int max_agents);
void sr_crowd_update(SrCrowd *cr, const SrCity *c, float dt, float rain);
/* Gunfire at pos: agents within radius react within this tick; witnesses may report. */
/* District blackout: citizens there stop, gawk or hurry for shelter; logs a world event. */
int sr_crowd_blackout(SrCrowd *cr, const SrCity *c, int district, SrEventLog *log, int64_t t);
int sr_crowd_gunfire(SrCrowd *cr, const SrCity *c, SrVec3 pos, float radius, SrEventLog *log, int64_t t);
int sr_crowd_find(const SrCrowd *cr, uint64_t key);
float sr_crowd_exposed_share(const SrCrowd *cr); /* outdoor, no cover, no umbrella */
int sr_crowd_visible(const SrCrowd *cr, const SrCamera *cam, float max_dist);
/* True when a pedestrian is on the crossing in front of the vehicle. */
bool sr_vehicle_should_stop(const SrVehicle *v, const SrVec3 *peds, int n);
/* The same within a look-ahead of `range` metres (the braking distance plus a margin). */
bool sr_vehicle_should_stop_within(const SrVehicle *v, const SrVec3 *peds, int n, float range);
/* One driving step: brake for pedestrians, accelerate, follow turns at junctions. */
void sr_vehicle_drive(SrVehicle *v, const SrCity *c, const SrVec3 *peds, int np, float dt);
/* The turn a vehicle takes at the junction with grid line `k` across its axis: -1, 0 or 1. */
int sr_vehicle_turn_at(const SrVehicle *v, int k);
/* Activity for a citizen stopping at pos (props within 3 m decide first); faces the prop. */
SrStreetAct sr_activity_pick(const SrCity *c, int block, SrVec3 pos, uint64_t key, int role, SrVec3 *face);
/* True when the activity is one listed for that street prop kind. */
bool sr_activity_fits_prop(int prop_kind, SrStreetAct a);
/* The street prop kind within 3 m of pos that decides the activity there, or -1. */
int sr_activity_prop_near(const SrCity *c, int block, SrVec3 pos, SrVec3 *prop_pos, float *prop_yaw);
/* Spawns, moves and despawns traffic around the camera; the player counts as a pedestrian. */
void sr_crowd_traffic(SrCrowd *cr, const SrCity *c, const SrCamera *cam, SrVec3 player, float dt);

#endif
