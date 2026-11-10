/* The saga: the corporate game never ends, and it gets personal.
 *
 * Seasons. Two weeks after an ending the next season begins: the board is shaken by how the last
 * one ended (the winner is everybody's target, the burned corps rebuild, the crown gets heavy, the
 * machine is rebuilt), old grudges fade unless they are personal, the acts run again toward a new
 * climax with a different scenario, and every ending can be reached again.
 *
 * People. The landlord, the fixer, the neighbour, the handler, the rival runner, the journalist and
 * the Ghost each have a bond with the player (-100..100). They ask for things (money, a leak, a
 * name, a floor to sleep on); a yes or a no moves the bond and has consequences. Friends pay back,
 * warn of hunters, bury stories and vouch for the player; people who were let down sell the player
 * out; someone close can be taken by a corporation and lost. Bonds open endings.
 *
 * Plain data advanced by pure functions over (city, time), like the rest of the corporate game. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

void sr_saga_init(SrSaga *g) {
  memset(g, 0, sizeof *g);
  for (int i = 0; i < SR_SAGA_HISTORY; i++) g->kings[i] = -1;
  g->ask.corp = -1;
}

void sr_saga_bond(SrChess *ch, SrCastRole r, int delta) {
  if ((unsigned)r >= SR_CAST_COUNT) return;
  ch->saga.bond[r] = (int8_t)SR_CLAMP(ch->saga.bond[r] + delta, -100, 100);
}

const char *sr_saga_standing(int bond) {
  return bond <= SR_BOND_ENEMY    ? "ENEMY"
         : bond <= SR_BOND_COLD   ? "COLD"
         : bond >= SR_BOND_CLOSE  ? "CLOSE"
         : bond >= SR_BOND_FRIEND ? "FRIEND"
         : bond >= 15             ? "WARM"
                                  : "NEUTRAL";
}

const char *sr_saga_season_name(const SrSaga *g) {
  if (g->season == 0) return "The Climb";
  switch ((SrEnding)g->endings[(g->season - 1) % SR_SAGA_HISTORY]) {
    case SR_END_LOYAL: return "The Reckoning";
    case SR_END_DEFECTOR: return "Old Debts";
    case SR_END_BURN: return "Ashes";
    case SR_END_KINGMAKER: return "The Crown";
    case SR_END_SUNRISE: return "Daylight";
    default: return "The Long Night";
  }
}

const char *sr_saga_ask_name(SrAskType t) {
  static const char *const n[SR_ASK_COUNT] = {"-",     "debt", "bail", "source", "name",
                                              "truce", "hide", "deep", "lease",  "ransom"};
  return (unsigned)t < SR_ASK_COUNT ? n[t] : "?";
}

static uint32_t line_hash(const char *text) {
  uint32_t h = (uint32_t)sr_hash_bytes(0x5a1dull, text, strlen(text));
  return h ? h : 1;
}

bool sr_saga_was_said(const SrChess *ch, const char *text) {
  uint32_t h = line_hash(text);
  for (int i = 0; i < 8; i++)
    if (ch->saga.said[i] == h) return true;
  return false;
}

bool sr_saga_said(SrChess *ch, const char *text) {
  SrSaga *g = &ch->saga;
  if (sr_saga_was_said(ch, text)) return true;
  g->said[g->said_n % 8] = line_hash(text);
  g->said_n++;
  return false;
}

/* ---- helpers --------------------------------------------------------------------------------- */

static const char *name_of(const SrChess *ch, int role) { return ch->story.cast[role % SR_CAST_COUNT].name; }

static const char *corp_name(const SrCity *c, int f) {
  return f >= 0 && f < c->factions.count ? c->factions.f[f].name : "someone";
}

static uint64_t key_of(const SrChess *ch, int64_t now, uint64_t salt) {
  return sr_mix64(sr_hash_combine(ch->seed, (uint64_t)(now / 86400) * 131u + salt + (uint64_t)ch->saga.asks * 7919u));
}

static void sayf(SrChess *ch, SrCastRole role, const char *tmpl, const char *a, const char *b, const char *c3) {
  char line[112];
  sr_story_fill(line, sizeof line, tmpl, a, b, c3);
  sr_story_say(ch, role, line);
}

static void logf2(SrChess *ch, int64_t now, const char *tmpl, const char *a, const char *b) {
  char line[72];
  sr_story_fill(line, sizeof line, tmpl, a, b, NULL);
  sr_story_log(ch, now, line);
  sr_chess_note(ch, now, line);
}

static int32_t cost_of(const SrSaga *g, int pct, int32_t lo, int32_t hi) {
  int64_t v = SR_CLAMP(g->cash * pct / 100, (int64_t)lo, (int64_t)hi);
  if (v > g->cash / 2) v = g->cash / 2; /* never more than half of what the player has */
  return (int32_t)SR_MAX(v, 0);
}

