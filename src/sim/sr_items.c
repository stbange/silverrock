#include "sim/sr_items.h"

#include "core/sr_rng.h"

#include <stdio.h>
#include <string.h>

/* name, category, damage, rate, spread, range, noise, weight, mag, type, penetration, price */
static const SrArchetype k_archetypes[] = {
    {"Holdout Pistol", SR_WC_PISTOL, 14, 3.0f, 2.5f, 25, 18, 0.6f, 6, SR_DMG_KINETIC, 0.1f, 120},
    {"Service Pistol", SR_WC_PISTOL, 18, 3.5f, 2.0f, 35, 25, 0.9f, 12, SR_DMG_KINETIC, 0.2f, 220},
    {"Heavy Pistol", SR_WC_PISTOL, 30, 2.0f, 2.2f, 40, 32, 1.3f, 8, SR_DMG_KINETIC, 0.35f, 420},
    {"Revolver", SR_WC_PISTOL, 38, 1.4f, 1.6f, 45, 35, 1.4f, 6, SR_DMG_KINETIC, 0.4f, 380},
    {"Machine Pistol", SR_WC_PISTOL, 11, 12.0f, 4.0f, 25, 28, 1.1f, 24, SR_DMG_KINETIC, 0.1f, 450},
    {"SMG", SR_WC_LONG, 14, 11.0f, 3.2f, 35, 30, 2.4f, 30, SR_DMG_KINETIC, 0.15f, 700},
    {"Pump Shotgun", SR_WC_LONG, 70, 1.0f, 7.0f, 15, 45, 3.2f, 6, SR_DMG_KINETIC, 0.1f, 600},
    {"Auto Shotgun", SR_WC_LONG, 55, 3.0f, 8.0f, 15, 48, 4.0f, 10, SR_DMG_KINETIC, 0.1f, 1400},
    {"Assault Rifle", SR_WC_LONG, 24, 8.0f, 1.8f, 70, 40, 3.5f, 30, SR_DMG_KINETIC, 0.4f, 1600},
    {"Battle Rifle", SR_WC_LONG, 36, 4.0f, 1.4f, 90, 45, 4.2f, 20, SR_DMG_KINETIC, 0.5f, 2200},
    {"DMR", SR_WC_LONG, 48, 2.0f, 0.6f, 140, 48, 4.4f, 10, SR_DMG_KINETIC, 0.6f, 2600},
    {"Sniper Rifle", SR_WC_LONG, 95, 0.6f, 0.2f, 250, 60, 6.0f, 5, SR_DMG_KINETIC, 0.8f, 4200},
    {"Coilgun", SR_WC_HEAVY, 80, 0.8f, 0.4f, 160, 20, 7.0f, 4, SR_DMG_KINETIC, 1.0f, 7800},
    {"Smart Launcher", SR_WC_HEAVY, 120, 0.5f, 1.0f, 120, 70, 8.0f, 4, SR_DMG_THERMAL, 0.5f, 9000},
    {"Arc Projector", SR_WC_HEAVY, 40, 2.0f, 3.0f, 30, 35, 6.5f, 12, SR_DMG_SHOCK, 0.3f, 6500},
    {"Flamer", SR_WC_HEAVY, 20, 10.0f, 6.0f, 12, 40, 7.5f, 40, SR_DMG_THERMAL, 0.0f, 5200},
    {"Taser Pistol", SR_WC_NONLETHAL, 30, 1.0f, 1.5f, 10, 8, 0.8f, 2, SR_DMG_SHOCK, 0.0f, 300},
    {"Sonic Disruptor", SR_WC_NONLETHAL, 25, 2.0f, 5.0f, 15, 15, 2.5f, 8, SR_DMG_SHOCK, 0.2f, 1800},
    {"Knife", SR_WC_MELEE, 22, 2.0f, 0.0f, 1.5f, 3, 0.3f, 0, SR_DMG_KINETIC, 0.2f, 60},
    {"Machete", SR_WC_MELEE, 34, 1.3f, 0.0f, 1.8f, 5, 1.0f, 0, SR_DMG_KINETIC, 0.2f, 140},
    {"Mono-Katana", SR_WC_MELEE, 50, 1.5f, 0.0f, 2.2f, 4, 1.6f, 0, SR_DMG_KINETIC, 0.7f, 3200},
    {"Stun Baton", SR_WC_MELEE, 28, 1.6f, 0.0f, 1.6f, 6, 1.0f, 0, SR_DMG_SHOCK, 0.0f, 260},
    {"Sledgehammer", SR_WC_MELEE, 60, 0.8f, 0.0f, 2.0f, 12, 5.0f, 0, SR_DMG_KINETIC, 0.3f, 180},
    {"Power Fist", SR_WC_MELEE, 55, 1.2f, 0.0f, 1.4f, 10, 3.0f, 0, SR_DMG_KINETIC, 0.4f, 2400},
};

