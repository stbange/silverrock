/* The share market: one price per megacorp, updated every strategic tick from its resources and
 * the shocks of recent outcomes; trading for a fee; the portfolio; and the suspicion large trades
 * draw when they come just before an operation the player takes part in. */
#include "sim/sr_chess.h"

#include <string.h>

/* The price a corp's resources justify. */
static int32_t fair_price(const SrChess *ch, int f) {
  const int16_t *r = ch->f[f].res;
  return 10 + r[SR_RES_CAPITAL] / 2 + r[SR_RES_IMAGE] / 3 + r[SR_RES_INFLUENCE] / 5;
}

void sr_market_init(SrMarket *m, const SrChess *ch, const SrCity *c) {
  memset(m, 0, sizeof *m);
  for (int f = 0; f < c->factions.corp_count; f++) m->price[f] = fair_price(ch, f);
}

void sr_market_tick(SrChess *ch, const SrCity *c) {
  SrMarket *m = &ch->market;
  for (int f = 0; f < c->factions.corp_count; f++) {
    int32_t p = m->price[f];
    int32_t fair = fair_price(ch, f);
    int64_t next = p + (fair - p) / 4 + (int64_t)p * m->shock[f] / 1000;
    if (m->shock[f] < 0 && next >= p) next = p - 1; /* bad news always shows */
    if (m->shock[f] > 0 && next <= p) next = p + 1;
    m->price[f] = (int32_t)SR_MAX(SR_PRICE_MIN, next);
    m->shock[f] = 0;
  }
}

int32_t sr_market_fee(int32_t value) {
  if (value < 0) value = -value;
  return SR_MAX(2, value / 50); /* 2 % with a 2 cred minimum */
}

int64_t sr_market_trade(SrChess *ch, int corp, int32_t qty, int64_t cash, int64_t now, bool *ok) {
  SrMarket *m = &ch->market;
  *ok = false;
  if (corp < 0 || corp >= SR_MAX_FACTIONS || m->price[corp] <= 0 || qty == 0) return 0;
  int64_t value = (int64_t)m->price[corp] * qty;
  int32_t fee = sr_market_fee((int32_t)SR_CLAMP(value, -2000000000LL, 2000000000LL));
  int64_t delta = -value - fee;
  /* buying needs the cash; selling short is allowed down to -100 shares (margin) */
  if (delta < 0 && cash + delta < 0 && qty > 0) return 0;
  if (qty < 0 && m->shares[corp] + qty < -100) return 0;
  m->shares[corp] += qty;
  if (m->trade_count >= SR_MAX_TRADES) {
    memmove(&m->trades[0], &m->trades[1], sizeof(SrTrade) * (SR_MAX_TRADES - 1));
    m->trade_count--;
  }
  m->trades[m->trade_count++] = (SrTrade){(int8_t)corp, qty, m->price[corp], now};
  *ok = true;
  return delta;
}

int64_t sr_market_portfolio(const SrMarket *m, const SrCity *c) {
  int64_t v = 0;
  for (int f = 0; f < c->factions.corp_count; f++) v += (int64_t)m->shares[f] * m->price[f];
  return v;
}

static bool finance_corp(const SrCity *c, int f) {
  return c->factions.f[f].sector1 == SR_SEC_FINANCE || c->factions.f[f].sector2 == SR_SEC_FINANCE;
}

int sr_market_check_insider(SrChess *ch, const SrCity *c, int corp, int64_t now) {
  SrMarket *m = &ch->market;
  int flagged = 0;
  for (int i = 0; i < m->trade_count; i++) {
    const SrTrade *t = &m->trades[i];
    int32_t size = t->qty < 0 ? -t->qty : t->qty;
    if (t->corp != corp || size < SR_INSIDER_QTY || now - t->time > 86400 || t->time > now) continue;
    /* every Finance & Insurance corp notices (the target's own bank first, when it has one) */
    for (int f = 0; f < c->factions.corp_count; f++) {
      if (!finance_corp(c, f)) continue;
      if (m->suspicion_count >= SR_MAX_SUSPICION) {
        memmove(&m->suspicion[0], &m->suspicion[1], sizeof(SrSuspicion) * (SR_MAX_SUSPICION - 1));
        m->suspicion_count--;
      }
      m->suspicion[m->suspicion_count++] = (SrSuspicion){(int8_t)corp, (int8_t)f, now};
      flagged++;
    }
    if (flagged == 0) { /* no bank in town: the market regulator (the city) still notes it */
      if (m->suspicion_count >= SR_MAX_SUSPICION) {
        memmove(&m->suspicion[0], &m->suspicion[1], sizeof(SrSuspicion) * (SR_MAX_SUSPICION - 1));
        m->suspicion_count--;
      }
      m->suspicion[m->suspicion_count++] = (SrSuspicion){(int8_t)corp, -1, now};
      flagged++;
    }
    break;
  }
  return flagged;
}