/* The corporation that makes trouble for the player's people: the one hunting the player, else the
 * patron's worst rival. */
static int trouble_corp(const SrChess *ch, const SrCity *c, uint64_t key) {
  const SrStory *s = &ch->story;
  if (s->hostile >= 0) return s->hostile;
  int leader = 0;
  sr_chess_dominance(ch, c, &leader);
  return sr_story_rival_of(c, s->patron >= 0 ? s->patron : leader, key);
}

static bool alive(const SrChess *ch, int role) { return ch->story.cast[role].alive; }

/* Someone is gone for good; a successor takes the role two days later, a stranger. */
static void gone(SrChess *ch, SrCastRole r, int64_t now) {
  sr_story_cast_died(&ch->story, r, now);
  ch->saga.flags[r] = 0;
}

static void set_hunter(SrChess *ch, int corp, int64_t at) {
  if (corp < 0 || ch->camp.hunter >= 0) return;
  ch->camp.hunter = (int8_t)corp;
  ch->camp.hunt_at = at;
}

/* ---- seasons --------------------------------------------------------------------------------- */

static void boost(SrChess *ch, int f, SrResource r, int delta) {
  if (f < 0 || f >= SR_MAX_FACTIONS) return;
  int16_t lo, hi;
  sr_resource_bounds(r, &lo, &hi);
  ch->f[f].res[r] = (int16_t)SR_CLAMP(ch->f[f].res[r] + delta, lo, hi);
}

void sr_saga_ending(SrChess *ch, const SrCity *c, SrEnding e, int64_t now) {
  (void)c;
  (void)now;
  SrSaga *g = &ch->saga;
  switch (e) {
    case SR_END_LOYAL:
      sr_saga_bond(ch, SR_CAST_HANDLER, 15);
      sr_saga_bond(ch, SR_CAST_JOURNALIST, -10);
      break;
    case SR_END_DEFECTOR:
      sr_saga_bond(ch, SR_CAST_HANDLER, 10);
      sr_saga_bond(ch, SR_CAST_RIVAL, -10);
      break;
    case SR_END_BURN:
      sr_saga_bond(ch, SR_CAST_JOURNALIST, 25);
      sr_saga_bond(ch, SR_CAST_HANDLER, -60); /* this one is personal */
      break;
    case SR_END_KINGMAKER:
      sr_saga_bond(ch, SR_CAST_RIVAL, -15);
      sr_saga_bond(ch, SR_CAST_FIXER, 10);
      break;
    case SR_END_SUNRISE:
      sr_saga_bond(ch, SR_CAST_GHOST, 30);
      sr_saga_bond(ch, SR_CAST_HANDLER, -20);
      g->sun_seen = true;
      break;
    default: break;
  }
}

