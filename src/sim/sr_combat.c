#include "sim/sr_combat.h"

#include "core/sr_rng.h"

#include <math.h>
#include <string.h>

#define SUSPICIOUS_AT 0.3f
#define COMBAT_AT 1.0f
#define SEARCH_TIME 20.0f
#define KO_TIME (30.0f * 60.0f)

const SrDifficultyDef *sr_difficulty(SrDifficulty d) {
  static const SrDifficultyDef defs[4] = {
      {0.55f, 0.6f, 0.7f}, {1.0f, 1.0f, 1.0f}, {1.25f, 1.2f, 1.25f}, {1.25f, 1.3f, 1.25f}};
  return &defs[(unsigned)d < 4 ? d : 1];
}

float sr_zone_multiplier(SrHitZone z) {
  switch (z) {
  case SR_ZONE_HEAD: return 2.0f;
  case SR_ZONE_LIMB: return 0.75f;
  default: return 1.0f;
  }
}

float sr_type_multiplier(SrDamageType t, bool drone, int cyber) {
  switch (t) {
  case SR_DMG_EMP: return drone ? 3.0f : 0.2f + 0.25f * (float)cyber;
  case SR_DMG_SHOCK: return drone ? 0.5f : 1.0f;
  case SR_DMG_TOXIN: return drone ? 0.0f : 1.0f;
  case SR_DMG_THERMAL: return 1.1f;
  default: return 1.0f;
  }
}

float sr_compute_damage(const SrWeaponStats *w, SrHitZone zone, const SrEnemy *t) {
  float dmg = w->damage * sr_zone_multiplier(zone) *
              sr_type_multiplier((SrDamageType)w->damage_type, t->drone, t->cyber);
  float armor_left = t->armor * (1.0f - SR_CLAMP(w->penetration, 0.0f, 1.0f));
  float reduction = armor_left / (armor_left + 50.0f);
  return dmg * (1.0f - reduction);
}

void sr_enemy_init(SrEnemy *e, SrVec3 pos, int faction, float health) {
  memset(e, 0, sizeof *e);
  e->pos = pos;
  e->faction = (int8_t)faction;
  e->health = e->max_health = health;
  e->morale = 1.0f;
}

bool sr_enemy_hit(SrEnemy *e, const SrWeaponStats *w, SrHitZone zone) {
  if (e->dead || e->ko) return false;
  float d = sr_compute_damage(w, zone, e);
  e->health -= d;
  e->morale -= d / e->max_health * 0.6f;
  if (e->alert != SR_ALERT_COMBAT) {
    e->alert = SR_ALERT_COMBAT;
    e->awareness = COMBAT_AT;
  }
  if (e->health > 0) return false;
  e->health = 0;
  if (w->lethal) {
    e->dead = true;
  } else {
    e->ko = true;
    e->ko_timer = KO_TIME;
  }
  return true;
}

bool sr_takedown(SrEnemy *e, SrVec3 from, bool lethal, float *noise) {
  if (e->dead || e->ko || e->alert == SR_ALERT_COMBAT) return false;
  SrVec3 to_attacker = sr_v3_norm(sr_v3_sub(from, e->pos));
  SrVec3 facing = sr_dir_from_yaw_pitch(e->yaw, 0);
  if (sr_v3_dot(to_attacker, facing) > -0.3f) return false; /* must be behind */
  if (sr_v3_len(sr_v3_sub(from, e->pos)) > 2.0f) return false;
  e->health = 0;
  if (lethal) {
    e->dead = true;
    if (noise) *noise = 4.0f;
  } else {
    e->ko = true;
    e->ko_timer = KO_TIME;
    if (noise) *noise = 2.0f;
  }
  return true;
}

void sr_enemy_restrain(SrEnemy *e) {
  if (e->ko) e->restrained = true;
}

void sr_enemy_update_ko(SrEnemy *e, float game_dt) {
  if (!e->ko || e->restrained) return;
  e->ko_timer -= game_dt;
  if (e->ko_timer <= 0) {
    e->ko = false;
    e->health = e->max_health * 0.5f;
    e->alert = SR_ALERT_SEARCHING;
    e->search_timer = SEARCH_TIME;
    e->heightened = true;
  }
}

float sr_regen_health(float health, float max, float dt) {
  float cap = max * 0.3f;
  if (health >= cap) return health;
  health += max * 0.01f * dt;
  return health > cap ? cap : health;
}

SrFlatline sr_flatline(bool medevac, int64_t cash, uint64_t seed, int64_t t, bool hardcore) {
  SrFlatline f;
  memset(&f, 0, sizeof f);
  if (hardcore) {
    f.delete_save = true;
    return f;
  }
  if (medevac) {
    f.wake_in_clinic = true;
    f.bill = 250;
    return f;
  }
  SrRng r = sr_rng_stream(seed, "flatline", (uint64_t)t, 0);
  f.cash_lost = (int32_t)(cash * sr_rng_range(&r, 20, 50) / 100);
  return f;
}

/* ---- perception ---------------------------------------------------------------------------- */

