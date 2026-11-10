/* The faction simulation: resources, doctrine-driven strategy, operations and their contracts,
 * off-screen resolution, attribution and framing, counterplay, visible consequences, gang turf
 * wars, protected megacorps and the intel Board. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

/* ---- names and bounds --------------------------------------------------------------------- */

void sr_resource_bounds(SrResource r, int16_t *lo, int16_t *hi) {
  *lo = 0;
  *hi = r == SR_RES_CAPITAL ? 200 : 100;
}

const char *sr_op_name(SrOpType t) {
  static const char *const n[SR_OP_COUNT] = {"sabotage",    "data theft", "takeover", "smear campaign",
                                             "bribery",     "turf war",   "fortify",  "expand",
                                             "retaliation", "manhunt",    "errand"};
  return (unsigned)t < SR_OP_COUNT ? n[t] : "?";
}

void sr_chess_note(SrChess *ch, int64_t now, const char *text) {
  int i = ch->history_count % SR_MAX_HISTORY;
  snprintf(ch->history[i], sizeof ch->history[i], "d%lld %s", (long long)(now / 86400), text);
  ch->history_count++;
}

static bool is_corp(const SrCity *c, int f) { return f >= 0 && f < c->factions.corp_count; }
static bool is_gang(const SrCity *c, int f) {
  return f >= c->factions.gang_first && f < c->factions.gang_first + c->factions.gang_count;
}

static void clamp_res(SrChess *ch, const SrCity *c, int f) {
  for (int r = 0; r < SR_RES_COUNT; r++) {
    int16_t lo, hi;
    sr_resource_bounds((SrResource)r, &lo, &hi);
    /* megacorps are protected pieces until the endgame */
    if (is_corp(c, f) && ch->story.act < SR_ACT_PROMOTION) lo = r == SR_RES_CAPITAL ? 25 : 10;
    ch->f[f].res[r] = (int16_t)SR_CLAMP(ch->f[f].res[r], lo, hi);
  }
}

static void add_res(SrChess *ch, const SrCity *c, int f, SrResource r, int delta) {
  if (f < 0 || f >= c->factions.count) return;
  ch->f[f].res[r] = (int16_t)(ch->f[f].res[r] + delta);
  clamp_res(ch, c, f);
}

/* Long-run level each resource reverts to. */
static int16_t base_level(const SrChess *ch, const SrCity *c, int f, SrResource r) {
  const SrFaction *fa = &c->factions.f[f];
  static const int16_t corp[SR_RES_COUNT] = {90, 45, 35, 45, 50, 50};
  static const int16_t gang[SR_RES_COUNT] = {25, 15, 50, 25, 15, 30};
  static const int16_t other[SR_RES_COUNT] = {50, 50, 30, 35, 40, 55};
  int16_t v = fa->kind == SR_FK_CORP ? corp[r] : fa->kind == SR_FK_GANG ? gang[r] : other[r];
  if (r == SR_RES_NETDEF && fa->kind == SR_FK_CORP && (fa->sector1 == SR_SEC_NET || fa->sector2 == SR_SEC_NET)) v += 25;
  if (r == SR_RES_FORCE && fa->kind == SR_FK_POLICE) v += 30;
  if (r == SR_RES_CAPITAL) v = (int16_t)(v + ch->camp.capital_bonus[f]); /* what takeovers won stays won */
  return v;
}

/* ---- init ------------------------------------------------------------------------------------ */

static void add_link(SrChess *ch, int a, int b, SrLinkType t) {
  if (ch->link_count >= SR_MAX_LINKS) return;
  ch->links[ch->link_count++] = (SrLink){(int8_t)a, (int8_t)b, (uint8_t)t, false};
}

void sr_chess_init(SrChess *ch, SrCity *c, uint64_t seed, int patron_choice, int64_t now) {
  memset(ch, 0, sizeof *ch);
  ch->seed = seed;
  ch->next_tick = (now / SR_TICK_S + 1) * SR_TICK_S;
  ch->next_op_id = 1;
  ch->next_contract_id = 1;
  SrRng r = sr_rng_stream(seed, "chess-init", 0, 0);
  for (int f = 0; f < c->factions.count; f++) {
    SrFactionSim *s = &ch->f[f];
    for (int k = 0; k < SR_RES_COUNT; k++)
      s->res[k] = (int16_t)(base_level(ch, c, f, (SrResource)k) + sr_rng_range(&r, -8, 8));
    clamp_res(ch, c, f);
    s->retaliate_against = -1;
    const SrFaction *fa = &c->factions.f[f];
    /* goals by doctrine */
    if (fa->kind == SR_FK_CORP) {
      static const uint8_t by_doc[SR_DOC_COUNT][2] = {{SR_OP_SABOTAGE, SR_OP_TAKEOVER},
                                                      {SR_OP_FORTIFY, SR_OP_EXPAND},
                                                      {SR_OP_THEFT, SR_OP_BRIBE},
                                                      {SR_OP_SMEAR, SR_OP_EXPAND},
                                                      {SR_OP_TAKEOVER, SR_OP_THEFT}};
      s->goals[0] = by_doc[fa->doctrine][0];
      s->goals[1] = by_doc[fa->doctrine][1];
      s->goal_count = 2;
    } else if (fa->kind == SR_FK_GANG) {
      s->goals[0] = SR_OP_TURF;
      s->goals[1] = SR_OP_EXPAND;
      s->goal_count = 2;
    } else {
      s->goals[0] = fa->kind == SR_FK_MEDIA ? SR_OP_SMEAR : SR_OP_FORTIFY;
      s->goal_count = 1;
    }
  }
  /* hidden links the Board can reveal */
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++) {
    int best = 0;
    for (int k = 0; k < c->factions.corp_count; k++)
      if (c->factions.rel[k][g] > c->factions.rel[best][g]) best = k;
    add_link(ch, best, g, SR_LINK_FUNDS);
  }
  for (int m = c->factions.media_first; m < c->factions.media_first + c->factions.media_count; m++)
    if (c->factions.f[m].owner >= 0) add_link(ch, c->factions.f[m].owner, m, SR_LINK_OWNS);
  for (int a = 0; a < c->factions.corp_count; a++)
    for (int b = a + 1; b < c->factions.corp_count; b++)
      if (c->factions.rel[a][b] < -40) add_link(ch, a, b, SR_LINK_FEUD);
  sr_politics_init(&ch->pol, c, now);
  for (int s = 0; s < c->factions.council_count && s < SR_COUNCIL_SEATS; s++)
    if (c->factions.council[s].patron >= 0) add_link(ch, c->factions.council[s].patron, s, SR_LINK_BRIBES);
  sr_market_init(&ch->market, ch, c);
  sr_story_init(&ch->story, c, seed, patron_choice);
  sr_saga_init(&ch->saga);
  ch->camp.hunter = -1;
  ch->camp.king = -1;
  ch->story.act_started = now;
}

