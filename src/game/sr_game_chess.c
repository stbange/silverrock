/* The corporate game in play: contracts appear as jobs through their channels, the player's job
 * outcomes resolve contracts and move the story, the board's effects reach prices, the police and
 * the streets, Matrix hosts of outlets and city hall offer their own actions, and the terminal
 * shows the news, the market, the intel Board and the story's choices. */
#include "core/sr_rng.h"
#include "game/sr_game.h"

#include <stdio.h>
#include <string.h>

static int64_t now_of(const SrGame *g) { return sr_game_time(g); }
static void sync_world(SrGame *g, int64_t t);
static void hunters(SrGame *g, int64_t t);
static void living_costs(SrGame *g, int64_t t);
static void notoriety_story(SrGame *g, int64_t t);
static void deliver_story(SrGame *g);

void sr_game_chess_init(SrGame *g) {
  sr_chess_init(&g->chess, g->city, sr_hash_combine(g->seed, 0xC4E55u), g->patron_choice, now_of(g));
  g->chess.news.events_seen = g->events.count;
  g->curfew_stop_at = 0;
  g->confirm_job = -1;
  g->enc.active = false;
  g->last_raid = -SR_WEEK_S; /* no raid cooldown at the start */
  g->term_tab = SR_TAB_HOME;
  g->term_sel = 0;
}

void sr_game_chess_loaded(SrGame *g) {
  sr_chess_apply_changes(&g->chess, g->city);
  g->chess.news.events_seen = g->events.count; /* the event log was rebuilt */
  sync_world(g, now_of(g));
}

bool sr_game_job_against_patron(const SrGame *g, const SrJob *j) {
  return g->chess.story.retainer && sr_story_against_patron(&g->chess, j->target_faction);
}

int sr_game_police(const SrGame *g) { return sr_politics_police(&g->chess.pol, g->city); }

int sr_game_jurisdiction(const SrGame *g, int b) {
  int j = sr_heat_jurisdiction(g->city, b);
  return j == g->city->factions.police ? sr_game_police(g) : j;
}

int sr_game_price_pct(const SrGame *g, int b) {
  if (b < 0 || b >= g->city->building_count) return 100;
  return sr_chess_price_pct(&g->chess, g->city->blocks[g->city->buildings[b].block].district);
}

static const char *patron_name(const SrGame *g) {
  int p = g->chess.story.patron;
  return p >= 0 ? g->city->factions.f[p].name : "nobody";
}

static void handler_says(SrGame *g, const char *text) {
  char from[24];
  snprintf(from, sizeof from, "%s", g->chess.story.cast[SR_CAST_HANDLER].name);
  sr_player_message(&g->player, from, text, now_of(g));
}

/* Story beats from the handler stay in the inbox. */
static void handler_story(SrGame *g, const char *text) {
  char from[24];
  snprintf(from, sizeof from, "%s", g->chess.story.cast[SR_CAST_HANDLER].name);
  sr_player_story_message(&g->player, from, text, now_of(g));
}

/* ---- contracts as jobs ---------------------------------------------------------------------- */

static uint8_t source_of(uint8_t channel) {
  switch (channel) {
    case SR_CH_FIXER: return SR_SRC_FIXER;
    case SR_CH_BOARD: return SR_SRC_BOARD;
    default: return SR_SRC_CONTACT; /* the handler and gang contacts call */
  }
}

static void post_contracts(SrGame *g, int64_t t) {
  SrChess *ch = &g->chess;
  SrOperation *spo = ch->camp.set_piece ? sr_chess_op(ch, ch->camp.set_piece) : NULL;
  SrContract *sp = spo && spo->active ? sr_chess_contract(ch, spo->contract) : NULL;
  SrJob *spj = sp && sp->job_id >= 0 ? sr_jobs_get(&g->jobs, sp->job_id) : NULL;
  if (sp && sp->open && sp->job_id >= 0 && (!spj || spj->state == SR_JS_WITHDRAWN)) {
    sp->job_id = -1; /* gone from the board: post the final job again */
    spj = NULL;
  }
  if (spj && (spj->state == SR_JS_OFFERED || spj->state == SR_JS_ACCEPTED) && spj->deadline - t < 2 * 86400)
    spj->deadline = t + 7 * 86400; /* the set piece never expires */
  if (sp) sp->deadline = SR_MAX(sp->deadline, t + 86400);
  for (int i = 0; i < ch->contract_count; i++) {
    SrContract *k = &ch->contracts[i];
    if (!k->open || k->job_id != -1 || t >= k->deadline) continue;
    /* only work the player can take reaches them; the rest goes to NPC crews */
    if (sr_job_tier_of_type((SrJobType)k->job_type) > sr_max_tier_for_cred(g->player.prog.street_cred) &&
        !(ch->camp.set_piece && k->op == ch->camp.set_piece))
      continue;
    SrJob j;
    uint64_t key = sr_hash_combine(ch->seed, (uint64_t)k->id * 31u + 7u);
    SrOperation *op = sr_chess_op(ch, k->op);
    if (!sr_job_for_contract(g->city, (SrJobType)k->job_type, k->building, k->client, op ? op->target : -1, key, t, &j))
      continue; /* GCOVR_EXCL_LINE reason: generation always binds */
    j.source = source_of(k->channel);
    j.deadline = k->deadline;
    int d = g->city->blocks[g->city->buildings[j.target_building].block].district;
    /* the district's fixer brokers everything but the handler's work */
    j.client_building = k->channel == SR_CH_HANDLER ? g->player.home : g->city->factions.fixer_building[d];
    if (j.client_building < 0) j.client_building = g->player.home;
    if (j.tier == 2 && sr_max_tier_for_cred(g->player.prog.street_cred) >= 3 && key % 10u < 4u &&
        j.client_faction >= 0 && j.client_faction < g->city->factions.corp_count) {
      int32_t lo, hi; /* an Operator-level variant of the job */
      j.tier = 3;
      sr_tier_pay_range(3, &lo, &hi);
      j.pay = lo + (int32_t)((key >> 8) % (uint64_t)(hi - lo));
      j.upfront = j.pay / 4;
    }
    if (ch->camp.set_piece && k->op == ch->camp.set_piece) { /* the climax's set piece: it waits for the player */
      j.client_building = g->player.home;
      j.deadline = t + 14 * 86400;
      char title[64];
      snprintf(title, sizeof title, "FINAL JOB: %s", sr_job_type_name((SrJobType)j.type));
      snprintf(j.title, sizeof j.title, "%s", title);
    }
    SrJob *s = sr_jobs_push(&g->jobs, &j);
    if (!s) return;
    k->job_id = s->id;
    if (k->channel == SR_CH_HANDLER) {
      char msg[112];
      snprintf(msg, sizeof msg, "%s needs this done quietly: %s.", patron_name(g), s->title);
      handler_says(g, msg);
    }
  }
}