float sr_player_visibility(float light, float speed, bool crouched, bool camo) {
  float v = 0.05f + 0.95f * SR_CLAMP(light, 0.0f, 1.0f);
  v *= 0.6f + 0.4f * SR_CLAMP(speed / 6.0f, 0.0f, 1.0f) + (speed > 0.1f ? 0.1f : 0.0f);
  if (crouched) v *= 0.6f;
  if (camo) v *= 0.15f;
  return SR_CLAMP(v, 0.0f, 1.0f);
}

bool sr_in_view(const SrEnemy *e, SrVec3 p, float max_dist) {
  SrVec3 d = sr_v3_sub(p, e->pos);
  float dist = sr_v3_len(d);
  if (dist > max_dist) return false;
  if (dist < 1.0f) return true;
  SrVec3 f = sr_dir_from_yaw_pitch(e->yaw, 0);
  return sr_v3_dot(sr_v3_norm(d), f) > 0.5f; /* 120 degree cone */
}

SrAlert sr_perceive(SrEnemy *e, const SrPerceptInput *in, float dt) {
  if (e->dead || e->ko || e->surrendered) return e->alert;
  float range = SR_MIN(in->visibility, 45.0f);
  bool seen = sr_in_view(e, in->player_pos, range);
  if (seen) {
    float dist = sr_v3_len(sr_v3_sub(in->player_pos, e->pos));
    float vis = sr_player_visibility(in->light, in->speed, in->crouched, in->camo);
    float near = 1.0f - SR_CLAMP(dist / range, 0.0f, 1.0f) * 0.8f;
    float rate = vis * near * sr_difficulty(in->difficulty)->perception * (e->heightened ? 1.5f : 1.0f);
    e->awareness += rate * dt;
    e->investigate = in->player_pos;
  } else if (e->alert != SR_ALERT_COMBAT) {
    e->awareness -= 0.05f * dt;
    if (e->awareness < 0) e->awareness = 0;
  }
  if (e->awareness >= COMBAT_AT) {
    e->alert = SR_ALERT_COMBAT;
    e->search_timer = SEARCH_TIME;
  } else if (e->awareness >= SUSPICIOUS_AT && e->alert == SR_ALERT_UNAWARE) {
    e->alert = SR_ALERT_SUSPICIOUS;
  }
  if (e->alert == SR_ALERT_COMBAT && !seen) {
    e->alert = SR_ALERT_SEARCHING;
    e->awareness = SUSPICIOUS_AT;
  }
  if (e->alert == SR_ALERT_SEARCHING || e->alert == SR_ALERT_SUSPICIOUS) {
    if (!seen) {
      e->search_timer -= dt;
      if (e->search_timer <= 0) {
        e->alert = SR_ALERT_UNAWARE;
        e->awareness = 0;
        e->heightened = true;
      }
    } else {
      e->search_timer = SEARCH_TIME;
    }
  }
  return e->alert;
}

void sr_hear_noise(SrEnemy *e, SrVec3 noise_pos, float radius) {
  if (e->dead || e->ko || e->alert == SR_ALERT_COMBAT) return;
  if (sr_v3_len(sr_v3_sub(noise_pos, e->pos)) > radius) return;
  e->investigate = noise_pos;
  if (e->alert == SR_ALERT_UNAWARE) e->alert = SR_ALERT_SUSPICIOUS;
  if (e->awareness < SUSPICIOUS_AT) e->awareness = SUSPICIOUS_AT;
  e->search_timer = SEARCH_TIME;
}

bool sr_enemy_tactics(SrEnemy *e, bool jammed, int allies_down) {
  if (e->dead || e->ko || e->surrendered || e->alert != SR_ALERT_COMBAT) return false;
  float morale = e->morale - 0.2f * (float)allies_down;
  if (morale < 0.2f) {
    e->surrendered = true;
    return false;
  }
  if (!jammed && !e->called_reinforcements) {
    e->called_reinforcements = true;
    return true;
  }
  return false;
}

void sr_body_discovered(SrEnemy *e, SrVec3 body_pos) {
  if (e->dead || e->ko) return;
  e->alert = SR_ALERT_SEARCHING;
  e->awareness = SR_MAX(e->awareness, SUSPICIOUS_AT);
  e->search_timer = SEARCH_TIME * 3;
  e->investigate = body_pos;
  e->heightened = true;
}

const char *sr_alert_name(SrAlert a) {
  static const char *const n[4] = {"unaware", "suspicious", "searching", "combat"};
  return (unsigned)a < 4 ? n[a] : "?";
}

bool sr_enemy_fire(const SrEnemy *e, float distance, SrDifficulty d, uint64_t shot_key) {
  if (e->dead || e->ko || e->surrendered) return false;
  float base = 0.75f - SR_CLAMP(distance / 60.0f, 0.0f, 0.6f);
  float chance = SR_CLAMP(base * sr_difficulty(d)->enemy_accuracy, 0.02f, 0.95f);
  return (float)(sr_mix64(shot_key) % 10000) / 10000.0f < chance;
}
