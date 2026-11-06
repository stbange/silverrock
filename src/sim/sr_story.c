/* The story campaign on top of the corporate game: the patron and its black-ops handler, the
 * prologue test and the retainer, trust and betrayal, the acts, rival offers and defection,
 * procedural arcs with branching beats, the authored cast and its successors, the endgame
 * director and the five endings. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"
#include "sim/sr_progress.h"

#include <stdio.h>
#include <string.h>

const char *sr_arc_name(SrArcType t) {
  static const char *const n[SR_ARC_COUNT] = {"Hostile Takeover", "The Prototype",   "Election Season", "Proxy War",
                                              "The Defector",     "Supply Shortage", "The Mole",        "Scandal"};
  return (unsigned)t < SR_ARC_COUNT ? n[t] : "?";
}

const char *sr_ending_name(SrEnding e) {
  static const char *const n[] = {"-", "Loyal", "Defector", "Burn the Board", "Kingmaker", "Sunrise"};
  return (unsigned)e <= SR_END_SUNRISE ? n[e] : "?";
}

const char *sr_scenario_name(SrScenario s) {
  static const char *const n[] = {"-", "The Merger", "The Coup", "The Pall Protocol", "The Ghost"};
  return (unsigned)s <= SR_SC_GHOST ? n[s] : "?";
}

const char *sr_cast_role_name(SrCastRole r) {
  static const char *const n[SR_CAST_COUNT] = {"landlord",     "fixer",      "neighbour", "handler",
                                               "rival runner", "journalist", "the Ghost"};
  return (unsigned)r < SR_CAST_COUNT ? n[r] : "?";
}

static void cast_name(SrStory *s, uint64_t seed, int role) {
  sr_name_person(seed, sr_hash_combine(0x5ca57u + (uint64_t)role, s->cast[role].generation), s->cast[role].name,
                 sizeof s->cast[role].name);
}

void sr_story_init(SrStory *s, const SrCity *c, uint64_t seed, int patron_choice) {
  memset(s, 0, sizeof *s);
  int n = SR_MAX(1, c->factions.corp_count);
  if (patron_choice >= 0 && patron_choice < c->factions.corp_count)
    s->patron = (int8_t)patron_choice;
  else
    s->patron = (int8_t)(sr_mix64(sr_hash_combine(seed, 0x9a720u)) % (uint64_t)n);
  s->first_patron = s->patron;
  s->rival_offer = -1;
  s->hostile = -1;
  s->trust = 30;
  s->test_job = -1;
  s->scenario_corp = -1;
  s->scenario_other = -1;
  for (int r = 0; r < SR_CAST_COUNT; r++) {
    s->cast[r].alive = true;
    cast_name(s, seed, r);
  }
}

void sr_story_trust(SrStory *s, int delta) { s->trust = SR_CLAMP(s->trust + delta, 0, 100); }

bool sr_story_t4_allowed(const SrStory *s, int tier) {
  return s->retainer && s->patron >= 0 && tier >= SR_CRED_KNIGHT && s->trust >= SR_TRUST_T4;
}

void sr_story_test_done(SrStory *s, int64_t now) {
  if (s->retainer || s->offer_open) return;
  if (s->offer_at == 0 || s->offer_at > now + 86400) s->offer_at = now + 86400;
}

void sr_story_accept_retainer(SrStory *s, int64_t now) {
  if (s->retainer || s->patron < 0) return;
  s->retainer = true;
  s->offer_open = false;
  s->act = SR_ACT_PAWN;
  s->act_started = now;
  s->next_offer = now;
  sr_story_trust(s, 10);
}

/* ---- arcs ---------------------------------------------------------------------------------- */