/* ---- fixers -------------------------------------------------------------------------------- */

static int district_at(const SrCity *c, int b) { return c->blocks[c->buildings[b].block].district; }

/* The district and its two nearest neighbours. */
static int area_of(const SrCity *c, int d, int out[3]) {
  int n = 0;
  out[n++] = d;
  for (int pass = 0; pass < 2; pass++) {
    int best = -1;
    int64_t bd = INT64_MAX;
    for (int k = 0; k < c->district_count; k++) {
      if (k == out[0] || (n > 1 && k == out[1])) continue;
      int64_t dx = c->districts[k].cx - c->districts[d].cx, dy = c->districts[k].cy - c->districts[d].cy;
      if (dx * dx + dy * dy < bd) bd = dx * dx + dy * dy, best = k;
    }
    if (best >= 0) out[n++] = best;
  }
  return n;
}

static int collect_offers(SrGame *g, const int *area, int na, int out[SR_FIXER_OFFERS]) {
  int n = 0, max = sr_max_tier_for_cred(g->player.prog.street_cred);
  for (int i = 0; i < g->jobs.count && n < SR_FIXER_OFFERS; i++) {
    const SrJob *j = &g->jobs.jobs[i];
    if (j->state != SR_JS_OFFERED || j->tier > max || j->client_building == g->player.home) continue;
    if (!sr_chess_contract_for_job(&g->chess, j->id)) continue; /* only steps of operations */
    int d = district_at(g->city, j->target_building);
    for (int a = 0; a < na; a++)
      if (d == area[a]) {
        out[n++] = j->id;
        break;
      }
  }
  return n;
}

int sr_game_fixer(SrGame *g, int fixer, int out[SR_FIXER_OFFERS]) {
  int64_t t = now_of(g);
  if (fixer < 0) return 0;
  int area[3];
  int na = area_of(g->city, district_at(g->city, fixer), area);
  post_contracts(g, t);
  int n = collect_offers(g, area, na, out);
  /* too little work around: the district's factions start operations of fitting tiers */
  int max = sr_max_tier_for_cred(g->player.prog.street_cred);
  for (int a = 0; a < na && n < 3; a++) {
    int tier = (int)(sr_mix64(sr_hash_combine(g->chess.seed, (uint64_t)(t / 86400) * 7u + (uint64_t)a)) % 2u)
                   ? max
                   : (int)(sr_mix64((uint64_t)t + (uint64_t)a) % (uint64_t)(max + 1));
    if (sr_chess_demand_op(&g->chess, g->city, area[a], SR_MIN(tier, 2), t) < 0) continue;
    post_contracts(g, t);
    n = collect_offers(g, area, na, out);
  }
  return n;
}

bool sr_game_jobs_open(SrGame *g, int fixer) {
  int ids[SR_FIXER_OFFERS];
  int n = sr_game_fixer(g, fixer, ids);
  if (n == 0) return false;
  SrMenu *m = &g->menu;
  memset(m, 0, sizeof *m);
  m->building = fixer;
  m->job_menu = true;
  for (int i = 0; i < n && i < SR_MENU_MAX; i++) m->job[m->count++] = ids[i];
  for (int f = 0; f < g->city->factions.count; f++)
    if (g->heat.level[f] > 0) { /* the fixer can make a call about the Heat */
      m->job[m->count++] = SR_JOB_HEAT_CLEANUP;
      break;
    }
  g->screen = SR_SCREEN_SHOP;
  g->confirm_job = -1;
  return true;
}

int sr_game_terminal_jobs(const SrGame *g, int32_t *out, int max) {
  int n = 0;
  /* 0 handler, 1 gang contacts, 2 public board, 3 accepted */
  for (int group = 0; group < 4; group++)
    for (int i = 0; i < g->jobs.count && n < max; i++) {
      const SrJob *j = &g->jobs.jobs[i];
      bool offered = j->state == SR_JS_OFFERED, handler = j->client_building == g->player.home;
      bool in = group == 0   ? offered && handler
                : group == 1 ? offered && !handler && j->source == SR_SRC_CONTACT
                : group == 2 ? offered && !handler && j->source == SR_SRC_BOARD
                             : j->state == SR_JS_ACCEPTED;
      if (in && group < 3 && j->tier > sr_max_tier_for_cred(g->player.prog.street_cred) && j->tier != 4) in = false;
      if (in) out[n++] = j->id;
    }
  return n;
}

