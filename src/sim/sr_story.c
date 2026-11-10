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
  static const char *const n[SR_END_COUNT] = {"-",         "Loyal",        "Defector",       "Burn the Board",
                                              "Kingmaker", "Sunrise",      "The Chair",      "Exodus",
                                              "City Hall", "Street Kings", "The Shareholder"};
  return (unsigned)e < SR_END_COUNT ? n[e] : "?";
}

const char *sr_ending_hint(SrEnding e) {
  static const char *const h[SR_END_COUNT] = {"",
                                              "stay with your first patron",
                                              "switch patrons",
                                              "leak files, be hunted, or a reporter who trusts you",
                                              "crown any megacorp",
                                              "go deep in the Matrix, or a Ghost who trusts you",
                                              "trust 90 and a handler who is your friend",
                                              "two people close to you and 20000 cred",
                                              "a council scandal or the Coup, and a reporter on your side",
                                              "a gang holding 3 districts and a fixer who is your friend",
                                              "shares worth 100000 cred"};
  return (unsigned)e < SR_END_COUNT ? h[e] : "";
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

/* ---- the story log and the cast's lines ------------------------------------------------------ */

void sr_story_log(SrChess *ch, int64_t now, const char *text) {
  for (int i = 0; i < 6; i++) { /* the log tells each thing once, not every week */
    const SrLogEntry *old = sr_story_log_entry(ch, i);
    if (old && strcmp(old->text, text) == 0) return;
  }
  SrLogEntry *e = &ch->camp.log[ch->camp.log_count % SR_STORY_LOG];
  e->time = now;
  snprintf(e->text, sizeof e->text, "%s", text);
  ch->camp.log_count++;
}

const SrLogEntry *sr_story_log_entry(const SrChess *ch, int i) {
  int n = SR_MIN(ch->camp.log_count, SR_STORY_LOG);
  if (i < 0 || i >= n) return NULL;
  return &ch->camp.log[(ch->camp.log_count - 1 - i) % SR_STORY_LOG];
}

void sr_story_say(SrChess *ch, SrCastRole role, const char *text) {
  SrCampaign *cp = &ch->camp;
  if (sr_saga_said(ch, text)) return;    /* nobody says the same thing twice in a row */
  if (cp->msg_count >= SR_STORY_QUEUE) { /* the oldest unread line gives way */
    memmove(&cp->msgs[0], &cp->msgs[1], sizeof(SrStoryMsg) * (SR_STORY_QUEUE - 1));
    cp->msg_count--;
  }
  cp->msgs[cp->msg_count].role = (uint8_t)role;
  snprintf(cp->msgs[cp->msg_count].text, sizeof cp->msgs[cp->msg_count].text, "%s", text);
  cp->msg_count++;
}

void sr_story_crown(SrChess *ch, int corp) { ch->camp.king = (int8_t)corp; }

bool sr_story_t4_allowed(const SrStory *s, int tier) {
  return s->retainer && s->patron >= 0 && tier >= SR_CRED_KNIGHT && s->trust >= SR_TRUST_T4;
}

void sr_story_test_done(SrStory *s, int64_t now) {
  if (s->retainer || s->offer_open) return;
  int64_t at = SR_MAX(now + 86400, s->act_started + SR_RETAINER_MIN); /* never before day 3 */
  if (s->offer_at == 0 || s->offer_at > at) s->offer_at = at;
}

void sr_story_accept_retainer(SrStory *s, int64_t now) {
  if (s->retainer || s->patron < 0) return;
  s->retainer = true;
  s->offer_open = false;
  if (s->act != SR_ACT_EPILOGUE) { /* after an ending the city goes on: a new cycle, not a new campaign */
    s->act = SR_ACT_PAWN;
    s->act_started = now;
  }
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

int sr_story_rival_of(const SrCity *c, int a, uint64_t key) {
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
  SrOpType t = (SrOpType)ARC_OPS[a->type][SR_MIN(a->beat, 2)][branch];
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
  /* the climax's last beat is a set piece: it waits for the player */
  bool climax = ch->story.act == SR_ACT_PROMOTION && idx == 0 && a->beat == a->beats - 1;
  if (climax && op) ch->camp.set_piece = op->id;
  /* a cast member tells the player what is going on */
  const char *an = a->a >= 0 ? c->factions.f[a->a].name : "someone";
  const char *bn = a->b >= 0 ? c->factions.f[a->b].name : "someone";
  const char *place =
      a->building >= 0 ? c->districts[c->blocks[c->buildings[a->building].block].district].name : "town";
  char line[112];
  uint64_t vkey = sr_mix64(sr_hash_combine(ch->seed, (uint64_t)a->started + a->beat));
  for (int v = 0; v < 3; v++) { /* another wording when this one was said lately */
    sr_story_beat_text(line, sizeof line, (SrArcType)a->type, a->beat, branch, an, bn, place, vkey + (uint64_t)v);
    if (!sr_saga_was_said(ch, line)) break;
  }
  if (climax) snprintf(line, sizeof line, "This is it: %s against %s, and you are the one going in.", an, bn);
  /* the handler only briefs the patron's own operations; other people's wars come from the street */
  SrCastRole teller = sr_story_teller((SrArcType)a->type);
  if (teller == SR_CAST_HANDLER && a->a != ch->story.patron) teller = SR_CAST_FIXER;
  sr_story_say(ch, teller, line);
  char entry[72];
  snprintf(entry, sizeof entry, "%s, step %d: %s", sr_arc_name((SrArcType)a->type), a->beat + 1, sr_op_name(t));
  sr_story_log(ch, now, entry);
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
  if (a >= 0 && b < 0) b = sr_story_rival_of(c, a, key);
  if (a < 0) {
    /* prefer the patron's arcs */
    if (s->patron >= 0 && sr_mix64(key) % 10u < 6u)
      a = s->patron;
    else
      a = (int)(sr_mix64(key + 1u) % (uint64_t)SR_MAX(1, c->factions.corp_count));
    b = sr_story_rival_of(c, a, key);
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

int sr_story_arc_between(SrChess *ch, SrCity *c, const SrPopulation *pop, SrArcType t, int a, int b, int64_t now) {
  return arc_start(ch, c, pop, t, a, b, now);
}

int sr_story_arc_start(SrChess *ch, SrCity *c, const SrPopulation *pop, SrArcType t, int64_t now) {
  return arc_start(ch, c, pop, t, -1, -1, now);
}

void sr_story_beat_done(SrChess *ch, SrCity *c, int idx, bool success, bool offscreen, int64_t now) {
  if (idx < 0 || idx >= SR_MAX_ARCS || !ch->story.arcs[idx].active) return;
  SrArc *a = &ch->story.arcs[idx];
  if (success) a->path = (uint8_t)(a->path | (1u << a->beat));
  bool theirs = ch->story.patron >= 0 && (a->a == ch->story.patron || a->b == ch->story.patron);
  if (offscreen && theirs && (a->beat == 0 || a->beat == a->beats - 1)) { /* how it went without the player */
    char line[112];
    sr_story_skipped_text(line, sizeof line, success, a->a >= 0 ? c->factions.f[a->a].name : "someone",
                          a->b >= 0 ? c->factions.f[a->b].name : "someone");
    SrCastRole teller = sr_story_teller((SrArcType)a->type);
    if (teller == SR_CAST_HANDLER && a->a != ch->story.patron) teller = SR_CAST_FIXER;
    sr_story_say(ch, teller, line);
  }
  if (ch->story.act == SR_ACT_PROMOTION && idx == 0 && a->beat == a->beats - 1) ch->camp.set_piece_done = true;
  a->beat++;
  if (a->beat < a->beats) { /* the next beat comes a few days later */
    ch->camp.beat_at[idx] = now + SR_BEAT_GAP_S;
    a->op = -1;
    return;
  }
  a->active = false;
  ch->story.arcs_finished++;
  char msg[72];
  snprintf(msg, sizeof msg, "Arc ends: %s (%s)", sr_arc_name((SrArcType)a->type), success ? "won" : "lost");
  sr_chess_note(ch, now, msg);
}

/* ---- trust, betrayal, defection ---------------------------------------------------------------- */

bool sr_story_blacklisted(const SrChess *ch, int corp) {
  return corp >= 0 && corp < 16 && (ch->camp.blacklist >> corp) & 1u;
}

static void blacklist(SrChess *ch, int corp) {
  if (corp >= 0 && corp < 16) ch->camp.blacklist = (uint16_t)(ch->camp.blacklist | (1u << corp));
}

static void turn_hostile(SrChess *ch, const SrCity *c, int64_t now) {
  SrStory *s = &ch->story;
  if (s->patron < 0) return;
  blacklist(ch, s->patron);
  s->hostile = s->patron;
  s->patron = -1;
  s->retainer = false;
  s->next_hunt = now;
  sr_saga_patron_changed(ch, now); /* rivals smell an opportunity, but nobody hires a burned runner overnight */
  char msg[72];
  snprintf(msg, sizeof msg, "%s turns on the player", c->factions.f[s->hostile].name);
  sr_chess_note(ch, now, msg);
}

bool sr_story_against_patron(const SrChess *ch, int target) {
  int p = ch->story.patron;
  if (p < 0 || target < 0) return false;
  if (target == p) return true;
  for (int i = 0; i < ch->link_count; i++) {
    const SrLink *l = &ch->links[i];
    if (l->a == p && l->b == target && (l->type == SR_LINK_FUNDS || l->type == SR_LINK_OWNS)) return true;
  }
  return false;
}

void sr_story_discovered(SrChess *ch, SrCity *c, int64_t now) {
  SrStory *s = &ch->story;
  if (s->patron < 0) return;
  ch->camp.discoveries++;
  sr_story_trust(s, -SR_BETRAYAL);
  char msg[72];
  snprintf(msg, sizeof msg, "%s learns the player worked against it", c->factions.f[s->patron].name);
  sr_chess_note(ch, now, msg);
  sr_story_log(ch, now, msg);
  if (s->trust == 0) turn_hostile(ch, c, now);
}

void sr_story_job_done(SrChess *ch, SrCity *c, int client, int target, bool success, int evidence, bool framed,
                       int64_t now) {
  SrStory *s = &ch->story;
  if (s->act == SR_ACT_EPILOGUE) return;
  if (s->act == SR_ACT_PROMOTION && client >= 0) s->corps_worked[client]++;
  if (s->patron < 0) return;
  if (sr_story_against_patron(ch, target) && success) {
    if (framed) return; /* the evidence points elsewhere: the patron never knows */
    for (int i = 0; i < SR_MAX_SUSPECTS; i++)
      if (!ch->camp.suspects[i].active) {
        uint64_t h = sr_mix64(sr_hash_combine(ch->seed, (uint64_t)now * 3u + (uint64_t)i));
        ch->camp.suspects[i].active = true;
        ch->camp.suspects[i].evidence = (uint8_t)SR_MIN(evidence, 3);
        ch->camp.suspects[i].at = now + 86400 + (int64_t)(h % (2 * 86400));
        break;
      }
    return;
  }
  if (client == s->patron) {
    sr_story_trust(s, success ? (evidence > 0 ? 3 : 6) : -8);
    /* the handler's neck is on the line too; good work alone makes a colleague, not a friend */
    if (!success || ch->saga.bond[SR_CAST_HANDLER] < SR_BOND_FRIEND)
      sr_saga_bond(ch, SR_CAST_HANDLER, success ? 2 : -3);
  }
}

/* The patron's people finish looking into the player's jobs against it. */
static void look_into_suspects(SrChess *ch, SrCity *c, int64_t now) {
  for (int i = 0; i < SR_MAX_SUSPECTS; i++) {
    SrSuspect *q = &ch->camp.suspects[i];
    if (!q->active || now < q->at) continue;
    q->active = false;
    int chance = 25 + 25 * q->evidence;
    if ((int)(sr_mix64(sr_hash_combine(ch->seed, (uint64_t)q->at + (uint64_t)i * 977u)) % 100u) < chance)
      sr_story_discovered(ch, c, now);
  }
}

bool sr_story_defect(SrChess *ch, const SrCity *c, int corp, int64_t now) {
  SrStory *s = &ch->story;
  if (corp < 0 || corp >= c->factions.corp_count || corp == s->patron || sr_story_blacklisted(ch, corp)) return false;
  if (s->patron >= 0) { /* a voluntary defection: the old patron turns and never takes the player back */
    ch->camp.defections++;
    blacklist(ch, s->patron);
    s->hostile = s->patron;
  }
  s->patron = (int8_t)corp;
  s->retainer = true;
  s->rival_offer = -1;
  s->trust = SR_TRUST_DEFECT;
  s->next_hunt = now;
  sr_saga_patron_changed(ch, now);
  ch->saga.bond[SR_CAST_HANDLER] = 0; /* a new handler, a stranger */
  ch->saga.flags[SR_CAST_HANDLER] = 0;
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
  return true;
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

unsigned sr_saga_endings(const SrChess *ch) {
  const SrStory *s = &ch->story;
  const SrSaga *g = &ch->saga;
  unsigned m = sr_story_endings(s);
  /* the people around the player open doors: a reporter who trusts them, a Ghost who listens */
  if (g->bond[SR_CAST_JOURNALIST] >= SR_BOND_FRIEND && s->cast[SR_CAST_JOURNALIST].alive) m |= 1u << SR_END_BURN;
  if (g->bond[SR_CAST_GHOST] >= SR_BOND_FRIEND) m |= 1u << SR_END_SUNRISE;
  if (g->sun_seen) m &= ~(1u << SR_END_SUNRISE); /* the sun is already out */
  /* The Chair: loyal to the end, trusted completely, and the handler wants you in their seat */
  if (s->patron >= 0 && s->retainer && s->patron == s->first_patron && s->trust >= 90 &&
      g->bond[SR_CAST_HANDLER] >= SR_BOND_FRIEND && s->cast[SR_CAST_HANDLER].alive)
    m |= 1u << SR_END_CHAIR;
  /* Exodus: people to leave with and the money to do it */
  int close = 0;
  for (int r = 0; r < SR_CAST_COUNT; r++) close += s->cast[r].alive && g->bond[r] >= SR_BOND_CLOSE;
  if (close >= 2 && g->cash >= 20000) m |= 1u << SR_END_EXODUS;
  /* City Hall: a scandal to stand on and a reporter to tell it */
  bool exposed = false;
  for (int i = 0; i < ch->pol.seat_count; i++) exposed |= ch->pol.seats[i].exposed;
  if ((exposed || s->scenario == SR_SC_COUP) && s->cast[SR_CAST_JOURNALIST].alive && g->bond[SR_CAST_JOURNALIST] >= 15)
    m |= 1u << SR_END_CITY_HALL;
  /* Street Kings: a gang strong enough and a fixer who will make the introductions */
  if (g->street_gang >= 0 && g->street_districts >= 3 && s->cast[SR_CAST_FIXER].alive &&
      g->bond[SR_CAST_FIXER] >= SR_BOND_FRIEND)
    m |= 1u << SR_END_STREET;
  /* The Shareholder: enough stock to demand a seat */
  if (g->portfolio >= 100000) m |= 1u << SR_END_SHAREHOLDER;
  return m;
}

static void shift(SrChess *ch, int f, SrResource r, int delta) {
  if (f < 0 || f >= SR_MAX_FACTIONS) return;
  int16_t lo, hi;
  sr_resource_bounds(r, &lo, &hi);
  ch->f[f].res[r] = (int16_t)SR_CLAMP(ch->f[f].res[r] + delta, lo, hi);
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
    case SR_END_KINGMAKER: {
      int king = ch->camp.king >= 0 ? ch->camp.king : s->scenario_other;
      boost(ch, king, 60, 15);
      if (king != s->scenario_corp) boost(ch, s->scenario_corp, -40, -10);
      ch->camp.king = (int8_t)king;
      break;
    }
    case SR_END_SUNRISE: boost(ch, s->scenario_corp, -60, -40); break;
    case SR_END_CHAIR: /* the patron's machine now runs through the player */
      boost(ch, s->patron, 40, 5);
      ch->saga.stipend = 1500;
      s->trust = 100;
      break;
    case SR_END_EXODUS: /* the player is gone: nobody wins, the hunters lose the trail */
      ch->saga.exodus = true;
      s->hostile = -1;
      ch->camp.hunter = -1;
      ch->camp.blacklist = 0;
      break;
    case SR_END_CITY_HALL: /* reformers in City Hall: every bought councillor exposed */
      for (int i = 0; i < ch->pol.seat_count; i++)
        if (ch->pol.seats[i].bribed >= 0) {
          ch->pol.seats[i].exposed = true;
          ch->pol.seats[i].bribed = -1;
        }
      ch->pol.police_budget = 1;
      for (int f = 0; f < c->factions.corp_count; f++) shift(ch, f, SR_RES_INFLUENCE, -30);
      break;
    case SR_END_STREET: { /* the gangs take the city: corporate muscle withdraws */
      int gang = ch->saga.street_gang;
      ch->camp.king = (int8_t)gang;
      for (int f = 0; f < c->factions.corp_count; f++) shift(ch, f, SR_RES_FORCE, -20);
      shift(ch, gang, SR_RES_FORCE, 40);
      break;
    }
    case SR_END_SHAREHOLDER: { /* a seat on the board of the corporation the player owns most of */
      int best = -1;
      for (int f = 0; f < c->factions.corp_count; f++)
        if (best < 0 || (int64_t)ch->market.shares[f] * ch->market.price[f] >
                            (int64_t)ch->market.shares[best] * ch->market.price[best])
          best = f;
      ch->camp.king = (int8_t)best;
      boost(ch, best, 30, 10);
      if (best >= 0) ch->market.price[best] = ch->market.price[best] * 5 / 4;
      break;
    }
    default: break;
  }
  /* the sequence: three voices, a news montage, a city that looks different */
  int winner = e == SR_END_KINGMAKER || e == SR_END_SHAREHOLDER || e == SR_END_STREET ? ch->camp.king
               : e == SR_END_LOYAL || e == SR_END_DEFECTOR || e == SR_END_CHAIR       ? s->patron
                                                                                      : -1;
  const char *wn = winner >= 0 ? c->factions.f[winner].name : "Nobody";
  static const SrCastRole voices[3] = {SR_CAST_HANDLER, SR_CAST_JOURNALIST, SR_CAST_GHOST};
  for (int i = 0; i < 3; i++) {
    char line[112];
    sr_story_ending_text(line, sizeof line, e, i, wn);
    sr_story_say(ch, voices[i], line);
  }
  char head[64];
  snprintf(head, sizeof head, "The board changes: %s", sr_ending_name(e));
  sr_news_publish(ch, c, -1, s->scenario_corp, -1, -1, SR_NEWS_STORY, false, head, now);
  if (winner >= 0 && e != SR_END_STREET) {
    sr_headline(head, sizeof head, "%1 now runs the city in all but name", wn, NULL, NULL);
    sr_news_publish(ch, c, c->factions.media_count > 0 ? c->factions.media_first : -1, winner, -1, -1, SR_NEWS_STORY,
                    false, head, now);
    ch->pol.police_contract = (int8_t)winner; /* the winner's colours on the streets */
  } else if (e == SR_END_STREET) {            /* a gang: its colours on the streets, the police stand aside */
    ch->pol.police_contract = -1;
    sr_headline(head, sizeof head, "%1 rule the streets; corp security pulls back", wn, NULL, NULL);
    sr_news_publish(ch, c, -1, winner, -1, -1, SR_NEWS_STORY, false, head, now);
  } else {
    ch->pol.police_contract = -1;
    ch->pol.curfew = 0; /* the old order is gone: curfews lifted */
    const char *what = e == SR_END_SUNRISE     ? "Daylight over the city"
                       : e == SR_END_EXODUS    ? "A runner vanishes; the corps stop looking"
                       : e == SR_END_CITY_HALL ? "Reformers sweep City Hall; bribed seats fall"
                                               : "Corporate files flood the streets";
    sr_news_publish(ch, c, -1, -1, -1, -1, SR_NEWS_STORY, false, what, now);
  }
  sr_chess_note(ch, now, head);
  sr_story_log(ch, now, sr_ending_name(e));
  ch->camp.ending_at = now;
  sr_saga_ending(ch, c, e, now);
}

void sr_story_set_piece_abandoned(SrChess *ch, SrCity *c, int64_t now) {
  SrStory *s = &ch->story;
  if (s->act != SR_ACT_PROMOTION || s->ending != SR_END_NONE) return;
  ch->camp.king = s->scenario_other;
  sr_story_log(ch, now, "You walked away from the final job");
  sr_story_end(ch, c, SR_END_KINGMAKER, now);
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
  if (ch->saga.season > 0) { /* no season climaxes the way the last one did */
    uint8_t last = ch->saga.scenarios[(ch->saga.season - 1) % SR_SAGA_HISTORY];
    for (int k = 0; k < 4 && (s->scenario == last || (s->scenario == SR_SC_PALL && ch->saga.sun_seen)); k++)
      s->scenario = (uint8_t)(s->scenario % SR_SC_GHOST + 1);
  }
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
  ch->camp.set_piece = 0;
  ch->camp.set_piece_done = false;
  for (int i = 0; i < SR_MAX_ARCS; i++) ch->camp.beat_at[i] = 0;
  int idx = arc_start(ch, c, pop, finale_arc((SrScenario)s->scenario), s->scenario_corp, s->scenario_other, now);
  if (idx >= 0) s->arcs[idx].beats = 4; /* the climax: four beats, the last one a set piece */
}

/* Every act change is staged: the handler calls, the news reports the board, the city changes. */
static void stage_act(SrChess *ch, SrCity *c, int64_t now) {
  SrStory *s = &ch->story;
  sr_story_say(ch, SR_CAST_HANDLER, sr_story_act_text((SrAct)s->act));
  static const char *const names[] = {"Prologue", "Act I: Pawn", "Act II: Knight", "Act III: Promotion", "Epilogue"};
  sr_story_log(ch, now, names[SR_MIN(s->act, 4)]);
  int leader = 0;
  float dom = sr_chess_dominance(ch, c, &leader);
  char head[64], pct[8];
  snprintf(pct, sizeof pct, "%d%%", (int)(dom * 100.0f));
  sr_headline(head, sizeof head, "%1 now holds %2 of the corporate pie", c->factions.f[leader].name, pct, NULL);
  sr_news_publish(ch, c, -1, leader, -1, -1, SR_NEWS_STORY, false, head, now);
  if (s->patron >= 0) {
    sr_headline(head, sizeof head, "%1 hires quietly, analysts puzzled", c->factions.f[s->patron].name, NULL, NULL);
    sr_news_publish(ch, c, -1, s->patron, -1, -1, SR_NEWS_STORY, false, head, now);
  }
  int rival = s->patron >= 0 ? sr_story_rival_of(c, s->patron, (uint64_t)now) : (leader + 1) % c->factions.corp_count;
  sr_headline(head, sizeof head, "%1 vows to defend its interests", c->factions.f[rival].name, NULL, NULL);
  sr_news_publish(ch, c, -1, rival, -1, -1, SR_NEWS_STORY, false, head, now);
  if (s->act == SR_ACT_KNIGHT) { /* the streets tighten: a curfew where the gangs are */
    ch->pol.police_budget = 2;
    for (int d = 0; d < c->district_count && d < 16; d++)
      if (sr_chess_gang_of(c, d) >= 0) {
        ch->pol.curfew = (uint16_t)(ch->pol.curfew | (1u << d));
        break;
      }
  }
  if (s->act == SR_ACT_PROMOTION && s->scenario_corp >= 0) /* the scenario corp's message on every billboard */
    for (int k = 0; k < 2; k++) {
      sr_headline(head, sizeof head, k ? "%1: the future is already here" : "%1: trust us with tomorrow",
                  c->factions.f[s->scenario_corp].name, NULL, NULL);
      sr_news_publish(ch, c, -1, s->scenario_corp, -1, -1, SR_NEWS_STORY, false, head, now);
    }
}

void sr_story_tick(SrChess *ch, SrCity *c, const SrPopulation *pop, int64_t now) {
  SrStory *s = &ch->story;
  look_into_suspects(ch, c, now);
  if (sr_story_succession(s, ch->seed, now) > 0) sr_chess_note(ch, now, "A successor steps into a vacant role");
  SrCampaign *cp = &ch->camp;
  /* the handler calls with the retainer: after the test (day 3 at the earliest), or after a week */
  if (!s->retainer && s->act == SR_ACT_PROLOGUE && s->patron >= 0) {
    if (s->offer_at == 0 && now >= s->act_started + 7 * 86400) s->offer_at = now;
    if (s->offer_at > 0 && now >= s->offer_at && now >= s->act_started + SR_RETAINER_MIN) s->offer_open = true;
  }
  uint8_t act_before = s->act;
  /* arc beats waiting for their day */
  for (int i = 0; i < SR_MAX_ARCS; i++)
    if (s->arcs[i].active && cp->beat_at[i] > 0 && now >= cp->beat_at[i]) {
      cp->beat_at[i] = 0;
      beat_start(ch, c, i, now);
    }
  if (s->act == SR_ACT_EPILOGUE) return; /* the interlude: the saga starts the next season */
  SrCredTier tier = sr_cred_tier(ch->street_cred);
  if (s->act == SR_ACT_PAWN && s->retainer && tier >= SR_CRED_RUNNER && s->trust >= SR_TRUST_ACT2 &&
      now - s->act_started >= (ch->saga.season > 0 ? SR_WEEK_S : SR_ACT1_MIN_S)) {
    s->act = SR_ACT_KNIGHT;
    s->act_started = now;
    s->next_counter = now + SR_WEEK_S;
    sr_chess_note(ch, now, "Act II begins");
  }
  /* stuck at Pawn: the handler shows the way */
  int64_t since = SR_MAX(s->act_started, cp->last_real_job);
  if (s->act == SR_ACT_PAWN && !cp->nudged && tier <= SR_CRED_PAWN && now - since >= 2 * SR_WEEK_S && s->patron >= 0 &&
      s->retainer) { /* two weeks of nothing but street gigs */
    cp->nudged = true;
    sr_story_say(ch, SR_CAST_HANDLER, sr_story_nudge_text());
    int rival = sr_story_rival_of(c, s->patron, (uint64_t)now);
    int b = pick_facility(c, rival, (uint64_t)now, false);
    int id = sr_chess_start_op(ch, c, SR_OP_THEFT, s->patron, rival, b, -1, now);
    SrOperation *op = sr_chess_op(ch, id);
    SrContract *k = op ? sr_chess_contract(ch, op->contract) : NULL;
    if (k) k->job_type = 4; /* surveillance: a T1 job */
    sr_story_log(ch, now, "The handler hands you a starter job");
  }
  /* deep in the Matrix: the Ghost gets in touch */
  if (!cp->ghost_hint && s->act >= SR_ACT_KNIGHT && s->matrix_depth >= 10) {
    cp->ghost_hint = true;
    sr_story_say(ch, SR_CAST_GHOST, sr_story_ghost_text());
    sr_story_log(ch, now, "A message from the Ghost");
  }
  /* a hostile former patron hunts the player every week */
  if (s->hostile >= 0 && now >= s->next_hunt) {
    sr_chess_start_op(ch, c, SR_OP_HUNT, s->hostile, -1, -1, -1, now);
    s->hunts++;
    s->next_hunt = now + SR_WEEK_S;
  }
  /* counter-offers: rivals court the player in Act II (one voluntary defection per campaign), or
   * once the patron turned; a megacorp the player betrayed never offers again */
  bool courted = (s->act == SR_ACT_KNIGHT && ch->camp.defections == 0) || s->patron < 0;
  if (courted && (s->act >= SR_ACT_PAWN || s->hostile >= 0) && s->rival_offer < 0 && now >= s->next_counter &&
      c->factions.corp_count > 1) {
    int k = (int)(sr_mix64(sr_hash_combine(ch->seed, (uint64_t)now)) % (uint64_t)c->factions.corp_count);
    for (int tries = 0;
         tries < c->factions.corp_count && (k == s->patron || k == s->hostile || sr_story_blacklisted(ch, k)); tries++)
      k = (k + 1) % c->factions.corp_count;
    if (k != s->patron && k != s->hostile && !sr_story_blacklisted(ch, k)) s->rival_offer = (int8_t)k;
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
    bool dominant = dom >= sr_chess_dominance_threshold(c) && now - s->act_started >= SR_ACT2_MIN_S;
    if (dominant || now - s->act_started >= SR_ACT2_MAX_S) begin_act3(ch, c, pop, leader, now);
  }
  if (s->act == SR_ACT_PROMOTION) {
    /* the choice opens after the set piece, and not before two weeks of the climax */
    if (!s->finale_ready && cp->set_piece_done && now - s->act_started >= SR_ACT3_MIN_S) {
      s->finale_ready = true;
      cp->ready_at = now;
      sr_story_say(ch, SR_CAST_HANDLER, "It's done. Now you decide how this ends. Check your terminal.");
    }
    /* nobody chose: the board decides two weeks later */
    if (s->finale_ready && now >= cp->ready_at + SR_FINALE_S) {
      unsigned m = sr_saga_endings(ch);
      sr_story_end(ch, c, (m & (1u << SR_END_LOYAL)) ? SR_END_LOYAL : SR_END_KINGMAKER, now);
    }
  }
  if (s->act != act_before && s->act != SR_ACT_EPILOGUE) stage_act(ch, c, now);
}
