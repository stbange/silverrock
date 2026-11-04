#include "sim/sr_jobs.h"

#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "world/sr_environ.h"

#include <stdio.h>
#include <string.h>

void sr_jobs_init(SrJobBoard *b) {
  memset(b, 0, sizeof *b);
  b->next_id = 1;
}

const char *sr_job_type_name(SrJobType t) {
  static const char *const n[SR_JT_COUNT] = {"Delivery", "Debt Collection", "Find Item",
                                             "Plant Bug", "Surveillance", "Theft", "Data Theft",
                                             "Sabotage", "Hideout Raid", "Extraction"};
  return (unsigned)t < SR_JT_COUNT ? n[t] : "?";
}

const char *sr_approach_name(SrApproach a) {
  static const char *const n[SR_AP_COUNT] = {"Ghost", "Face", "Runner", "Hammer"};
  return (unsigned)a < SR_AP_COUNT ? n[a] : "?";
}

void sr_tier_pay_range(int tier, int32_t *lo, int32_t *hi) {
  static const int32_t ranges[5][2] = {{40, 200}, {200, 800}, {800, 3000}, {3000, 12000},
                                       {10000, 50000}};
  tier = SR_CLAMP(tier, 0, 4);
  *lo = ranges[tier][0];
  *hi = ranges[tier][1];
}

int sr_job_tier_of_type(SrJobType t) {
  if (t <= SR_JT_FIND) return 0;
  if (t <= SR_JT_THEFT) return 1;
  return 2;
}

int sr_max_tier_for_cred(int32_t cred) {
  SrCredTier t = sr_cred_tier(cred);
  /* T4 black ops additionally need the patron (story campaign, M5); not offered here. */
  return t >= SR_CRED_OPERATOR ? 3 : (int)t;
}

static bool site_job(SrJobType t) {
  return t == SR_JT_PLANT || t == SR_JT_THEFT || t == SR_JT_STEAL_DATA || t == SR_JT_SABOTAGE ||
         t == SR_JT_RAID || t == SR_JT_EXTRACT;
}

static int pick_building(const SrCity *c, SrRng *r, int poi, int owner_kind, int exclude_owner) {
  for (int tries = 0; tries < 4000; tries++) {
    int b = sr_rng_range(r, 0, c->building_count - 1);
    const SrBuilding *bd = &c->buildings[b];
    if (b == c->home_building) continue;
    if (poi >= 0 && bd->poi != poi) continue;
    if (owner_kind >= 0) {
      if (bd->owner < 0 || (int)c->factions.f[bd->owner].kind != owner_kind) continue;
      if (bd->owner == exclude_owner) continue;
    }
    return b;
  }
  return -1;
}

static int rival_of(const SrCity *c, int faction, SrRng *r) {
  const SrFactions *fs = &c->factions;
  int best = -1, best_rel = 1000;
  for (int i = 0; i < fs->corp_count + fs->gang_count + fs->gang_first - fs->corp_count; i++) {
    int f = i < fs->corp_count ? i : fs->gang_first + (i - fs->corp_count);
    if (f == faction || f >= fs->count) continue;
    int rel = fs->rel[f][faction] + sr_rng_range(r, -10, 10);
    if (rel < best_rel) {
      best_rel = rel;
      best = f;
    }
  }
  return best;
}