int sr_game_handler_offers(SrGame *g) {
  SrStory *s = &g->chess.story;
  if (!sr_story_t4_allowed(s, sr_cred_tier(g->player.prog.street_cred))) return 0;
  int32_t day = (int32_t)(now_of(g) / 86400) + 1; /* stored +1: 0 means never */
  if (g->chess.camp.last_blackop_day > 0 && day - g->chess.camp.last_blackop_day < 7) return 0; /* weekly */
  for (int i = 0; i < g->jobs.count; i++) /* one black op at a time */
    if (g->jobs.jobs[i].tier == 4 &&
        (g->jobs.jobs[i].state == SR_JS_OFFERED || g->jobs.jobs[i].state == SR_JS_ACCEPTED))
      return 0;
  int64_t t = now_of(g);
  static const SrJobType types[3] = {SR_JT_STEAL_DATA, SR_JT_SABOTAGE, SR_JT_EXTRACT};
  uint64_t key = sr_hash_combine(g->chess.seed, (uint64_t)(t / 86400) * 3u + 0xB1ACu);
  SrJobType type = types[sr_mix64(key) % 3u];
  int rival = -1, low = 1000;
  for (int k = 0; k < g->city->factions.corp_count; k++)
    if (k != s->patron && g->city->factions.rel[s->patron][k] < low)
      low = g->city->factions.rel[s->patron][k], rival = k;
  if (rival < 0) return 0; /* GCOVR_EXCL_LINE reason: every city has several corps */
  int hq = g->city->factions.f[rival].hq_building;
  SrJob j;
  if (!sr_job_for_contract(g->city, type, hq, s->patron, rival, key, t, &j))
    return 0; /* GCOVR_EXCL_LINE reason: generation always binds */
  j.tier = 4;
  int32_t lo, hi;
  sr_tier_pay_range(4, &lo, &hi);
  j.pay = (lo + (int32_t)(sr_mix64(key + 1u) % (uint64_t)(hi - lo))) / 2; /* the other half is a favour */
  j.upfront = j.pay / 4;
  j.source = SR_SRC_CONTACT;
  j.client_building = g->player.home;
  j.deadline = t + 4 * 86400;
  snprintf(j.title, sizeof j.title, "Black op: %s at %s", sr_job_type_name(type), g->city->factions.f[rival].name);
  SrJob *p = sr_jobs_push(&g->jobs, &j);
  if (!p) return 0; /* GCOVR_EXCL_LINE reason: the board always has room for one more */
  g->chess.camp.last_blackop_day = day;
  handler_says(g, "Something deniable for you. Half in cred, half in favours. Check your jobs.");
  return 1;
}

/* Street Cred for notable work: a job at the edge of what the player can do, a high-profile target,
 * a netrunner's Matrix plan; failures cost. At most SR_CRED_WEEK_CAP a week, and the story gates
 * Knight (Act II) and Legend (Act III or an ending). */
static void award_cred(SrGame *g, const SrJob *j, bool done, int64_t t) {
  SrCampaign *cp = &g->chess.camp;
  SrProgress *pr = &g->player.prog;
  int gain;
  if (!done) {
    gain = -1;
  } else {
    gain = j->tier >= sr_max_tier_for_cred(pr->street_cred) ? 1 : 0;
    bool high = j->target_faction >= 0 && j->target_faction < g->city->factions.corp_count;
    if (gain > 0 && high) gain++;
    if (gain > 0 && j->approach == SR_AP_RUNNER) gain++; /* the Matrix plan counts too */
  }
  int32_t week = (int32_t)(t / SR_WEEK_S);
  if (cp->cred_week != week) cp->cred_week = week, cp->cred_gained = 0;
  if (gain > 0) {
    gain = SR_MIN(gain, SR_CRED_WEEK_CAP - cp->cred_gained);
    if (gain <= 0) return;
    int cap = g->chess.story.act >= SR_ACT_PROMOTION || g->chess.story.ending != SR_END_NONE || g->chess.saga.season > 0
                  ? 100
              : g->chess.story.act >= SR_ACT_KNIGHT ? 89
                                                    : 69;
    gain = SR_MIN(gain, cap - pr->street_cred);
    if (gain <= 0) return;
    cp->cred_gained = (int8_t)(cp->cred_gained + gain);
  }
  if (sr_progress_add_cred(pr, gain)) {
    char msg[96];
    snprintf(msg, sizeof msg, "Street Cred tier: %s", sr_cred_tier_name(sr_cred_tier(pr->street_cred)));
    sr_game_toast(g, msg);
    if (gain > 0) notoriety_story(g, t); /* a new name on the street */
  }
}

/* Half a black op's pay comes as a favour. */
static void blackop_reward(SrGame *g, const SrJob *j, int64_t t) {
  uint64_t h = sr_mix64(sr_hash_combine(g->chess.seed, (uint64_t)j->id));
  char msg[112];
  switch (h % 3u) {
    case 0: {
      int f = j->target_faction >= 0 ? j->target_faction : 0;
      sr_chess_reveal_faction(&g->chess, f);
      snprintf(msg, sizeof msg, "As promised: everything we have on %s. Check your Board.",
               g->city->factions.f[f].name);
      break;
    }
    case 1: {
      int worst = 0;
      for (int f = 1; f < g->city->factions.count; f++)
        if (g->heat.level[f] > g->heat.level[worst]) worst = f;
      g->heat.level[worst] = 0;
      snprintf(msg, sizeof msg, "We made a call. %s has forgotten your face.", g->city->factions.f[worst].name);
      break;
    }
    default: {
      SrItem it = sr_item_simple(SR_IK_CYBER, (int)((h >> 8) % 8u), 1);
      sr_inv_add(&g->player.inv, it);
      snprintf(msg, sizeof msg, "A gift from our clinic. Have it installed somewhere discreet.");
      break;
    }
  }
  handler_story(g, msg);
  (void)t;
}