static const SrGadgetDef k_gadgets[SR_GD_COUNT] = {
    {"Frag Grenade", SR_GC_THROWABLE, 6, 0, 90, SR_DMG_KINETIC, 0.4f, 120},
    {"EMP Grenade", SR_GC_THROWABLE, 7, 0, 80, SR_DMG_EMP, 0.4f, 160},
    {"Flashbang", SR_GC_THROWABLE, 8, 4, 0, SR_DMG_KINETIC, 0.3f, 90},
    {"Smoke Grenade", SR_GC_THROWABLE, 8, 20, 0, SR_DMG_KINETIC, 0.3f, 60},
    {"Sleep Gas", SR_GC_THROWABLE, 6, 12, 60, SR_DMG_TOXIN, 0.4f, 180},
    {"Noise Lure", SR_GC_THROWABLE, 25, 6, 0, SR_DMG_KINETIC, 0.2f, 40},
    {"Sticky Cam", SR_GC_THROWABLE, 0, 600, 0, SR_DMG_KINETIC, 0.2f, 150},
    {"Data Spike", SR_GC_THROWABLE, 30, 300, 0, SR_DMG_KINETIC, 0.3f, 400},
    {"Holo-Decoy", SR_GC_DEPLOYABLE, 0, 20, 0, SR_DMG_KINETIC, 0.5f, 350},
    {"Sentry Turret", SR_GC_DEPLOYABLE, 25, 120, 15, SR_DMG_KINETIC, 4.0f, 1500},
    {"Laser Tripmine", SR_GC_DEPLOYABLE, 4, 0, 100, SR_DMG_KINETIC, 0.6f, 220},
    {"Comms Jammer", SR_GC_DEPLOYABLE, 40, 90, 0, SR_DMG_KINETIC, 1.0f, 600},
    {"Breaching Charge", SR_GC_DEPLOYABLE, 3, 0, 120, SR_DMG_KINETIC, 0.8f, 250},
    {"Grapple Line", SR_GC_TOOL, 30, 0, 0, SR_DMG_KINETIC, 1.2f, 900},
    {"Multitool", SR_GC_TOOL, 0, 0, 0, SR_DMG_KINETIC, 0.4f, 200},
    {"Thermal Lance", SR_GC_TOOL, 1, 0, 30, SR_DMG_THERMAL, 1.5f, 500},
    {"Recon Drone", SR_GC_TOOL, 80, 180, 0, SR_DMG_KINETIC, 1.5f, 1200},
    {"Spider Drone", SR_GC_TOOL, 40, 180, 0, SR_DMG_KINETIC, 1.0f, 1400},
    {"Holo-Mask", SR_GC_TOOL, 0, 600, 0, SR_DMG_KINETIC, 0.3f, 2500},
    {"Forged ID Chip", SR_GC_TOOL, 0, 0, 0, SR_DMG_KINETIC, 0.1f, 700},
    {"Optical Camo", SR_GC_TOOL, 0, 15, 0, SR_DMG_KINETIC, 0.8f, 3800},
    {"Medkit", SR_GC_CONSUMABLE, 0, 0, -40, SR_DMG_KINETIC, 0.5f, 80},
    {"Trauma Injector", SR_GC_CONSUMABLE, 0, 0, -80, SR_DMG_KINETIC, 0.3f, 260},
    {"Reflex Stim", SR_GC_CONSUMABLE, 0, 30, 0, SR_DMG_KINETIC, 0.1f, 120},
    {"Painkiller", SR_GC_CONSUMABLE, 0, 120, -10, SR_DMG_KINETIC, 0.1f, 40},
    {"Focus Stim", SR_GC_CONSUMABLE, 0, 120, 0, SR_DMG_KINETIC, 0.1f, 110},
};

