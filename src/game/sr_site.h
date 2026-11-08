/* Walkable interiors: the player's apartment and mission sites, entered from the street. The
 * interior is laid out under its building (floor at SR_SITE_FLOOR_Y), so world queries that
 * use x/z (district, home block, Heat jurisdiction) keep working while the player is inside.
 * Guards patrol and react through the combat rules, cameras watch rooms, gadgets leave
 * effects, and every change worth keeping (doors opened, cameras disabled, guards down, the
 * objective done) is written to the world deltas so the next visit finds it again. */
#ifndef SR_SITE_H
#define SR_SITE_H

#include "sim/sr_combat.h"
#include "sim/sr_jobs.h"
#include "world/sr_interior.h"

#define SR_MAX_DELTAS        1024
#define SR_SITE_FLOOR_Y      (-40.0f)
#define SR_SITE_WALL_H       3.0f
#define SR_SITE_DOOR_HALF    0.7f /* door openings are 1.4 m wide, centred on the wall */
#define SR_SITE_VENT_HALF    0.45f
#define SR_SITE_VENT_OFFSET  1.8f /* vents sit 1.8 m from the wall centre */
#define SR_SITE_WALL_HALF    0.1f
#define SR_SITE_MAX_GUARDS   10
#define SR_SITE_MAX_EFFECTS  8
#define SR_SITE_CAMERA_TIME  1.2f /* seconds of camera contact until the alarm */
#define SR_SITE_GUARD_RANGE  18.0f
#define SR_SITE_DECOY_REVEAL 2.5f

typedef struct SrDeltas {
  int count;
  uint32_t id[SR_MAX_DELTAS];
  uint8_t value[SR_MAX_DELTAS];
} SrDeltas;

void sr_deltas_set(SrDeltas *d, uint32_t id, uint8_t value);
int sr_deltas_get(const SrDeltas *d, uint32_t id); /* -1 if unset */

/* Delta ids and values for interiors. Devices use their stable device id. */
#define SR_DV_UNLOCKED 1
#define SR_DV_DISABLED 2
#define SR_GV_KO       1
#define SR_GV_DEAD     2
uint32_t sr_site_guard_id(int building, int k);
#define SR_GUARD_PAUSE_MIN 2.0f /* seconds a patrolling guard looks around at each waypoint */
uint32_t sr_site_objective_id(int32_t job);

typedef enum SrEffectType {
  SR_FX_SMOKE,
  SR_FX_DECOY,
  SR_FX_JAMMER,
  SR_FX_TRIPMINE,
  SR_FX_TURRET,
  SR_FX_LOOP, /* data spike: cameras see nothing */
  SR_FX_COUNT
} SrEffectType;

typedef struct SrSiteEffect {
  uint8_t type;
  bool active;
  SrVec3 pos;
  float radius;
  float time; /* seconds left */
  float cooldown;
} SrSiteEffect;

typedef struct SrGuard {
  SrEnemy e;
  uint32_t id;
  uint8_t route[4];
  int route_n, route_i;
  float fire_cd;
  float stun;    /* flashbang: seconds blinded */
  int8_t decoy;  /* effect index the guard is chasing, -1 none */
  bool reported; /* this body was already found */
  bool tagged;   /* revealed by recon gadgets */
  float walk;    /* distance walked, for the walk cycle */
  /* presentation (not saved): speed, patrol pause, and site times of the last hit, fall, waking
   * and shot, read by the animation */
  float speed, pause;
  float hit_time, hit_dir; /* hit_dir: world yaw from the guard toward the shooter */
  uint8_t hit_kind;        /* 0 none, 1 flinch, 2 stagger, 3 headshot */
  float down_time, wake_time, shot_time;
  bool shot_tracer;
} SrGuard;

typedef struct SrSite {
  bool active;
  int32_t building;
  int32_t job; /* job being played here, -1 */
  int entry;   /* entry index used */
  SrInterior in;
  float ox, oz; /* world position of the interior's north-west corner, metres */
  int guard_count;
  SrGuard guards[SR_SITE_MAX_GUARDS];
  int effect_count;
  SrSiteEffect effects[SR_SITE_MAX_EFFECTS];
  float time;       /* seconds inside */
  float cam_detect; /* camera contact 0..1 */
  float awareness;  /* highest guard awareness, for the HUD */
  bool alarm, objective_done, camera_seen, witnessed, hostile;
  bool lights_off;
  float turret_cd;
  bool authorised;      /* forged credentials: the staff treats the player as one of them */
  float reinforce_at;   /* site time when called reinforcements arrive, 0 none */
  float elevator_floor; /* current floor of the elevator (moves towards its target) */
  int elevator_target;
  float check_t[SR_INT_MAX_DEVICES]; /* disabled cameras: time until a guard comes to check */
  bool checked[SR_INT_MAX_DEVICES];
  int kills, knockouts;
  float damage; /* taken inside */
  int64_t entered_at;
  int shots_fired; /* by guards and turrets, for tests */
} SrSite;