static void begin_season(SrChess *ch, SrCity *c, const SrPopulation *pop, int64_t now) {
  SrSaga *g = &ch->saga;
  SrStory *s = &ch->story;
  SrCampaign *cp = &ch->camp;
  SrEnding prev = (SrEnding)s->ending;
  int winner = prev == SR_END_KINGMAKER ? cp->king : prev == SR_END_LOYAL || prev == SR_END_DEFECTOR ? s->patron : -1;
  int slot = g->season % SR_SAGA_HISTORY;
  g->endings[slot] = (uint8_t)prev;
  g->scenarios[slot] = s->scenario;
  g->kings[slot] = (int8_t)winner;
  int shaken = s->scenario_corp;
  g->season++;
  g->season_started = now;
  g->switches = 0;
  /* old grudges fade with the old board, unless the handler made it personal */
  if (s->hostile >= 0 && g->bond[SR_CAST_HANDLER] > SR_BOND_COLD) {
    sayf(ch, SR_CAST_FIXER, "%1 called off the hunt. New board, bigger problems than you.", corp_name(c, s->hostile),
         NULL, NULL);
    if (cp->hunter == s->hostile) cp->hunter = -1;
    s->hostile = -1;
  }
  cp->blacklist = (uint16_t)(s->hostile >= 0 && s->hostile < 16 ? 1u << s->hostile : 0);
  /* a clean slate for the story's machinery */
  s->ending = SR_END_NONE;
  s->sandbox = false;
  s->finale_ready = false;
  s->scenario = SR_SC_NONE;
  s->scenario_corp = s->scenario_other = -1;
  s->rival_offer = -1;
  s->leaks = 0;
  memset(s->corps_worked, 0, sizeof s->corps_worked);
  for (int i = 0; i < SR_MAX_ARCS; i++) {
    s->arcs[i].active = false;
    cp->beat_at[i] = 0;
  }
  memset(cp->suspects, 0, sizeof cp->suspects);
  cp->set_piece = 0;
  cp->set_piece_done = false;
  cp->new_cycle = false;
  cp->ending_at = 0;
  cp->ready_at = 0;
  cp->king = -1;
  cp->defections = 0;
  cp->nudged = false;
  /* without a patron the last winner (or anyone who will have the player) makes an offer */
  if (s->patron < 0 || !s->retainer) {
    int pick = winner >= 0 && winner != s->hostile ? winner : -1;
    for (int k = 0; k < c->factions.corp_count && pick < 0; k++)
      if (k != s->hostile) pick = k;
    s->patron = (int8_t)pick;
    s->retainer = false;
    s->offer_open = pick >= 0;
    s->offer_told = false;
    s->trust = 30;
    s->act = SR_ACT_PAWN;
  } else {
    s->act = SR_ACT_KNIGHT;
  }
  s->first_patron = s->patron; /* loyalty is counted from here */
  s->act_started = now;
  s->next_offer = now;
  s->next_counter = now + 2 * SR_WEEK_S;
  /* how the last season ended decides what this one is about */
  int leader = 0;
  sr_chess_dominance(ch, c, &leader);
  int second = -1;
  for (int f = 0; f < c->factions.corp_count; f++)
    if (f != leader && (second < 0 || ch->f[f].res[SR_RES_CAPITAL] > ch->f[second].res[SR_RES_CAPITAL])) second = f;
  int a = s->patron >= 0 ? s->patron : leader, b = -1;
  SrArcType opener = SR_ARC_TAKEOVER;
  switch (prev) {
    case SR_END_LOYAL:
    case SR_END_DEFECTOR: /* the winner is everybody's target */
      for (int f = 0; f < c->factions.corp_count; f++)
        if (f != winner) boost(ch, f, SR_RES_CAPITAL, 15), boost(ch, f, SR_RES_FORCE, 8);
      boost(ch, winner, SR_RES_CAPITAL, -20);
      sayf(ch, SR_CAST_HANDLER, "We won, and now every knife in the city points at %1. Back to work.",
           corp_name(c, winner), NULL, NULL);
      b = second >= 0 && second != a ? second : sr_story_rival_of(c, a, (uint64_t)now);
      opener = SR_ARC_MOLE;
      break;
    case SR_END_BURN: /* the corps rebuild, the gangs smell blood */
      for (int f = 0; f < c->factions.corp_count; f++) boost(ch, f, SR_RES_CAPITAL, 20);
      for (int f = c->factions.gang_first; f < c->factions.gang_first + c->factions.gang_count; f++)
        boost(ch, f, SR_RES_FORCE, 15);
      sr_story_say(ch, SR_CAST_FIXER, "The corps are bleeding and the gangs smell it. Busiest season I've seen.");
      opener = SR_ARC_PROXY;
      a = -1;
      break;
    case SR_END_KINGMAKER: /* the crown gets heavy */
      boost(ch, winner, SR_RES_IMAGE, -15);
      for (int f = 0; f < c->factions.corp_count; f++)
        if (f != winner) boost(ch, f, SR_RES_INTEL, 10);
      sayf(ch, SR_CAST_FIXER, "The crown you handed %1 is getting heavy. Everyone else wants it off.",
           corp_name(c, winner), NULL, NULL);
      a = second >= 0 && second != winner ? second : a;
      b = winner;
      opener = SR_ARC_SCANDAL;
      break;
    case SR_END_SUNRISE: /* the machine is being rebuilt */
      boost(ch, shaken, SR_RES_CAPITAL, 35);
      sr_story_say(ch, SR_CAST_GHOST, "They are rebuilding the machine. I can hear it in the grid. We are not done.");
      a = s->patron >= 0 && s->patron != shaken ? s->patron : second;
      b = shaken;
      opener = SR_ARC_PROTOTYPE;
      break;
    default: break;
  }
  char title[72], num[8];
  snprintf(num, sizeof num, "%d", g->season + 1);
  sr_story_fill(title, sizeof title, "Season %1: %2", num, sr_saga_season_name(g), NULL);
  sr_story_log(ch, now, title);
  sr_chess_note(ch, now, title);
  char head[64];
  sr_headline(head, sizeof head, "A new board: %1 leads, rivals circle", corp_name(c, leader), NULL, NULL);
  sr_news_publish(ch, c, -1, leader, -1, -1, SR_NEWS_STORY, false, head, now);
  if (a >= 0 && b >= 0 && a == b) b = sr_story_rival_of(c, a, (uint64_t)now);
  sr_story_arc_between(ch, c, pop, opener, a, b, now);
}

/* ---- requests --------------------------------------------------------------------------------- */

void sr_saga_ask_label(const SrChess *ch, char *out, size_t cap) {
  const SrAsk *k = &ch->saga.ask;
  out[0] = '\0';
  if (!k->open) return;
  char cost[16];
  snprintf(cost, sizeof cost, "%d", (int)k->cost);
  static const char *const t[SR_ASK_COUNT] = {"",
                                              "Pay %1's debt (%2 cred)",
                                              "Post bail for %1 (%2 cred)",
                                              "Leak a file to %1",
                                              "Give %1 the reporter's name",
                                              "Split the job with %1",
                                              "Hide %1",
                                              "Carry the Ghost's packet",
                                              "Lend %1 the money (%2 cred)",
                                              "Pay the ransom for %1 (%2 cred)"};
  sr_story_fill(out, cap, t[k->type % SR_ASK_COUNT], name_of(ch, k->role), cost, NULL);
}