static const SrCyberDef k_cyber[] = {
    {"Reflex Booster", SR_CS_NEURAL, 3, 3500},   {"Memory Expansion", SR_CS_NEURAL, 2, 2200},
    {"Thermal Optics", SR_CS_OPTICS, 2, 1800},   {"Netsight Optics", SR_CS_OPTICS, 2, 2400},
    {"Smartlink", SR_CS_ARMS, 2, 2600},          {"Mono-Claws", SR_CS_ARMS, 3, 3900},
    {"Lockpick Fingers", SR_CS_HANDS, 1, 900},   {"Data Jack", SR_CS_HANDS, 1, 1200},
    {"Jump Boosters", SR_CS_LEGS, 3, 3200},      {"Silent Steps", SR_CS_LEGS, 1, 1100},
    {"Subdermal Armour", SR_CS_SKIN, 3, 3000},   {"Chameleon Skin", SR_CS_SKIN, 4, 6500},
    {"Toxin Filter", SR_CS_TORSO, 1, 800},       {"Second Heart", SR_CS_TORSO, 4, 9000},
    {"Stabiliser Spine", SR_CS_SPINE, 2, 2100},  {"Interface Port", SR_CS_SPINE, 2, 2800},
};

static const SrFoodDef k_food[] = {
    {"Street Noodles", 35, 12}, {"Food Pack", 25, 8}, {"Bar Meal", 50, 25}, {"Synth Bar", 15, 5}};

static const SrRecipe k_recipes[] = {
    {SR_GD_SMOKE, 2}, {SR_GD_FLASH, 3}, {SR_GD_FRAG, 4}, {SR_GD_LURE, 1}, {SR_GD_MEDKIT, 2},
    {SR_GD_EMP, 5},   {SR_GD_DATASPIKE, 6}};

int sr_archetype_count(void) { return (int)SR_ARRAY_LEN(k_archetypes); }
const SrArchetype *sr_archetype(int i) {
  return i >= 0 && i < sr_archetype_count() ? &k_archetypes[i] : NULL;
}
const SrGadgetDef *sr_gadget_def(int g) { return g >= 0 && g < SR_GD_COUNT ? &k_gadgets[g] : NULL; }
int sr_cyber_count(void) { return (int)SR_ARRAY_LEN(k_cyber); }
const SrCyberDef *sr_cyber_def(int i) { return i >= 0 && i < sr_cyber_count() ? &k_cyber[i] : NULL; }
int sr_food_count(void) { return (int)SR_ARRAY_LEN(k_food); }
const SrFoodDef *sr_food_def(int i) { return i >= 0 && i < sr_food_count() ? &k_food[i] : NULL; }

const char *sr_mod_name(int m) {
  static const char *const n[SR_MOD_COUNT] = {"none", "Suppressor", "Compensator", "Red Dot",
                                              "Scope", "Extended Mag", "Weapon Light",
                                              "Smart Chip"};
  return m >= 0 && m < SR_MOD_COUNT ? n[m] : "?";
}

SrModSlot sr_mod_slot(SrMod m) {
  switch (m) {
  case SR_MOD_SUPPRESSOR: case SR_MOD_COMPENSATOR: return SR_SLOT_MUZZLE;
  case SR_MOD_REDDOT: case SR_MOD_SCOPE: return SR_SLOT_OPTIC;
  case SR_MOD_EXTMAG: return SR_SLOT_MAG;
  case SR_MOD_LIGHT: return SR_SLOT_UNDER;
  default: return SR_SLOT_CHIP;
  }
}

