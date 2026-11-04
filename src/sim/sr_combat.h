/* Combat rules and enemy AI: hit zones, armour, damage types, melee and takedowns,
 * knockouts, healing, flatlining, difficulty, light-based perception, alert states, tactics. */
#ifndef SR_COMBAT_H
#define SR_COMBAT_H

#include "core/sr_math.h"
#include "sim/sr_items.h"

typedef enum SrHitZone { SR_ZONE_HEAD, SR_ZONE_TORSO, SR_ZONE_LIMB } SrHitZone;
typedef enum SrDifficulty { SR_DIFF_STORY, SR_DIFF_STANDARD, SR_DIFF_STREET, SR_DIFF_HARDCORE } SrDifficulty;
typedef enum SrAlert { SR_ALERT_UNAWARE, SR_ALERT_SUSPICIOUS, SR_ALERT_SEARCHING, SR_ALERT_COMBAT } SrAlert;

typedef struct SrDifficultyDef {
  float enemy_accuracy, enemy_damage, perception;
} SrDifficultyDef;

typedef struct SrEnemy {
  SrVec3 pos;
  float yaw;
  float health, max_health, armor;
  SrAlert alert;
  float awareness;     /* 0..1+ */
  float search_timer;  /* seconds left searching */
  SrVec3 investigate;
  float morale;        /* 0..1 */
  float ko_timer;      /* seconds until waking, when knocked out */
  int8_t faction;
  bool drone, ko, dead, restrained, surrendered, heightened, called_reinforcements;
  int8_t cyber;        /* augmentation level 0..5 */
} SrEnemy;

typedef struct SrPerceptInput {
  SrVec3 player_pos;
  float light;      /* 0 darkness .. 1 bright neon */
  float speed;      /* m/s */
  bool crouched;
  bool camo;
  float visibility; /* weather visibility in metres */
  SrDifficulty difficulty;
} SrPerceptInput;

typedef struct SrFlatline {
  bool wake_in_clinic;
  int32_t bill;
  int32_t cash_lost;
  bool delete_save;
} SrFlatline;

const SrDifficultyDef *sr_difficulty(SrDifficulty d);
float sr_zone_multiplier(SrHitZone z);
float sr_type_multiplier(SrDamageType t, bool drone, int cyber);
/* Damage after zone, type and armour. */
float sr_compute_damage(const SrWeaponStats *w, SrHitZone zone, const SrEnemy *target);
/* Applies damage; lethal false knocks out at zero. Returns true if the target went down. */
bool sr_enemy_hit(SrEnemy *e, const SrWeaponStats *w, SrHitZone zone);
void sr_enemy_init(SrEnemy *e, SrVec3 pos, int faction, float health);
/* Silent takedown from behind on an unaware target; noise level written to *noise. */
bool sr_takedown(SrEnemy *e, SrVec3 attacker_pos, bool lethal, float *noise);
void sr_enemy_restrain(SrEnemy *e);
/* Advances knockout timers and morale; dt in game seconds for KO, real seconds otherwise. */
void sr_enemy_update_ko(SrEnemy *e, float game_dt);
float sr_regen_health(float health, float max, float dt);
SrFlatline sr_flatline(bool medevac, int64_t cash, uint64_t seed, int64_t t, bool hardcore);

/* Perception */
float sr_player_visibility(float light, float speed, bool crouched, bool camo);
bool sr_in_view(const SrEnemy *e, SrVec3 p, float max_dist);
/* Updates awareness and alert state for dt seconds. Returns the new alert state. */
SrAlert sr_perceive(SrEnemy *e, const SrPerceptInput *in, float dt);
void sr_hear_noise(SrEnemy *e, SrVec3 noise_pos, float radius);
/* Combat tactics: returns true if this enemy calls reinforcements now. */
bool sr_enemy_tactics(SrEnemy *e, bool comms_jammed, int allies_down);
void sr_body_discovered(SrEnemy *e, SrVec3 body_pos);
const char *sr_alert_name(SrAlert a);

#endif