/* ---- queries -------------------------------------------------------------------------------- */

SrOperation *sr_chess_op(SrChess *ch, int32_t id) {
  for (int i = 0; i < ch->op_count; i++)
    if (ch->ops[i].id == id) return &ch->ops[i];
  return NULL;
}

SrContract *sr_chess_contract(SrChess *ch, int32_t id) {
  for (int i = 0; i < ch->contract_count; i++)
    if (ch->contracts[i].id == id) return &ch->contracts[i];
  return NULL;
}

SrContract *sr_chess_contract_for_job(SrChess *ch, int32_t job) {
  for (int i = 0; i < ch->contract_count; i++)
    if (ch->contracts[i].job_id == job && job >= 0) return &ch->contracts[i];
  return NULL;
}

int sr_chess_security(const SrChess *ch, int b) {
  for (int i = 0; i < ch->secmod_count; i++)
    if (ch->secmods[i].building == b) return ch->secmods[i].level;
  return 0;
}

static void raise_security(SrChess *ch, int b, int by) {
  if (b < 0) return;
  for (int i = 0; i < ch->secmod_count; i++)
    if (ch->secmods[i].building == b) {
      ch->secmods[i].level = (int8_t)SR_MIN(5, ch->secmods[i].level + by);
      return;
    }
  if (ch->secmod_count < SR_MAX_SECMODS) ch->secmods[ch->secmod_count++] = (SrSecMod){b, (int8_t)SR_MIN(5, by)};
}

int sr_chess_price_pct(const SrChess *ch, int d) {
  return d >= 0 && d < SR_MAX_DISTRICTS ? 100 + ch->price_mod[d] : 100;
}

float sr_chess_dominance(const SrChess *ch, const SrCity *c, int *leader) {
  long total = 0, best = -1;
  int who = 0;
  for (int f = 0; f < c->factions.corp_count; f++) {
    long v = ch->f[f].res[SR_RES_CAPITAL] + ch->f[f].res[SR_RES_INFLUENCE] + ch->f[f].res[SR_RES_FORCE];
    total += v;
    if (v > best) best = v, who = f;
  }
  if (leader) *leader = who;
  return total > 0 ? (float)best / (float)total : 0.0f;
}

int sr_chess_city_hall(const SrCity *c) {
  for (int b = 0; b < c->building_count; b++)
    if (c->buildings[b].poi == SR_POI_CITY_HALL) return b;
  return -1; /* GCOVR_EXCL_LINE reason: every city has a city hall (generation invariant) */
}

float sr_chess_dominance_threshold(const SrCity *c) {
  float t = 2.2f / (float)SR_MAX(1, c->factions.corp_count);
  return SR_CLAMP(t, 0.25f, SR_DOMINANCE);
}

int sr_chess_gang_of(const SrCity *c, int district) {
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++)
    if (c->factions.f[g].turf & (1u << district)) return g;
  return -1;
}

void sr_chess_reveal(SrChess *ch, int a, int b, SrLinkType t) {
  for (int i = 0; i < ch->link_count; i++)
    if (ch->links[i].a == a && ch->links[i].b == b && ch->links[i].type == t) {
      ch->links[i].known = true;
      return;
    }
  /* evidence of a link the board did not track yet */
  add_link(ch, a, b, t);
  if (ch->link_count > 0 && ch->links[ch->link_count - 1].a == a) ch->links[ch->link_count - 1].known = true;
}

int sr_chess_reveal_faction(SrChess *ch, int f) {
  int n = 0;
  for (int i = 0; i < ch->link_count; i++)
    if ((ch->links[i].a == f || (ch->links[i].b == f && ch->links[i].type != SR_LINK_BRIBES)) && !ch->links[i].known) {
      ch->links[i].known = true;
      n++;
    }
  return n;
}

