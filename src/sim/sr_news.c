/* The news: significant world events become items within the hour, slanted by whoever owns the
 * outlet; scandals about an owner stay off its channel until an independent outlet runs them;
 * billboards rotate the headlines of their district; stories can be planted and killed; the
 * player is named only as far as the evidence goes. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

static bool independent(const SrCity *c, int outlet) { return outlet < 0 || c->factions.f[outlet].owner < 0; }

bool sr_news_independent_scandal(const SrNewsDesk *d, const SrCity *c, int subject) {
  int n = SR_MIN(d->count, SR_MAX_NEWS);
  for (int i = 0; i < n; i++) {
    const SrNews *it = &d->items[i];
    if (it->scandal && it->subject == subject && independent(c, it->outlet)) return true;
  }
  return false;
}

bool sr_news_publish(SrChess *ch, const SrCity *c, int outlet, int subject, int district, int perpetrator, int kind,
                     bool scandal, const char *headline, int64_t now) {
  if (outlet >= 0 && scandal && subject >= 0 && c->factions.f[outlet].owner == subject &&
      !sr_news_independent_scandal(&ch->news, c, subject))
    return false; /* owner protection */
  /* an owned outlet never names its owner as the culprit */
  if (outlet >= 0 && perpetrator >= 0 && c->factions.f[outlet].owner == perpetrator) perpetrator = -1;
  SrNews *it = &ch->news.items[ch->news.count % SR_MAX_NEWS];
  memset(it, 0, sizeof *it);
  it->time = now;
  it->outlet = (int8_t)outlet;
  it->subject = (int8_t)subject;
  it->district = (int8_t)district;
  it->perpetrator = (int8_t)perpetrator;
  it->kind = (uint8_t)kind;
  it->scandal = scandal;
  snprintf(it->headline, sizeof it->headline, "%s", headline);
  ch->news.count++;
  return true;
}

const SrNews *sr_news_latest(const SrNewsDesk *d, int i) {
  if (i < 0 || i >= SR_MIN(d->count, SR_MAX_NEWS)) return NULL;
  return &d->items[(d->count - 1 - i) % SR_MAX_NEWS];
}

int sr_news_size(const SrNewsDesk *d) { return SR_MIN(d->count, SR_MAX_NEWS); }

bool sr_news_reported(const SrNewsDesk *d, int subject, bool scandal_only) {
  for (int i = 0; i < sr_news_size(d); i++) {
    const SrNews *it = sr_news_latest(d, i);
    if (it->subject == subject && (!scandal_only || it->scandal)) return true;
  }
  return false;
}

const char *sr_news_billboard(const SrNewsDesk *d, int district, int64_t t) {
  const char *pick[6];
  int n = 0;
  for (int i = 0; i < sr_news_size(d) && n < 6; i++) {
    const SrNews *it = sr_news_latest(d, i);
    if (it->district == district || it->district < 0) pick[n++] = it->headline;
  }
  if (n == 0) return NULL;
  return pick[(t / 20) % n];
}

void sr_news_queue(SrChess *ch, int outlet, int subject, int seat, bool scandal, bool planted, const char *headline,
                   int64_t at) {
  SrNewsDesk *d = &ch->news;
  if (d->pending_count >= SR_MAX_PENDING) {
    memmove(&d->pending[0], &d->pending[1], sizeof(SrPendingStory) * (SR_MAX_PENDING - 1));
    d->pending_count--;
  }
  SrPendingStory *p = &d->pending[d->pending_count++];
  memset(p, 0, sizeof *p);
  p->publish_at = at;
  p->outlet = (int8_t)outlet;
  p->subject = (int8_t)subject;
  p->seat = (int8_t)seat;
  p->scandal = scandal;
  p->planted = planted;
  snprintf(p->headline, sizeof p->headline, "%s", headline);
}

/* An outlet that will run a story about subject: one it does not belong to, else the pirate feed. */
static int outlet_against(const SrChess *ch, const SrCity *c, int subject) {
  int n = c->factions.media_count;
  for (int k = 0; k < n; k++) {
    int m = c->factions.media_first + (int)((uint64_t)(ch->news.count + k) % (uint64_t)n);
    if (c->factions.f[m].owner != subject) return m;
  }
  return -1;
}