/* Books job outcomes: contracts, trust, the test gig, insider suspicion. */
static void book_jobs(SrGame *g, int64_t t) {
  SrChess *ch = &g->chess;
  for (int i = 0; i < g->jobs.count; i++) {
    SrJob *j = &g->jobs.jobs[i];
    if (j->booked) continue;
    bool done = j->state == SR_JS_COMPLETED, failed = j->state == SR_JS_FAILED;
    SrContract *k = sr_chess_contract_for_job(ch, j->id);
    if (j->state == SR_JS_WITHDRAWN) { /* never taken: the contract goes to an NPC crew */
      j->booked = true;
      if (k) k->job_id = -2;
      continue;
    }
    if (!done && !failed) continue;
    j->booked = true;
    int evidence = (j->alarms > 0) + (j->kills > 0) + (j->alarms > 1);
    if (k && k->open && failed && j->abandoned && ch->camp.set_piece && k->op == ch->camp.set_piece) { /* walked away */
      k->open = false;
      sr_story_set_piece_abandoned(ch, g->city, t);
      continue;
    }
    if (k && k->open) {
      k->framed = j->framed;
      k->player_evidence = evidence > 0;
      sr_chess_resolve(ch, g->city, g->pop, &g->events, k, done, true, t);
    }
    sr_story_job_done(ch, g->city, j->client_faction, j->target_faction, done, evidence, j->framed >= 0, t);
    award_cred(g, j, done, t);
    if (done) {
      ch->camp.last_job_at = t;
      if (j->tier >= 1) ch->camp.last_real_job = t;
      bool clean = j->alarms == 0 && j->kills == 0 && j->approach != SR_AP_HAMMER;
      if (clean) { /* quiet work pays: a bonus, less Heat, the patron's appreciation */
        int32_t bonus = j->pay / 5;
        sr_player_tx(&g->player, t, bonus, "clean work bonus");
        int jur = sr_game_jurisdiction(g, j->target_building);
        if (g->heat.level[jur] > 0) g->heat.level[jur]--;
        if (j->client_faction == ch->story.patron && ch->story.retainer) sr_story_trust(&ch->story, 2);
      }
      if (j->client_building != g->player.home) { /* the fixer keeps a cut */
        int32_t cut = SR_MIN(j->paid / 10, (int32_t)SR_MAX(0, g->player.cred));
        sr_player_tx(&g->player, t, -cut, "fixer's cut");
      }
      if (j->tier == 4) blackop_reward(g, j, t);
    }
    if (done && j->id == ch->story.test_job) {
      sr_story_test_done(&ch->story, t);
      ch->story.test_job = -2; /* used */
    }
  }
}

/* ---- world effects ------------------------------------------------------------------------ */

static void sync_world(SrGame *g, int64_t t) {
  const SrPolitics *p = &g->chess.pol;
  g->crowd.police_outfit = (int8_t)(p->police_contract >= 0 ? g->city->factions.f[p->police_contract].hue % 16 : -1);
  g->crowd.police_faction = (int8_t)sr_game_police(g);
  uint16_t mask = 0;
  for (int d = 0; d < g->city->district_count && d < 16; d++)
    if (sr_politics_curfew_now(p, d, t)) mask = (uint16_t)(mask | (1u << d));
  g->crowd.curfew = mask;
  if (g->chess.story.ending == SR_END_SUNRISE || g->chess.saga.sun_seen) g->pall_removed = true; /* daylight */
}

/* Police stop people on curfew streets. */
static void curfew_checks(SrGame *g, int64_t t) {
  if (g->site.active || g->dive.active || t < g->curfew_stop_at) return;
  int blk = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  if (blk < 0) return;
  int d = g->city->blocks[blk].district;
  if (!sr_politics_curfew_now(&g->chess.pol, d, t)) return;
  for (int i = 0; i < g->crowd.count; i++) {
    const SrAgent *a = &g->crowd.agents[i];
    float dx = a->pos.x - g->player.pos.x, dz = a->pos.z - g->player.pos.z;
    if (a->role != SR_ROLE_COP || dx * dx + dz * dz > 15.0f * 15.0f) continue;
    g->curfew_stop_at = t + 3600;
    int32_t fine = SR_MIN(50, (int32_t)SR_MAX(0, g->player.cred));
    sr_player_tx(&g->player, t, -fine, "curfew fine");
    sr_game_toast(g, "Curfew check. Papers, please. You pay the fine.");
    if (g->heat.level[sr_game_police(g)] >= 2) { /* wanted: the stop turns into a chase */
      SrCrime cr;
      memset(&cr, 0, sizeof cr);
      cr.jurisdiction = (int8_t)sr_game_police(g);
      cr.severity = 1;
      cr.witnessed = true;
      cr.look = sr_appearance_key(&g->player.look);
      cr.time = t;
      sr_heat_crime(&g->heat, &cr);
    }
    return;
  }
}

/* Lines the cast says arrive in the inbox, pinned. */
static void deliver_story(SrGame *g) {
  SrCampaign *cp = &g->chess.camp;
  for (int i = 0; i < cp->msg_count; i++) {
    const SrStoryMsg *m = &cp->msgs[i];
    char from[24];
    snprintf(from, sizeof from, "%s", g->chess.story.cast[m->role % SR_CAST_COUNT].name);
    sr_player_story_message(&g->player, from, m->text, now_of(g));
  }
  cp->msg_count = 0;
}

/* The press notices a runner on the rise. */
static void notoriety_story(SrGame *g, int64_t t) {
  int32_t week = (int32_t)(t / SR_WEEK_S) + 1;
  if (g->chess.camp.notoriety_week == week) return;
  g->chess.camp.notoriety_week = week;
  static const char *const k_stories[3] = {"Who is the runner everyone in %2 talks about?",
                                           "A %1 among runners: the street has a new name",
                                           "Corps on edge as a %1 works the city"};
  const char *tier = sr_cred_tier_name(sr_cred_tier(g->player.prog.street_cred));
  int d = g->city->blocks[g->city->buildings[g->player.home].block].district;
  char head[64];
  sr_headline(head, sizeof head, k_stories[(uint64_t)week % 3u], tier, g->city->districts[d].name, NULL);
  int outlet = g->city->factions.media_count > 0
                   ? g->city->factions.media_first + (int)((uint64_t)week % (uint64_t)g->city->factions.media_count)
                   : -1;
  sr_news_publish(&g->chess, g->city, outlet, -1, -1, -2, SR_NEWS_STORY, false, head, t);
}