static void open_ask(SrChess *ch, SrAskType t, SrCastRole role, int corp, int32_t cost, int64_t now) {
  SrSaga *g = &ch->saga;
  g->ask = (SrAsk){(uint8_t)t, (uint8_t)role, (int8_t)corp, true, cost, now + SR_ASK_OPEN_S};
  g->last_ask[g->asks % 3] = (uint8_t)t;
  g->ask_day[t] = (int32_t)(now / 86400) + 1;
  g->asks++;
}

/* Someone close is taken: pay, or lose them. */
static bool try_ransom(SrChess *ch, const SrCity *c, int64_t now) {
  SrSaga *g = &ch->saga;
  const SrStory *s = &ch->story;
  if (now < g->next_ransom || (s->hostile < 0 && g->season == 0 && s->act < SR_ACT_PROMOTION)) return false;
  static const SrCastRole who[4] = {SR_CAST_NEIGHBOUR, SR_CAST_FIXER, SR_CAST_JOURNALIST, SR_CAST_RIVAL};
  uint64_t key = key_of(ch, now, 0x7a4eull);
  for (int k = 0; k < 4; k++) {
    SrCastRole r = who[(k + (int)(key % 4u)) % 4];
    if (!alive(ch, r) || g->bond[r] < SR_BOND_CLOSE || (g->flags[r] & SR_TIE_RANSOMED)) continue;
    int corp = trouble_corp(ch, c, key);
    if (corp < 0) return false;
    g->flags[r] |= SR_TIE_TAKEN | SR_TIE_RANSOMED;
    g->next_ransom = now + 10 * SR_WEEK_S; /* about once a season */
    int32_t cost = cost_of(g, 20, 2000, 40000);
    open_ask(ch, SR_ASK_RANSOM, r, corp, cost, now);
    char amount[16];
    snprintf(amount, sizeof amount, "%d", (int)cost);
    SrCastRole teller = r == SR_CAST_FIXER ? SR_CAST_NEIGHBOUR : SR_CAST_FIXER;
    if (!alive(ch, teller)) teller = SR_CAST_HANDLER;
    sayf(ch, teller, "%1 took %2. They want %3 cred in four days or they send pieces.", corp_name(c, corp),
         name_of(ch, r), amount);
    logf2(ch, now, "%1 was taken by %2", name_of(ch, r), corp_name(c, corp));
    return true;
  }
  return false;
}

