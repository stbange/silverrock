/* City politics: the council and the mayor as pieces in the corporate game. Elections on a fixed
 * schedule (a rotating third of the council every 8 weeks, the mayor every 24), vote shares from
 * influence, public image, scandals and tampering, policies every 2 weeks (police budget, curfews,
 * the police contract), bribery and the resignations that follow exposure. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

#define COUNCIL_TERM_S (8 * SR_WEEK_S)
#define MAYOR_TERM_S   (24 * SR_WEEK_S)
#define POLICY_S       (2 * SR_WEEK_S)

static SrSeat *seat_of(SrPolitics *p, int seat) { return seat >= SR_COUNCIL_SEATS ? &p->mayor : &p->seats[seat]; }

void sr_politics_init(SrPolitics *p, const SrCity *c, int64_t now) {
  memset(p, 0, sizeof *p);
  p->seat_count = SR_CLAMP(c->factions.council_count, 1, SR_COUNCIL_SEATS);
  for (int s = 0; s < SR_COUNCIL_SEATS; s++) {
    SrSeat *st = &p->seats[s];
    st->backer = -1;
    st->bribed = -1;
    if (s >= p->seat_count) continue;
    st->backer = c->factions.council[s].patron;
    st->lean = c->factions.council[s].lean;
    snprintf(st->name, sizeof st->name, "%s", c->factions.council[s].name);
  }
  p->mayor.backer = c->factions.mayor_patron;
  p->mayor.bribed = -1;
  snprintf(p->mayor.name, sizeof p->mayor.name, "%s", c->factions.mayor);
  p->next_council = now + COUNCIL_TERM_S;
  p->next_mayor = now + MAYOR_TERM_S;
  p->next_vote = now + POLICY_S;
  p->police_budget = 1;
  p->police_contract = -1;
  p->tamper_for = -1;
}

int sr_politics_seats_of(const SrPolitics *p, int f) {
  int n = 0;
  for (int s = 0; s < p->seat_count; s++)
    if (!p->seats[s].vacant && (p->seats[s].backer == f || p->seats[s].bribed == f)) n++;
  return n;
}

float sr_politics_share(const SrChess *ch, const SrCity *c, int backer, int rival, int scandal) {
  (void)c;
  int inf_b = backer >= 0 ? ch->f[backer].res[SR_RES_INFLUENCE] : 35;
  int inf_r = rival >= 0 ? ch->f[rival].res[SR_RES_INFLUENCE] : 35;
  int img_b = backer >= 0 ? ch->f[backer].res[SR_RES_IMAGE] : 55;
  int img_r = rival >= 0 ? ch->f[rival].res[SR_RES_IMAGE] : 55;
  float s = 0.5f + (float)(inf_b - inf_r) / 250.0f + (float)(img_b - img_r) / 300.0f - (float)scandal / 100.0f;
  if (ch->pol.tamper_for >= 0) {
    if (ch->pol.tamper_for == backer)
      s += 0.35f;
    else if (ch->pol.tamper_for == rival)
      s -= 0.35f;
  }
  return SR_CLAMP(s, 0.03f, 0.97f);
}

static int outlet_for(const SrCity *c, int subject) {
  for (int m = c->factions.media_first; m < c->factions.media_first + c->factions.media_count; m++)
    if (c->factions.f[m].owner != subject) return m;
  return -1;
}

int sr_politics_elect(SrChess *ch, const SrCity *c, int seat, int64_t now) {
  SrPolitics *p = &ch->pol;
  SrSeat *st = seat_of(p, seat);
  SrRng r = sr_rng_stream(ch->seed, "election", (uint64_t)p->elections_held, (uint64_t)seat);
  int inc = st->backer;
  /* the challenger: the most influential other corp, sometimes an independent */
  int chal = -1, best = -1;
  for (int k = 0; k < c->factions.corp_count; k++) {
    if (k == inc) continue;
    int v = ch->f[k].res[SR_RES_INFLUENCE] + sr_rng_range(&r, 0, 20);
    if (v > best) best = v, chal = k;
  }
  if (inc >= 0 && sr_rng_chance(&r, 250)) chal = -1;
  float share = sr_politics_share(ch, c, inc, chal, st->scandal);
  bool keep = !st->vacant && share >= 0.5f;
  if (st->vacant) keep = share >= 0.5f; /* the backer runs a new candidate in the by-election */
  int winner = keep ? inc : chal;
  float won = keep ? share : 1.0f - share;
  if (!keep || st->vacant) {
    sr_name_person(ch->seed, sr_hash_combine((uint64_t)seat * 7919u, (uint64_t)p->elections_held + 1u), st->name,
                   sizeof st->name);
    st->lean = (int8_t)sr_rng_range(&r, -2, 2);
    st->bribed = -1;
    st->scandal = 0;
  }
  st->backer = (int8_t)winner;
  st->vacant = false;
  st->exposed = false;
  st->resign_at = 0;
  p->last_share[seat >= SR_COUNCIL_SEATS ? SR_COUNCIL_SEATS : seat] = won;
  p->elections_held++;
  char head[64], num[8], pct[8];
  snprintf(num, sizeof num, "%d", seat + 1);
  snprintf(pct, sizeof pct, "%d%%", (int)(won * 100.0f + 0.5f));
  if (seat >= SR_COUNCIL_SEATS)
    sr_headline(head, sizeof head, "%1 elected mayor with %2", st->name, pct, NULL);
  else
    sr_headline(head, sizeof head, "Council seat %3: %1 wins with %2", st->name, pct, num);
  sr_news_publish(ch, c, outlet_for(c, winner), winner, -1, -1, SR_NEWS_ELECTION, false, head, now);
  sr_chess_note(ch, now, head);
  return winner;
}

