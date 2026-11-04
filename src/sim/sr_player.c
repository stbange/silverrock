#include "sim/sr_player.h"

#include "core/sr_fixed.h"
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "world/sr_environ.h"

#include <stdio.h>
#include <string.h>

static const SrBackgroundDef k_backgrounds[SR_BG_COUNT] = {
    {"Street Kid", {3, 5, 3, 3, 5}, {SR_SK_PISTOLS, SR_SK_STREETWISE}, 412, SR_ROLE_GANG},
    {"Ex-Corp Wageslave", {3, 3, 3, 5, 5}, {SR_SK_PERSUASION, SR_SK_ELECTRONICS}, 650, SR_ROLE_EXEC},
    {"Ex-Military", {5, 5, 3, 3, 3}, {SR_SK_LONGGUNS, SR_SK_ATHLETICS}, 380, SR_ROLE_GUARD},
    {"Dropout Netrunner", {3, 3, 5, 5, 3}, {SR_SK_NETRUNNING, SR_SK_ELECTRONICS}, 300, SR_ROLE_WORKER},
    {"Back-Alley Medic", {3, 3, 5, 5, 3}, {SR_SK_MEDICINE, SR_SK_ENGINEERING}, 450, SR_ROLE_VENDOR},
};

static const SrHousingDef k_housing[SR_HOUSE_COUNT] = {
    {"Coffin Cube", 120, 8, false, false, SR_D_UNDERCITY},
    {"Starter Studio", 300, 24, false, false, SR_D_RESIDENTIAL},
    {"Mid-Block Apartment", 900, 40, true, true, SR_D_RESIDENTIAL},
    {"Skyway Loft", 2500, 56, true, true, SR_D_COMMERCIAL},
    {"Spire Penthouse", 8000, 64, true, true, SR_D_SPIRE},
};

const SrBackgroundDef *sr_background_def(int bg) {
  return bg >= 0 && bg < SR_BG_COUNT ? &k_backgrounds[bg] : NULL;
}

const SrHousingDef *sr_housing_def(int tier) {
  return tier >= 0 && tier < SR_HOUSE_COUNT ? &k_housing[tier] : NULL;
}

int64_t sr_rent_due_for(int64_t t) { return (t / SR_WEEK_S + 1) * SR_WEEK_S - 60; }

void sr_player_message(SrPlayer *p, const char *from, const char *text, int64_t t) {
  if (p->msg_count == SR_MAX_MESSAGES) {
    memmove(&p->msgs[0], &p->msgs[1], sizeof(SrMessage) * (SR_MAX_MESSAGES - 1));
    p->msg_count--;
  }
  SrMessage *m = &p->msgs[p->msg_count++];
  sr_strlcpy(m->from, from, sizeof m->from);
  sr_strlcpy(m->text, text, sizeof m->text);
  m->time = t;
  m->read = false;
}

int sr_player_unread(const SrPlayer *p) {
  int n = 0;
  for (int i = 0; i < p->msg_count; i++) n += !p->msgs[i].read;
  return n;
}

void sr_player_tx(SrPlayer *p, int64_t t, int32_t amount, const char *what) {
  p->cred += amount;
  if (p->tx_count == SR_MAX_TX) {
    memmove(&p->tx[0], &p->tx[1], sizeof(SrTx) * (SR_MAX_TX - 1));
    p->tx_count--;
  }
  SrTx *x = &p->tx[p->tx_count++];
  x->time = t;
  x->amount = amount;
  sr_strlcpy(x->what, what, sizeof x->what);
}