bool sr_job_generate(const SrCity *c, SrJobType type, uint64_t key, int64_t t, SrJob *j) {
  memset(j, 0, sizeof *j);
  SrRng r = sr_rng_stream(c->seed, "job", key, (uint64_t)type);
  j->type = (uint8_t)type;
  j->tier = (uint8_t)sr_job_tier_of_type(type);
  j->state = SR_JS_OFFERED;
  j->client_faction = -1;
  j->target_faction = -1;
  j->offered = t;
  j->dest_building = -1;
  const SrFactions *fs = &c->factions;
  switch (type) {
  case SR_JT_DELIVER:
    j->target_building = pick_building(c, &r, -1, -1, -1);
    j->dest_building = j->target_building;
    while (c->buildings[j->target_building].poi == SR_POI_NONE ||
           c->buildings[j->target_building].poi >= SR_POI_FIRST_SPECIAL)
      j->target_building = j->dest_building = pick_building(c, &r, -1, -1, -1);
    if (sr_rng_chance(&r, 400)) j->client_faction = (int8_t)(fs->gang_first + sr_rng_range(&r, 0, fs->gang_count - 1));
    break;
  case SR_JT_COLLECT:
  case SR_JT_FIND:
  case SR_JT_SURVEIL:
    j->target_building = pick_building(c, &r, SR_POI_NONE, -1, -1);
    if (type == SR_JT_SURVEIL) {
      int tb = pick_building(c, &r, SR_POI_CORP_FACILITY, SR_FK_CORP, -1);
      j->target_building = tb;
      j->target_faction = c->buildings[tb].owner;
      j->client_faction = (int8_t)rival_of(c, j->target_faction, &r);
    } else if (sr_rng_chance(&r, 500)) {
      j->client_faction = (int8_t)(fs->gang_first + sr_rng_range(&r, 0, fs->gang_count - 1));
    }
    break;
  case SR_JT_RAID: {
    int g = fs->gang_first + sr_rng_range(&r, 0, fs->gang_count - 1);
    j->target_building = fs->f[g].hq_building;
    j->target_faction = (int8_t)g;
    j->client_faction = (int8_t)rival_of(c, g, &r);
    break;
  }
  default: { /* corporate sites */
    int b = pick_building(c, &r, sr_rng_chance(&r, 800) ? SR_POI_CORP_FACILITY : SR_POI_CORP_HQ,
                          SR_FK_CORP, -1);
    j->target_building = b;
    j->target_faction = c->buildings[b].owner;
    j->client_faction = (int8_t)rival_of(c, j->target_faction, &r);
    break;
  }
  }
  if (j->target_building < 0) return false; /* GCOVR_EXCL_LINE reason: cities always have targets */
  if (j->client_faction >= 0 && j->client_faction == j->target_faction) j->client_faction = -1;
  int32_t lo, hi;
  sr_tier_pay_range(j->tier, &lo, &hi);
  j->pay = sr_rng_range(&r, lo, hi);
  j->upfront = j->tier == 0 ? 0 : j->pay / 4;
  j->deadline = t + (int64_t)(j->tier + 1) * SR_DAY_S;
  if (j->tier >= 1) {
    if (sr_rng_chance(&r, 300)) j->constraints |= SR_JC_NO_KILLS;
    if (sr_rng_chance(&r, 200)) j->constraints |= SR_JC_NO_ALARMS;
    j->constraint_fails = sr_rng_chance(&r, 400);
  }
  if (type == SR_JT_THEFT || type == SR_JT_EXTRACT) j->constraints |= SR_JC_ITEM_INTACT;
  j->has_bonus = sr_rng_chance(&r, 400);
  j->bonus = j->has_bonus ? j->pay / 4 : 0;
  j->approaches = sr_job_validate_approaches(c, j);
  const char *who = j->client_faction >= 0 ? fs->f[j->client_faction].name : "A private client";
  snprintf(j->title, sizeof j->title, "%s for %s", sr_job_type_name(type), who);
  return true;
}

uint8_t sr_job_validate_approaches(const SrCity *c, const SrJob *j) {
  if (!site_job((SrJobType)j->type)) {
    /* street jobs: sneak past, talk your way through, or push through */
    return (1u << SR_AP_GHOST) | (1u << SR_AP_FACE) | (1u << SR_AP_HAMMER);
  }
  SrInterior in;
  sr_interior_generate(c, j->target_building, &in);
  uint8_t m = 0;
  for (int e = 0; e < in.entry_count; e++) {
    SrEntryType et = in.entries[e].type;
    bool crawl = et == SR_ENTRY_VENT || et == SR_ENTRY_ROOF || et == SR_ENTRY_WINDOW;
    if (crawl && sr_interior_reachable(&in, in.entries[e].room, in.target_room, true, false))
      m |= 1u << SR_AP_GHOST;
  }
  const SrBuilding *b = &c->buildings[j->target_building];
  if (b->owner >= 0 || b->poi < SR_POI_FIRST_SPECIAL) m |= 1u << SR_AP_FACE; /* uniforms, IDs */
  if ((b->flags & SR_BF_NETWORKED) && sr_interior_security_chain_ok(&in) &&
      sr_interior_reachable(&in, in.entries[0].room, in.target_room, false, true))
    m |= 1u << SR_AP_RUNNER;
  if (sr_interior_reachable(&in, in.entries[0].room, in.target_room, false, true) &&
      in.guards <= 4 + 3 * (j->tier + 1))
    m |= 1u << SR_AP_HAMMER;
  return m;
}