void sr_politics_expose(SrChess *ch, const SrCity *c, int seat, int64_t now) {
  if (seat < 0 || seat >= ch->pol.seat_count) return;
  SrSeat *st = &ch->pol.seats[seat];
  if (st->exposed || st->vacant) return;
  st->exposed = true;
  st->resign_at = now + 2 * 86400;
  st->scandal = (int8_t)SR_MIN(60, st->scandal + 30);
  int corp = st->bribed >= 0 ? st->bribed : st->backer;
  if (corp >= 0) ch->f[corp].res[SR_RES_IMAGE] = (int16_t)SR_MAX(0, ch->f[corp].res[SR_RES_IMAGE] - 10);
  char head[64];
  sr_headline(head, sizeof head, "Councillor %1 caught taking bribes", st->name, NULL, NULL);
  sr_news_publish(ch, c, -1, corp, -1, -1, SR_NEWS_SCANDAL, true, head, now);
}

bool sr_politics_curfew_now(const SrPolitics *p, int district, int64_t t) {
  if (district < 0 || district >= 16 || !(p->curfew & (1u << district))) return false;
  int hour = (int)((t % 86400) / 3600);
  return hour >= 22 || hour < 6;
}

int sr_politics_police(const SrPolitics *p, const SrCity *c) {
  return p->police_contract >= 0 ? p->police_contract : c->factions.police;
}

/* Violent incidents in a district over the last two weeks. */
static int violence(const SrEventLog *log, int d, int64_t now) {
  int n = 0;
  for (int i = 0; log && i < sr_events_size(log); i++) {
    const SrWorldEvent *e = sr_events_recent(log, i);
    if (now - e->time > POLICY_S) break;
    if (e->district == d && (e->type == SR_EV_GUNFIRE || e->type == SR_EV_KILLING)) n++;
  }
  return n;
}