/* Operation per beat: [beat][branch]; branch 1 follows a success of the previous beat. */
static const uint8_t ARC_OPS[SR_ARC_COUNT][3][2] = {
    {{SR_OP_THEFT, SR_OP_THEFT}, {SR_OP_SMEAR, SR_OP_TAKEOVER}, {SR_OP_SABOTAGE, SR_OP_TAKEOVER}},       /* takeover */
    {{SR_OP_THEFT, SR_OP_THEFT}, {SR_OP_THEFT, SR_OP_SABOTAGE}, {SR_OP_RETALIATE, SR_OP_SMEAR}},         /* prototype */
    {{SR_OP_BRIBE, SR_OP_BRIBE}, {SR_OP_BRIBE, SR_OP_SMEAR}, {SR_OP_SMEAR, SR_OP_BRIBE}},                /* election */
    {{SR_OP_TURF, SR_OP_TURF}, {SR_OP_SABOTAGE, SR_OP_TURF}, {SR_OP_TURF, SR_OP_SABOTAGE}},              /* proxy war */
    {{SR_OP_THEFT, SR_OP_THEFT}, {SR_OP_THEFT, SR_OP_SABOTAGE}, {SR_OP_SMEAR, SR_OP_RETALIATE}},         /* defector */
    {{SR_OP_SABOTAGE, SR_OP_SABOTAGE}, {SR_OP_SMEAR, SR_OP_SABOTAGE}, {SR_OP_SABOTAGE, SR_OP_TAKEOVER}}, /* shortage */
    {{SR_OP_THEFT, SR_OP_THEFT}, {SR_OP_THEFT, SR_OP_SMEAR}, {SR_OP_THEFT, SR_OP_RETALIATE}},            /* mole */
    {{SR_OP_THEFT, SR_OP_THEFT}, {SR_OP_BRIBE, SR_OP_SMEAR}, {SR_OP_THEFT, SR_OP_SMEAR}},                /* scandal */
};

static int rival_of(const SrCity *c, int a, uint64_t key) {
  int best = -1, bv = 1000;
  for (int k = 0; k < c->factions.corp_count; k++) {
    if (k == a) continue;
    int v = c->factions.rel[a][k] + (int)(sr_mix64(key + (uint64_t)k) % 30u);
    if (v < bv) bv = v, best = k;
  }
  return best;
}

static int pick_facility(const SrCity *c, int f, uint64_t key, bool not_hq) {
  if (f < 0 || f >= c->factions.count) return -1;
  const SrFaction *fa = &c->factions.f[f];
  if (fa->facility_count > 0) return fa->facilities[sr_mix64(key) % fa->facility_count];
  return not_hq ? -1 : fa->hq_building;
}

static int pick_person(const SrPopulation *pop, int f, uint64_t key) {
  if (!pop) return -1;
  int n = 0;
  for (int i = 0; i < pop->count; i++)
    if (pop->npcs[i].faction == f && pop->npcs[i].status == SR_NPC_ALIVE) n++;
  if (n == 0) return -1;
  int want = (int)(sr_mix64(key) % (uint64_t)n);
  for (int i = 0; i < pop->count; i++)
    if (pop->npcs[i].faction == f && pop->npcs[i].status == SR_NPC_ALIVE && want-- == 0)
      return (int32_t)pop->npcs[i].id;
  return -1; /* GCOVR_EXCL_LINE reason: the count above guarantees a match */
}

static void beat_start(SrChess *ch, SrCity *c, int idx, int64_t now) {
  SrArc *a = &ch->story.arcs[idx];
  int branch = a->beat == 0 ? 0 : (a->path >> (a->beat - 1)) & 1;
  SrOpType t = (SrOpType)ARC_OPS[a->type][a->beat][branch];
  int client = a->a, target = a->b, building = a->building, district = -1;
  if (t == SR_OP_BRIBE) {
    district = (int)(sr_mix64(ch->seed + (uint64_t)a->started + a->beat) % (uint64_t)SR_MAX(1, ch->pol.seat_count));
    target = c->factions.gov;
    building = sr_chess_city_hall(c);
  } else if (t == SR_OP_TURF) {
    district = -1;
    for (int d = 0; d < c->district_count && district < 0; d++)
      if (target >= 0 && (c->factions.f[target].turf & (1u << d))) district = d;
    building = target >= 0 ? c->factions.f[target].hq_building : -1;
    if (district < 0) t = SR_OP_SABOTAGE; /* the rival gang is gone: hit what is left */
  } else if (t == SR_OP_TAKEOVER && (building < 0 || (target >= 0 && building == c->factions.f[target].hq_building))) {
    t = SR_OP_SABOTAGE;
  }
  int id = sr_chess_start_op(ch, c, t, client, target, building, district, now);
  SrOperation *op = sr_chess_op(ch, id);
  if (op) op->arc = (int16_t)idx;
  a->op = id;
}