SrJob *sr_jobs_get(SrJobBoard *b, int32_t id) {
  for (int i = 0; i < b->count; i++)
    if (b->jobs[i].id == id) return &b->jobs[i];
  return NULL;
}

static int count_bits(unsigned v) {
  int n = 0;
  for (; v; v >>= 1) n += (int)(v & 1u);
  return n;
}

static SrJob *board_push(SrJobBoard *b, const SrJob *j) {
  if (b->count >= SR_MAX_JOBS) {
    /* drop the oldest job that is no longer active */
    int victim = -1;
    for (int i = 0; i < b->count && victim < 0; i++)
      if (b->jobs[i].state != SR_JS_ACCEPTED) victim = i;
    if (victim < 0) return NULL;
    memmove(&b->jobs[victim], &b->jobs[victim + 1], sizeof(SrJob) * (size_t)(b->count - victim - 1));
    b->count--;
  }
  SrJob *slot = &b->jobs[b->count++];
  *slot = *j;
  slot->id = b->next_id++;
  return slot;
}

static SrJobType pick_type(SrRng *r, int max_tier) {
  static const SrJobType by_tier[3][4] = {
      {SR_JT_DELIVER, SR_JT_COLLECT, SR_JT_FIND, SR_JT_DELIVER},
      {SR_JT_PLANT, SR_JT_SURVEIL, SR_JT_THEFT, SR_JT_SURVEIL},
      {SR_JT_STEAL_DATA, SR_JT_SABOTAGE, SR_JT_RAID, SR_JT_EXTRACT}};
  int tier = sr_rng_range(r, 0, SR_MIN(max_tier, 2));
  return by_tier[tier][sr_rng_range(r, 0, 3)];
}

static bool make_job(const SrCity *c, SrRng *r, int max_tier, uint64_t key, int64_t t, SrJob *j) {
  for (int attempt = 0; attempt < 8; attempt++) {
    SrJobType type = pick_type(r, max_tier);
    if (!sr_job_generate(c, type, key * 16u + (uint64_t)attempt, t, j)) continue; /* GCOVR_EXCL_LINE */
    if (count_bits(j->approaches) < 2) continue; /* never offer a job without a choice */
    if (max_tier >= 3 && j->tier == 2 && sr_rng_chance(r, 400)) { /* Operator-level variant */
      int32_t lo, hi;
      j->tier = 3;
      sr_tier_pay_range(3, &lo, &hi);
      j->pay = sr_rng_range(r, lo, hi);
      j->upfront = j->pay / 4;
      j->bonus = j->has_bonus ? j->pay / 4 : 0;
    }
    return true;
  }
  return false;
}