static void make_ask(SrChess *ch, const SrCity *c, int64_t now) {
  SrSaga *g = &ch->saga;
  const SrStory *s = &ch->story;
  if (try_ransom(ch, c, now)) return;
  uint64_t key = key_of(ch, now, 0xa5cull);
  int corp = trouble_corp(ch, c, key);
  bool knight = s->act >= SR_ACT_KNIGHT;
  for (int tries = 0; tries < SR_ASK_COUNT * 2; tries++) {
    SrAskType t = (SrAskType)(1 + (int)((key + (uint64_t)tries * 5u) % (uint64_t)(SR_ASK_COUNT - 2)));
    if (t == g->last_ask[0] || t == g->last_ask[1] || t == g->last_ask[2]) continue;
    /* each kind has its own rhythm: a reporter's name once a season, a lease now and then */
    static const int8_t k_gap_weeks[SR_ASK_COUNT] = {0, 3, 4, 3, 12, 4, 5, 4, 6, 0};
    if (g->ask_day[t] > 0 && now / 86400 + 1 - g->ask_day[t] < k_gap_weeks[t] * 7) continue;
    char amount[16];
    switch (t) {
      case SR_ASK_DEBT: {
        if (!alive(ch, SR_CAST_NEIGHBOUR) || g->cash < 600) continue;
        int32_t cost = cost_of(g, 5, 300, 8000);
        snprintf(amount, sizeof amount, "%d", (int)cost);
        open_ask(ch, t, SR_CAST_NEIGHBOUR, -1, cost, now);
        sayf(ch, SR_CAST_NEIGHBOUR, "I'm short %1 cred with the wrong people. I hate asking. Can you cover it?", amount,
             NULL, NULL);
        return;
      }
      case SR_ASK_BAIL: {
        if (!alive(ch, SR_CAST_FIXER) || g->cash < 1000) continue;
        int32_t cost = cost_of(g, 8, 500, 12000);
        snprintf(amount, sizeof amount, "%d", (int)cost);
        open_ask(ch, t, SR_CAST_FIXER, -1, cost, now);
        sayf(ch, SR_CAST_FIXER, "They pulled me in on a nothing charge. Bail is %1 cred. I'll make it worth it.",
             amount, NULL, NULL);
        return;
      }
      case SR_ASK_SOURCE:
        if (!alive(ch, SR_CAST_JOURNALIST) || !knight || s->patron < 0) continue;
        open_ask(ch, t, SR_CAST_JOURNALIST, s->patron, 0, now);
        sayf(ch, SR_CAST_JOURNALIST, "I need one file from %1 to make the story stand. You're inside. Will you?",
             corp_name(c, s->patron), NULL, NULL);
        return;
      case SR_ASK_NAME:
        if (!alive(ch, SR_CAST_HANDLER) || !alive(ch, SR_CAST_JOURNALIST) || !knight || s->patron < 0 || !s->retainer ||
            g->bond[SR_CAST_JOURNALIST] < 15)
          continue;
        open_ask(ch, t, SR_CAST_HANDLER, s->patron, 0, now);
        sr_story_say(ch, SR_CAST_HANDLER,
                     "A reporter is sniffing around us. You know who. Give me the name and I forget you knew.");
        return;
      case SR_ASK_TRUCE:
        if (!alive(ch, SR_CAST_RIVAL)) continue;
        open_ask(ch, t, SR_CAST_RIVAL, -1, 0, now);
        sr_story_say(ch, SR_CAST_RIVAL,
                     "We keep tripping over each other. Split the next one with me? Half each, no knives.");
        return;
      case SR_ASK_HIDE: {
        SrCastRole r = key % 2u ? SR_CAST_RIVAL : SR_CAST_NEIGHBOUR;
        if (!alive(ch, r) || !knight || corp < 0) continue;
        open_ask(ch, t, r, corp, 0, now);
        sayf(ch, r, "%1 has people looking for me. I need a floor to sleep on for a few days. Please.",
             corp_name(c, corp), NULL, NULL);
        return;
      }
      case SR_ASK_DEEP:
        if (!alive(ch, SR_CAST_GHOST) || s->matrix_depth < 10) continue;
        open_ask(ch, t, SR_CAST_GHOST, -1, 0, now);
        sr_story_say(ch, SR_CAST_GHOST,
                     "I can show you who really moves the pieces. Carry a packet through the grid for me first.");
        return;
      case SR_ASK_LEASE: {
        if (!alive(ch, SR_CAST_LANDLORD) || g->cash < 4000 || corp < 0) continue;
        int32_t cost = cost_of(g, 10, 1000, 20000);
        snprintf(amount, sizeof amount, "%d", (int)cost);
        open_ask(ch, t, SR_CAST_LANDLORD, corp, cost, now);
        sayf(ch, SR_CAST_LANDLORD, "%1 wants the building. Lend me %2 cred and it stays mine, and your rent yours.",
             corp_name(c, corp), amount, NULL);
        return;
      }
      default: continue;
    }
  }
}