static void story_messages(SrGame *g) {
  SrStory *s = &g->chess.story;
  static const char *const offer = "We liked your work. A retainer: steady pay, better jobs. Accept on the terminal.";
  if (s->offer_open && !s->retainer && !s->offer_told) {
    handler_story(g, offer);
    s->offer_told = true;
  }
}

void sr_game_chess_minute(SrGame *g, int64_t t) {
  SrChess *ch = &g->chess;
  ch->street_cred = g->player.prog.street_cred;
  ch->saga.cash = g->player.cred;
  if (ch->saga.refund > 0) { /* someone pays the player back */
    char why[48];
    snprintf(why, sizeof why, "from %s", ch->story.cast[ch->saga.refund_role % SR_CAST_COUNT].name);
    sr_player_tx(&g->player, t, ch->saga.refund, why);
    ch->saga.refund = 0;
  }
  book_jobs(g, t);
  deliver_story(g);
  int handlers_before = ch->story.cast[SR_CAST_HANDLER].generation;
  sr_chess_advance(ch, g->city, g->pop, &g->events, t);
  sr_news_tick(ch, g->city, &g->events, t); /* within the hour of an event */
  deliver_story(g);
  if (ch->story.cast[SR_CAST_HANDLER].generation != handlers_before && ch->story.cast[SR_CAST_HANDLER].alive)
    handler_story(g, "Your old handler is gone. I'm taking over. Nothing changes.");
  if (t % 3600 == 0) {
    living_costs(g, t);
    int reports = 0; /* three reports about the runner in a week: the press digs */
    for (int i = 0; i < sr_news_size(&ch->news); i++) {
      const SrNews *n = sr_news_latest(&ch->news, i);
      if (t - n->time > SR_WEEK_S) break;
      reports += n->perpetrator == -2 && n->kind != SR_NEWS_STORY;
    }
    if (reports >= 3) notoriety_story(g, t);
    post_contracts(g, t);
    sync_world(g, t);
    story_messages(g);
    if (t % 86400 == 9 * 3600) {
      sr_game_handler_offers(g);
      /* gangs and minor groups post street work on the board and through their contacts */
      int d = (int)(sr_mix64(sr_hash_combine(ch->seed, (uint64_t)t)) % (uint64_t)g->city->district_count);
      if (sr_chess_demand_op(ch, g->city, d, 0, t) >= 0) post_contracts(g, t);
    }
  }
  curfew_checks(g, t);
  hunters(g, t);
}

/* ---- living costs ----------------------------------------------------------------------------- */

int32_t sr_game_weekly_upkeep(const SrGame *g) {
  const SrPlayer *p = &g->player;
  int64_t value = 0;
  for (int i = 0; i < p->cyber_count; i++) value += sr_item_base_price(&p->cyber[i]);
  if (p->equipped >= 0 && p->equipped < p->inv.count) value += sr_item_base_price(&p->inv.items[p->equipped]);
  return (int32_t)(value * 2 / 100);
}

int32_t sr_game_rent_for(const SrGame *g) {
  const SrHousingDef *h = sr_housing_def(g->player.tier);
  int32_t base = h ? h->rent : g->player.rent;
  int over = (int)sr_cred_tier(g->player.prog.street_cred) - (int)SR_CRED_RUNNER;
  return base * (100 + 10 * SR_MAX(0, over)) / 100; /* the landlord knows who you are */
}

/* Charges a bill, taking at most half of the player's cash; the rest is owed. */
static void charge(SrGame *g, int32_t amount, const char *what, int64_t t) {
  int32_t paid = (int32_t)SR_MIN((int64_t)amount, SR_MAX(0, g->player.cred / 2));
  if (paid > 0) sr_player_tx(&g->player, t, -paid, what);
  g->chess.camp.medical_debt += amount - paid; /* the rest is owed */
}

static void living_costs(SrGame *g, int64_t t) {
  SrPlayer *p = &g->player;
  SrCampaign *cp = &g->chess.camp;
  if (!p->evicted) p->rent = sr_game_rent_for(g);
  if (cp->upkeep_due == 0) cp->upkeep_due = p->rent_due;
  if (p->rent_due != cp->upkeep_due) { /* rent was charged: the weekly costs go with it */
    cp->upkeep_due = p->rent_due;
    int32_t debt = cp->medical_debt;
    cp->medical_debt = 0;
    int32_t upkeep = sr_game_weekly_upkeep(g);
    if (upkeep > 0) charge(g, upkeep, "upkeep", t);
    if (debt > 0) charge(g, debt, "medical debt", t);
  }
  /* idle above Runner: the street forgets */
  if (p->prog.street_cred >= 25 && t - cp->last_job_at >= SR_WEEK_S && t - cp->last_decay_at >= SR_WEEK_S) {
    cp->last_decay_at = t;
    sr_progress_add_cred(&p->prog, -1);
  }
}

void sr_game_medical_bill(SrGame *g, float healed_hp, int32_t per_hp) {
  if (healed_hp <= 0) return;
  charge(g, (int32_t)(healed_hp * (float)per_hp), "medical bill", now_of(g));
}

SrResult sr_game_heat_cleanup(SrGame *g) {
  int worst = 0;
  for (int f = 1; f < g->city->factions.count; f++)
    if (g->heat.level[f] > g->heat.level[worst]) worst = f;
  if (g->heat.level[worst] <= 0) return SR_ERR_INVALID;
  int32_t price = 500 * g->heat.level[worst];
  if (g->player.cred < price) return SR_ERR_DENIED;
  sr_player_tx(&g->player, now_of(g), -price, "heat clean-up");
  g->heat.level[worst]--;
  sr_game_toast(g, "A few calls, a few favours. Heat drops a level.");
  return SR_OK;
}

