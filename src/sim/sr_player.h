/* The player's everyday life: character creation, needs, money, rent, eviction, housing,
 * travel, messages and transactions. */
#ifndef SR_PLAYER_H
#define SR_PLAYER_H

#include "core/sr_math.h"
#include "sim/sr_heat.h"
#include "sim/sr_items.h"
#include "sim/sr_population.h"
#include "sim/sr_progress.h"

#define SR_MAX_MESSAGES 32
#define SR_MAX_TX 64
#define SR_LATE_FEE 50
#define SR_MAX_CYBER 8

typedef enum SrBackground {
  SR_BG_STREET_KID, SR_BG_WAGESLAVE, SR_BG_MILITARY, SR_BG_NETRUNNER, SR_BG_MEDIC, SR_BG_COUNT
} SrBackground;

typedef enum SrHousing {
  SR_HOUSE_COFFIN, SR_HOUSE_STUDIO, SR_HOUSE_MID, SR_HOUSE_LOFT, SR_HOUSE_PENTHOUSE, SR_HOUSE_COUNT
} SrHousing;

typedef enum SrSleepPlace { SR_SLEEP_HOME, SR_SLEEP_CAPSULE, SR_SLEEP_STREET } SrSleepPlace;

typedef struct SrBackgroundDef {
  const char *name;
  int8_t attr[SR_ATTR_COUNT];
  uint8_t trained[2];
  int32_t cash;
  uint8_t contact_role;
} SrBackgroundDef;

typedef struct SrHousingDef {
  const char *name;
  int32_t rent;
  uint8_t stash_slots;
  bool workbench, rig;
  int8_t district_type; /* preferred district */
} SrHousingDef;

typedef struct SrMessage {
  char from[24];
  char text[112];
  int64_t time;
  bool read;
} SrMessage;

typedef struct SrTx {
  int64_t time;
  int32_t amount;
  char what[28];
} SrTx;

typedef struct SrPlayer {
  char name[SR_NAME_LEN];
  char pronouns[12];
  SrAppearance look;
  uint8_t background;
  SrProgress prog;
  int8_t base_attr[SR_ATTR_COUNT];
  int64_t cred;
  float health, max_health, armor;
  float satiety, fatigue;
  float starving_hours;
  /* housing */
  int32_t home;
  uint8_t tier;
  int32_t rent;
  int64_t rent_due;
  int64_t rent_debt;
  uint8_t missed;
  bool evicted;
  uint8_t reminders; /* bit0: two days, bit1: one day (for the current due date) */
  bool medevac;
  /* gear */
  SrInventory inv;
  SrInventory stash;
  SrItem cyber[SR_MAX_CYBER];
  int cyber_count;
  uint32_t blueprints;
  int8_t equipped; /* inventory index of the active weapon, -1 none */
  uint8_t quick[4]; /* gadget types bound to quick slots, 255 empty */
  uint8_t outfit;
  SrDisguise disguise;
  /* body */
  SrVec3 pos;
  float yaw, pitch;
  bool crouched;
  int kills, knockouts;
  /* communication */
  int msg_count;
  SrMessage msgs[SR_MAX_MESSAGES];
  int tx_count;
  SrTx tx[SR_MAX_TX];
} SrPlayer;

const SrBackgroundDef *sr_background_def(int bg);
const SrHousingDef *sr_housing_def(int tier);

void sr_player_create(SrPlayer *p, const SrCity *c, const char *name, SrBackground bg,
                      uint64_t look_seed);
/* Due time of the rent for the week containing t (Sunday 23:59). */
int64_t sr_rent_due_for(int64_t t);

/* Needs: advance one in-game minute. */
void sr_player_minute(SrPlayer *p, bool sleeping);
float sr_player_stamina_regen(const SrPlayer *p);
float sr_player_aim_sway(const SrPlayer *p);
float sr_player_matrix_time(const SrPlayer *p);
void sr_player_eat(SrPlayer *p, int food);

/* Money */
void sr_player_tx(SrPlayer *p, int64_t t, int32_t amount, const char *what);
SrResult sr_player_buy(SrPlayer *p, const SrCity *c, int building, const SrItem *it, int64_t t);
SrResult sr_player_sell(SrPlayer *p, const SrCity *c, int building, int inv_index, int64_t t);

/* Rent: call whenever time passes; handles reminders, payment, missed payments, eviction. */
void sr_player_rent_tick(SrPlayer *p, int64_t t, SrEventLog *log);
SrResult sr_player_pay_debt(SrPlayer *p, int64_t t);
SrResult sr_player_move(SrPlayer *p, int tier, int building, int64_t t);
bool sr_player_can_use_home(const SrPlayer *p);

/* Sleep: returns SR_ERR_DENIED if hostiles are near. Robbery decided from seed and day. */
SrResult sr_player_sleep_allowed(const SrPlayer *p, int alerted_hostiles_within_50m);
void sr_player_wake(SrPlayer *p, SrSleepPlace place, uint64_t seed, int64_t t,
                    SrDistrictType district);
int32_t sr_capsule_price(const SrCity *c, int building);

/* Travel */
typedef struct SrTrip {
  int32_t fare;
  int64_t seconds;
  bool possible;
} SrTrip;
SrTrip sr_metro_trip(const SrCity *c, int from_station, int to_station, int64_t t);
SrTrip sr_cab_trip(const SrCity *c, int from_building, int to_building);

void sr_player_message(SrPlayer *p, const char *from, const char *text, int64_t t);
int sr_player_unread(const SrPlayer *p);

#endif
