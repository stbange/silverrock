#include "sim/sr_matrix.h"

#include "core/sr_rng.h"

#include <math.h>
#include <string.h>

/* ---- persona and deck ----------------------------------------------------------------------- */

SrPersona sr_persona_stats(int grade, int mind, int net) {
  SrPersona p;
  memset(&p, 0, sizeof p);
  if (grade <= 0) return p; /* no deck, no persona */
  grade = SR_MIN(grade, SR_DECK_GRADES - 1);
  p.max_integrity = 60.0f + 15.0f * (float)grade + 2.0f * (float)mind;
  p.integrity = p.max_integrity;
  p.firewall = SR_MIN(0.8f, 0.1f + 0.1f * (float)grade + 0.02f * (float)net);
  p.bandwidth = 4.0f + 3.0f * (float)grade + 0.5f * (float)mind + 0.5f * (float)net;
  p.stealth = SR_MIN(0.9f, 0.1f + 0.08f * (float)grade + 0.04f * (float)net);
  p.signal = 25.0f + 15.0f * (float)grade;
  return p;
}

int sr_deck_memory(int grade) { return grade <= 0 ? 0 : 4 + 3 * SR_MIN(grade, SR_DECK_GRADES - 1); }

/* ---- programs --------------------------------------------------------------------------------- */

static const SrProgramDef k_programs[SR_PRG_COUNT] = {
    {"Spike", SR_PK_ATTACK, 2, 1.5f, 25},   {"Crash", SR_PK_ATTACK, 4, 8.0f, 70},
    {"Shield", SR_PK_DEFENCE, 2, 12.0f, 6}, {"Mend", SR_PK_DEFENCE, 2, 15.0f, 30},
    {"Veil", SR_PK_STEALTH, 3, 20.0f, 8},   {"Ghost", SR_PK_STEALTH, 3, 30.0f, 0.4f},
    {"Worm", SR_PK_UTILITY, 3, 10.0f, 0},   {"Sniff", SR_PK_UTILITY, 1, 6.0f, 10},
};

const SrProgramDef *sr_program_def(int p) { return p >= 0 && p < SR_PRG_COUNT ? &k_programs[p] : NULL; }

int sr_loadout_memory(const SrLoadout *l) {
  int m = 0;
  for (int i = 0; i < l->count; i++) m += k_programs[l->prog[i]].memory;
  return m;
}

bool sr_loadout_add(SrLoadout *l, int grade, SrProgram p) {
  if ((unsigned)p >= SR_PRG_COUNT || l->count >= SR_LOADOUT_MAX) return false;
  if (sr_loadout_memory(l) + k_programs[p].memory > sr_deck_memory(grade)) return false;
  l->prog[l->count] = (uint8_t)p;
  l->cooldown[l->count] = 0;
  l->count++;
  return true;
}

bool sr_loadout_use(SrLoadout *l, int slot) {
  if (slot < 0 || slot >= l->count || l->cooldown[slot] > 0) return false;
  l->cooldown[slot] = k_programs[l->prog[slot]].cooldown;
  return true;
}

void sr_loadout_tick(SrLoadout *l, float dt) {
  for (int i = 0; i < l->count; i++) l->cooldown[i] = SR_MAX(0.0f, l->cooldown[i] - dt);
}

int sr_loadout_find(const SrLoadout *l, SrProgram p) {
  for (int i = 0; i < l->count; i++)
    if (l->prog[i] == p) return i;
  return -1;
}

/* ---- breach puzzle -------------------------------------------------------------------------- */

static float wrap(float a) { return sr_wrap_angle(a); }

void sr_breach_generate(SrBreach *b, int security, uint64_t seed) {
  memset(b, 0, sizeof *b);
  security = SR_CLAMP(security, 1, 10);
  SrRng r = sr_rng_stream(seed, "breach", (uint64_t)security, 0);
  b->rings = SR_MIN(SR_BREACH_MAX_RINGS, 2 + security / 2);
  for (int i = 0; i < b->rings; i++) {
    b->angle[i] = (float)sr_rng_range(&r, 60, 600) / 100.0f; /* never aligned at the start */
    float spd = 0.15f + 0.07f * (float)security + (float)sr_rng_range(&r, 0, 10) / 100.0f;
    b->speed[i] = sr_rng_chance(&r, 500) ? spd : -spd;
    b->decoys[i] = SR_MIN(3, security / 3);
    for (int k = 0; k < b->decoys[i]; k++)
      b->decoy_at[i][k] = (float)sr_rng_range(&r, 70, 560) / 100.0f; /* at least 0.7 rad away */
  }
  b->time_limit = 6.0f + 4.0f * (float)b->rings;
  float need = sr_breach_solve_time(b);
  if (need > b->time_limit * 0.9f) b->time_limit = need / 0.9f; /* always solvable */
  b->time_left = b->time_limit;
}