static int arc_start(SrChess *ch, SrCity *c, const SrPopulation *pop, SrArcType t, int fa, int fb, int64_t now) {
  SrStory *s = &ch->story;
  int idx = -1;
  for (int i = 0; i < SR_MAX_ARCS && idx < 0; i++)
    if (!s->arcs[i].active) idx = i;
  if (idx < 0) return -1;
  uint64_t key = sr_hash_combine(ch->seed, (uint64_t)s->arcs_started * 977u + (uint64_t)t);
  int a = fa, b = fb;
  if (t == SR_ARC_PROXY && a < 0) { /* two gangs fight for two corps */
    for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++) {
      if (ch->f[g].dissolved || c->factions.f[g].turf == 0) continue;
      if (a < 0)
        a = g;
      else if (b < 0)
        b = g;
    }
    if (b < 0) t = SR_ARC_SHORTAGE, a = -1;
  }
  if (a >= 0 && b < 0) b = rival_of(c, a, key);
  if (a < 0) {
    /* prefer the patron's arcs */
    if (s->patron >= 0 && sr_mix64(key) % 10u < 6u)
      a = s->patron;
    else
      a = (int)(sr_mix64(key + 1u) % (uint64_t)SR_MAX(1, c->factions.corp_count));
    b = rival_of(c, a, key);
  }
  if (b < 0) return -1; /* GCOVR_EXCL_LINE reason: cities have several corps */
  SrArc *arc = &s->arcs[idx];
  memset(arc, 0, sizeof *arc);
  arc->type = (uint8_t)t;
  arc->active = true;
  arc->a = (int8_t)a;
  arc->b = (int8_t)b;
  arc->building = pick_facility(c, b, key + 2u, t == SR_ARC_TAKEOVER);
  if (arc->building < 0) arc->building = c->factions.f[b].hq_building;
  arc->person = pick_person(pop, b, key + 3u);
  arc->beats = 3;
  arc->started = now;
  s->arcs_started++;
  char msg[72];
  snprintf(msg, sizeof msg, "Arc: %s (%s vs %s)", sr_arc_name(t), c->factions.f[a].name, c->factions.f[b].name);
  sr_chess_note(ch, now, msg);
  beat_start(ch, c, idx, now);
  return idx;
}

int sr_story_arc_start(SrChess *ch, SrCity *c, const SrPopulation *pop, SrArcType t, int64_t now) {
  return arc_start(ch, c, pop, t, -1, -1, now);
}

void sr_story_beat_done(SrChess *ch, SrCity *c, int idx, bool success, int64_t now) {
  if (idx < 0 || idx >= SR_MAX_ARCS || !ch->story.arcs[idx].active) return;
  SrArc *a = &ch->story.arcs[idx];
  if (success) a->path = (uint8_t)(a->path | (1u << a->beat));
  a->beat++;
  if (a->beat < a->beats) {
    beat_start(ch, c, idx, now);
    return;
  }
  a->active = false;
  ch->story.arcs_finished++;
  char msg[72];
  snprintf(msg, sizeof msg, "Arc ends: %s (%s)", sr_arc_name((SrArcType)a->type), success ? "won" : "lost");
  sr_chess_note(ch, now, msg);
  /* the Act III climax is the scenario's arc */
  if (ch->story.act == SR_ACT_PROMOTION && idx == 0) ch->story.finale_ready = true;
}