int sr_jobs_fixer_offers(SrJobBoard *b, const SrCity *c, int fixer, int64_t t, int32_t cred,
                         int out_ids[4]) {
  int64_t day = t / SR_DAY_S;
  SrRng r = sr_rng_stream(c->seed, "fixer", (uint64_t)fixer, (uint64_t)day);
  /* already offered today? return the existing offers */
  int n = 0;
  bool offered_today = false;
  for (int i = 0; i < b->count; i++) {
    SrJob *j = &b->jobs[i];
    if (j->source != SR_SRC_FIXER || j->client_building != fixer || j->offered / SR_DAY_S != day)
      continue;
    offered_today = true; /* one batch per fixer per day */
    if (j->state == SR_JS_OFFERED && n < 4) out_ids[n++] = j->id;
  }
  if (offered_today) return n;
  int want = sr_rng_range(&r, 1, 4);
  int max_tier = sr_max_tier_for_cred(cred);
  for (int k = 0; k < want; k++) {
    SrJob j;
    uint64_t key = sr_hash_combine((uint64_t)fixer * 1000u + (uint64_t)k, (uint64_t)day);
    if (!make_job(c, &r, max_tier, key, t, &j)) continue; /* GCOVR_EXCL_LINE */
    j.source = SR_SRC_FIXER;
    j.client_building = fixer;
    SrJob *s = board_push(b, &j);
    if (s) out_ids[n++] = s->id;
  }
  return n;
}

int sr_jobs_board_refresh(SrJobBoard *b, const SrCity *c, int64_t t, int32_t cred) {
  int64_t day = t / SR_DAY_S;
  for (int i = 0; i < b->count; i++)
    if (b->jobs[i].source == SR_SRC_BOARD && b->jobs[i].offered / SR_DAY_S == day) return 0;
  SrRng r = sr_rng_stream(c->seed, "jobboard", (uint64_t)day, 0);
  int max_tier = SR_MIN(sr_max_tier_for_cred(cred), 1);
  int added = 0;
  for (int k = 0; k < 4; k++) {
    SrJob j;
    if (!make_job(c, &r, max_tier, sr_hash_combine(0xB0A2Du, (uint64_t)(day * 8 + k)), t, &j))
      continue; /* GCOVR_EXCL_LINE */
    j.source = SR_SRC_BOARD;
    j.client_building = c->home_building; /* report via terminal */
    if (board_push(b, &j)) added++;
  }
  return added;
}

SrResult sr_job_accept(SrJob *j, SrPlayer *p, int64_t t) {
  if (j->state != SR_JS_OFFERED) return SR_ERR_INVALID;
  if (j->tier > sr_max_tier_for_cred(p->prog.street_cred)) return SR_ERR_DENIED;
  j->state = SR_JS_ACCEPTED;
  if (j->upfront > 0) {
    sr_player_tx(p, t, j->upfront, "job advance");
    j->paid += j->upfront;
  }
  if (j->type == SR_JT_DELIVER) {
    SrItem pkg = sr_item_simple(SR_IK_JOB, 0, 1);
    pkg.job = (int16_t)j->id;
    sr_inv_add(&p->inv, pkg);
  }
  return SR_OK;
}

int32_t sr_job_intel_price(const SrJob *j) { return SR_MAX(20, j->pay / 10); }

SrResult sr_job_buy_intel(SrJob *j, SrPlayer *p, int64_t t) {
  if (j->intel == 0xF) return SR_ERR_INVALID;
  int32_t price = sr_job_intel_price(j);
  if (p->cred < price) return SR_ERR_DENIED;
  sr_player_tx(p, t, -price, "intel");
  j->intel = 0xF;
  return SR_OK;
}

SrResult sr_job_negotiate(SrJob *j, const SrPlayer *p, uint64_t seed) {
  if (j->negotiated || j->state != SR_JS_OFFERED) return SR_ERR_INVALID;
  j->negotiated = true;
  SrRng r = sr_rng_stream(seed, "negotiate", (uint64_t)j->id, 0);
  int rank = p->prog.skill_rank[SR_SK_PERSUASION];
  int chance = 25 + rank * 3 + p->prog.attr[SR_ATTR_COOL] * 3;
  if (sr_rng_range(&r, 0, 99) < chance) {
    int pct = SR_MIN(30, 10 + rank);
    j->pay += j->pay * pct / 100;
    if (j->upfront) j->upfront = j->pay / 4;
    return SR_OK;
  }
  if (sr_rng_chance(&r, 500)) {
    j->pay -= j->pay * 15 / 100;
    if (j->upfront) j->upfront = j->pay / 4;
  } else {
    j->state = SR_JS_WITHDRAWN;
  }
  return SR_ERR_DENIED;
}