bool sr_saga_answer(SrChess *ch, SrCity *c, const SrPopulation *pop, bool yes, int64_t now) {
  SrSaga *g = &ch->saga;
  SrStory *s = &ch->story;
  if (!g->ask.open) return false;
  SrAsk k = g->ask;
  g->ask.open = false;
  SrCastRole r = (SrCastRole)(k.role % SR_CAST_COUNT);
  const char *who = name_of(ch, r);
  if (yes)
    g->helped++;
  else
    g->refused++;
  uint64_t key = key_of(ch, now, 0xa25ull);
  switch ((SrAskType)k.type) {
    case SR_ASK_DEBT:
      if (yes) {
        sr_saga_bond(ch, r, 25);
        g->flags[r] |= SR_TIE_OWES;
        sr_story_say(ch, r, "You didn't have to. I won't forget it.");
        logf2(ch, now, "You paid %1's debt", who, NULL);
      } else {
        sr_saga_bond(ch, r, -20);
        sr_story_say(ch, r, "Right. Forget I asked.");
        if (g->bond[r] <= SR_BOND_COLD && key % 2u) { /* the debt catches up with them */
          logf2(ch, now, "%1 lost the flat over a debt", who, NULL);
          gone(ch, r, now);
        }
      }
      break;
    case SR_ASK_BAIL:
      if (yes) {
        sr_saga_bond(ch, r, 25);
        g->flags[r] |= SR_TIE_OWES;
        sr_story_say(ch, r, "Out by morning. I owe you one, and I pay what I owe.");
        logf2(ch, now, "You bailed out %1", who, NULL);
      } else {
        sr_saga_bond(ch, r, -25);
        sr_story_say(ch, r, "I'll remember who picked up, and who didn't.");
      }
      break;
    case SR_ASK_SOURCE:
      if (yes) {
        sr_saga_bond(ch, r, 25);
        s->leaks++;
        if (s->patron >= 0) {
          sr_story_trust(s, -8);
          sr_news_plant(ch, c, s->patron, -1, now);
        }
        sr_story_say(ch, r, "This is gold. They'll never trace it to you. Probably.");
        logf2(ch, now, "You leaked a file to %1", who, NULL);
      } else {
        sr_saga_bond(ch, r, -15);
        sr_story_say(ch, r, "Then you're one of them. Fine.");
      }
      break;
    case SR_ASK_NAME:
      if (yes) { /* loyalty has a price, and someone else pays it */
        sr_story_trust(s, 15);
        sr_saga_bond(ch, SR_CAST_HANDLER, 20);
        sr_saga_bond(ch, SR_CAST_FIXER, -10); /* word gets around */
        sr_saga_bond(ch, SR_CAST_NEIGHBOUR, -5);
        sr_story_say(ch, SR_CAST_HANDLER, "Good. It never happened.");
        sayf(ch, SR_CAST_FIXER, "%1, the reporter, is gone. A car, the canal, no witnesses. You hear anything?",
             name_of(ch, SR_CAST_JOURNALIST), NULL, NULL);
        logf2(ch, now, "You gave up %1. The reporter is dead", name_of(ch, SR_CAST_JOURNALIST), NULL);
        gone(ch, SR_CAST_JOURNALIST, now);
        g->lost++;
      } else {
        sr_story_trust(s, -10);
        sr_saga_bond(ch, SR_CAST_HANDLER, -15);
        sr_saga_bond(ch, SR_CAST_JOURNALIST, 20);
        sr_story_say(ch, SR_CAST_HANDLER, "Careful. Loyalty isn't a thing you do halfway.");
        sr_story_say(ch, SR_CAST_JOURNALIST, "I heard what you didn't do. Thank you. I mean it.");
        logf2(ch, now, "You kept %1's name from the handler", name_of(ch, SR_CAST_JOURNALIST), NULL);
      }
      break;
    case SR_ASK_TRUCE:
      if (yes) {
        sr_saga_bond(ch, r, 20);
        g->refund = SR_CLAMP((int32_t)(g->cash * 3 / 100), 300, 3000);
        g->refund_role = (uint8_t)r;
        sr_story_say(ch, r, "Deal. Your half is on its way. Try to keep up.");
      } else {
        sr_saga_bond(ch, r, -12);
        sr_story_say(ch, r, "Your loss. See you on the job.");
      }
      break;
    case SR_ASK_HIDE:
      if (yes) {
        sr_saga_bond(ch, r, 30);
        g->flags[r] |= SR_TIE_OWES;
        sr_story_say(ch, r, "I'll be gone before you notice. Thank you.");
        logf2(ch, now, "You hid %1 from %2", who, corp_name(c, k.corp));
        if (key % 3u == 0) set_hunter(ch, k.corp, now + 2 * 86400); /* they come looking at your door */
      } else {
        sr_saga_bond(ch, r, -30);
        sr_story_say(ch, r, "Then I was never here.");
        if (key % 2u) {
          sayf(ch, alive(ch, SR_CAST_FIXER) ? SR_CAST_FIXER : SR_CAST_HANDLER,
               "%1 didn't make it out of the district. %2 doesn't leave loose ends.", who, corp_name(c, k.corp), NULL);
          logf2(ch, now, "%1 was caught by %2", who, corp_name(c, k.corp));
          gone(ch, r, now);
          g->lost++;
        }
      }
      break;
    case SR_ASK_DEEP:
      if (yes) {
        int leader = 0;
        sr_chess_dominance(ch, c, &leader);
        sr_saga_bond(ch, r, 20);
        s->matrix_depth += 3;
        sr_chess_reveal_faction(ch, leader);
        sr_story_say(ch, r, "Done. Look at your Board. Now you see it too.");
      } else {
        sr_saga_bond(ch, r, -10);
        sr_story_say(ch, r, "Then stay on the surface with the others.");
      }
      break;
    case SR_ASK_LEASE:
      if (yes) {
        sr_saga_bond(ch, r, 25);
        g->flags[r] |= SR_TIE_OWES;
        sr_story_say(ch, r, "Signed. You won't regret it. You'll have it back, with interest.");
        logf2(ch, now, "You helped %1 keep the building", who, NULL);
      } else {
        sr_saga_bond(ch, r, -15);
        sr_story_say(ch, r, "Then we both get a new landlord. Good luck with theirs.");
      }
      break;
    case SR_ASK_RANSOM:
      g->flags[r] &= (uint8_t)~SR_TIE_TAKEN;
      if (yes) {
        sr_saga_bond(ch, r, 25);
        sr_story_say(ch, r, "I'm out. I know what that cost you. I'll spend a long time paying it back.");
        logf2(ch, now, "You paid %2 for %1's life", who, corp_name(c, k.corp));
      } else { /* the worst message of the season */
        SrCastRole teller = r == SR_CAST_FIXER ? SR_CAST_NEIGHBOUR : SR_CAST_FIXER;
        if (!alive(ch, teller)) teller = SR_CAST_HANDLER;
        sayf(ch, teller, "They found %1 this morning. I'm sorry. %2 wanted you to know it was them.", who,
             corp_name(c, k.corp), NULL);
        logf2(ch, now, "%1 is dead. %2 did it", who, corp_name(c, k.corp));
        gone(ch, r, now);
        g->lost++;
        /* someone will want to make them pay */
        int a = s->patron >= 0 && s->patron != k.corp ? s->patron : sr_story_rival_of(c, k.corp, key);
        sr_story_arc_between(ch, c, pop, SR_ARC_SCANDAL, a, k.corp, now);
      }
      break;
    default: break;
  }
  return true;
}