bool sr_chess_link_known(const SrChess *ch, int a, int b, SrLinkType t) {
  for (int i = 0; i < ch->link_count; i++)
    if (ch->links[i].a == a && ch->links[i].b == b && ch->links[i].type == t) return ch->links[i].known;
  return false;
}

/* ---- changes to the city (recorded for loading) --------------------------------------------- */

static void set_owner(SrChess *ch, SrCity *c, int b, int owner) {
  c->buildings[b].owner = (int8_t)owner;
  for (int i = 0; i < ch->change_count; i++)
    if (ch->changes[i].building == b) {
      ch->changes[i].owner = (int8_t)owner;
      return;
    }
  if (ch->change_count < SR_MAX_CHANGES) ch->changes[ch->change_count++] = (SrOwnerChange){b, (int8_t)owner};
}

void sr_chess_apply_changes(const SrChess *ch, SrCity *c) {
  for (int i = 0; i < ch->change_count; i++) c->buildings[ch->changes[i].building].owner = ch->changes[i].owner;
  /* turf and gang identities are stored in the faction table snapshot below */
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++) {
    const SrFactionSim *s = &ch->f[g];
    if (s->generation > 0) {
      snprintf(c->factions.f[g].name, sizeof c->factions.f[g].name, "%s", s->name);
      c->factions.f[g].hue = (uint8_t)((c->factions.f[g].hue + 7u * s->generation) % 24u);
    }
  }
  if (ch->pol.police_contract >= 0) (void)0; /* read through sr_politics_police */
}

/* ---- operations and contracts --------------------------------------------------------------- */

static int steps_of(SrOpType t) {
  switch (t) {
    case SR_OP_TAKEOVER: return 2; /* scout, then seize */
    case SR_OP_FORTIFY:
    case SR_OP_EXPAND:
    case SR_OP_HUNT: return 0; /* internal: no work in the city */
    default: return 1;
  }
}

static uint8_t job_for(SrOpType t, int step) {
  switch (t) {
    case SR_OP_SABOTAGE:
    case SR_OP_RETALIATE: return 7; /* SR_JT_SABOTAGE */
    case SR_OP_THEFT:
    case SR_OP_SMEAR: return 6;                    /* SR_JT_STEAL_DATA */
    case SR_OP_TAKEOVER: return step == 0 ? 6 : 8; /* steal plans, then SR_JT_RAID */
    case SR_OP_BRIBE:
    case SR_OP_ERRAND: return 0; /* SR_JT_DELIVER (an errand's job is chosen when it is demanded) */
    default: return 8;           /* SR_JT_RAID (turf) */
  }
}

static uint8_t channel_for(const SrChess *ch, const SrCity *c, int client) {
  if (ch->story.retainer && client == ch->story.patron) return SR_CH_HANDLER;
  if (is_gang(c, client)) return SR_CH_GANG;
  if (is_corp(c, client)) return SR_CH_FIXER;
  return SR_CH_BOARD;
}

static void post_contract(SrChess *ch, const SrCity *c, SrOperation *op, int64_t now) {
  if (ch->contract_count >= SR_MAX_CONTRACTS) { /* drop the oldest closed one */
    int k = -1;
    for (int i = 0; i < ch->contract_count && k < 0; i++)
      if (!ch->contracts[i].open) k = i;
    if (k < 0) k = 0;
    memmove(&ch->contracts[k], &ch->contracts[k + 1], sizeof(SrContract) * (size_t)(ch->contract_count - k - 1));
    ch->contract_count--;
  }
  SrContract *k = &ch->contracts[ch->contract_count++];
  memset(k, 0, sizeof *k);
  k->id = ch->next_contract_id++;
  k->op = op->id;
  k->job_type = job_for((SrOpType)op->type, op->step);
  k->channel = channel_for(ch, c, op->client);
  k->client = op->client;
  k->building = op->building;
  k->deadline = now + 2 * 86400;
  k->job_id = -1;
  k->open = true;
  k->framed = -1;
  op->contract = k->id;
}

static int facility_of(const SrCity *c, SrRng *r, int target, bool not_hq) {
  const SrFaction *fa = &c->factions.f[target];
  int n = fa->facility_count;
  if (n > 0 && (not_hq || sr_rng_chance(r, 700))) return fa->facilities[sr_rng_range(r, 0, n - 1)];
  if (not_hq) return -1;
  return fa->hq_building;
}

int sr_chess_start_op(SrChess *ch, SrCity *c, SrOpType type, int client, int target, int building, int district,
                      int64_t now) {
  /* recycle finished operations when full */
  if (ch->op_count >= SR_MAX_OPS) {
    int k = -1;
    for (int i = 0; i < ch->op_count && k < 0; i++)
      if (!ch->ops[i].active) k = i;
    if (k < 0) return -1;
    memmove(&ch->ops[k], &ch->ops[k + 1], sizeof(SrOperation) * (size_t)(ch->op_count - k - 1));
    ch->op_count--;
  }
  SrOperation *op = &ch->ops[ch->op_count++];
  memset(op, 0, sizeof *op);
  op->id = ch->next_op_id++;
  op->type = (uint8_t)type;
  op->client = (int8_t)client;
  op->target = (int8_t)target;
  op->building = building;
  op->district = (int8_t)district;
  op->steps = (uint8_t)steps_of(type);
  op->active = true;
  op->blamed = -1;
  op->arc = -1;
  op->contract = -1;
  op->started = now;
  if (client >= 0 && client < SR_MAX_FACTIONS && type < SR_OP_TRACKED) ch->f[client].ops_by_type[type]++;
  if (op->steps > 0) post_contract(ch, c, op, now);
  (void)c;
  return op->id;
}