void sr_breach_tick(SrBreach *b, float dt) {
  if (b->done || b->failed) return;
  for (int i = 0; i < b->rings; i++)
    if (!b->locked[i]) b->angle[i] = wrap(b->angle[i] + b->speed[i] * dt);
  b->time_left -= dt;
  if (b->time_left <= 0) {
    b->time_left = 0;
    b->failed = true;
  }
}

void sr_breach_select(SrBreach *b, int delta) { b->sel = ((b->sel + delta) % b->rings + b->rings) % b->rings; }

void sr_breach_select_ring(SrBreach *b, int ring) {
  if (ring >= 0 && ring < b->rings) b->sel = ring;
}

void sr_breach_rotate(SrBreach *b, float radians) {
  if (b->done || b->failed || b->locked[b->sel]) return;
  b->angle[b->sel] = wrap(b->angle[b->sel] + radians);
}

bool sr_breach_aligned(const SrBreach *b, int ring) { return fabsf(wrap(b->angle[ring])) < SR_BREACH_TOLERANCE; }

static bool decoy_aligned(const SrBreach *b, int ring) {
  for (int k = 0; k < b->decoys[ring]; k++)
    if (fabsf(wrap(b->angle[ring] + b->decoy_at[ring][k])) < SR_BREACH_TOLERANCE) return true;
  return false;
}

bool sr_breach_commit(SrBreach *b) {
  if (b->done || b->failed) return b->done;
  int i = b->sel;
  if (b->locked[i]) return false;
  if (!sr_breach_aligned(b, i) || decoy_aligned(b, i)) { /* wrong: lose time, the ring jumps away */
    b->mistakes++;
    b->time_left -= SR_BREACH_PENALTY;
    b->angle[i] = wrap(b->angle[i] + 2.5f);
    if (b->time_left <= 0) {
      b->time_left = 0;
      b->failed = true;
    }
    return false;
  }
  b->locked[i] = true;
  b->angle[i] = 0;
  bool all = true;
  for (int k = 0; k < b->rings; k++) all = all && b->locked[k];
  if (all) {
    b->done = true;
    return true;
  }
  for (int k = 1; k <= b->rings; k++) /* move on to the next open ring */
    if (!b->locked[(i + k) % b->rings]) {
      b->sel = (i + k) % b->rings;
      break;
    }
  return false;
}

float sr_breach_solve_time(const SrBreach *b) {
  /* per ring, worst case half a turn against the drift, plus a moment to commit */
  float t = 0;
  for (int i = 0; i < b->rings; i++)
    if (!b->locked[i]) t += SR_PI / (SR_BREACH_TURN_RATE - fabsf(b->speed[i])) + 0.5f;
  return t;
}

bool sr_breach_autosolve(SrBreach *b, float dt) {
  for (int guard = 0; guard < 1000000 && !b->done && !b->failed; guard++) {
    int ring = b->sel;
    float a = wrap(b->angle[ring]);
    if (fabsf(a) < SR_BREACH_TOLERANCE * 0.5f && !decoy_aligned(b, ring)) {
      sr_breach_commit(b);
      continue;
    }
    float step = SR_MIN(fabsf(a), SR_BREACH_TURN_RATE * dt);
    sr_breach_rotate(b, a > 0 ? -step : step);
    sr_breach_tick(b, dt);
  }
  return b->done;
}

/* ---- ICE -------------------------------------------------------------------------------------- */

const char *sr_ice_name(SrIceType t) {
  static const char *const n[SR_ICE_COUNT] = {"Watcher", "Barrier",   "Tar Pit", "Tracer",
                                              "Killer",  "Black ICE", "Sysop"};
  return (unsigned)t < SR_ICE_COUNT ? n[t] : "?";
}

static void ice_at(SrIce *ice, SrIceType type, const SrHost *h, int node, float hp) {
  memset(ice, 0, sizeof *ice);
  ice->type = (uint8_t)type;
  ice->active = true;
  ice->node = (int16_t)node;
  const SrNode *n = &h->nodes[node];
  sr_enemy_init(&ice->e, sr_v3(n->x, n->y + 1.5f, n->z), -1, hp);
}

int sr_ice_populate(const SrHost *h, uint64_t seed, SrIce *out, int max) {
  SrRng r = sr_rng_stream(seed, "ice", h->building, 0);
  int n = 0;
  int watchers = 1 + h->security / 3;
  for (int k = 0; k < watchers && n < max; k++) {
    int node = sr_rng_range(&r, 1, h->node_count - 1);
    ice_at(&out[n], SR_ICE_WATCHER, h, node, 30);
    out[n].phase = (float)sr_rng_range(&r, 0, 628) / 100.0f;
    n++;
  }
  if (h->security >= 4 && n < max) { /* a barrier seals the core layer junction */
    int j3 = -1;
    for (int i = 0; i < h->node_count; i++)
      if (h->nodes[i].type == SR_NODE_JUNCTION && h->nodes[i].layer == 3) j3 = i;
    ice_at(&out[n++], SR_ICE_BARRIER, h, j3, 60.0f + 10.0f * (float)h->security);
  }
  if (h->security >= 6 && n < max) ice_at(&out[n++], SR_ICE_TARPIT, h, h->monitor, 40);
  return n;
}