void sr_news_plant(SrChess *ch, const SrCity *c, int subject, int seat, int64_t now) {
  char head[64];
  if (seat >= 0 && seat < ch->pol.seat_count)
    sr_headline(head, sizeof head, "Leaked: %1 on %2 payroll", ch->pol.seats[seat].name,
                subject >= 0 ? c->factions.f[subject].name : "a corp", NULL);
  else
    sr_headline(head, sizeof head, "Leaked files shame %1", subject >= 0 ? c->factions.f[subject].name : "the city",
                NULL, NULL);
  sr_news_queue(ch, outlet_against(ch, c, subject), subject, seat, true, true, head, now + 3600);
}

bool sr_news_kill(SrChess *ch, int subject) {
  SrNewsDesk *d = &ch->news;
  for (int i = 0; i < d->pending_count; i++) {
    if (subject >= 0 && d->pending[i].subject != subject) continue;
    memmove(&d->pending[i], &d->pending[i + 1], sizeof(SrPendingStory) * (size_t)(d->pending_count - i - 1));
    d->pending_count--;
    return true;
  }
  return false;
}

void sr_news_pending_tick(SrChess *ch, const SrCity *c, int64_t now) {
  SrNewsDesk *d = &ch->news;
  for (int i = 0; i < d->pending_count;) {
    SrPendingStory p = d->pending[i];
    if (now < p.publish_at) {
      i++;
      continue;
    }
    memmove(&d->pending[i], &d->pending[i + 1], sizeof(SrPendingStory) * (size_t)(d->pending_count - i - 1));
    d->pending_count--;
    int outlet = p.outlet;
    if (!sr_news_publish(ch, c, outlet, p.subject, -1, -1, p.planted ? SR_NEWS_PLANTED : SR_NEWS_STORY, p.scandal,
                         p.headline, now))
      continue; /* owner protection: the story dies on the owner's desk */
    if (p.scandal && p.subject >= 0 && p.subject < c->factions.count) {
      SrFactionSim *s = &ch->f[p.subject];
      s->res[SR_RES_IMAGE] = (int16_t)SR_MAX(0, s->res[SR_RES_IMAGE] - (p.planted ? 12 : 6));
      if (p.subject < c->factions.corp_count) ch->market.shock[p.subject] -= 50;
      if (p.planted) ch->story.leaks++;
    }
    if (p.seat >= 0) sr_politics_expose(ch, c, p.seat, now);
  }
}

static const char *faction_name(const SrCity *c, int f) {
  return f >= 0 && f < c->factions.count ? c->factions.f[f].name : "unknown";
}

void sr_news_tick(SrChess *ch, const SrCity *c, const SrEventLog *log, int64_t now) {
  SrNewsDesk *d = &ch->news;
  if (log) {
    int fresh = log->count - d->events_seen;
    if (fresh > SR_MAX_EVENTS) fresh = SR_MAX_EVENTS;
    for (int i = fresh - 1; i >= 0; i--) { /* oldest first */
      const SrWorldEvent *e = sr_events_recent(log, i);
      if (!e || !e->significant) continue;
      int subject = e->victim_faction;
      int perp = e->blamed_faction; /* only as far as the evidence goes */
      /* the outlet: rotate over the stations; the owner of the victim reports with sympathy */
      int outlet = c->factions.media_count > 0
                       ? c->factions.media_first + (int)((uint64_t)d->count % (uint64_t)c->factions.media_count)
                       : -1;
      /* an owned outlet never names its owner as the culprit */
      int shown = outlet >= 0 && perp >= 0 && c->factions.f[outlet].owner == perp ? -1 : perp;
      char head[64];
      sr_news_event_headline(head, sizeof head, c, e, outlet, shown);
      sr_news_publish(ch, c, outlet, subject, e->district, perp, e->type, false, head, now);
      /* an investigation follows corporate violence: a scandal that can be killed before it runs */
      if (perp >= 0 && perp < c->factions.corp_count && perp != subject) {
        char inv[64];
        sr_headline(inv, sizeof inv, "Investigation: %1 behind attacks", faction_name(c, perp), NULL, NULL);
        sr_news_queue(ch, outlet_against(ch, c, perp), perp, -1, true, false, inv, now + 12 * 3600);
      }
    }
    d->events_seen = log->count;
  }
  sr_news_pending_tick(ch, c, now);
}