const char *sr_ammo_name(int a) {
  static const char *const n[SR_AMMO_COUNT] = {"Standard", "Armour-Piercing", "Hollow-Point",
                                               "EMP", "Incendiary", "Gel", "Tracker"};
  return a >= 0 && a < SR_AMMO_COUNT ? n[a] : "?";
}

const char *sr_rarity_name(int r) {
  static const char *const n[SR_RAR_COUNT] = {"Street", "Standard", "Military", "Prototype",
                                              "Signature"};
  return r >= 0 && r < SR_RAR_COUNT ? n[r] : "?";
}

/* ---- manufacturers ------------------------------------------------------------------ */

void sr_maker_tendency(uint64_t city_seed, int maker, int8_t out[4]) {
  SrRng r = sr_rng_stream(city_seed, "maker", (uint64_t)maker, 0);
  for (int i = 0; i < 4; i++) out[i] = (int8_t)sr_rng_range(&r, -2, 2);
  if (maker >= SR_STREET_MAKER_FIRST) { /* street-made: cheap, heavy, loud, sloppy */
    out[1] = 2;
    out[2] = 2;
    out[3] = 1;
  }
}

int sr_maker_style(int maker) { return maker >= SR_STREET_MAKER_FIRST ? 15 : maker; }

bool sr_maker_valid(const SrCity *c, int maker) {
  if (maker >= SR_STREET_MAKER_FIRST) return maker < SR_STREET_MAKER_FIRST + 2;
  if (maker < 0 || maker >= c->factions.corp_count) return false;
  const SrFaction *f = &c->factions.f[maker];
  return f->sector1 == SR_SEC_ARMS || f->sector2 == SR_SEC_ARMS || f->sector1 == SR_SEC_CYBER ||
         f->sector2 == SR_SEC_CYBER;
}

void sr_maker_name(const SrCity *c, int maker, char *out, size_t cap) {
  if (maker == SR_STREET_MAKER_FIRST || maker == SR_STREET_MAKER_FIRST + 1)
    snprintf(out, cap, "%s", maker == SR_STREET_MAKER_FIRST ? "Gutter Works" : "Backalley Forge");
  else if (maker >= 0 && maker < c->factions.corp_count)
    snprintf(out, cap, "%s", c->factions.f[maker].name);
  else
    snprintf(out, cap, "Unknown");
}

void sr_rarity_range(int rarity, float *lo, float *hi) {
  *lo = 1.0f + 0.12f * (float)rarity;
  *hi = *lo + 0.08f;
}

/* ---- items ------------------------------------------------------------------------- */

SrItem sr_item_weapon(int archetype, int maker, int rarity, uint32_t seed) {
  SrItem it;
  memset(&it, 0, sizeof it);
  it.kind = SR_IK_WEAPON;
  it.type = (uint8_t)archetype;
  it.maker = (int8_t)maker;
  it.rarity = (uint8_t)rarity;
  it.seed = seed;
  it.count = 1;
  it.job = -1;
  return it;
}

SrItem sr_item_simple(SrItemKind kind, int type, int count) {
  SrItem it;
  memset(&it, 0, sizeof it);
  it.kind = (uint8_t)kind;
  it.type = (uint8_t)type;
  it.count = (uint16_t)count;
  it.maker = -1;
  it.job = -1;
  return it;
}