static int weapon_skill(const SrPlayer *p, float *damage, bool *lethal, uint64_t city_seed,
                        SrSkill *which) {
  *damage = 5.0f;
  *lethal = false;
  *which = SR_SK_MELEE;
  if (p->equipped < 0 || p->equipped >= p->inv.count) return p->prog.skill_rank[SR_SK_MELEE];
  const SrItem *w = &p->inv.items[p->equipped];
  if (w->kind != SR_IK_WEAPON) return p->prog.skill_rank[SR_SK_MELEE];
  SrWeaponStats s = sr_weapon_stats(city_seed, w);
  *damage = s.damage;
  *lethal = s.lethal;
  static const SrSkill by_cat[SR_WC_COUNT] = {SR_SK_PISTOLS, SR_SK_LONGGUNS, SR_SK_HEAVY,
                                              SR_SK_PISTOLS, SR_SK_MELEE};
  *which = by_cat[sr_archetype(w->type)->category];
  return p->prog.skill_rank[*which];
}

SrMissionResult sr_job_execute(const SrCity *c, SrJob *j, SrPlayer *p, SrApproach a, uint64_t seed) {
  SrMissionResult res;
  memset(&res, 0, sizeof res);
  if (j->state != SR_JS_ACCEPTED || j->executed) return res;
  SrRng r = sr_rng_stream(seed, "mission", (uint64_t)j->id, (uint64_t)a);
  res.seconds = (int64_t)(j->tier + 1) * 3600 + sr_rng_range(&r, 0, 3600 * (j->tier + 1));
  if (!(j->approaches & (1u << a))) {
    res.alarm = true; /* no way in like that */
    return res;
  }
  int guards = 1, cams = 0;
  if (site_job((SrJobType)j->type)) {
    SrInterior in;
    sr_interior_generate(c, j->target_building, &in);
    guards = in.guards;
    for (int d = 0; d < in.device_count; d++) cams += in.devices[d].type == SR_DEV_CAMERA;
  }
  int difficulty = j->tier * 12 + guards * 5 + cams * 3;
  const int8_t *at = p->prog.attr;
  const int8_t *sk = p->prog.skill_rank;
  float wdmg;
  bool lethal;
  SrSkill wskill_id;
  int wskill = weapon_skill(p, &wdmg, &lethal, c->seed, &wskill_id);
  int skill = 0;
  SrSkill used = SR_SK_STEALTH;
  switch (a) {
  case SR_AP_GHOST:
    skill = sk[SR_SK_STEALTH] * 4 + at[SR_ATTR_COOL] * 3 +
            (sr_inv_count(&p->inv, SR_IK_GADGET, SR_GD_CAMO) ? 15 : 0);
    used = SR_SK_STEALTH;
    break;
  case SR_AP_FACE:
    skill = sk[SR_SK_PERSUASION] * 4 + at[SR_ATTR_COOL] * 3 +
            (sr_inv_count(&p->inv, SR_IK_GADGET, SR_GD_FAKEID) ? 15 : 0);
    used = SR_SK_PERSUASION;
    break;
  case SR_AP_RUNNER: {
    int deck = sr_inv_find(&p->inv, SR_IK_DECK, -1);
    skill = deck < 0 ? -100 : sk[SR_SK_NETRUNNING] * 4 + at[SR_ATTR_MIND] * 3 + p->inv.items[deck].type * 10;
    used = SR_SK_NETRUNNING;
    break;
  }
  default:
    skill = wskill * 4 + at[SR_ATTR_BODY] * 2 + at[SR_ATTR_REFLEX] * 2 + (int)(wdmg / 2.0f);
    used = wskill_id;
    break;
  }
  if (!site_job((SrJobType)j->type) && a != SR_AP_HAMMER) {
    skill += sk[SR_SK_STREETWISE] * 3 + 10; /* street jobs are about knowing people */
  }
  int chance = SR_CLAMP(55 + skill - difficulty, 5, 95);
  bool ok = sr_rng_range(&r, 0, 99) < chance;
  if (a == SR_AP_HAMMER) {
    res.alarm = site_job((SrJobType)j->type);
    res.success = ok || sr_rng_chance(&r, 500);
    int downed = res.success ? guards : guards / 2;
    if (lethal) res.kills = downed; else res.knockouts = downed;
    res.damage = (float)SR_MAX(5, difficulty - skill / 2) * (ok ? 0.5f : 1.0f);
  } else if (ok) {
    res.success = true;
  } else {
    /* blown: a scramble out, possibly with violence */
    res.alarm = site_job((SrJobType)j->type);
    res.damage = (float)SR_MAX(5, difficulty / 2);
    res.success = sr_rng_chance(&r, 350);
    if (res.alarm && lethal && sr_rng_chance(&r, 500)) res.kills = 1;
  }
  res.witnessed = res.alarm || (a == SR_AP_FACE && sr_rng_chance(&r, 150));
  res.camera = cams > 0 && res.alarm;
  /* skills grow with real use */
  sr_progress_use_skill(&p->prog, used, res.success ? SR_USE_SUCCESS : SR_USE_HOSTILE,
                        10 + 5 * j->tier);
  p->health -= res.damage;
  if (p->health < 1.0f) p->health = 1.0f; /* the game layer handles flatlining from combat */
  p->kills += res.kills;
  p->knockouts += res.knockouts;
  j->kills += res.kills;
  j->alarms += res.alarm ? 1 : 0;
  if (res.success) {
    j->executed = true;
    if (j->type == SR_JT_THEFT || j->type == SR_JT_FIND || j->type == SR_JT_EXTRACT ||
        j->type == SR_JT_STEAL_DATA) {
      SrItem it = sr_item_simple(j->type == SR_JT_STEAL_DATA ? SR_IK_PAYDATA : SR_IK_JOB, j->tier, 1);
      it.job = (int16_t)j->id;
      if (j->type == SR_JT_THEFT) it.flags |= SR_IF_STOLEN;
      sr_inv_add(&p->inv, it);
    }
    if (j->type == SR_JT_DELIVER) {
      for (int i = 0; i < p->inv.count; i++)
        if (p->inv.items[i].kind == SR_IK_JOB && p->inv.items[i].job == j->id) {
          sr_inv_remove_at(&p->inv, i, 1);
          break;
        }
    }
  }
  return res;
}