int sr_ice_reinforce(const SrHost *h, int alert, uint64_t seed, SrIce *out, int max) {
  SrRng r = sr_rng_stream(seed, "ice-reinforce", h->building, (uint64_t)alert);
  int n = 0;
  if (alert >= 1 && n < max) ice_at(&out[n++], SR_ICE_TRACER, h, h->monitor, 25);
  if (alert >= 1 && n < max) ice_at(&out[n++], SR_ICE_KILLER, h, h->core, 40.0f + 5.0f * (float)h->security);
  if (alert >= 2 && h->security >= 7 && n < max) ice_at(&out[n++], SR_ICE_BLACK, h, h->core, 70);
  if (alert >= 3 && n < max) ice_at(&out[n++], SR_ICE_KILLER, h, sr_rng_range(&r, 1, h->node_count - 1), 50);
  if (alert >= 3 && h->security >= 5 && n < max) ice_at(&out[n++], SR_ICE_SYSOP, h, h->monitor, 90);
  return n;
}

static float dist3(SrVec3 a, SrVec3 b) { return sr_v3_len(sr_v3_sub(a, b)); }

void sr_ice_update(SrIce *ice, const SrHost *h, SrVec3 p, const SrPersona *ps, bool veiled, int security, float dt,
                   SrIceHit *hit) {
  if (!ice->active || ice->e.dead) return;
  const SrNode *home = &h->nodes[ice->node];
  SrVec3 anchor = sr_v3(home->x, home->y + 1.5f, home->z);
  float d = dist3(ice->e.pos, p);
  float sense = (6.0f + 0.5f * (float)security) * (1.0f - ps->stealth * 0.6f) * (veiled ? 0.35f : 1.0f);
  ice->cd -= dt;
  switch ((SrIceType)ice->type) {
    case SR_ICE_WATCHER: /* circles its node, raises the alert when the persona is close */
      ice->phase += dt * 0.8f;
      ice->e.pos = sr_v3(anchor.x + cosf(ice->phase) * 3.0f, anchor.y, anchor.z + sinf(ice->phase) * 3.0f);
      if (d < sense && ice->cd <= 0) {
        hit->detected = true;
        ice->cd = 5.0f;
      }
      break;
    case SR_ICE_BARRIER: break; /* static: seals its node until destroyed */
    case SR_ICE_TARPIT:
      if (d < 7.0f) hit->slow = SR_MAX(hit->slow, 0.6f);
      break;
    case SR_ICE_TRACER: hit->trace += sr_trace_rate(security) * dt * (veiled ? 0.5f : 1.0f); break;
    case SR_ICE_SYSOP: /* a person at a console: hunts like ICE and helps the trace along */
      hit->trace += sr_trace_rate(security) * dt * 0.5f;
      /* fall through */
    case SR_ICE_KILLER:
    case SR_ICE_BLACK: { /* hunts the persona along a straight line, strikes when close */
      SrVec3 to = sr_v3_sub(p, ice->e.pos);
      float speed = ice->type == SR_ICE_BLACK ? 5.0f : 4.0f;
      if (d > 1.5f) ice->e.pos = sr_v3_add(ice->e.pos, sr_v3_scale(sr_v3_norm(to), SR_MIN(d - 1.4f, speed * dt)));
      if (d < 3.0f && ice->cd <= 0) {
        float dmg = (ice->type == SR_ICE_BLACK ? 14.0f : 10.0f) + (float)security;
        hit->integrity += dmg * (1.0f - ps->firewall);
        if (ice->type == SR_ICE_BLACK) hit->body += 6.0f + (float)security * 0.8f;
        ice->cd = 1.2f;
      }
      break;
    }
    default: break; /* GCOVR_EXCL_LINE reason: every ICE type is handled above */
  }
}

bool sr_ice_damage(SrIce *ice, float amount) {
  if (!ice->active || ice->e.dead) return false;
  ice->e.health -= amount;
  if (ice->e.health <= 0) {
    ice->e.dead = true;
    ice->active = false;
    return true;
  }
  return false;
}

float sr_download_time(int mb, const SrPersona *ps, float speed) {
  float bw = ps->bandwidth * (speed > 0 ? speed : 1.0f);
  return bw > 0 ? (float)mb / bw : 1e9f;
}

float sr_discovery_time(int security) { return 1800.0f * (float)(12 - SR_CLAMP(security, 1, 10)); }

float sr_trace_rate(int security) { return 0.012f + 0.004f * (float)SR_CLAMP(security, 1, 10); }