SrWeaponStats sr_weapon_stats(uint64_t city_seed, const SrItem *w) {
  SrWeaponStats s;
  memset(&s, 0, sizeof s);
  const SrArchetype *a = sr_archetype(w->type);
  if (!a || w->kind != SR_IK_WEAPON) return s;
  SrRng r = sr_rng_stream(city_seed, "weapon", w->seed, (uint64_t)w->type);
  float lo, hi;
  sr_rarity_range(w->rarity, &lo, &hi);
  float u = sr_rng_float(&r);
  s.damage = a->damage * (lo + (hi - lo) * u);
  int8_t t[4];
  sr_maker_tendency(city_seed, w->maker, t);
  float k[4];
  for (int i = 0; i < 4; i++) k[i] = 0.2f + 0.8f * sr_rng_float(&r);
  s.rate = a->rate * (1.0f + 0.06f * (float)t[0] * k[0]);
  s.spread = a->spread * (1.0f + 0.08f * (float)t[1] * k[1]);
  s.noise = a->noise * (1.0f + 0.08f * (float)t[2] * k[2]);
  s.weight = a->weight * (1.0f + 0.06f * (float)t[3] * k[3]);
  s.range = a->range;
  s.mag = a->mag;
  s.penetration = a->penetration;
  s.damage_type = a->damage_type;
  s.lethal = a->category != SR_WC_NONLETHAL && a->damage_type != SR_DMG_SHOCK;
  s.style = (uint8_t)sr_maker_style(w->maker);
  for (int slot = 0; slot < SR_SLOT_COUNT; slot++) {
    switch (w->mods[slot]) {
    case SR_MOD_SUPPRESSOR: s.noise *= 0.35f; break;
    case SR_MOD_COMPENSATOR: s.spread *= 0.8f; break;
    case SR_MOD_REDDOT: s.spread *= 0.9f; break;
    case SR_MOD_SCOPE: s.range *= 1.5f; break;
    case SR_MOD_EXTMAG: s.mag = s.mag * 3 / 2; break;
    case SR_MOD_SMARTCHIP: s.spread *= 0.7f; break;
    default: break;
    }
  }
  if (a->category != SR_WC_MELEE) {
    switch (w->ammo) {
    case SR_AMMO_AP: s.penetration += 0.5f; s.damage *= 0.9f; break;
    case SR_AMMO_HOLLOW: s.damage *= 1.25f; s.penetration -= 0.3f; break;
    case SR_AMMO_EMP: s.damage_type = SR_DMG_EMP; break;
    case SR_AMMO_INCENDIARY: s.damage_type = SR_DMG_THERMAL; break;
    case SR_AMMO_GEL: s.damage *= 0.7f; s.lethal = false; break;
    default: break;
    }
  }
  if (s.penetration < 0) s.penetration = 0;
  return s;
}

float sr_item_weight(const SrItem *it) {
  float w = 0.2f;
  switch (it->kind) {
  case SR_IK_WEAPON: w = sr_archetype(it->type) ? sr_archetype(it->type)->weight : 1.0f; break;
  case SR_IK_GADGET: w = sr_gadget_def(it->type) ? sr_gadget_def(it->type)->weight : 0.3f; break;
  case SR_IK_AMMO: w = 0.02f; break;
  case SR_IK_FOOD: w = 0.3f; break;
  case SR_IK_COMPONENT: w = 0.1f; break;
  case SR_IK_OUTFIT: w = 1.0f; break;
  case SR_IK_DECK: w = 1.2f; break;
  default: break;
  }
  return w * (float)(it->count ? it->count : 1);
}

int32_t sr_item_base_price(const SrItem *it) {
  switch (it->kind) {
  case SR_IK_WEAPON: {
    const SrArchetype *a = sr_archetype(it->type);
    return a ? a->price * (2 + it->rarity) / 2 : 0;
  }
  case SR_IK_GADGET: return sr_gadget_def(it->type) ? sr_gadget_def(it->type)->price : 0;
  case SR_IK_FOOD: return sr_food_def(it->type) ? sr_food_def(it->type)->price : 0;
  case SR_IK_CYBER: return sr_cyber_def(it->type) ? sr_cyber_def(it->type)->price * (1 + it->grade) : 0;
  case SR_IK_AMMO: return 1 + it->type;
  case SR_IK_MOD: return 300;
  case SR_IK_COMPONENT: return 15;
  case SR_IK_PAYDATA: return 500 + 250 * it->type;
  case SR_IK_OUTFIT: return 150;
  case SR_IK_DECK: return 800 * (1 + it->type);
  default: return 0;
  }
}