/* ---- hunters --------------------------------------------------------------------------------- */

static bool at_home(const SrGame *g) { return g->site.active && g->site.in.apartment; }

void sr_game_encounter_start(SrGame *g, int f, bool raid) {
  if (f < 0 || f >= g->city->factions.count || g->enc.active) return;
  g->enc.active = true;
  g->enc.raid = raid;
  g->enc.faction = (int8_t)f;
  g->enc.payoff = 400 + 20 * g->chess.f[f].res[SR_RES_FORCE];
  char msg[112];
  snprintf(msg, sizeof msg, raid ? "%s hitters kick in your door." : "%s hitters block the street ahead.",
           g->city->factions.f[f].name);
  sr_player_story_message(&g->player, "Street", msg, now_of(g));
  sr_game_toast(g, raid ? "RAID! Fight, run or pay." : "AMBUSH! Fight, run or pay.");
  sr_game_encounter_menu(g);
}

void sr_game_encounter_menu(SrGame *g) {
  if (!g->enc.active || g->screen != SR_SCREEN_PLAY) return;
  SrMenu *m = &g->menu; /* the choice as a menu */
  memset(m, 0, sizeof *m);
  m->building = g->player.home;
  m->encounter = true;
  m->count = 3;
  g->screen = SR_SCREEN_SHOP;
}

static void end_hunt(SrGame *g) {
  if (g->chess.camp.hunter == g->enc.faction) g->chess.camp.hunter = -1;
  if (g->screen == SR_SCREEN_SHOP && g->menu.encounter) g->screen = SR_SCREEN_PLAY;
  g->enc.active = false;
}

SrResult sr_game_encounter(SrGame *g, SrEncChoice c) {
  if (!g->enc.active) return SR_ERR_INVALID;
  int f = g->enc.faction, force = g->chess.f[f].res[SR_RES_FORCE];
  int64_t t = now_of(g);
  uint64_t h = sr_mix64(sr_hash_combine(g->chess.seed, (uint64_t)t * 7u + (uint64_t)c));
  SrPlayer *p = &g->player;
  if (c == SR_ENC_PAY) {
    if (p->cred < g->enc.payoff) return SR_ERR_DENIED;
    sr_player_tx(p, t, -g->enc.payoff, "paid off hunters");
    if (g->heat.level[f] > 0) g->heat.level[f]--;
    sr_game_toast(g, "They count the cred and walk away.");
    end_hunt(g);
    return SR_OK;
  }
  int best_gun = SR_MAX(p->prog.skill_rank[SR_SK_PISTOLS], p->prog.skill_rank[SR_SK_LONGGUNS]);
  int chance = c == SR_ENC_FIGHT ? SR_CLAMP(35 + best_gun * 6 - force / 4, 10, 85)
                                 : SR_CLAMP(55 + p->prog.skill_rank[SR_SK_ATHLETICS] * 5 - force / 5, 15, 90);
  bool won = (int)(h % 100u) < chance;
  float damage = (float)(10 + (int)((h >> 8) % (uint64_t)(10 + force / 2)));
  if (c == SR_ENC_FLEE && won) damage = 0;
  if (c == SR_ENC_FIGHT && won) damage *= 0.5f;
  p->health -= damage;
  if (c == SR_ENC_FIGHT && won) {
    g->chess.f[f].res[SR_RES_FORCE] = (int16_t)SR_MAX(0, force - 5);
    sr_game_toast(g, "You drop them. Nobody else is coming. Today.");
    end_hunt(g);
  } else {
    sr_game_toast(g, c == SR_ENC_FLEE && won ? "You lose them in the crowd." : "You get away, bleeding.");
    if (g->chess.camp.hunter == f) g->chess.camp.hunt_at = t + 2 * 86400; /* they will try again */
    if (g->screen == SR_SCREEN_SHOP && g->menu.encounter) g->screen = SR_SCREEN_PLAY;
    g->enc.active = false;
  }
  if (p->health <= 0) sr_game_flatline(g);
  return SR_OK;
}

static void hunters(SrGame *g, int64_t t) {
  if (g->enc.active || g->dive.active) return;
  int hour = (int)((t % 86400) / 3600);
  bool night = hour >= 22 || hour < 6, home = at_home(g);
  SrCampaign *cp = &g->chess.camp;
  if (cp->hunter >= 0 && t >= cp->hunt_at) {
    if (home && night)
      sr_game_encounter_start(g, cp->hunter, true);
    else if (!g->site.active)
      sr_game_encounter_start(g, cp->hunter, false);
    return;
  }
  /* high Heat: raids on the apartment at night */
  if (home && night && t - g->last_raid >= 3 * 86400)
    for (int f = 0; f < g->city->factions.count; f++)
      if (g->heat.level[f] >= 4) {
        g->last_raid = t;
        sr_game_encounter_start(g, f, true);
        return;
      }
}

/* ---- player choices --------------------------------------------------------------------------- */

SrResult sr_game_accept_retainer(SrGame *g) {
  SrStory *s = &g->chess.story;
  if (!s->offer_open || s->retainer || s->patron < 0) return SR_ERR_INVALID;
  sr_story_accept_retainer(s, now_of(g));
  sr_story_log(&g->chess, now_of(g), "Act I: Pawn");
  char msg[112];
  snprintf(msg, sizeof msg, "Welcome to %s. Don't make me regret it.", g->city->factions.f[s->patron].blackops);
  handler_story(g, msg);
  return SR_OK;
}

SrResult sr_game_accept_offer(SrGame *g) {
  SrStory *s = &g->chess.story;
  if (s->rival_offer < 0) return SR_ERR_INVALID;
  if (!sr_story_defect(&g->chess, g->city, s->rival_offer, now_of(g))) {
    s->rival_offer = -1;  /* GCOVR_EXCL_LINE reason: offers never come from blacklisted corps */
    return SR_ERR_DENIED; /* GCOVR_EXCL_LINE */
  }
  handler_story(g, "You made the right call. Your old friends will not see it that way.");
  return SR_OK;
}