/* ---- trust, betrayal, defection ---------------------------------------------------------------- */

static void turn_hostile(SrChess *ch, const SrCity *c, int64_t now) {
  SrStory *s = &ch->story;
  if (s->patron < 0) return;
  s->hostile = s->patron;
  s->patron = -1;
  s->retainer = false;
  s->next_hunt = now;
  s->next_counter = now; /* rivals smell an opportunity */
  char msg[72];
  snprintf(msg, sizeof msg, "%s turns on the player", c->factions.f[s->hostile].name);
  sr_chess_note(ch, now, msg);
}

void sr_story_job_done(SrChess *ch, SrCity *c, int client, int target, bool success, bool attributed, int64_t now) {
  SrStory *s = &ch->story;
  if (s->act == SR_ACT_EPILOGUE) return;
  if (s->act == SR_ACT_PROMOTION && client >= 0) s->corps_worked[client]++;
  if (s->patron < 0) return;
  if (target == s->patron && attributed) {
    sr_story_trust(s, -SR_BETRAYAL);
    if (s->trust == 0) turn_hostile(ch, c, now);
    return;
  }
  if (client == s->patron) sr_story_trust(s, success ? (attributed ? 3 : 6) : -8);
}

void sr_story_defect(SrStory *s, const SrCity *c, int corp, int64_t now) {
  if (corp < 0 || corp >= c->factions.corp_count || corp == s->patron) return;
  if (s->patron >= 0) s->hostile = s->patron;
  s->patron = (int8_t)corp;
  s->retainer = true;
  s->rival_offer = -1;
  s->trust = SR_TRUST_ACT2;
  s->next_hunt = now;
  s->cast[SR_CAST_HANDLER].generation++;
  s->cast[SR_CAST_HANDLER].alive = true;
  s->cast[SR_CAST_HANDLER].vacant_since = 0;
  sr_name_person((uint64_t)corp * 0x9e37u,
                 sr_hash_combine(0x5ca57u + SR_CAST_HANDLER, s->cast[SR_CAST_HANDLER].generation),
                 s->cast[SR_CAST_HANDLER].name, sizeof s->cast[SR_CAST_HANDLER].name);
  if (s->act < SR_ACT_PAWN) {
    s->act = SR_ACT_PAWN;
    s->act_started = now;
  }
}

/* ---- cast ---------------------------------------------------------------------------------- */

void sr_story_cast_died(SrStory *s, SrCastRole r, int64_t now) {
  if ((unsigned)r >= SR_CAST_COUNT || !s->cast[r].alive) return;
  s->cast[r].alive = false;
  s->cast[r].vacant_since = now;
}

int sr_story_succession(SrStory *s, uint64_t seed, int64_t now) {
  int n = 0;
  for (int r = 0; r < SR_CAST_COUNT; r++) {
    SrCast *k = &s->cast[r];
    if (k->alive || now < k->vacant_since + 2 * 86400) continue;
    k->generation++;
    k->alive = true;
    k->vacant_since = 0;
    cast_name(s, seed, r);
    n++;
  }
  return n;
}

/* ---- endgame ------------------------------------------------------------------------------- */

static bool energy(const SrCity *c, int f) {
  return c->factions.f[f].sector1 == SR_SEC_ENERGY || c->factions.f[f].sector2 == SR_SEC_ENERGY;
}

SrScenario sr_story_pick_scenario(const SrChess *ch, const SrCity *c, int leader, int *other) {
  int second = -1;
  for (int f = 0; f < c->factions.corp_count; f++) {
    if (f == leader) continue;
    if (second < 0 || ch->f[f].res[SR_RES_CAPITAL] > ch->f[second].res[SR_RES_CAPITAL]) second = f;
  }
  *other = second;
  if (ch->story.matrix_depth >= 60) return SR_SC_GHOST;
  if (sr_politics_seats_of(&ch->pol, leader) >= ch->pol.seat_count / 2 + 1) return SR_SC_COUP;
  if (energy(c, leader) || (second >= 0 && energy(c, second))) return SR_SC_PALL;
  return SR_SC_MERGER; /* the two strongest corps join forces, allied or not */
}