void sr_item_name(const SrCity *c, const SrItem *it, char *out, size_t cap) {
  char maker[SR_NAME_LEN];
  switch (it->kind) {
  case SR_IK_WEAPON:
    sr_maker_name(c, it->maker, maker, sizeof maker);
    snprintf(out, cap, "%s %s (%s)", maker, sr_archetype(it->type)->name, sr_rarity_name(it->rarity));
    break;
  case SR_IK_GADGET: snprintf(out, cap, "%s", sr_gadget_def(it->type)->name); break;
  case SR_IK_FOOD: snprintf(out, cap, "%s", sr_food_def(it->type)->name); break;
  case SR_IK_CYBER: snprintf(out, cap, "%s", sr_cyber_def(it->type)->name); break;
  case SR_IK_AMMO: snprintf(out, cap, "%s Rounds", sr_ammo_name(it->type)); break;
  case SR_IK_MOD: snprintf(out, cap, "%s", sr_mod_name(it->type)); break;
  case SR_IK_PAYDATA: snprintf(out, cap, "Paydata (grade %d)", it->type); break;
  case SR_IK_JOB: snprintf(out, cap, "Sealed Package"); break;
  case SR_IK_DECK: snprintf(out, cap, "Cyberdeck Mk %d", it->type + 1); break;
  case SR_IK_OUTFIT: snprintf(out, cap, "Outfit %d", it->type); break;
  default: snprintf(out, cap, "Components"); break;
  }
}

bool sr_install_mod(SrItem *w, SrMod m, SrInventory *inv) {
  if (w->kind != SR_IK_WEAPON || m <= SR_MOD_NONE || m >= SR_MOD_COUNT) return false;
  const SrArchetype *a = sr_archetype(w->type);
  if (a->category == SR_WC_MELEE) return false;
  int idx = sr_inv_find(inv, SR_IK_MOD, m);
  if (idx < 0) return false;
  SrModSlot slot = sr_mod_slot(m);
  if (w->mods[slot] != SR_MOD_NONE && !sr_remove_mod(w, slot, inv)) return false;
  idx = sr_inv_find(inv, SR_IK_MOD, m);
  sr_inv_remove_at(inv, idx, 1);
  w->mods[slot] = (uint8_t)m;
  return true;
}

bool sr_remove_mod(SrItem *w, SrModSlot slot, SrInventory *inv) {
  if (slot >= SR_SLOT_COUNT || w->mods[slot] == SR_MOD_NONE) return false;
  if (!sr_inv_add(inv, sr_item_simple(SR_IK_MOD, w->mods[slot], 1))) return false;
  w->mods[slot] = SR_MOD_NONE;
  return true;
}

/* ---- inventory ---------------------------------------------------------------------- */

void sr_inv_init(SrInventory *inv) { memset(inv, 0, sizeof *inv); }

static bool stackable(SrItemKind k) {
  return k == SR_IK_AMMO || k == SR_IK_GADGET || k == SR_IK_FOOD || k == SR_IK_COMPONENT ||
         k == SR_IK_MOD;
}

bool sr_inv_add(SrInventory *inv, SrItem it) {
  if (stackable((SrItemKind)it.kind)) {
    for (int i = 0; i < inv->count; i++) {
      SrItem *o = &inv->items[i];
      if (o->kind == it.kind && o->type == it.type && o->flags == it.flags) {
        o->count = (uint16_t)(o->count + it.count);
        return true;
      }
    }
  }
  if (inv->count >= SR_INV_MAX) return false;
  inv->items[inv->count++] = it;
  return true;
}

bool sr_inv_remove_at(SrInventory *inv, int idx, int count) {
  if (idx < 0 || idx >= inv->count || count <= 0) return false;
  SrItem *it = &inv->items[idx];
  if (it->count < count) return false;
  it->count = (uint16_t)(it->count - count);
  if (it->count == 0) {
    memmove(&inv->items[idx], &inv->items[idx + 1], sizeof(SrItem) * (size_t)(inv->count - idx - 1));
    inv->count--;
  }
  return true;
}

int sr_inv_find(const SrInventory *inv, SrItemKind kind, int type) {
  for (int i = 0; i < inv->count; i++)
    if (inv->items[i].kind == kind && (type < 0 || inv->items[i].type == type)) return i;
  return -1;
}

