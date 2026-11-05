/* Items: weapons with procedural variants, mods, ammunition, gadgets, food, cyberware,
 * components; inventories with carry weight; crafting; vendors and fences. */
#ifndef SR_ITEMS_H
#define SR_ITEMS_H

#include "world/sr_world.h"

#define SR_INV_MAX            64
#define SR_STREET_MAKER_FIRST 32

typedef enum SrItemKind {
  SR_IK_NONE,
  SR_IK_WEAPON,
  SR_IK_AMMO,
  SR_IK_GADGET,
  SR_IK_FOOD,
  SR_IK_MOD,
  SR_IK_CYBER,
  SR_IK_COMPONENT,
  SR_IK_PAYDATA,
  SR_IK_JOB,
  SR_IK_OUTFIT,
  SR_IK_DECK
} SrItemKind;

typedef enum SrWeaponCat {
  SR_WC_PISTOL,
  SR_WC_LONG,
  SR_WC_HEAVY,
  SR_WC_NONLETHAL,
  SR_WC_MELEE,
  SR_WC_COUNT
} SrWeaponCat;
typedef enum SrDamageType {
  SR_DMG_KINETIC,
  SR_DMG_THERMAL,
  SR_DMG_EMP,
  SR_DMG_SHOCK,
  SR_DMG_TOXIN,
  SR_DMG_COUNT
} SrDamageType;
typedef enum SrRarity {
  SR_RAR_STREET,
  SR_RAR_STANDARD,
  SR_RAR_MILITARY,
  SR_RAR_PROTOTYPE,
  SR_RAR_SIGNATURE,
  SR_RAR_COUNT
} SrRarity;
typedef enum SrModSlot {
  SR_SLOT_MUZZLE,
  SR_SLOT_OPTIC,
  SR_SLOT_MAG,
  SR_SLOT_UNDER,
  SR_SLOT_CHIP,
  SR_SLOT_COUNT
} SrModSlot;
typedef enum SrMod {
  SR_MOD_NONE,
  SR_MOD_SUPPRESSOR,
  SR_MOD_COMPENSATOR,
  SR_MOD_REDDOT,
  SR_MOD_SCOPE,
  SR_MOD_EXTMAG,
  SR_MOD_LIGHT,
  SR_MOD_SMARTCHIP,
  SR_MOD_COUNT
} SrMod;
typedef enum SrAmmo {
  SR_AMMO_STANDARD,
  SR_AMMO_AP,
  SR_AMMO_HOLLOW,
  SR_AMMO_EMP,
  SR_AMMO_INCENDIARY,
  SR_AMMO_GEL,
  SR_AMMO_TRACKER,
  SR_AMMO_COUNT
} SrAmmo;

typedef enum SrGadget {
  SR_GD_FRAG,
  SR_GD_EMP,
  SR_GD_FLASH,
  SR_GD_SMOKE,
  SR_GD_SLEEPGAS,
  SR_GD_LURE,
  SR_GD_STICKYCAM,
  SR_GD_DATASPIKE,
  SR_GD_DECOY,
  SR_GD_TURRET,
  SR_GD_TRIPMINE,
  SR_GD_JAMMER,
  SR_GD_BREACH,
  SR_GD_GRAPPLE,
  SR_GD_MULTITOOL,
  SR_GD_LANCE,
  SR_GD_DRONE,
  SR_GD_SPIDER,
  SR_GD_HOLOMASK,
  SR_GD_FAKEID,
  SR_GD_CAMO,
  SR_GD_MEDKIT,
  SR_GD_TRAUMA,
  SR_GD_REFLEXSTIM,
  SR_GD_PAINKILLER,
  SR_GD_FOCUSSTIM,
  SR_GD_COUNT
} SrGadget;

typedef enum SrGadgetCat { SR_GC_THROWABLE, SR_GC_DEPLOYABLE, SR_GC_TOOL, SR_GC_CONSUMABLE } SrGadgetCat;

typedef enum SrCyberSlot {
  SR_CS_NEURAL,
  SR_CS_OPTICS,
  SR_CS_ARMS,
  SR_CS_HANDS,
  SR_CS_LEGS,
  SR_CS_SKIN,
  SR_CS_TORSO,
  SR_CS_SPINE,
  SR_CS_COUNT
} SrCyberSlot;

enum SrItemFlags { SR_IF_STOLEN = 1, SR_IF_BACKDOOR = 2, SR_IF_BLACKMARKET = 4 };

typedef struct SrArchetype {
  const char *name;
  uint8_t category;
  float damage, rate, spread, range, noise, weight;
  uint8_t mag;
  uint8_t damage_type;
  float penetration;
  int32_t price;
} SrArchetype;