/* What the site needs from the rest of the game during an update. */
typedef struct SrSiteCtx {
  SrDeltas *deltas;
  SrVec3 player; /* eye position */
  float player_speed;
  bool crouched, camo, masked, has_fake_id, toxin_filter;
  SrDifficulty difficulty;
  uint64_t key;        /* deterministic shot key base */
  float player_damage; /* out: damage dealt to the player this update */
} SrSiteCtx;

/* Opens a site for an interior (already generated): applies deltas and spawns guards. */
void sr_site_open(SrSite *s, const SrInterior *in, int building, float ox, float oz, const SrDeltas *d, int entry,
                  int32_t job);
void sr_site_close(SrSite *s);
/* Eye position and yaw just inside the exit of an entry. */
SrVec3 sr_site_entry_pos(const SrSite *s, int entry, float *yaw);
int sr_site_entry_of_type(const SrSite *s, SrEntryType t); /* -1 if absent */

/* Geometry (world coordinates). */
int sr_site_room_at(const SrSite *s, float x, float z);
SrVec3 sr_site_room_center(const SrSite *s, int room);
bool sr_site_door_open(const SrSite *s, int room, int dir);
bool sr_site_blocked(const SrSite *s, float x, float z, float radius, bool crouched);
/* Line of sight: walls, closed doors and smoke block it. */
bool sr_site_los(const SrSite *s, SrVec3 a, SrVec3 b);
float sr_site_light(const SrSite *s, int room);
bool sr_site_restricted(const SrSite *s, int room, bool has_fake_id);

/* Props and devices. */
SrVec3 sr_site_prop_pos(const SrSite *s, int prop);
int sr_site_prop_near(const SrSite *s, SrVec3 eye, float yaw, float range);
/* Locked door the player faces within range: device index, or -1. */
int sr_site_locked_door_ahead(const SrSite *s, SrVec3 eye, float yaw, float range);
void sr_site_unlock(SrSite *s, SrDeltas *d, int device);
/* Security console: cameras and turrets off, doors unlocked, alarm silenced. */
void sr_site_console(SrSite *s, SrDeltas *d);
bool sr_site_device_disabled(const SrSite *s, const SrDeltas *d, int device);
SrVec3 sr_site_camera_pos(const SrSite *s, int device);

/* Guards. */
int sr_site_guard_ahead(const SrSite *s, SrVec3 eye, float yaw, float pitch, float range, float cone_cos);
bool sr_site_takedown(SrSite *s, SrDeltas *d, SrVec3 eye, float yaw, bool lethal);
/* Hitscan shot: true if a guard was hit. */
bool sr_site_shoot(SrSite *s, SrDeltas *d, SrVec3 eye, float yaw, float pitch, const SrWeaponStats *w);
void sr_site_noise(SrSite *s, SrVec3 pos, float radius);
/* A false alarm in a room: every guard on site goes to look. */
void sr_site_false_alarm(SrSite *s, int room);
int sr_site_guards_up(const SrSite *s);

/* Gadgets used inside: returns true if the gadget had an effect (and is used up). */
bool sr_site_gadget(SrSite *s, SrSiteCtx *ctx, int gadget, SrVec3 eye, float yaw);
int sr_site_add_effect(SrSite *s, SrEffectType t, SrVec3 pos, float radius, float time);
/* Point where a throw from the eye along yaw lands (stops short of walls), max 12 m. */
SrVec3 sr_site_throw_point(const SrSite *s, SrVec3 eye, float yaw, float max_dist);

/* Advances guards, cameras, turrets and effects. */
void sr_site_update(SrSite *s, SrSiteCtx *ctx, float dt);
/* The outcome so far as a mission result. */
SrMissionResult sr_site_result(const SrSite *s, int64_t now);

#endif