unsigned sr_story_endings(const SrStory *s) {
  unsigned m = 1u << SR_END_KINGMAKER;
  if (s->patron >= 0 && s->retainer && s->patron == s->first_patron) m |= 1u << SR_END_LOYAL;
  if (s->patron >= 0 && s->retainer && s->patron != s->first_patron) m |= 1u << SR_END_DEFECTOR;
  if (s->leaks >= 2 || s->hostile >= 0) m |= 1u << SR_END_BURN;
  if ((s->scenario == SR_SC_PALL || s->scenario == SR_SC_GHOST) && s->matrix_depth >= 20) m |= 1u << SR_END_SUNRISE;
  return m;
}

static void boost(SrChess *ch, int f, int capital, int image) {
  if (f < 0) return;
  ch->f[f].res[SR_RES_CAPITAL] = (int16_t)SR_CLAMP(ch->f[f].res[SR_RES_CAPITAL] + capital, 0, 200);
  ch->f[f].res[SR_RES_IMAGE] = (int16_t)SR_CLAMP(ch->f[f].res[SR_RES_IMAGE] + image, 0, 100);
}

void sr_story_end(SrChess *ch, SrCity *c, SrEnding e, int64_t now) {
  SrStory *s = &ch->story;
  if (s->ending != SR_END_NONE) return;
  s->ending = (uint8_t)e;
  s->act = SR_ACT_EPILOGUE;
  s->sandbox = true;
  s->finale_ready = false;
  switch (e) {
    case SR_END_LOYAL:
    case SR_END_DEFECTOR: boost(ch, s->patron, 50, 10); break;
    case SR_END_BURN:
      for (int f = 0; f < c->factions.corp_count; f++) boost(ch, f, -40, -30);
      break;
    case SR_END_KINGMAKER:
      boost(ch, s->scenario_other, 60, 15);
      boost(ch, s->scenario_corp, -40, -10);
      break;
    case SR_END_SUNRISE: boost(ch, s->scenario_corp, -60, -40); break;
    default: break;
  }
  char head[64];
  snprintf(head, sizeof head, "The board changes: %s", sr_ending_name(e));
  sr_news_publish(ch, c, -1, s->scenario_corp, -1, -1, SR_NEWS_STORY, false, head, now);
  sr_chess_note(ch, now, head);
}

static SrArcType finale_arc(SrScenario sc) {
  switch (sc) {
    case SR_SC_COUP: return SR_ARC_ELECTION;
    case SR_SC_PALL: return SR_ARC_SHORTAGE;
    case SR_SC_GHOST: return SR_ARC_MOLE;
    default: return SR_ARC_TAKEOVER;
  }
}

static void begin_act3(SrChess *ch, SrCity *c, const SrPopulation *pop, int leader, int64_t now) {
  SrStory *s = &ch->story;
  int other = -1;
  s->act = SR_ACT_PROMOTION;
  s->act_started = now;
  s->scenario = (uint8_t)sr_story_pick_scenario(ch, c, leader, &other);
  s->scenario_corp =
      (int8_t)(s->scenario == SR_SC_PALL && other >= 0 && energy(c, other) && !energy(c, leader) ? other : leader);
  s->scenario_other = (int8_t)(s->scenario_corp == leader ? other : leader);
  s->finale_at = now + SR_FINALE_S;
  char msg[72];
  snprintf(msg, sizeof msg, "Act III: %s (%s)", sr_scenario_name((SrScenario)s->scenario),
           c->factions.f[s->scenario_corp].name);
  sr_chess_note(ch, now, msg);
  /* the climax arc takes slot 0: earlier arcs there are wound up off-screen */
  for (int i = 0; i < SR_MAX_ARCS; i++) s->arcs[i].active = false;
  arc_start(ch, c, pop, finale_arc((SrScenario)s->scenario), s->scenario_corp, s->scenario_other, now);
}

