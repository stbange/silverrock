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

void sr_game_chess_init(SrGame *g) {
  sr_chess_init(&g->chess, g->city, sr_hash_combine(g->seed, 0xC4E55u), g->patron_choice, now_of(g));
  g->chess.news.events_seen = g->events.count;
  g->curfew_stop_at = 0;
  g->confirm_job = -1;
  g->term_tab = SR_TAB_HOME;
  g->term_sel = 0;
}

void sr_game_chess_loaded(SrGame *g) {
  sr_chess_apply_changes(&g->chess, g->city);
  g->chess.news.events_seen = g->events.count; /* the event log was rebuilt */
  sync_world(g, now_of(g));
}

bool sr_game_job_against_patron(const SrGame *g, const SrJob *j) {
  int p = g->chess.story.patron, t = j->target_faction;
  if (p < 0 || t < 0 || !g->chess.story.retainer) return false;
  if (t == p) return true;
  for (int i = 0; i < g->chess.link_count; i++) { /* the patron's gangs and stations */
    const SrLink *l = &g->chess.links[i];
    if (l->a == p && l->b == t && (l->type == SR_LINK_FUNDS || l->type == SR_LINK_OWNS)) return true;
  }
  return false;
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
  for (int i = 0; i < ch->contract_count; i++) {
    SrContract *k = &ch->contracts[i];
    if (!k->open || k->job_id != -1 || t >= k->deadline) continue;
    /* only work the player can take reaches them; the rest goes to NPC crews */
    if (sr_job_tier_of_type((SrJobType)k->job_type) > sr_max_tier_for_cred(g->player.prog.street_cred)) continue;
    SrJob j;
    uint64_t key = sr_hash_combine(ch->seed, (uint64_t)k->id * 31u + 7u);
    SrOperation *op = sr_chess_op(ch, k->op);
    if (!sr_job_for_contract(g->city, (SrJobType)k->job_type, k->building, k->client, op ? op->target : -1, key, t, &j))
      continue; /* GCOVR_EXCL_LINE reason: generation always binds */
    j.source = source_of(k->channel);
    j.deadline = k->deadline;
    int d = g->city->blocks[g->city->buildings[j.target_building].block].district;
    j.client_building = k->channel == SR_CH_FIXER ? g->city->factions.fixer_building[d] : g->player.home;
    if (j.client_building < 0) j.client_building = g->player.home;
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

int sr_game_handler_offers(SrGame *g) {
  SrStory *s = &g->chess.story;
  if (!sr_story_t4_allowed(s, sr_cred_tier(g->player.prog.street_cred))) return 0;
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
  j.pay = lo + (int32_t)(sr_mix64(key + 1u) % (uint64_t)(hi - lo));
  j.upfront = j.pay / 4;
  j.source = SR_SRC_CONTACT;
  j.client_building = g->player.home;
  j.deadline = t + 4 * 86400;
  snprintf(j.title, sizeof j.title, "Black op: %s at %s", sr_job_type_name(type), g->city->factions.f[rival].name);
  SrJob *p = sr_jobs_push(&g->jobs, &j);
  if (!p) return 0; /* GCOVR_EXCL_LINE reason: the board always has room for one more */
  handler_says(g, "Something deniable for you. Black-op rates. Check your jobs.");
  return 1;
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
    bool evidence = j->alarms > 0 || j->kills > 0;
    bool attributed = evidence && j->framed < 0;
    if (k && k->open) {
      k->framed = j->framed;
      k->player_evidence = evidence;
      sr_chess_resolve(ch, g->city, g->pop, &g->events, k, done, true, t);
    }
    sr_story_job_done(ch, g->city, j->client_faction, j->target_faction, done, attributed, t);
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
  if (g->chess.story.ending == SR_END_SUNRISE) g->pall_removed = true; /* the only daylight in the game */
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
  book_jobs(g, t);
  int handlers_before = ch->story.cast[SR_CAST_HANDLER].generation;
  sr_chess_advance(ch, g->city, g->pop, &g->events, t);
  sr_news_tick(ch, g->city, &g->events, t); /* within the hour of an event */
  if (ch->story.cast[SR_CAST_HANDLER].generation != handlers_before && ch->story.cast[SR_CAST_HANDLER].alive)
    handler_story(g, "Your old handler is gone. I'm taking over. Nothing changes.");
  if (t % 3600 == 0) {
    post_contracts(g, t);
    sync_world(g, t);
    story_messages(g);
    if (t % 86400 == 9 * 3600) sr_game_handler_offers(g);
  }
  curfew_checks(g, t);
}

/* ---- player choices --------------------------------------------------------------------------- */

SrResult sr_game_accept_retainer(SrGame *g) {
  SrStory *s = &g->chess.story;
  if (!s->offer_open || s->retainer || s->patron < 0) return SR_ERR_INVALID;
  sr_story_accept_retainer(s, now_of(g));
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
  if (!s->finale_ready || !(sr_story_endings(s) & (1u << e))) return SR_ERR_INVALID;
  sr_story_end(&g->chess, g->city, e, now_of(g));
  sync_world(g, now_of(g));
  g->autosave_requests++;
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
    unsigned m = sr_story_endings(s);
    for (int e = SR_END_LOYAL; e <= SR_END_SUNRISE; e++)
      if ((m & (1u << e)) && n++ == i) {
        *kind = 10 + e;
        snprintf(buf, sizeof buf, "Ending: %s", sr_ending_name((SrEnding)e));
        return buf;
      }
  }
  return NULL;
}

int sr_game_term_rows(const SrGame *g) {
  switch ((SrTermTab)g->term_tab) {
    case SR_TAB_MARKET: return g->city->factions.corp_count;
    case SR_TAB_STORY: {
      int n = 0, k;
      while (sr_game_story_option(g, n, &k)) n++;
      return n;
    }
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
  if (g->term_tab == SR_TAB_STORY && rows > 0 && a->pressed[SR_ACT_INTERACT]) {
    int kind = 0;
    if (sr_game_story_option(g, SR_MIN(g->term_sel, rows - 1), &kind)) {
      if (kind == 1)
        sr_game_accept_retainer(g);
      else if (kind == 2)
        sr_game_accept_offer(g);
      else
        sr_game_choose_ending(g, (SrEnding)(kind - 10));
      g->term_sel = 0;
    }
  }
}