static void remove_job_items(SrPlayer *p, int32_t id) {
  for (int i = p->inv.count - 1; i >= 0; i--)
    if ((p->inv.items[i].kind == SR_IK_JOB || p->inv.items[i].kind == SR_IK_PAYDATA) &&
        p->inv.items[i].job == id)
      sr_inv_remove_at(&p->inv, i, p->inv.items[i].count);
}

static void job_event(SrEventLog *log, const SrCity *c, const SrJob *j, SrEventType type,
                      int64_t t, bool attributed) {
  if (!log) return;
  SrWorldEvent ev;
  memset(&ev, 0, sizeof ev);
  ev.type = type;
  ev.time = t;
  ev.building = j->target_building;
  ev.district = (int8_t)c->blocks[c->buildings[j->target_building].block].district;
  ev.victim_faction = j->target_faction;
  ev.blamed_faction = (int8_t)(attributed ? -2 : -1);
  ev.significant = j->tier >= 1;
  snprintf(ev.text, sizeof ev.text, "the %s at %s",
           sr_job_type_name((SrJobType)j->type), c->buildings[j->target_building].name[0]
               ? c->buildings[j->target_building].name : c->districts[ev.district].name);
  sr_events_add(log, &ev);
}

int32_t sr_job_complete(SrJob *j, SrPlayer *p, int64_t t, SrEventLog *log, const SrCity *c) {
  if (j->state != SR_JS_ACCEPTED || !j->executed) return 0;
  bool violated = ((j->constraints & SR_JC_NO_KILLS) && j->kills > 0) ||
                  ((j->constraints & SR_JC_NO_ALARMS) && j->alarms > 0);
  remove_job_items(p, j->id);
  if (violated && j->constraint_fails) {
    j->state = SR_JS_FAILED;
    sr_progress_standing(&p->prog, j->client_faction, -10, true);
    job_event(log, c, j, SR_EV_JOB_FAILED, t, false);
    return 0;
  }
  int32_t pay = j->pay - j->upfront;
  if (violated) pay /= 2;
  if (j->has_bonus && j->alarms == 0) {
    pay += j->bonus;
    j->bonus_done = true;
  }
  sr_player_tx(p, t, pay, "job payment");
  j->paid += pay;
  j->state = SR_JS_COMPLETED;
  sr_progress_add_xp(&p->prog, 60 * (j->tier + 1));
  /* Street Cred grows only with work at the edge of what you can do */
  if (j->tier >= sr_max_tier_for_cred(p->prog.street_cred)) sr_progress_add_cred(&p->prog, j->tier + 1);
  sr_progress_standing(&p->prog, j->client_faction, 5, true);
  SrEventType et = j->type == SR_JT_SABOTAGE ? SR_EV_SABOTAGE
                   : j->type == SR_JT_THEFT || j->type == SR_JT_STEAL_DATA ? SR_EV_THEFT
                                                                         : SR_EV_JOB_DONE;
  job_event(log, c, j, et, t, j->alarms > 0);
  return pay;
}