int sr_inv_count(const SrInventory *inv, SrItemKind kind, int type) {
  int n = 0;
  for (int i = 0; i < inv->count; i++)
    if (inv->items[i].kind == kind && (type < 0 || inv->items[i].type == type))
      n += inv->items[i].count;
  return n;
}

float sr_inv_weight(const SrInventory *inv) {
  float w = 0;
  for (int i = 0; i < inv->count; i++) w += sr_item_weight(&inv->items[i]);
  return w;
}

float sr_carry_limit(int body) { return 20.0f + 5.0f * (float)body; }

bool sr_inv_transfer_all(SrInventory *from, SrInventory *to) {
  while (from->count > 0) {
    if (!sr_inv_add(to, from->items[0])) return false;
    sr_inv_remove_at(from, 0, from->items[0].count);
  }
  return true;
}

/* ---- crafting ------------------------------------------------------------------------- */

const SrRecipe *sr_recipe_for(SrGadget g) {
  for (size_t i = 0; i < SR_ARRAY_LEN(k_recipes); i++)
    if (k_recipes[i].gadget == g) return &k_recipes[i];
  return NULL;
}

int sr_salvage_value(const SrItem *it) {
  if (it->kind == SR_IK_WEAPON) return 2 + it->rarity * 2;
  if (it->kind == SR_IK_GADGET) return 1;
  if (it->kind == SR_IK_CYBER) return 3;
  return 0;
}

bool sr_craft(SrInventory *inv, uint32_t blueprints, SrGadget g) {
  const SrRecipe *rc = sr_recipe_for(g);
  if (!rc || !(blueprints & (1u << g))) return false;
  int idx = sr_inv_find(inv, SR_IK_COMPONENT, 0);
  if (idx < 0 || inv->items[idx].count < rc->components) return false;
  sr_inv_remove_at(inv, idx, rc->components);
  return sr_inv_add(inv, sr_item_simple(SR_IK_GADGET, g, 1));
}

/* ---- cyberware -------------------------------------------------------------------------- */

int sr_neural_capacity(int body, int tech) { return 4 + body + tech; }

int sr_cyber_load(const SrItem *installed, int n) {
  int load = 0;
  for (int i = 0; i < n; i++)
    if (installed[i].kind == SR_IK_CYBER) load += sr_cyber_def(installed[i].type)->cost;
  return load;
}

bool sr_cyber_glitching(int load, int capacity) { return load > capacity; }

bool sr_cyber_malfunctions(const SrItem *it, uint64_t seed, int64_t day) {
  if (it->kind != SR_IK_CYBER || it->grade > 0) return false;
  return sr_mix64(sr_hash_combine(seed ^ it->seed, (uint64_t)day)) % 100 < 3;
}

void sr_cyber_jailbreak(SrItem *it) { it->flags &= (uint8_t)~SR_IF_BACKDOOR; }

/* ---- vendors -------------------------------------------------------------------------------- */

int32_t sr_price_at(const SrCity *c, int building, const SrItem *it) {
  int wealth = c->districts[c->blocks[c->buildings[building].block].district].wealth;
  int32_t index = 70 + wealth * 6 / 10; /* Undercity ~75 %, Spire ~127 % */
  int64_t p = (int64_t)sr_item_base_price(it) * index / 100;
  return (int32_t)(p < 1 ? 1 : p);
}