void sr_player_create(SrPlayer *p, const SrCity *c, const char *name, SrBackground bg,
                      uint64_t look_seed) {
  memset(p, 0, sizeof *p);
  const SrBackgroundDef *d = sr_background_def(bg);
  if (!d) d = &k_backgrounds[0];
  sr_strlcpy(p->name, name, sizeof p->name);
  sr_strlcpy(p->pronouns, "they/them", sizeof p->pronouns);
  p->look = sr_appearance_generate(look_seed, SR_ROLE_WORKER, 0);
  p->background = (uint8_t)bg;
  sr_progress_init(&p->prog);
  memcpy(p->prog.attr, d->attr, SR_ATTR_COUNT);
  memcpy(p->base_attr, d->attr, SR_ATTR_COUNT);
  p->prog.skill_rank[d->trained[0]] = 3;
  p->prog.skill_rank[d->trained[1]] = 3;
  p->cred = d->cash;
  p->max_health = 80.0f + 6.0f * (float)p->prog.attr[SR_ATTR_BODY];
  p->health = p->max_health;
  p->satiety = 70.0f;
  p->fatigue = 20.0f;
  p->home = c->home_building;
  p->tier = SR_HOUSE_STUDIO;
  p->rent = k_housing[SR_HOUSE_STUDIO].rent;
  p->rent_due = sr_rent_due_for(SR_START_TIME_S);
  p->equipped = -1;
  memset(p->quick, 255, sizeof p->quick);
  p->disguise.faction = -1;
  sr_inv_init(&p->inv);
  sr_inv_init(&p->stash);
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_FOOD, 1, 2));
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_DECK, 0, 1)); /* every background has a deck */
  switch (bg) {
  case SR_BG_MILITARY:
    sr_inv_add(&p->inv, sr_item_weapon(1, SR_STREET_MAKER_FIRST, SR_RAR_STANDARD, 11));
    p->armor = 20;
    break;
  case SR_BG_MEDIC:
    sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_MEDKIT, 3));
    sr_inv_add(&p->inv, sr_item_weapon(18, SR_STREET_MAKER_FIRST, SR_RAR_STREET, 12));
    break;
  case SR_BG_NETRUNNER:
    p->inv.items[sr_inv_find(&p->inv, SR_IK_DECK, -1)].type = 1;
    sr_inv_add(&p->inv, sr_item_weapon(16, SR_STREET_MAKER_FIRST, SR_RAR_STREET, 13));
    break;
  default:
    sr_inv_add(&p->inv, sr_item_weapon(0, SR_STREET_MAKER_FIRST, SR_RAR_STREET, 10));
    sr_inv_add(&p->inv, sr_item_weapon(18, SR_STREET_MAKER_FIRST, SR_RAR_STREET, 14));
    break;
  }
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_AMMO, SR_AMMO_STANDARD, 24));
  p->equipped = (int8_t)sr_inv_find(&p->inv, SR_IK_WEAPON, -1);
  p->blueprints = (1u << SR_GD_SMOKE) | (1u << SR_GD_LURE) | (1u << SR_GD_MEDKIT);
  int32_t hx, hy;
  sr_building_center(&c->buildings[c->home_building], &hx, &hy);
  p->pos = sr_v3((float)hx / 100.0f, 1.7f, (float)hy / 100.0f);
  char msg[112];
  snprintf(msg, sizeof msg, "Rent for unit 33-118 is %d cred, due Sunday 23:59. No excuses.", p->rent);
  sr_player_message(p, "Landlord", msg, SR_START_TIME_S);
}

/* ---- needs --------------------------------------------------------------------------- */

void sr_player_minute(SrPlayer *p, bool sleeping) {
  const float satiety_per_min = 100.0f / (36.0f * 60.0f);
  p->satiety -= sleeping ? satiety_per_min * 0.5f : satiety_per_min;
  if (p->satiety < 0) p->satiety = 0;
  if (sleeping) {
    p->fatigue -= 100.0f / (8.0f * 60.0f);
    if (p->fatigue < 0) p->fatigue = 0;
  } else {
    p->fatigue += 100.0f / (18.0f * 60.0f);
    if (p->fatigue > 100) p->fatigue = 100;
  }
  if (p->satiety <= 0.0f) {
    p->starving_hours += 1.0f / 60.0f;
    float floor = p->max_health * 0.25f;
    if (p->health > floor) {
      p->health -= p->max_health * 0.01f / 60.0f;
      if (p->health < floor) p->health = floor;
    }
  }
}

