/* Near-tier simulation: citizens around the player as full agents. Spawning respects the
 * no-pop-in rule (never inside the view within 30 m), agents walk sidewalks, react to danger
 * and rain, and witnesses emit crime reports. */
#ifndef SR_CROWD_H
#define SR_CROWD_H

#include "core/sr_math.h"
#include "sim/sr_population.h"

#define SR_MAX_AGENTS 320
#define SR_CROWD_RADIUS 110.0f
#define SR_NO_POP_RADIUS 30.0f

typedef enum SrAgentState {
  SR_AG_WALK, SR_AG_FLEE, SR_AG_COVER, SR_AG_GAWK, SR_AG_SHELTER, SR_AG_IDLE
} SrAgentState;

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
} SrAgent;

typedef struct SrCamera {
  SrVec3 pos;
  SrVec3 forward;
  float cos_half_fov;
} SrCamera;

typedef struct SrCrowd {
  int count;
  SrAgent agents[SR_MAX_AGENTS];
  int64_t last_slot;
  int spawned_in_view_near; /* diagnostics: must stay 0 */
  int despawned_in_view_near;
  int reports;
} SrCrowd;

void sr_crowd_init(SrCrowd *cr);
bool sr_camera_sees(const SrCamera *cam, SrVec3 p);
/* Rebuilds the desired crowd around the camera and spawns or despawns agents. */
void sr_crowd_refresh(SrCrowd *cr, const SrCity *c, const SrPopulation *pop, const SrCamera *cam,
                      int64_t t, int max_agents);
void sr_crowd_update(SrCrowd *cr, const SrCity *c, float dt, float rain);
/* Gunfire at pos: agents within radius react within this tick; witnesses may report. */
int sr_crowd_gunfire(SrCrowd *cr, const SrCity *c, SrVec3 pos, float radius, SrEventLog *log,
                     int64_t t);
int sr_crowd_find(const SrCrowd *cr, uint64_t key);
float sr_crowd_exposed_share(const SrCrowd *cr); /* outdoor, no cover, no umbrella */
int sr_crowd_visible(const SrCrowd *cr, const SrCamera *cam, float max_dist);

#endif