int sr_jobs_tick(SrJobBoard *b, SrPlayer *p, int64_t t, SrEventLog *log) {
  int failed = 0;
  for (int i = 0; i < b->count; i++) {
    SrJob *j = &b->jobs[i];
    if (j->state == SR_JS_ACCEPTED && t > j->deadline) {
      j->state = SR_JS_FAILED;
      remove_job_items(p, j->id);
      sr_progress_standing(&p->prog, j->client_faction, -10, true);
      b->failed++;
      failed++;
      if (log) {
        SrWorldEvent ev = {SR_EV_JOB_FAILED, t, -1, j->target_building, j->target_faction, -1, false, "missed deadline"};
        sr_events_add(log, &ev);
      }
    } else if (j->state == SR_JS_OFFERED && t > j->deadline) {
      j->state = SR_JS_WITHDRAWN;
    }
  }
  return failed;
}

SrResult sr_job_abandon(SrJob *j, SrPlayer *p, int64_t t, SrEventLog *log) {
  if (j->state != SR_JS_ACCEPTED) return SR_ERR_INVALID;
  j->deadline = t - 1;
  SrJobBoard tmp;
  tmp.count = 1;
  tmp.jobs[0] = *j;
  tmp.failed = 0;
  sr_jobs_tick(&tmp, p, t, log);
  *j = tmp.jobs[0];
  return SR_OK;
}

void sr_job_briefing(const SrCity *c, const SrJob *j, char *out, size_t cap) {
  char known[160] = "";
  if (site_job((SrJobType)j->type)) {
    SrInterior in;
    sr_interior_generate(c, j->target_building, &in);
    int cams = 0;
    for (int d = 0; d < in.device_count; d++) cams += in.devices[d].type == SR_DEV_CAMERA;
    char g[24] = "guards: unknown", cm[24] = "cameras: unknown";
    if (j->intel & SR_INTEL_GUARDS) snprintf(g, sizeof g, "guards: %d", in.guards);
    if (j->intel & SR_INTEL_CAMERAS) snprintf(cm, sizeof cm, "cameras: %d", cams);
    snprintf(known, sizeof known, "%s, %s, entries: %s", g, cm,
             (j->intel & SR_INTEL_ENTRANCE) ? "front, service, roof, vent" : "front known");
  } else {
    snprintf(known, sizeof known, "street job, low risk");
  }
  const SrBuilding *b = &c->buildings[j->target_building];
  int d = c->blocks[b->block].district;
  snprintf(out, cap, "[T%d] %s. Target: %s (%s). Pay %d (+%d bonus). %s%s%s", j->tier, j->title,
           b->name[0] ? b->name : "a building", c->districts[d].name, j->pay, j->bonus, known,
           (j->constraints & SR_JC_NO_KILLS) ? ". No kills" : "",
           (j->constraints & SR_JC_NO_ALARMS) ? ". No alarms" : "");
}
