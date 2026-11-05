/* Citizens: appearance, persistent notable citizens with weekly schedules, ambient crowds
 * seeded per block and time slot, services with opening hours, metro timetables, rumours. */
#ifndef SR_POPULATION_H
#define SR_POPULATION_H

#include "sim/sr_events.h"
#include "world/sr_world.h"

#define SR_MAX_NPCS              3000
#define SR_MAX_AMBIENT_PER_BLOCK 96

typedef struct SrAppearance {
  uint8_t body, head, skin, face, hair, hair_color, outfit, outfit_color, marker, cyber, accessory;
} SrAppearance;

typedef enum SrRole {
  SR_ROLE_WORKER,
  SR_ROLE_VENDOR,
  SR_ROLE_GANG,
  SR_ROLE_GUARD,
  SR_ROLE_EXEC,
  SR_ROLE_COP,
  SR_ROLE_FIXER,
  SR_ROLE_DRIFTER,
  SR_ROLE_COUNT
} SrRole;

typedef enum SrNpcStatus { SR_NPC_ALIVE, SR_NPC_DEAD, SR_NPC_ARRESTED } SrNpcStatus;

typedef enum SrActivity { SR_ACT_SLEEP, SR_ACT_WORK, SR_ACT_EAT, SR_ACT_LEISURE, SR_ACT_COMMUTE } SrActivity;

typedef struct SrNpc {
  uint32_t id;
  char name[SR_NAME_LEN];
  SrAppearance look;
  uint8_t role;
  int8_t faction;
  uint8_t bravery; /* 0 timid .. 3 brave */
  uint8_t status;
  uint8_t shift_start;              /* hour */
  uint8_t shift_len;                /* hours */
  int32_t home, work, eat, leisure; /* building indices */
  int8_t attitude;                  /* memory of the player: -100..100 */
  bool promoted;                    /* was an ambient citizen */
  uint64_t ambient_key;
} SrNpc;

typedef struct SrPopulation {
  int count;
  SrNpc npcs[SR_MAX_NPCS];
} SrPopulation;

typedef struct SrPlace {
  SrActivity activity;
  int32_t building;    /* current building, or the origin while commuting */
  int32_t to_building; /* commuting destination, else == building */
  float progress;      /* 0..1 along the commute */
  int32_t x, y;        /* position in cm */
} SrPlace;

typedef struct SrAmbient {
  uint64_t key; /* stable identity */
  SrAppearance look;
  uint8_t role;
  int8_t faction;
  uint8_t bravery;
  bool umbrella;
  int32_t x, y;
} SrAmbient;

/* Appearance */
SrAppearance sr_appearance_generate(uint64_t key, SrRole role, int marker);
uint64_t sr_appearance_key(const SrAppearance *a);

/* Notables */
void sr_population_generate(SrPopulation *p, const SrCity *c);
SrPlace sr_npc_place(const SrCity *c, const SrNpc *n, int64_t t);
int sr_population_staff_of(const SrPopulation *p, int building);
int sr_population_promote(SrPopulation *p, const SrCity *c, const SrAmbient *a, int64_t t);
int sr_population_find_by_key(const SrPopulation *p, uint64_t key);
const char *sr_role_name(SrRole r);
/* Memory of the player: helping raises, harming lowers the attitude (-100..100). */
void sr_npc_remember(SrNpc *n, int delta);
/* Greeting that reflects the remembered attitude. */
const char *sr_npc_greeting(const SrNpc *n);

/* Services */
bool sr_poi_open(const SrCity *c, int building, int64_t t);
void sr_poi_hours(const SrCity *c, int building, int *open_hour, int *hours);

/* Ambient crowds */
int sr_crowd_density(SrDistrictType t, int hour); /* citizens per block */
int sr_ambient_generate(const SrCity *c, int block, int64_t t, SrAmbient *out, int max);

/* Transit */
int64_t sr_metro_next_train(const SrCity *c, int station, int direction, int64_t t);
/* Reactions */
typedef enum SrResponse { SR_RESP_NONE, SR_RESP_FLEE, SR_RESP_COVER, SR_RESP_GAWK } SrResponse;
SrResponse sr_react_gunfire(uint8_t bravery, SrDistrictType district, uint64_t key);
bool sr_witness_reports(SrDistrictType district, uint8_t bravery, uint64_t key);
/* Rumours: writes a bark for a citizen in district at time t. Returns true if it references an
 * event. */
bool sr_bark(const SrCity *c, const SrEventLog *log, int district, int64_t t, uint64_t key, char *out, size_t cap);

#endif