typedef struct SrGadgetDef {
  const char *name;
  uint8_t category;
  float radius, duration, damage;
  uint8_t damage_type;
  float weight;
  int32_t price;
} SrGadgetDef;

typedef struct SrCyberDef {
  const char *name;
  uint8_t slot;
  uint8_t cost; /* neural capacity */
  int32_t price;
} SrCyberDef;

typedef struct SrFoodDef {
  const char *name;
  float satiety;
  int32_t price;
} SrFoodDef;

typedef struct SrItem {
  uint8_t kind;
  uint8_t type;
  int8_t maker; /* corp index, or SR_STREET_MAKER_FIRST + n */
  uint8_t rarity;
  uint8_t grade; /* cyberware: 0 street .. 3 prototype */
  uint8_t ammo;
  uint8_t flags;
  uint8_t mods[SR_SLOT_COUNT];
  uint16_t count;
  int16_t job; /* job item: job id, else -1 */
  uint32_t seed;
} SrItem;

typedef struct SrWeaponStats {
  float damage, rate, spread, range, noise, weight, penetration;
  int mag;
  uint8_t damage_type;
  bool lethal;
  uint8_t style; /* manufacturer visual style */
} SrWeaponStats;

typedef struct SrInventory {
  int count;
  SrItem items[SR_INV_MAX];
} SrInventory;

int sr_archetype_count(void);
const SrArchetype *sr_archetype(int i);
const SrGadgetDef *sr_gadget_def(int g);
const SrCyberDef *sr_cyber_def(int i);
int sr_cyber_count(void);
const SrFoodDef *sr_food_def(int i);
int sr_food_count(void);
const char *sr_mod_name(int m);
SrModSlot sr_mod_slot(SrMod m);
const char *sr_ammo_name(int a);
const char *sr_rarity_name(int r);

/* Manufacturer stat tendencies (-2..2 for rate, spread, noise, weight) and style. */
void sr_maker_tendency(uint64_t city_seed, int maker, int8_t out[4]);
int sr_maker_style(int maker);
bool sr_maker_valid(const SrCity *c, int maker);
void sr_maker_name(const SrCity *c, int maker, char *out, size_t cap);
/* Rarity damage multiplier range. */
void sr_rarity_range(int rarity, float *lo, float *hi);

SrItem sr_item_weapon(int archetype, int maker, int rarity, uint32_t seed);
SrItem sr_item_simple(SrItemKind kind, int type, int count);
SrWeaponStats sr_weapon_stats(uint64_t city_seed, const SrItem *w);
float sr_item_weight(const SrItem *it);
int32_t sr_item_base_price(const SrItem *it);
void sr_item_name(const SrCity *c, const SrItem *it, char *out, size_t cap);
bool sr_install_mod(SrItem *w, SrMod m, SrInventory *inv);
bool sr_remove_mod(SrItem *w, SrModSlot slot, SrInventory *inv);

void sr_inv_init(SrInventory *inv);
/* Adds (stacking simple items); false when full. */
bool sr_inv_add(SrInventory *inv, SrItem it);
bool sr_inv_remove_at(SrInventory *inv, int idx, int count);
int sr_inv_find(const SrInventory *inv, SrItemKind kind, int type);
int sr_inv_count(const SrInventory *inv, SrItemKind kind, int type);
float sr_inv_weight(const SrInventory *inv);
float sr_carry_limit(int body);
bool sr_inv_transfer_all(SrInventory *from, SrInventory *to);

/* Crafting */
typedef struct SrRecipe {
  uint8_t gadget;
  uint8_t components;
} SrRecipe;
const SrRecipe *sr_recipe_for(SrGadget g);
int sr_salvage_value(const SrItem *it);
/* Crafts a gadget; requires a known blueprint (bitmask) and components. */
bool sr_craft(SrInventory *inv, uint32_t blueprints, SrGadget g);

/* Cyberware */
int sr_neural_capacity(int body, int tech);
int sr_cyber_load(const SrItem *installed, int n);
bool sr_cyber_glitching(int load, int capacity);
/* Daily malfunction check for low-grade cyberware. */
bool sr_cyber_malfunctions(const SrItem *it, uint64_t seed, int64_t day);
void sr_cyber_jailbreak(SrItem *it);

/* Vendors */
int32_t sr_price_at(const SrCity *c, int building, const SrItem *it);
int sr_vendor_stock(const SrCity *c, int building, int64_t day, SrItem *out, int max);
/* Whether a vendor accepts an item for sale. */
bool sr_vendor_buys(const SrCity *c, int building, const SrItem *it);
bool sr_vendor_sells_blackmarket(int streetwise, int standing);

#endif