SrResult sr_game_choose_ending(SrGame *g, SrEnding e) {
  SrStory *s = &g->chess.story;
  if (!s->finale_ready || !(sr_saga_endings(&g->chess) & (1u << e))) return SR_ERR_INVALID;
  sr_story_end(&g->chess, g->city, e, now_of(g));
  deliver_story(g);
  sync_world(g, now_of(g));
  g->autosave_requests++;
  return SR_OK;
}

SrResult sr_game_answer_ask(SrGame *g, bool yes) {
  SrSaga *sg = &g->chess.saga;
  if (!sg->ask.open) return SR_ERR_INVALID;
  if (yes && sg->ask.cost > 0) {
    if (g->player.cred < sg->ask.cost) return SR_ERR_DENIED;
    char why[48];
    snprintf(why, sizeof why, "%s: %s", sr_saga_ask_name((SrAskType)sg->ask.type),
             g->chess.story.cast[sg->ask.role % SR_CAST_COUNT].name);
    sr_player_tx(&g->player, now_of(g), -sg->ask.cost, why);
  }
  sg->cash = g->player.cred;
  sr_saga_answer(&g->chess, g->city, g->pop, yes, now_of(g));
  deliver_story(g);
  return SR_OK;
}

SrResult sr_game_trade(SrGame *g, int corp, int qty) {
  bool ok = false;
  int64_t delta = sr_market_trade(&g->chess, corp, qty, g->player.cred, now_of(g), &ok);
  if (!ok) return SR_ERR_DENIED;
  sr_player_tx(&g->player, now_of(g), (int32_t)delta, qty > 0 ? "shares bought" : "shares sold");
  return SR_OK;
}

SrResult sr_game_frame(SrGame *g, int32_t job, int faction) {
  SrJob *j = sr_jobs_get(&g->jobs, job);
  if (!j || j->state != SR_JS_ACCEPTED || faction < 0 || faction >= g->city->factions.count) return SR_ERR_INVALID;
  if (g->player.cred < 300) return SR_ERR_DENIED;
  sr_player_tx(&g->player, now_of(g), -300, "planted evidence");
  j->framed = (int8_t)faction;
  return SR_OK;
}

/* ---- the Matrix ------------------------------------------------------------------------------- */

static bool media_host(const SrGame *g, int b) {
  int o = g->city->buildings[b].owner;
  return o >= g->city->factions.media_first && o < g->city->factions.media_first + g->city->factions.media_count;
}

static bool city_hall(const SrGame *g, int b) { return g->city->buildings[b].poi == SR_POI_CITY_HALL; }

/* A seat the owner of this host bought. */
static int bribed_seat(const SrGame *g, int b) {
  int o = g->city->buildings[b].owner;
  if (o < 0 || o >= g->city->factions.corp_count) return -1;
  for (int s = 0; s < g->chess.pol.seat_count; s++)
    if (g->chess.pol.seats[s].bribed == o || (g->chess.pol.seats[s].backer == o && !g->chess.pol.seats[s].vacant))
      return s;
  return -1;
}

int sr_game_chess_node_actions(const SrGame *g, int b, SrNodeAction *out) {
  int n = 0;
  if (b < 0) return 0;
  if (media_host(g, b)) {
    out[n++] = SR_NA_PLANT_STORY;
    if (g->chess.news.pending_count > 0) out[n++] = SR_NA_KILL_STORY;
  }
  if (city_hall(g, b)) out[n++] = SR_NA_TAMPER_VOTE;
  if (bribed_seat(g, b) >= 0) out[n++] = SR_NA_LEAK_BRIBES;
  return n;
}

/* Whom a planted story hurts: the strongest corp that is not the patron. */
static int story_target(const SrGame *g) {
  int best = -1;
  for (int f = 0; f < g->city->factions.corp_count; f++) {
    if (f == g->chess.story.patron) continue;
    if (best < 0 || g->chess.f[f].res[SR_RES_CAPITAL] > g->chess.f[best].res[SR_RES_CAPITAL]) best = f;
  }
  return best;
}

SrResult sr_game_chess_node_act(SrGame *g, int b, SrNodeAction a) {
  SrChess *ch = &g->chess;
  int64_t t = now_of(g);
  switch (a) {
    case SR_NA_PLANT_STORY: {
      int target = story_target(g);
      sr_news_plant(ch, g->city, target, -1, t);
      sr_game_toast(g, "Story planted. It runs within the hour.");
      return SR_OK;
    }
    case SR_NA_KILL_STORY:
      if (!sr_news_kill(ch, ch->story.patron) && !sr_news_kill(ch, -1)) return SR_ERR_INVALID;
      sr_game_toast(g, "Story killed before it ran.");
      return SR_OK;
    case SR_NA_TAMPER_VOTE: {
      int f = ch->story.patron >= 0 ? ch->story.patron : story_target(g);
      ch->pol.tamper_for = (int8_t)f;
      sr_game_toast(g, "Vote count altered. The next result is yours to decide.");
      return SR_OK;
    }
    case SR_NA_LEAK_BRIBES: {
      int seat = bribed_seat(g, b);
      if (seat < 0) return SR_ERR_INVALID;
      int o = g->city->buildings[b].owner;
      sr_chess_reveal(ch, o, seat, SR_LINK_BRIBES);
      sr_news_plant(ch, g->city, o, seat, t);
      sr_game_toast(g, "Bribery files leaked to the press.");
      return SR_OK;
    }
    default: return SR_ERR_INVALID; /* GCOVR_EXCL_LINE reason: only the actions above are routed here */
  }
}

void sr_game_chess_paydata(SrGame *g, int b) {
  int o = g->city->buildings[b].owner;
  if (o < 0) return;
  int n = sr_chess_reveal_faction(&g->chess, o);
  if (n > 0) sr_game_toast(g, "The files map who funds whom. Your Board grew.");
}