/* ---- operations on demand --------------------------------------------------------------------- */

static int pick_rival_corp(const SrCity *c, SrRng *r, int self);

static int district_of_building(const SrCity *c, int b) { return c->blocks[c->buildings[b].block].district; }

/* A building of a faction in the district (or any of its buildings when none is there). */
static int faction_building_in(const SrCity *c, int f, int d, uint64_t key) {
  const SrFaction *fa = &c->factions.f[f];
  int any = fa->facility_count > 0 ? fa->facilities[key % fa->facility_count] : fa->hq_building;
  for (int i = 0; i < fa->facility_count; i++)
    if (district_of_building(c, fa->facilities[(i + key) % fa->facility_count]) == d)
      return fa->facilities[(i + key) % fa->facility_count];
  return any;
}

/* A place of business in the district (shops, bars, clinics) for errands. */
static int business_in(const SrCity *c, int d, uint64_t key) {
  int n = 0;
  for (int b = 0; b < c->building_count; b++)
    n += c->buildings[b].poi != SR_POI_NONE && c->buildings[b].poi < SR_POI_FIRST_SPECIAL &&
         district_of_building(c, b) == d;
  if (n == 0) return -1;
  int want = (int)(key % (uint64_t)n);
  for (int b = 0; b < c->building_count; b++)
    if (c->buildings[b].poi != SR_POI_NONE && c->buildings[b].poi < SR_POI_FIRST_SPECIAL &&
        district_of_building(c, b) == d && want-- == 0)
      return b;
  return -1; /* GCOVR_EXCL_LINE reason: the count above guarantees a match */
}

int32_t sr_chess_demand_op(SrChess *ch, SrCity *c, int district, int tier, int64_t now) {
  if (district < 0 || district >= c->district_count) return -1;
  int32_t day = (int32_t)(now / 86400) + 1;
  if (ch->camp.demand_day[district] == day) return -1;
  ch->camp.demand_day[district] = day;
  SrRng r = sr_rng_stream(ch->seed, "demand", (uint64_t)day, (uint64_t)district * 8u + (uint64_t)tier);
  int id = -1;
  uint8_t job = 0;
  if (tier <= 0) { /* street business for the gang that runs the district, or a minor faction */
    int client = sr_chess_gang_of(c, district);
    if (client < 0 || ch->f[client].dissolved) {
      static const int minors[3] = {0, 1, 2};
      int pick = minors[sr_rng_range(&r, 0, 2)];
      client = pick == 0 ? c->factions.union_ : pick == 1 ? c->factions.church : c->factions.collective;
    }
    int b = business_in(c, district, sr_rng_u32(&r));
    if (b < 0) return -1;
    static const uint8_t errands[3] = {0, 1, 2}; /* deliver, collect, find */
    job = errands[sr_rng_range(&r, 0, 2)];
    id = sr_chess_start_op(ch, c, SR_OP_ERRAND, client, -1, b, district, now);
  } else {
    /* a megacorp moves against a rival's site in or near the district */
    int client = sr_rng_range(&r, 0, c->factions.corp_count - 1);
    int target = pick_rival_corp(c, &r, client);
    if (target < 0) return -1; /* GCOVR_EXCL_LINE reason: every city has several corps */
    int b = faction_building_in(c, target, district, sr_rng_u32(&r));
    SrOpType type;
    if (tier == 1) {
      static const uint8_t small[3] = {3, 4, 5}; /* plant a bug, surveil, steal an item */
      job = small[sr_rng_range(&r, 0, 2)];
      type = job == 3 ? SR_OP_SMEAR : SR_OP_THEFT;
    } else {
      static const SrOpType big[3] = {SR_OP_SABOTAGE, SR_OP_THEFT, SR_OP_TAKEOVER};
      type = big[sr_rng_range(&r, 0, 2)];
      if (type == SR_OP_TAKEOVER && (b < 0 || b == c->factions.f[target].hq_building)) type = SR_OP_SABOTAGE;
    }
    add_res(ch, c, client, SR_RES_CAPITAL, -3);
    id = sr_chess_start_op(ch, c, type, client, target, b, -1, now);
    if (tier >= 2) job = 255; /* the operation's own job */
  }
  SrOperation *op = sr_chess_op(ch, id);
  if (!op) return -1; /* GCOVR_EXCL_LINE reason: operations are recycled when the list is full */
  SrContract *k = sr_chess_contract(ch, op->contract);
  if (!k) return -1; /* GCOVR_EXCL_LINE reason: every demanded operation has a step */
  if (job != 255) k->job_type = job;
  return k->id;
}

/* People connected to the target may pay for it. */
static void people_fate(SrPopulation *pop, int target, uint64_t key) {
  if (!pop) return;
  for (int i = 0; i < pop->count; i++) {
    SrNpc *n = &pop->npcs[i];
    if (n->faction != target || n->status != SR_NPC_ALIVE || n->role != SR_ROLE_EXEC) continue;
    if (sr_mix64(key ^ n->id) % 4 == 0) n->status = (sr_mix64(key + n->id) & 1) ? SR_NPC_DEAD : SR_NPC_ARRESTED;
    return;
  }
}