float sr_player_stamina_regen(const SrPlayer *p) { return p->satiety < 25.0f ? 0.5f : 1.0f; }
float sr_player_aim_sway(const SrPlayer *p) { return p->fatigue > 75.0f ? 1.5f : 1.0f; }
float sr_player_matrix_time(const SrPlayer *p) { return p->fatigue > 75.0f ? 1.3f : 1.0f; }

void sr_player_eat(SrPlayer *p, int food) {
  const SrFoodDef *f = sr_food_def(food);
  if (!f) return;
  p->satiety += f->satiety;
  if (p->satiety > 100) p->satiety = 100;
}

/* ---- money ------------------------------------------------------------------------------ */

SrResult sr_player_buy(SrPlayer *p, const SrCity *c, int building, const SrItem *it, int64_t t) {
  if (!sr_poi_open(c, building, t)) return SR_ERR_DENIED;
  int32_t price = sr_price_at(c, building, it);
  if (p->cred < price) return SR_ERR_DENIED;
  if (it->kind == SR_IK_FOOD) {
    sr_player_eat(p, it->type); /* street food is eaten on the spot */
  } else if (!sr_inv_add(&p->inv, *it)) {
    return SR_ERR_NOMEM;
  }
  char name[SR_NAME_LEN];
  sr_item_name(c, it, name, sizeof name);
  sr_player_tx(p, t, -price, name);
  return SR_OK;
}

SrResult sr_player_sell(SrPlayer *p, const SrCity *c, int building, int idx, int64_t t) {
  if (idx < 0 || idx >= p->inv.count) return SR_ERR_INVALID;
  SrItem it = p->inv.items[idx];
  it.count = 1;
  if (!sr_vendor_buys(c, building, &it)) return SR_ERR_DENIED;
  int32_t price = sr_price_at(c, building, &it) * 4 / 10;
  if (it.flags & SR_IF_STOLEN) price = price * 6 / 10;
  if (price < 1) price = 1;
  int before = p->inv.count;
  sr_inv_remove_at(&p->inv, idx, 1);
  if (p->inv.count < before) {
    if (p->equipped == idx) p->equipped = -1;
    else if (p->equipped > idx) p->equipped--;
  }
  sr_player_tx(p, t, price, "sale");
  return SR_OK;
}

/* ---- rent ----------------------------------------------------------------------------------- */

void sr_player_rent_tick(SrPlayer *p, int64_t t, SrEventLog *log) {
  while (t >= p->rent_due - 2 * SR_DAY_S) {
    if (!(p->reminders & 1) && t >= p->rent_due - 2 * SR_DAY_S) {
      p->reminders |= 1;
      sr_player_message(p, "Landlord", "Reminder: rent is due in two days.", p->rent_due - 2 * SR_DAY_S);
    }
    if (!(p->reminders & 2) && t >= p->rent_due - SR_DAY_S) {
      p->reminders |= 2;
      sr_player_message(p, "Landlord", "Rent is due tomorrow night.", p->rent_due - SR_DAY_S);
    }
    if (t < p->rent_due) break;
    if (p->evicted) {
      /* no lease while evicted; the debt stays */
    } else if (p->cred >= p->rent) {
      sr_player_tx(p, p->rent_due, -p->rent, "rent");
      p->missed = 0;
    } else {
      p->rent_debt += p->rent + SR_LATE_FEE;
      p->missed++;
      sr_player_message(p, "Landlord", "You missed the rent. Late fee added.", p->rent_due);
      SrWorldEvent ev = {SR_EV_RENT_MISSED, p->rent_due, -1, p->home, -1, -2, false, "rent missed"};
      if (log) sr_events_add(log, &ev);
      if (p->missed >= 2) {
        p->evicted = true;
        sr_player_message(p, "Landlord", "You're evicted. Your stuff stays until you pay.", p->rent_due);
        SrWorldEvent ev2 = {SR_EV_EVICTION, p->rent_due, -1, p->home, -1, -2, false, "eviction"};
        if (log) sr_events_add(log, &ev2);
      }
    }
    p->rent_due += SR_WEEK_S;
    p->reminders = 0;
  }
}