int sr_vendor_stock(const SrCity *c, int building, int64_t day, SrItem *out, int max) {
  SrRng r = sr_rng_stream(c->seed, "stock", (uint64_t)building, (uint64_t)day);
  int n = 0;
  int poi = c->buildings[building].poi;
#define PUSH(item) do { if (n < max) out[n++] = (item); } while (0)
  switch (poi) {
  case SR_POI_FOOD:
    PUSH(sr_item_simple(SR_IK_FOOD, 0, 1));
    PUSH(sr_item_simple(SR_IK_FOOD, 3, 1));
    break;
  case SR_POI_BAR: PUSH(sr_item_simple(SR_IK_FOOD, 2, 1)); break;
  case SR_POI_GROCERY:
    PUSH(sr_item_simple(SR_IK_FOOD, 1, 1));
    PUSH(sr_item_simple(SR_IK_FOOD, 3, 1));
    break;
  case SR_POI_CLINIC:
    PUSH(sr_item_simple(SR_IK_GADGET, SR_GD_MEDKIT, 1));
    PUSH(sr_item_simple(SR_IK_GADGET, SR_GD_TRAUMA, 1));
    PUSH(sr_item_simple(SR_IK_GADGET, SR_GD_PAINKILLER, 1));
    break;
  case SR_POI_GUNSHOP: {
    for (int k = 0; k < 5; k++) {
      int maker = SR_STREET_MAKER_FIRST + sr_rng_range(&r, 0, 1);
      int corp = sr_rng_range(&r, 0, c->factions.corp_count - 1);
      if (sr_maker_valid(c, corp) && sr_rng_chance(&r, 600)) maker = corp;
      int arch = sr_rng_range(&r, 0, sr_archetype_count() - 1);
      int rarity = sr_rng_range(&r, 0, 2);
      uint32_t wseed = sr_rng_u32(&r);
      PUSH(sr_item_weapon(arch, maker, rarity, wseed));
    }
    PUSH(sr_item_simple(SR_IK_AMMO, SR_AMMO_STANDARD, 30));
    PUSH(sr_item_simple(SR_IK_AMMO, SR_AMMO_GEL, 30));
    PUSH(sr_item_simple(SR_IK_MOD, SR_MOD_SUPPRESSOR, 1));
    break;
  }
  case SR_POI_ELECTRONICS:
    for (int g = SR_GD_FRAG; g <= SR_GD_CAMO && n < max; g += 2 + sr_rng_range(&r, 0, 2))
      PUSH(sr_item_simple(SR_IK_GADGET, g, 1));
    PUSH(sr_item_simple(SR_IK_DECK, 1, 1));
    PUSH(sr_item_simple(SR_IK_COMPONENT, 0, 5));
    break;
  case SR_POI_RIPPERDOC:
    for (int k = 0; k < 4; k++) {
      SrItem cy = sr_item_simple(SR_IK_CYBER, sr_rng_range(&r, 0, sr_cyber_count() - 1), 1);
      cy.grade = (uint8_t)sr_rng_range(&r, 0, 2);
      cy.maker = (int8_t)sr_rng_range(&r, 0, c->factions.corp_count - 1);
      if (cy.grade > 0) cy.flags |= SR_IF_BACKDOOR; /* corporate chrome phones home */
      cy.seed = sr_rng_u32(&r);
      PUSH(cy);
    }
    break;
  case SR_POI_CLOTHING:
    for (int k = 0; k < 3; k++) PUSH(sr_item_simple(SR_IK_OUTFIT, sr_rng_range(&r, 1, 12), 1));
    break;
  case SR_POI_FENCE: {
    int arch = sr_rng_range(&r, 0, sr_archetype_count() - 1);
    uint32_t wseed = sr_rng_u32(&r);
    PUSH(sr_item_weapon(arch, SR_STREET_MAKER_FIRST, 2, wseed));
    out[n - 1].flags |= SR_IF_BLACKMARKET;
    break;
  }
  default: break;
  }
#undef PUSH
  return n;
}

bool sr_vendor_buys(const SrCity *c, int building, const SrItem *it) {
  int poi = c->buildings[building].poi;
  if (it->kind == SR_IK_JOB) return false;
  if (poi == SR_POI_FENCE) return true;
  if (it->flags & SR_IF_STOLEN) return false;
  switch (poi) {
  case SR_POI_GUNSHOP: return it->kind == SR_IK_WEAPON || it->kind == SR_IK_AMMO || it->kind == SR_IK_MOD;
  case SR_POI_ELECTRONICS: return it->kind == SR_IK_GADGET || it->kind == SR_IK_DECK || it->kind == SR_IK_PAYDATA;
  case SR_POI_RIPPERDOC: return it->kind == SR_IK_CYBER;
  default: return false;
  }
}

bool sr_vendor_sells_blackmarket(int streetwise, int standing) {
  return streetwise >= 5 || standing >= 50;
}