static void dissolve_gang(SrChess *ch, SrCity *c, int g, int64_t now, SrEventLog *log) {
  ch->f[g].dissolved = true;
  ch->f[g].reborn_at = now + SR_GANG_REBIRTH_S;
  char msg[72];
  snprintf(msg, sizeof msg, "%s is finished", c->factions.f[g].name);
  sr_chess_note(ch, now, msg);
  SrWorldEvent e;
  memset(&e, 0, sizeof e);
  e.type = SR_EV_KILLING;
  e.time = now;
  e.district = -1;
  e.building = -1;
  e.victim_faction = (int8_t)g;
  e.blamed_faction = -1;
  e.significant = true;
  snprintf(e.text, sizeof e.text, "Gang wiped out: %s", c->factions.f[g].name);
  sr_events_add(log, &e);
}

static void outcome_event(SrEventLog *log, const SrCity *c, const SrOperation *op, int blamed, int64_t now) {
  SrWorldEvent e;
  memset(&e, 0, sizeof e);
  e.type = op->type == SR_OP_THEFT || op->type == SR_OP_SMEAR ? SR_EV_THEFT : SR_EV_SABOTAGE;
  e.time = now;
  e.building = op->building;
  e.district = op->building >= 0 ? c->blocks[c->buildings[op->building].block].district : op->district;
  e.victim_faction = op->target;
  e.blamed_faction = (int8_t)blamed;
  e.significant = true;
  snprintf(e.text, sizeof e.text, "%s hit by %s", op->target >= 0 ? c->factions.f[op->target].name : "the city",
           sr_op_name((SrOpType)op->type));
  sr_events_add(log, &e);
}

/* Applies the success of an operation's last step to the world. */
static void apply_success(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, SrOperation *op, int64_t now) {
  int t = op->target, cl = op->client;
  char msg[72];
  switch ((SrOpType)op->type) {
    case SR_OP_SABOTAGE:
    case SR_OP_RETALIATE: {
      add_res(ch, c, t, SR_RES_CAPITAL, -10);
      add_res(ch, c, t, SR_RES_IMAGE, -4);
      if (is_corp(c, t)) ch->market.shock[t] -= 80;
      if (op->building >= 0) { /* supplies stop: prices rise in the district and its neighbours */
        int d = c->blocks[c->buildings[op->building].block].district;
        for (int k = 0; k < c->district_count; k++) {
          int32_t dx = c->districts[k].cx - c->districts[d].cx, dy = c->districts[k].cy - c->districts[d].cy;
          if (k == d || (int64_t)dx * dx + (int64_t)dy * dy < (int64_t)180000 * 180000)
            ch->price_mod[k] = (int8_t)SR_MIN(60, ch->price_mod[k] + (k == d ? 15 : 8));
        }
      }
      people_fate(pop, t, (uint64_t)op->id);
      break;
    }
    case SR_OP_THEFT:
      add_res(ch, c, cl, SR_RES_INTEL, 6);
      add_res(ch, c, t, SR_RES_INTEL, -4);
      if (is_corp(c, t)) ch->market.shock[t] -= 30;
      break;
    case SR_OP_TAKEOVER:
      if (op->building >= 0 && op->building != c->factions.f[t].hq_building) set_owner(ch, c, op->building, cl);
      if (cl >= 0 && cl < SR_MAX_FACTIONS)
        ch->camp.capital_bonus[cl] = (int16_t)SR_MIN(80, ch->camp.capital_bonus[cl] + 8);
      add_res(ch, c, t, SR_RES_CAPITAL, -8);
      add_res(ch, c, cl, SR_RES_CAPITAL, 6);
      if (is_corp(c, t)) ch->market.shock[t] -= 60;
      if (is_corp(c, cl)) ch->market.shock[cl] += 40;
      break;
    case SR_OP_SMEAR:
      add_res(ch, c, t, SR_RES_IMAGE, -9);
      if (is_corp(c, t)) ch->market.shock[t] -= 40;
      break;
    case SR_OP_BRIBE: {
      int seat = op->district; /* the seat */
      if (seat >= 0 && seat < ch->pol.seat_count) {
        ch->pol.seats[seat].bribed = (int8_t)cl;
        add_link(ch, cl, seat, SR_LINK_BRIBES);
      }
      add_res(ch, c, cl, SR_RES_INFLUENCE, 6);
      break;
    }
    case SR_OP_TURF:
      if (op->district >= 0 && t >= 0) {
        c->factions.f[t].turf = (uint16_t)(c->factions.f[t].turf & ~(1u << op->district));
        c->factions.f[cl].turf = (uint16_t)(c->factions.f[cl].turf | (1u << op->district));
        ch->turf_version++;
        snprintf(msg, sizeof msg, "%s takes %s", c->factions.f[cl].name, c->districts[op->district].name);
        sr_chess_note(ch, now, msg);
        SrWorldEvent e; /* a turf war is fought on the street */
        memset(&e, 0, sizeof e);
        e.type = SR_EV_GUNFIRE;
        e.time = now;
        e.district = op->district;
        e.building = -1;
        e.victim_faction = (int8_t)t;
        e.blamed_faction = (int8_t)cl;
        e.significant = true;
        snprintf(e.text, sizeof e.text, "Gang war in %s", c->districts[op->district].name);
        sr_events_add(log, &e);
        if (c->factions.f[t].turf == 0) dissolve_gang(ch, c, t, now, log);
      }
      break;
    case SR_OP_FORTIFY:
      add_res(ch, c, cl, SR_RES_NETDEF, 5);
      add_res(ch, c, cl, SR_RES_FORCE, 3);
      for (int i = 0; i < c->factions.f[cl].facility_count; i++) raise_security(ch, c->factions.f[cl].facilities[i], 1);
      break;
    case SR_OP_EXPAND:
      add_res(ch, c, cl, SR_RES_CAPITAL, 7);
      add_res(ch, c, cl, SR_RES_FORCE, 3);
      if (is_corp(c, cl)) ch->market.shock[cl] += 25;
      break;
    case SR_OP_HUNT: /* the hunters are on their way: the game layer stages the encounter */
      if (cl >= 0) {
        ch->camp.hunter = (int8_t)cl;
        ch->camp.hunt_at = now;
      }
      break;
    case SR_OP_ERRAND: /* street business: a little money and goodwill */
      add_res(ch, c, cl, SR_RES_CAPITAL, 3);
      add_res(ch, c, cl, SR_RES_INFLUENCE, 1);
      break;
    default: break; /* GCOVR_EXCL_LINE reason: every operation type is listed */
  }
  if (op->type != SR_OP_FORTIFY && op->type != SR_OP_EXPAND && op->type != SR_OP_HUNT)
    outcome_event(log, c, op, op->blamed, now);
}