/* ---- betrayals and favours --------------------------------------------------------------------- */

static void betray(SrChess *ch, SrCity *c, SrCastRole r, int64_t now) {
  SrSaga *g = &ch->saga;
  SrStory *s = &ch->story;
  uint64_t key = key_of(ch, now, 0xbe7ull + (uint64_t)r);
  int corp = trouble_corp(ch, c, key);
  const char *who = name_of(ch, r);
  g->flags[r] |= SR_TIE_BETRAYED;
  g->betrayals++;
  logf2(ch, now, "%1 sold you out", who, NULL);
  switch (r) {
    case SR_CAST_FIXER:
      sr_story_say(ch, r, "Nothing personal. They pay better than you do. Watch your back this week.");
      set_hunter(ch, corp, now + 6 * 3600);
      gone(ch, r, now); /* and skips town */
      break;
    case SR_CAST_NEIGHBOUR:
      sr_story_say(ch, r, "I told them which door is yours. I'm sorry. I needed the money.");
      set_hunter(ch, corp, now + 6 * 3600);
      gone(ch, r, now);
      break;
    case SR_CAST_JOURNALIST: {
      char head[64];
      sr_headline(head, sizeof head, "Exposed: the runner behind the %1 raids", corp_name(c, corp), NULL, NULL);
      sr_news_publish(ch, c, -1, -1, -1, -2, SR_NEWS_STORY, false, head, now);
      if (s->patron >= 0) sr_story_trust(s, -10);
      sr_story_say(ch, r, "You're the story now. You could have been the source.");
      g->bond[r] = -20;
      break;
    }
    case SR_CAST_HANDLER:
      if (s->patron >= 0) sr_story_trust(s, -20);
      sr_story_say(ch, r, "I covered for you upstairs. That stops today.");
      g->bond[r] = -20;
      break;
    case SR_CAST_RIVAL:
      if (s->patron >= 0) sr_story_trust(s, -8);
      if (corp >= 0) boost(ch, corp, SR_RES_INTEL, 10);
      sayf(ch, r, "That contract you were counting on? Mine now. %1 pays old friends well.", corp_name(c, corp), NULL,
           NULL);
      g->bond[r] = -20;
      break;
    default: /* the landlord and the Ghost just stop talking */ g->bond[r] = -20; break;
  }
}

/* One favour a week from someone close or in the player's debt. */
static void perk(SrChess *ch, SrCity *c, int64_t now) {
  SrSaga *g = &ch->saga;
  SrStory *s = &ch->story;
  SrCampaign *cp = &ch->camp;
  int start = (int)((uint64_t)(now / SR_WEEK_S) % SR_CAST_COUNT);
  for (int k = 0; k < SR_CAST_COUNT; k++) {
    SrCastRole r = (SrCastRole)((start + k) % SR_CAST_COUNT);
    bool close = g->bond[r] >= SR_BOND_CLOSE, owes = (g->flags[r] & SR_TIE_OWES) != 0;
    if (!alive(ch, r) || (!close && !owes) || (g->flags[r] & SR_TIE_TAKEN)) continue;
    switch (r) {
      case SR_CAST_FIXER:
        if (s->hostile < 0 || s->next_hunt - now > 3 * 86400) continue;
        s->next_hunt += 4 * 86400;
        sr_story_say(ch, r, "They were coming for you this week. I sent them to the wrong district.");
        break;
      case SR_CAST_NEIGHBOUR:
        if (cp->medical_debt > 0) {
          cp->medical_debt = 0;
          sr_story_say(ch, r, "The clinic lost your bill. Don't ask me how.");
        } else if (owes && g->refund == 0) {
          g->refund = SR_CLAMP((int32_t)(g->cash / 100), 200, 2000);
          g->refund_role = (uint8_t)r;
          sr_story_say(ch, r, "Here's some of what I owe you. More when I have it.");
        } else {
          continue;
        }
        break;
      case SR_CAST_JOURNALIST:
        if (!close || ch->news.pending_count == 0 || !sr_news_kill(ch, -1)) continue;
        sr_story_say(ch, r, "A story with your face on it just died on my desk. You're welcome.");
        break;
      case SR_CAST_HANDLER:
        if (!close || s->patron < 0 || s->trust >= 60) continue;
        sr_story_trust(s, 5);
        sr_story_say(ch, r, "I vouched for you upstairs. Don't make me a liar.");
        break;
      case SR_CAST_RIVAL:
        if (s->patron < 0 || s->trust >= 90) continue;
        sr_story_trust(s, 3);
        sr_story_say(ch, r, "I took the heat for that last one. We're even. Almost.");
        break;
      case SR_CAST_GHOST: {
        if (!close) continue;
        int f = (int)(key_of(ch, now, 0x6057ull) % (uint64_t)SR_MAX(1, c->factions.corp_count));
        if (sr_chess_reveal_faction(ch, f) <= 0) continue;
        sr_story_say(ch, r, "Another thread for your Board. Pull it.");
        break;
      }
      default: /* the landlord pays back */
        if (!owes || g->refund != 0) continue;
        g->refund = SR_CLAMP((int32_t)(g->cash * 2 / 100), 1000, 4000);
        g->refund_role = (uint8_t)r;
        sr_story_say(ch, r, "First instalment. Told you you'd have it back.");
        break;
    }
    g->flags[r] &= (uint8_t)~SR_TIE_OWES;
    return;
  }
}