SrResult sr_player_pay_debt(SrPlayer *p, int64_t t) {
  if (p->rent_debt <= 0) return SR_ERR_INVALID;
  if (p->cred < p->rent_debt) return SR_ERR_DENIED;
  sr_player_tx(p, t, (int32_t)-p->rent_debt, "rent debt");
  p->rent_debt = 0;
  p->missed = 0;
  p->evicted = false;
  return SR_OK;
}

bool sr_player_can_use_home(const SrPlayer *p) { return !p->evicted; }

SrResult sr_player_move(SrPlayer *p, int tier, int building, int64_t t) {
  const SrHousingDef *h = sr_housing_def(tier);
  if (!h || p->evicted || p->rent_debt > 0) return SR_ERR_DENIED;
  if (p->cred < h->rent) return SR_ERR_DENIED; /* deposit */
  sr_player_tx(p, t, -h->rent, "deposit");
  p->tier = (uint8_t)tier;
  p->rent = h->rent; /* applies from the next due date */
  p->home = building;
  return SR_OK;
}

/* ---- sleep ---------------------------------------------------------------------------------- */

SrResult sr_player_sleep_allowed(const SrPlayer *p, int hostiles) {
  (void)p;
  return hostiles > 0 ? SR_ERR_DENIED : SR_OK;
}

void sr_player_wake(SrPlayer *p, SrSleepPlace place, uint64_t seed, int64_t t,
                    SrDistrictType district) {
  if (place != SR_SLEEP_STREET) return;
  int chance = district == SR_D_UNDERCITY || district == SR_D_BARRENS ? 450 : 250;
  SrRng r = sr_rng_stream(seed, "robbery", (uint64_t)(t / SR_DAY_S), (uint64_t)district);
  if (sr_rng_chance(&r, chance) && p->cred > 0) {
    int32_t lost = (int32_t)(p->cred * sr_rng_range(&r, 20, 60) / 100);
    sr_player_tx(p, t, -lost, "robbed while asleep");
    sr_player_message(p, "You", "Woke up lighter. Someone went through your pockets.", t);
  }
}

int32_t sr_capsule_price(const SrCity *c, int building) {
  SrItem night = sr_item_simple(SR_IK_FOOD, 2, 1); /* priced like a bar meal */
  return sr_price_at(c, building, &night);
}

/* ---- travel ---------------------------------------------------------------------------------- */

SrTrip sr_metro_trip(const SrCity *c, int from, int to, int64_t t) {
  SrTrip trip = {0, 0, false};
  int hops = sr_city_metro_hops(c, from, to);
  if (hops < 0) return trip;
  int64_t board = sr_metro_next_train(c, from, 0, t);
  trip.possible = true;
  trip.fare = 5 + 2 * hops;
  trip.seconds = (board - t) + (int64_t)hops * 150;
  return trip;
}

SrTrip sr_cab_trip(const SrCity *c, int a, int b) {
  SrTrip trip = {0, 0, false};
  int32_t ax, ay, bx, by;
  sr_building_center(&c->buildings[a], &ax, &ay);
  sr_building_center(&c->buildings[b], &bx, &by);
  int32_t dist_m = sr_manhattan_i(ax, ay, bx, by) / 100;
  trip.possible = true;
  trip.fare = 15 + dist_m / 25;
  trip.seconds = 120 + dist_m / 10; /* ~36 km/h through traffic */
  return trip;
}