void sr_chess_resolve(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, SrContract *k, bool success,
                      bool by_player, int64_t now) {
  if (!k->open) return;
  k->open = false;
  k->success = success;
  k->by_player = by_player;
  SrOperation *op = sr_chess_op(ch, k->op);
  if (!op || !op->active) return;
  /* attribution: whoever the evidence points to */
  int blamed;
  if (by_player)
    blamed = k->framed >= 0 ? k->framed : k->player_evidence ? -2 : -1;
  else
    blamed = sr_mix64(sr_hash_combine(ch->seed, (uint64_t)k->id)) % 10 < 7 ? op->client : -1;
  op->blamed = (int8_t)blamed;
  /* counterplay: an attacked facility tightens up, the victim plans to strike back */
  if (op->target >= 0 && op->type != SR_OP_BRIBE) {
    if (success) raise_security(ch, op->building, 2);
    /* a strike back is not itself answered, or the city would never calm down */
    bool answer = op->type != SR_OP_RETALIATE && sr_mix64(sr_hash_combine(ch->seed, (uint64_t)k->id * 13u)) % 10 < 4;
    if (answer && blamed >= 0 && blamed < c->factions.count && blamed != op->target) {
      ch->f[op->target].retaliate_against = (int8_t)blamed;
      ch->f[op->target].retaliate_by = now + 7 * 86400;
    }
  }
  if (!success) {
    op->failed = true;
    op->active = false;
    char msg[72];
    snprintf(msg, sizeof msg, "%s's %s fails", c->factions.f[op->client].name, sr_op_name((SrOpType)op->type));
    sr_chess_note(ch, now, msg);
    if (op->arc >= 0) sr_story_beat_done(ch, c, op->arc, false, !by_player, now);
    return;
  }
  op->step++;
  if (op->step < op->steps) {
    post_contract(ch, c, op, now);
    return;
  }
  op->succeeded = true;
  op->active = false;
  apply_success(ch, c, pop, log, op, now);
  char msg[72];
  if (op->type == SR_OP_BRIBE && op->district >= 0 && op->district < ch->pol.seat_count)
    snprintf(msg, sizeof msg, "%s buys councillor %s", c->factions.f[op->client].name,
             ch->pol.seats[op->district].name);
  else
    snprintf(msg, sizeof msg, "%s: %s on %s succeeds", c->factions.f[op->client].name, sr_op_name((SrOpType)op->type),
             op->target >= 0 ? c->factions.f[op->target].name : "-");
  sr_chess_note(ch, now, msg);
  if (op->arc >= 0) sr_story_beat_done(ch, c, op->arc, true, !by_player, now);
}

/* Off-screen odds: the client's means against the target's defences. */
static bool offscreen_success(const SrChess *ch, const SrCity *c, const SrOperation *op, uint64_t key) {
  int atk = ch->f[op->client].res[SR_RES_FORCE] + ch->f[op->client].res[SR_RES_INTEL];
  int def = op->target >= 0 ? ch->f[op->target].res[SR_RES_NETDEF] + ch->f[op->target].res[SR_RES_FORCE] : 40;
  def += op->building >= 0 ? sr_chess_security(ch, op->building) * 12 : 0;
  /* a gang defends its last streets to the end */
  if (op->type == SR_OP_TURF && op->target >= 0) {
    unsigned turf = c->factions.f[op->target].turf;
    if (turf != 0 && (turf & (turf - 1u)) == 0) def += 45;
  }
  int chance = SR_CLAMP(50 + (atk - def) / 3, 15, 85);
  return (int)(sr_mix64(key) % 100) < chance;
}

/* ---- strategy -------------------------------------------------------------------------------- */

static int pick_rival_corp(const SrCity *c, SrRng *r, int self) {
  int best = -1, bv = 1000;
  for (int k = 0; k < c->factions.corp_count; k++) {
    if (k == self) continue;
    int v = c->factions.rel[self][k] + sr_rng_range(r, -25, 25);
    if (v < bv) bv = v, best = k;
  }
  return best;
}