/* ---- terminal ------------------------------------------------------------------------------- */

/* Story options: kind 1 retainer, 2 counter-offer, 10+e ending e. */
const char *sr_game_story_option(const SrGame *g, int i, int *kind) {
  static char buf[64];
  const SrStory *s = &g->chess.story;
  int n = 0;
  if (s->offer_open && !s->retainer && s->patron >= 0) {
    if (n++ == i) {
      *kind = 1;
      snprintf(buf, sizeof buf, "Accept the retainer from %s", g->city->factions.f[s->patron].blackops);
      return buf;
    }
  }
  if (s->rival_offer >= 0) {
    if (n++ == i) {
      *kind = 2;
      snprintf(buf, sizeof buf, "Defect to %s", g->city->factions.f[s->rival_offer].name);
      return buf;
    }
  }
  if (s->finale_ready) {
    unsigned m = sr_saga_endings(&g->chess);
    for (int e = SR_END_LOYAL; e <= SR_END_SUNRISE; e++) {
      if (!(m & (1u << e))) continue;
      if (e == SR_END_KINGMAKER) { /* the player picks the king */
        for (int k = 0; k < g->city->factions.corp_count; k++)
          if (n++ == i) {
            *kind = 100 + k;
            snprintf(buf, sizeof buf, "Ending: Kingmaker - crown %s", g->city->factions.f[k].name);
            return buf;
          }
        continue;
      }
      if (n++ == i) {
        *kind = 10 + e;
        snprintf(buf, sizeof buf, "Ending: %s", sr_ending_name((SrEnding)e));
        return buf;
      }
    }
  }
  return NULL;
}

int sr_game_term_rows(const SrGame *g) {
  switch ((SrTermTab)g->term_tab) {
    case SR_TAB_MARKET: return g->city->factions.corp_count;
    case SR_TAB_JOBS: {
      int32_t ids[32];
      return sr_game_terminal_jobs(g, ids, 32);
    }
    case SR_TAB_STORY: {
      int n = 0, k;
      while (sr_game_story_option(g, n, &k)) n++;
      return n;
    }
    case SR_TAB_PEOPLE: return g->chess.saga.ask.open ? 2 : 0; /* help, refuse */
    default: return 0;
  }
}

void sr_game_term_input(SrGame *g, const SrActionState *a) {
  int dx = a->move_x > 0.5f ? 1 : a->move_x < -0.5f ? -1 : 0;
  int dy = a->move_y > 0.5f ? -1 : a->move_y < -0.5f ? 1 : 0;
  if (dx != 0 && dx != g->term_axis_x) {
    g->term_tab = (g->term_tab + dx + SR_TAB_COUNT) % SR_TAB_COUNT;
    g->term_sel = 0;
  }
  int rows = sr_game_term_rows(g);
  if (dy != 0 && dy != g->term_axis_y && rows > 0) g->term_sel = (g->term_sel + dy + rows) % rows;
  g->term_axis_x = dx;
  g->term_axis_y = dy;
  if (g->term_tab == SR_TAB_MARKET && rows > 0) {
    if (a->pressed[SR_ACT_INTERACT])
      sr_game_toast(g, sr_game_trade(g, g->term_sel, 10) == SR_OK ? "Bought 10 shares." : "Not enough cred.");
    if (a->pressed[SR_ACT_RELOAD])
      sr_game_toast(g, sr_game_trade(g, g->term_sel, -10) == SR_OK ? "Sold 10 shares." : "Margin limit reached.");
  }
  if (g->term_tab == SR_TAB_JOBS && rows > 0 && a->pressed[SR_ACT_RELOAD]) { /* R walks away from a job */
    int32_t ids[32];
    sr_game_terminal_jobs(g, ids, 32);
    SrJob *j = sr_jobs_get(&g->jobs, ids[SR_MIN(g->term_sel, rows - 1)]);
    if (j && j->state == SR_JS_ACCEPTED && sr_game_abandon(g, j->id) == SR_OK)
      sr_game_toast(g, "You walk away from it.");
  }
  if (g->term_tab == SR_TAB_JOBS && rows > 0 && a->pressed[SR_ACT_INTERACT]) {
    int32_t ids[32];
    sr_game_terminal_jobs(g, ids, 32);
    SrJob *j = sr_jobs_get(&g->jobs, ids[SR_MIN(g->term_sel, rows - 1)]);
    if (j && j->state == SR_JS_OFFERED) {
      SrResult r = sr_game_accept(g, j->id);
      if (r == SR_OK)
        sr_game_toast(g, "Job accepted. It's on your list.");
      else if (r == SR_ERR_DENIED)
        sr_game_toast(g, "You are not ready for that one.");
    }
  }
  if (g->term_tab == SR_TAB_PEOPLE && rows > 0 && a->pressed[SR_ACT_INTERACT]) {
    SrResult r = sr_game_answer_ask(g, SR_MIN(g->term_sel, rows - 1) == 0);
    if (r == SR_ERR_DENIED) sr_game_toast(g, "You don't have the cred.");
    g->term_sel = 0;
  }
  if (g->term_tab == SR_TAB_STORY && rows > 0 && a->pressed[SR_ACT_INTERACT]) {
    int kind = 0;
    if (sr_game_story_option(g, SR_MIN(g->term_sel, rows - 1), &kind)) {
      if (kind == 1) {
        sr_game_accept_retainer(g);
      } else if (kind == 2) {
        sr_game_accept_offer(g);
      } else if (kind >= 100) { /* Kingmaker: the chosen megacorp */
        sr_story_crown(&g->chess, kind - 100);
        sr_game_choose_ending(g, SR_END_KINGMAKER);
      } else {
        sr_game_choose_ending(g, (SrEnding)(kind - 10));
      }
      g->term_sel = 0;
    }
  }
}
