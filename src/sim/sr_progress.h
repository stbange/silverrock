/* Character progression: attributes, use-based skills, levels, perks, Street Cred, standing. */
#ifndef SR_PROGRESS_H
#define SR_PROGRESS_H

#include "world/sr_world.h"

typedef enum SrAttr { SR_ATTR_BODY, SR_ATTR_REFLEX, SR_ATTR_TECH, SR_ATTR_MIND, SR_ATTR_COOL, SR_ATTR_COUNT } SrAttr;

typedef enum SrSkill {
  SR_SK_PISTOLS,
  SR_SK_LONGGUNS,
  SR_SK_HEAVY,
  SR_SK_MELEE,
  SR_SK_ATHLETICS,
  SR_SK_STEALTH,
  SR_SK_NETRUNNING,
  SR_SK_ELECTRONICS,
  SR_SK_ENGINEERING,
  SR_SK_MEDICINE,
  SR_SK_PERSUASION,
  SR_SK_STREETWISE,
  SR_SK_COUNT
} SrSkill;

typedef enum SrCredTier {
  SR_CRED_NOBODY,
  SR_CRED_PAWN,
  SR_CRED_RUNNER,
  SR_CRED_OPERATOR,
  SR_CRED_KNIGHT,
  SR_CRED_LEGEND
} SrCredTier;

#define SR_MAX_LEVEL       50
#define SR_MAX_SKILL_RANK  20
#define SR_PERKS_PER_SKILL 4

typedef struct SrProgress {
  int8_t attr[SR_ATTR_COUNT];
  int8_t skill_rank[SR_SK_COUNT];
  int32_t skill_xp[SR_SK_COUNT];
  int32_t level;
  int32_t xp;
  int32_t perk_points;
  int32_t attr_points;
  uint8_t perks[SR_SK_COUNT];       /* bitmask of bought perk tiers */
  int32_t street_cred;              /* 0..100 */
  int8_t standing[SR_MAX_FACTIONS]; /* -100..100 */
  int32_t notifications;            /* incremented on rank-ups and tier changes */
} SrProgress;

typedef enum SrSkillUse { SR_USE_TRIVIAL, SR_USE_HOSTILE, SR_USE_SUCCESS } SrSkillUse;

void sr_progress_init(SrProgress *p);
const char *sr_attr_name(SrAttr a);
const char *sr_skill_name(SrSkill s);
SrAttr sr_skill_attr(SrSkill s);
bool sr_progress_raise_attr(SrProgress *p, SrAttr a);
int32_t sr_skill_xp_for_rank(int rank);
/* Records a use; returns true if a rank was gained. */
bool sr_progress_use_skill(SrProgress *p, SrSkill s, SrSkillUse use, int amount);
int32_t sr_level_xp(int level);
/* Adds job experience; returns levels gained. */
int sr_progress_add_xp(SrProgress *p, int32_t xp);
int sr_perk_required_rank(int tier);
bool sr_progress_buy_perk(SrProgress *p, SrSkill s, int tier);
bool sr_progress_has_perk(const SrProgress *p, SrSkill s, int tier);
SrCredTier sr_cred_tier(int32_t cred);
const char *sr_cred_tier_name(SrCredTier t);
/* Adds Street Cred (clamped); returns true when the tier changed. */
bool sr_progress_add_cred(SrProgress *p, int32_t amount);
/* Changes standing only if the faction attributes the action to the player. */
void sr_progress_standing(SrProgress *p, int faction, int delta, bool attributed);
bool sr_faction_hostile(const SrProgress *p, int faction);
const char *sr_standing_name(int standing);
/* Refunds every spent attribute and perk point. */
void sr_progress_respec(SrProgress *p, const int8_t base_attr[SR_ATTR_COUNT]);

#endif