static int pick_gang_target(const SrChess *ch, const SrCity *c, SrRng *r, int self, int *district) {
  /* a district next to own turf, held by another gang */
  for (int tries = 0; tries < 20; tries++) {
    int g = c->factions.gang_first + sr_rng_range(r, 0, c->factions.gang_count - 1);
    if (g == self || ch->f[g].dissolved || c->factions.f[g].turf == 0) continue;
    for (int d = 0; d < c->district_count; d++)
      if (c->factions.f[g].turf & (1u << d)) {
        *district = d;
        if (sr_rng_chance(r, 500)) return g;
      }
    return g;
  }
  return -1;
}

/* Doctrine weights for operation types (per mille), corps by doctrine, then gangs and others. */
static int weight(const SrCity *c, int f, SrOpType t) {
  const SrFaction *fa = &c->factions.f[f];
  if (fa->kind == SR_FK_CORP) {
    static const uint16_t w[SR_DOC_COUNT][SR_OP_COUNT] = {/* sab  theft take smear bribe turf fort exp  ret hunt */
                                                          {300, 120, 250, 80, 60, 0, 60, 130, 0, 0},  /* Shark */
                                                          {40, 80, 30, 40, 60, 0, 450, 300, 0, 0},    /* Fortress */
                                                          {80, 380, 60, 80, 250, 0, 80, 70, 0, 0},    /* Spider */
                                                          {60, 80, 50, 380, 120, 0, 100, 210, 0, 0},  /* Prophet */
                                                          {120, 180, 380, 60, 60, 0, 60, 140, 0, 0}}; /* Vulture */
    return w[fa->doctrine][t];
  }
  if (fa->kind == SR_FK_GANG) return t == SR_OP_TURF ? 350 : t == SR_OP_EXPAND ? 650 : 0;
  if (fa->kind == SR_FK_MEDIA) return t == SR_OP_SMEAR ? 500 : t == SR_OP_EXPAND ? 500 : 0;
  return t == SR_OP_FORTIFY ? 500 : t == SR_OP_EXPAND ? 500 : 0;
}

static bool has_active_op(const SrChess *ch, int f) {
  for (int i = 0; i < ch->op_count; i++)
    if (ch->ops[i].active && ch->ops[i].client == f && ch->ops[i].arc < 0) return true;
  return false;
}

static void strategy(SrChess *ch, SrCity *c, int f, int64_t now) {
  SrFactionSim *s = &ch->f[f];
  if (s->dissolved || now < s->cooldown_until || has_active_op(ch, f)) return;
  SrRng r = sr_rng_stream(ch->seed, "strategy", (uint64_t)(now / SR_TICK_S), (uint64_t)f);
  /* retaliation first: strike back within the week */
  if (s->retaliate_against >= 0 && now < s->retaliate_by) {
    int t = s->retaliate_against;
    int b = t < c->factions.count && c->factions.f[t].facility_count > 0 ? facility_of(c, &r, t, true) : -1;
    sr_chess_start_op(ch, c, SR_OP_RETALIATE, f, t, b, -1, now);
    s->retaliate_against = -1;
    s->cooldown_until = now + 2 * 86400;
    return;
  }
  if (!sr_rng_chance(&r, 250)) return; /* not every tick */
  int total = 0;
  int w[SR_OP_COUNT];
  for (int t = 0; t < SR_OP_COUNT; t++) {
    w[t] = weight(c, f, (SrOpType)t);
    total += w[t];
  }
  if (total <= 0) return; /* GCOVR_EXCL_LINE reason: every faction kind has weights */
  int x = sr_rng_range(&r, 0, total - 1), type = 0;
  while (x >= w[type]) x -= w[type++];
  if (s->res[SR_RES_CAPITAL] < 20 && type != SR_OP_EXPAND) type = SR_OP_EXPAND; /* broke: rebuild first */
  int target = -1, building = -1, district = -1;
  switch ((SrOpType)type) {
    case SR_OP_TURF:
      target = pick_gang_target(ch, c, &r, f, &district);
      if (target < 0) return;
      building = c->factions.f[target].hq_building;
      break;
    case SR_OP_BRIBE:
      district = sr_rng_range(&r, 0, SR_MAX(1, ch->pol.seat_count) - 1);
      building = sr_chess_city_hall(c);
      target = c->factions.gov;
      break;
    case SR_OP_FORTIFY:
    case SR_OP_EXPAND: break;
    default:
      target = pick_rival_corp(c, &r, f);
      if (target < 0) return; /* GCOVR_EXCL_LINE reason: every city has several corps */
      building = facility_of(c, &r, target, type == SR_OP_TAKEOVER);
      if (type == SR_OP_TAKEOVER && building < 0) type = SR_OP_SABOTAGE, building = facility_of(c, &r, target, false);
      break;
  }
  add_res(ch, c, f, SR_RES_CAPITAL, -6);
  sr_chess_start_op(ch, c, (SrOpType)type, f, target, building, district, now);
  s->cooldown_until = now + 3 * 86400 + sr_rng_range(&r, 0, 4 * 86400);
}

