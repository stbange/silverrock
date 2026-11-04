#include "sim/sr_progress.h"

#include <string.h>

void sr_progress_init(SrProgress *p) {
  memset(p, 0, sizeof *p);
  for (int a = 0; a < SR_ATTR_COUNT; a++) p->attr[a] = 3;
  p->level = 1;
}

const char *sr_attr_name(SrAttr a) {
  static const char *const n[SR_ATTR_COUNT] = {"BODY", "REFLEX", "TECH", "MIND", "COOL"};
  return (unsigned)a < SR_ATTR_COUNT ? n[a] : "?";
}

const char *sr_skill_name(SrSkill s) {
  static const char *const n[SR_SK_COUNT] = {
      "Pistols", "Long Guns", "Heavy Weapons", "Melee", "Athletics", "Stealth", "Netrunning",
      "Electronics", "Engineering", "Medicine", "Persuasion", "Streetwise"};
  return (unsigned)s < SR_SK_COUNT ? n[s] : "?";
}

SrAttr sr_skill_attr(SrSkill s) {
  static const SrAttr a[SR_SK_COUNT] = {
      SR_ATTR_REFLEX, SR_ATTR_REFLEX, SR_ATTR_BODY, SR_ATTR_BODY, SR_ATTR_BODY, SR_ATTR_COOL,
      SR_ATTR_MIND, SR_ATTR_TECH, SR_ATTR_TECH, SR_ATTR_TECH, SR_ATTR_COOL, SR_ATTR_COOL};
  return (unsigned)s < SR_SK_COUNT ? a[s] : SR_ATTR_BODY;
}

bool sr_progress_raise_attr(SrProgress *p, SrAttr a) {
  if ((unsigned)a >= SR_ATTR_COUNT || p->attr_points <= 0 || p->attr[a] >= 10) return false;
  p->attr[a]++;
  p->attr_points--;
  return true;
}

int32_t sr_skill_xp_for_rank(int rank) { return 50 + 40 * rank; }

bool sr_progress_use_skill(SrProgress *p, SrSkill s, SrSkillUse use, int amount) {
  if ((unsigned)s >= SR_SK_COUNT || use == SR_USE_TRIVIAL || amount <= 0) return false;
  if (p->skill_rank[s] >= SR_MAX_SKILL_RANK) return false;
  p->skill_xp[s] += amount * (use == SR_USE_SUCCESS ? 2 : 1);
  bool ranked = false;
  while (p->skill_rank[s] < SR_MAX_SKILL_RANK &&
         p->skill_xp[s] >= sr_skill_xp_for_rank(p->skill_rank[s])) {
    p->skill_xp[s] -= sr_skill_xp_for_rank(p->skill_rank[s]);
    p->skill_rank[s]++;
    p->notifications++;
    ranked = true;
  }
  return ranked;
}

int32_t sr_level_xp(int level) { return 100 * level + 25 * level * level; }

int sr_progress_add_xp(SrProgress *p, int32_t xp) {
  int gained = 0;
  p->xp += xp;
  while (p->level < SR_MAX_LEVEL && p->xp >= sr_level_xp(p->level)) {
    p->xp -= sr_level_xp(p->level);
    p->level++;
    p->perk_points++;
    if (p->level % 3 == 0) p->attr_points++;
    gained++;
  }
  return gained;
}

int sr_perk_required_rank(int tier) { return 5 * (tier + 1); }

bool sr_progress_buy_perk(SrProgress *p, SrSkill s, int tier) {
  if ((unsigned)s >= SR_SK_COUNT || tier < 0 || tier >= SR_PERKS_PER_SKILL) return false;
  if (p->perk_points <= 0 || p->skill_rank[s] < sr_perk_required_rank(tier)) return false;
  if (p->perks[s] & (1u << tier)) return false;
  p->perks[s] |= (uint8_t)(1u << tier);
  p->perk_points--;
  return true;
}

bool sr_progress_has_perk(const SrProgress *p, SrSkill s, int tier) {
  return (unsigned)s < SR_SK_COUNT && tier >= 0 && tier < 8 && (p->perks[s] & (1u << tier));
}

SrCredTier sr_cred_tier(int32_t cred) {
  if (cred >= 90) return SR_CRED_LEGEND;
  if (cred >= 70) return SR_CRED_KNIGHT;
  if (cred >= 45) return SR_CRED_OPERATOR;
  if (cred >= 25) return SR_CRED_RUNNER;
  if (cred >= 10) return SR_CRED_PAWN;
  return SR_CRED_NOBODY;
}

const char *sr_cred_tier_name(SrCredTier t) {
  static const char *const n[] = {"Nobody", "Pawn", "Runner", "Operator", "Knight", "Legend"};
  return (unsigned)t <= SR_CRED_LEGEND ? n[t] : "?";
}

bool sr_progress_add_cred(SrProgress *p, int32_t amount) {
  SrCredTier before = sr_cred_tier(p->street_cred);
  p->street_cred = SR_CLAMP(p->street_cred + amount, 0, 100);
  bool changed = sr_cred_tier(p->street_cred) != before;
  if (changed) p->notifications++;
  return changed;
}

void sr_progress_standing(SrProgress *p, int faction, int delta, bool attributed) {
  if (!attributed || faction < 0 || faction >= SR_MAX_FACTIONS) return;
  p->standing[faction] = (int8_t)SR_CLAMP(p->standing[faction] + delta, -100, 100);
}

bool sr_faction_hostile(const SrProgress *p, int faction) {
  return faction >= 0 && faction < SR_MAX_FACTIONS && p->standing[faction] <= -60;
}

const char *sr_standing_name(int s) {
  if (s <= -60) return "Hostile";
  if (s <= -20) return "Unfriendly";
  if (s < 20) return "Neutral";
  if (s < 60) return "Friendly";
  return "Trusted";
}

void sr_progress_respec(SrProgress *p, const int8_t base_attr[SR_ATTR_COUNT]) {
  for (int a = 0; a < SR_ATTR_COUNT; a++) {
    p->attr_points += p->attr[a] - base_attr[a];
    p->attr[a] = base_attr[a];
  }
  for (int s = 0; s < SR_SK_COUNT; s++) {
    for (int t = 0; t < 8; t++)
      if (p->perks[s] & (1u << t)) p->perk_points++;
    p->perks[s] = 0;
  }
}