void sr_saga_patron_changed(SrChess *ch, int64_t now) {
  SrSaga *g = &ch->saga;
  memset(ch->camp.suspects, 0, sizeof ch->camp.suspects); /* the old patron's suspicions stay with it */
  if (g->switches < 250) g->switches++;
  /* every new employer makes the next one warier */
  ch->story.next_counter = now + (int64_t)(1 + g->switches) * SR_WEEK_S;
}

void sr_saga_tick(SrChess *ch, SrCity *c, const SrPopulation *pop, int64_t now) {
  SrSaga *g = &ch->saga;
  SrStory *s = &ch->story;
  if (s->act == SR_ACT_EPILOGUE && ch->camp.ending_at > 0 && now >= ch->camp.ending_at + SR_INTERLUDE_S)
    begin_season(ch, c, pop, now);
  /* a successor is a stranger */
  for (int r = 0; r < SR_CAST_COUNT; r++)
    if (!s->cast[r].alive && !(g->flags[r] & SR_TIE_TAKEN)) g->bond[r] = 0, g->flags[r] = 0;
  bool started = s->retainer || g->season > 0 || s->hostile >= 0;
  if (!started) return;
  if (g->next_ask == 0) {
    g->next_ask = now + 5 * 86400;
    g->next_perk = now + SR_WEEK_S;
  }
  /* the Ghost warms to a runner who goes deep */
  if (g->bond[SR_CAST_GHOST] < SR_MIN(45, s->matrix_depth / 2)) sr_saga_bond(ch, SR_CAST_GHOST, 1);
  if (g->ask.open && now >= g->ask.deadline) sr_saga_answer(ch, c, pop, false, now); /* silence is an answer */
  for (int r = 0; r < SR_CAST_COUNT; r++) {
    if (!alive(ch, r) || (g->flags[r] & SR_TIE_TAKEN)) continue;
    if (g->bond[r] <= SR_BOND_ENEMY && !(g->flags[r] & SR_TIE_BETRAYED)) {
      betray(ch, c, (SrCastRole)r, now);
    } else if (g->bond[r] >= SR_BOND_CLOSE && !(g->flags[r] & SR_TIE_CLOSE)) {
      g->flags[r] |= SR_TIE_CLOSE;
      static const char *const k_close[SR_CAST_COUNT] = {
          "You're the only tenant I'd miss. Don't tell the others.",
          "I don't have friends in this business. You're the exception.",
          "You're the only one in this building I trust. Knock any time.",
          "Off the record: I'd take a bullet for you. Don't make me.",
          "Never thought I'd say it. I'd rather work with you than against you.",
          "If anything happens to me, my notes go to you. All of them.",
          "You listen. Nobody listens. I will not forget you."};
      sr_story_say(ch, (SrCastRole)r, k_close[r]);
      logf2(ch, now, "%1 trusts you", name_of(ch, r), NULL);
    }
  }
  if (now >= g->next_perk) {
    perk(ch, c, now);
    g->next_perk = now + SR_WEEK_S;
    /* friendship needs keeping up: strong feelings fade a little every week, both ways */
    for (int r = 0; r < SR_CAST_COUNT; r++) {
      if (g->bond[r] > 40) sr_saga_bond(ch, (SrCastRole)r, -3);
      if (g->bond[r] < -20) sr_saga_bond(ch, (SrCastRole)r, 2);
    }
  }
  if (!g->ask.open && now >= g->next_ask && s->act >= SR_ACT_PAWN && s->act != SR_ACT_EPILOGUE) {
    make_ask(ch, c, now);
    g->next_ask = now + (int64_t)(5 + key_of(ch, now, 0x11ull) % 4u) * 86400;
  }
}