static void gang_rebirth(SrChess *ch, SrCity *c, SrEventLog *log, int64_t now) {
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++) {
    SrFactionSim *s = &ch->f[g];
    if (!s->dissolved || now < s->reborn_at) continue;
    SrFaction *fa = &c->factions.f[g];
    /* a free district, or one taken from a gang that holds several */
    int pick = -1;
    for (int d = 0; d < c->district_count && pick < 0; d++)
      if (sr_chess_gang_of(c, d) < 0 && c->districts[d].type != SR_D_SPIRE) pick = d;
    for (int d = 0; d < c->district_count && pick < 0; d++) {
      int holder = sr_chess_gang_of(c, d);
      if (holder >= 0 && (c->factions.f[holder].turf & (c->factions.f[holder].turf - 1u)) != 0) {
        c->factions.f[holder].turf = (uint16_t)(c->factions.f[holder].turf & ~(1u << d));
        pick = d;
      }
    }
    if (pick < 0) { /* no room yet: try again next week */
      s->reborn_at = now + SR_WEEK_S;
      continue;
    }
    s->dissolved = false;
    s->generation++;
    /* a new name that reads unlike every other corp and gang in town */
    for (uint64_t salt = 0;; salt++) {
      sr_name_gang(ch->seed, ((uint64_t)g * 131u + s->generation) * 64u + salt, s->name, sizeof s->name);
      if (sr_faction_name_free(&c->factions, s->name, g) || salt > 400) break;
    }
    snprintf(fa->name, sizeof fa->name, "%s", s->name);
    fa->hue = (uint8_t)((fa->hue + 7u) % 24u);
    fa->turf = (uint16_t)(1u << pick);
    for (int k = 0; k < SR_RES_COUNT; k++) s->res[k] = base_level(ch, c, g, (SrResource)k);
    ch->turf_version++;
    char msg[72];
    snprintf(msg, sizeof msg, "New gang %s claims %s", fa->name, c->districts[pick].name);
    sr_chess_note(ch, now, msg);
    SrWorldEvent e;
    memset(&e, 0, sizeof e);
    e.type = SR_EV_GUNFIRE;
    e.time = now;
    e.district = (int8_t)pick;
    e.building = -1;
    e.victim_faction = -1;
    e.blamed_faction = (int8_t)g;
    e.significant = true;
    snprintf(e.text, sizeof e.text, "New gang on the streets: %s", fa->name);
    sr_events_add(log, &e);
  }
}

void sr_chess_tick(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, int64_t now) {
  ch->ticks++;
  SrRng r = sr_rng_stream(ch->seed, "income", (uint64_t)ch->ticks, 0);
  for (int f = 0; f < c->factions.count; f++) { /* mean-reverting income */
    if (ch->f[f].dissolved) continue;
    for (int k = 0; k < SR_RES_COUNT; k++) {
      int16_t base = base_level(ch, c, f, (SrResource)k);
      ch->f[f].res[k] = (int16_t)(ch->f[f].res[k] + (base - ch->f[f].res[k]) / 8 + sr_rng_range(&r, -2, 2));
    }
    clamp_res(ch, c, f);
  }
  for (int d = 0; d < SR_MAX_DISTRICTS; d++) { /* shortages ease */
    if (ch->price_mod[d] > 0) ch->price_mod[d]--;
    if (ch->patrol_mod[d] > 0) ch->patrol_mod[d]--;
  }
  /* open contracts past their deadline: NPC crews try them */
  for (int i = 0; i < ch->contract_count; i++) {
    SrContract *k = &ch->contracts[i];
    if (!k->open || now < k->deadline || (ch->camp.set_piece && k->op == ch->camp.set_piece))
      continue; /* the set piece waits for the player */
    SrOperation *op = sr_chess_op(ch, k->op);
    bool ok = op && offscreen_success(ch, c, op, sr_hash_combine(ch->seed, (uint64_t)k->id * 7u + 3u));
    if (op && op->arc >= 0) ch->story.beats_offscreen++;
    sr_chess_resolve(ch, c, pop, log, k, ok, false, now);
  }
  /* internal operations (no contract) resolve on their own */
  for (int i = 0; i < ch->op_count; i++) {
    SrOperation *op = &ch->ops[i];
    if (!op->active || op->steps > 0 || now - op->started < SR_TICK_S) continue;
    op->active = false;
    op->succeeded = true;
    apply_success(ch, c, pop, log, op, now);
  }
  for (int f = 0; f < c->factions.count; f++) strategy(ch, c, f, now);
  gang_rebirth(ch, c, log, now);
  sr_politics_tick(ch, c, log, now);
  sr_market_tick(ch, c);
  sr_news_tick(ch, c, log, now);
  sr_story_tick(ch, c, pop, now);
  sr_saga_tick(ch, c, pop, now);
}

void sr_chess_advance(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, int64_t now) {
  while (ch->next_tick <= now) {
    sr_chess_tick(ch, c, pop, log, ch->next_tick);
    ch->next_tick += SR_TICK_S;
  }
}

uint64_t sr_chess_digest(const SrChess *ch) {
  uint64_t h = sr_hash_combine(ch->seed, (uint64_t)ch->ticks);
  for (int i = 0; i < ch->op_count; i++) {
    const SrOperation *o = &ch->ops[i];
    uint64_t v = (uint64_t)o->id << 32 | (uint64_t)o->type << 24 | (uint64_t)(uint8_t)o->client << 16 |
                 (uint64_t)(uint8_t)o->target << 8 | (uint64_t)o->succeeded;
    h = sr_hash_combine(h, v ^ (uint64_t)o->started);
  }
  for (int f = 0; f < SR_MAX_FACTIONS; f++) h = sr_hash_bytes(h, ch->f[f].res, sizeof ch->f[f].res);
  return h;
}