static void policies(SrChess *ch, const SrCity *c, const SrEventLog *log, int64_t now) {
  SrPolitics *p = &ch->pol;
  int lean = 0;
  for (int s = 0; s < p->seat_count; s++) lean += p->seats[s].vacant ? 0 : p->seats[s].lean;
  int budget = lean >= 2 ? 2 : lean <= -2 ? 0 : 1;
  char head[64];
  if (budget != p->police_budget) {
    snprintf(head, sizeof head, "Council %s the police budget", budget > p->police_budget ? "raises" : "cuts");
    sr_news_publish(ch, c, outlet_for(c, -1), c->factions.police, -1, -1, SR_NEWS_POLICY, false, head, now);
    p->police_budget = (int8_t)budget;
  }
  for (int d = 0; d < c->district_count && d < 16; d++) {
    int v = violence(log, d, now);
    bool on = (p->curfew >> d) & 1u;
    if (!on && v >= 2 && lean >= 0) {
      p->curfew = (uint16_t)(p->curfew | (1u << d));
      sr_headline(head, sizeof head, "Curfew declared in %1", c->districts[d].name, NULL, NULL);
      sr_news_publish(ch, c, outlet_for(c, -1), -1, d, -1, SR_NEWS_POLICY, false, head, now);
      sr_chess_note(ch, now, head);
    } else if (on && (v == 0 || lean < 0)) {
      p->curfew = (uint16_t)(p->curfew & ~(1u << d));
      sr_headline(head, sizeof head, "Curfew lifted in %1", c->districts[d].name, NULL, NULL);
      sr_news_publish(ch, c, outlet_for(c, -1), -1, d, -1, SR_NEWS_POLICY, false, head, now);
      sr_chess_note(ch, now, head);
    }
  }
  int majority = p->seat_count / 2 + 1;
  for (int k = 0; k < c->factions.corp_count; k++)
    if (sr_politics_seats_of(p, k) >= majority && p->police_contract != k) {
      p->police_contract = (int8_t)k;
      sr_headline(head, sizeof head, "Police contract awarded to %1", c->factions.f[k].name, NULL, NULL);
      sr_news_publish(ch, c, outlet_for(c, k), k, -1, -1, SR_NEWS_POLICY, false, head, now);
      sr_chess_note(ch, now, head);
    }
  if (p->police_contract >= 0 && sr_politics_seats_of(p, p->police_contract) < majority - 1) {
    snprintf(head, sizeof head, "Police back under city control");
    p->police_contract = -1;
    sr_news_publish(ch, c, outlet_for(c, -1), c->factions.police, -1, -1, SR_NEWS_POLICY, false, head, now);
  }
}

void sr_politics_tick(SrChess *ch, SrCity *c, SrEventLog *log, int64_t now) {
  SrPolitics *p = &ch->pol;
  if (ch->ticks % 4 == 0)
    for (int s = 0; s <= SR_COUNCIL_SEATS; s++) {
      SrSeat *st = seat_of(p, s);
      if (st->scandal > 0 && !st->exposed) st->scandal--;
    }
  for (int s = 0; s < p->seat_count; s++) {
    SrSeat *st = &p->seats[s];
    if (!st->exposed || now < st->resign_at) continue;
    char head[64];
    sr_headline(head, sizeof head, "Councillor %1 resigns", st->name, NULL, NULL);
    sr_news_publish(ch, c, outlet_for(c, st->backer), st->backer, -1, -1, SR_NEWS_RESIGN, false, head, now);
    sr_chess_note(ch, now, head);
    st->exposed = false;
    st->vacant = true;
    st->bribed = -1;
    snprintf(st->name, sizeof st->name, "(vacant)");
    if (p->by_election == 0) p->by_election = now + 7 * 86400;
  }
  bool voted = false;
  if (p->by_election > 0 && now >= p->by_election) {
    for (int s = 0; s < p->seat_count; s++)
      if (p->seats[s].vacant) sr_politics_elect(ch, c, s, now);
    p->by_election = 0;
    voted = true;
  }
  if (now >= p->next_council) {
    for (int s = p->seat_group; s < p->seat_count; s += 3)
      if (!p->seats[s].exposed) sr_politics_elect(ch, c, s, now);
    p->seat_group = (uint8_t)((p->seat_group + 1) % 3);
    p->next_council += COUNCIL_TERM_S;
    voted = true;
  }
  if (now >= p->next_mayor) {
    sr_politics_elect(ch, c, SR_COUNCIL_SEATS, now);
    p->next_mayor += MAYOR_TERM_S;
    voted = true;
  }
  if (voted) p->tamper_for = -1;
  if (now >= p->next_vote) {
    policies(ch, c, log, now);
    p->next_vote += POLICY_S;
  }
}