void sr_story_tick(SrChess *ch, SrCity *c, const SrPopulation *pop, int64_t now) {
  SrStory *s = &ch->story;
  if (sr_story_succession(s, ch->seed, now) > 0) sr_chess_note(ch, now, "A successor steps into a vacant role");
  /* the handler calls with the retainer: after the test, or at the end of the first week */
  if (!s->retainer && s->act == SR_ACT_PROLOGUE && s->patron >= 0) {
    if (s->offer_at == 0 && now >= s->act_started + 7 * 86400) s->offer_at = now;
    if (s->offer_at > 0 && now >= s->offer_at) s->offer_open = true;
  }
  if (s->act == SR_ACT_EPILOGUE) return;
  SrCredTier tier = sr_cred_tier(ch->street_cred);
  if (s->act == SR_ACT_PAWN && s->retainer && tier >= SR_CRED_RUNNER && s->trust >= SR_TRUST_ACT2) {
    s->act = SR_ACT_KNIGHT;
    s->act_started = now;
    s->next_counter = now + SR_WEEK_S;
    sr_chess_note(ch, now, "Act II begins");
  }
  /* a hostile former patron hunts the player every week */
  if (s->hostile >= 0 && now >= s->next_hunt) {
    sr_chess_start_op(ch, c, SR_OP_HUNT, s->hostile, -1, -1, -1, now);
    s->hunts++;
    s->next_hunt = now + SR_WEEK_S;
  }
  /* counter-offers: rivals court the player in Act II, or once the patron turned */
  if ((s->act == SR_ACT_KNIGHT || s->hostile >= 0) && s->rival_offer < 0 && now >= s->next_counter &&
      c->factions.corp_count > 1) {
    int k = (int)(sr_mix64(sr_hash_combine(ch->seed, (uint64_t)now)) % (uint64_t)c->factions.corp_count);
    for (int tries = 0; tries < c->factions.corp_count && (k == s->patron || k == s->hostile); tries++)
      k = (k + 1) % c->factions.corp_count;
    if (k != s->patron && k != s->hostile) s->rival_offer = (int8_t)k;
    s->next_counter = now + 2 * SR_WEEK_S;
  }
  /* the director keeps 1-3 arcs going */
  if (s->act >= SR_ACT_PAWN && s->act <= SR_ACT_KNIGHT && now >= s->next_offer) {
    int active = 0;
    for (int i = 0; i < SR_MAX_ARCS; i++) active += s->arcs[i].active;
    int want = s->act == SR_ACT_PAWN ? 1 : 2;
    if (active < want) {
      SrArcType t = (SrArcType)(sr_mix64(sr_hash_combine(ch->seed, (uint64_t)s->arcs_started + 11u)) % SR_ARC_COUNT);
      sr_story_arc_start(ch, c, pop, t, now);
    }
    s->next_offer = now + 3 * 86400;
  }
  /* the endgame: dominance, or Act II running too long */
  if (s->act == SR_ACT_KNIGHT) {
    int leader = 0;
    float dom = sr_chess_dominance(ch, c, &leader);
    if (dom >= SR_DOMINANCE || now - s->act_started >= SR_ACT2_MAX_S) begin_act3(ch, c, pop, leader, now);
  }
  if (s->act == SR_ACT_PROMOTION) {
    if (!s->finale_ready && now >= s->finale_at) s->finale_ready = true;
    /* nobody chose: the board decides two weeks after the climax */
    if (s->finale_ready && now >= s->finale_at + SR_FINALE_S) {
      unsigned m = sr_story_endings(s);
      sr_story_end(ch, c, (m & (1u << SR_END_LOYAL)) ? SR_END_LOYAL : SR_END_KINGMAKER, now);
    }
  }
}
