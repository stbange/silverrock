#include "sim/sr_population.h"

#include "core/sr_rng.h"

#include <stdio.h>
#include <string.h>

const char *sr_role_name(SrRole r) {
  static const char *const n[SR_ROLE_COUNT] = {"worker",    "vendor",         "gang member", "guard",
                                               "executive", "police officer", "fixer",       "drifter"};
  return (unsigned)r < SR_ROLE_COUNT ? n[r] : "?";
}

/* ---- appearance --------------------------------------------------------------------- */

SrAppearance sr_appearance_generate(uint64_t key, SrRole role, int marker) {
  SrRng r = sr_rng_stream(key, "look", (uint64_t)role, 0);
  SrAppearance a;
  a.body = (uint8_t)sr_rng_range(&r, 0, 3);
  a.head = (uint8_t)sr_rng_range(&r, 0, 7);
  a.skin = (uint8_t)sr_rng_range(&r, 0, 7);
  a.face = (uint8_t)sr_rng_range(&r, 0, 15);
  a.hair = (uint8_t)sr_rng_range(&r, 0, 11);
  a.hair_color = (uint8_t)sr_rng_range(&r, 0, 11);
  a.outfit = (uint8_t)((unsigned)role * 4u + (unsigned)sr_rng_range(&r, 0, 3)); /* role clothing */
  a.outfit_color = (uint8_t)sr_rng_range(&r, 0, 15);
  a.marker = (uint8_t)marker;
  int cyber_chance = role == SR_ROLE_GANG ? 700 : role == SR_ROLE_GUARD ? 500 : 200;
  a.cyber = (uint8_t)(sr_rng_chance(&r, cyber_chance) ? sr_rng_range(&r, 1, 5) : 0);
  a.accessory = (uint8_t)sr_rng_range(&r, 0, 7);
  /* the body: build weighted by role, frame, height; chrome limbs for the augmented (drawn after the
   * older fields, so heads, hair and clothes stay as they were) */
  static const uint8_t k_builds[SR_ROLE_COUNT][8] = {
      [SR_ROLE_WORKER] = {SR_BUILD_AVERAGE, SR_BUILD_AVERAGE, SR_BUILD_HEAVY, SR_BUILD_STOCKY, SR_BUILD_SLENDER,
                          SR_BUILD_THIN, SR_BUILD_CURVY, SR_BUILD_ATHLETIC},
      [SR_ROLE_VENDOR] = {SR_BUILD_AVERAGE, SR_BUILD_HEAVY, SR_BUILD_HEAVY, SR_BUILD_STOCKY, SR_BUILD_CURVY,
                          SR_BUILD_THIN, SR_BUILD_AVERAGE, SR_BUILD_SLENDER},
      [SR_ROLE_GANG] = {SR_BUILD_MUSCULAR, SR_BUILD_ATHLETIC, SR_BUILD_STOCKY, SR_BUILD_THIN, SR_BUILD_SLENDER,
                        SR_BUILD_CURVY, SR_BUILD_MUSCULAR, SR_BUILD_AVERAGE},
      [SR_ROLE_GUARD] = {SR_BUILD_MUSCULAR, SR_BUILD_ATHLETIC, SR_BUILD_ATHLETIC, SR_BUILD_STOCKY, SR_BUILD_AVERAGE,
                         SR_BUILD_MUSCULAR, SR_BUILD_HEAVY, SR_BUILD_ATHLETIC},
      [SR_ROLE_EXEC] = {SR_BUILD_SLENDER, SR_BUILD_SLENDER, SR_BUILD_AVERAGE, SR_BUILD_ATHLETIC, SR_BUILD_CURVY,
                        SR_BUILD_HEAVY, SR_BUILD_THIN, SR_BUILD_AVERAGE},
      [SR_ROLE_COP] = {SR_BUILD_ATHLETIC, SR_BUILD_MUSCULAR, SR_BUILD_STOCKY, SR_BUILD_HEAVY, SR_BUILD_AVERAGE,
                       SR_BUILD_ATHLETIC, SR_BUILD_AVERAGE, SR_BUILD_MUSCULAR},
      [SR_ROLE_FIXER] = {SR_BUILD_AVERAGE, SR_BUILD_SLENDER, SR_BUILD_HEAVY, SR_BUILD_CURVY, SR_BUILD_STOCKY,
                         SR_BUILD_THIN, SR_BUILD_ATHLETIC, SR_BUILD_AVERAGE},
      [SR_ROLE_DRIFTER] = {SR_BUILD_THIN, SR_BUILD_THIN, SR_BUILD_SLENDER, SR_BUILD_AVERAGE, SR_BUILD_HEAVY,
                           SR_BUILD_STOCKY, SR_BUILD_THIN, SR_BUILD_AVERAGE},
  };
  bool female = sr_rng_chance(&r, 500);
  int build = k_builds[(unsigned)role < SR_ROLE_COUNT ? role : 0][sr_rng_range(&r, 0, 7)];
  static const uint8_t k_heights[8] = {SR_HEIGHT_AVERAGE, SR_HEIGHT_AVERAGE, SR_HEIGHT_AVERAGE, SR_HEIGHT_SHORT,
                                       SR_HEIGHT_SHORT,   SR_HEIGHT_TALL,    SR_HEIGHT_TALL,    SR_HEIGHT_VERY_TALL};
  sr_look_set_body(&a, build, female, k_heights[sr_rng_range(&r, 0, 7)]);
  int level = a.cyber, limbs = 0;
  if (level >= 2 && sr_rng_chance(&r, 350)) limbs |= sr_rng_chance(&r, 500) ? SR_LIMB_ARM_L : SR_LIMB_ARM_R;
  if (level >= 4) limbs |= SR_LIMB_ARM_L | SR_LIMB_ARM_R;
  if (level >= 3 && sr_rng_chance(&r, 300)) limbs |= SR_LIMB_LEGS;
  sr_look_set_cyber(&a, level, limbs, sr_rng_range(&r, 0, 3));
  return a;
}

void sr_look_set_body(SrAppearance *a, int build, bool female, int height) {
  a->body = (uint8_t)((build & 7) | (female ? 8 : 0) | ((height & 3) << 4));
}

void sr_look_set_cyber(SrAppearance *a, int level, int limbs, int finish) {
  a->cyber = (uint8_t)((level & 7) | ((limbs & 7) << 3) | (limbs ? (finish & 3) << 6 : 0));
}

uint64_t sr_appearance_key(const SrAppearance *a) {
  uint64_t k = a->body;
  k = (k << 3) | a->head;
  k = (k << 3) | a->skin;
  k = (k << 4) | a->face;
  k = (k << 4) | a->hair;
  k = (k << 4) | a->hair_color;
  k = (k << 6) | a->outfit;
  k = (k << 4) | a->outfit_color;
  k = (k << 6) | a->marker;
  k = (k << 3) | (a->cyber & 7u);
  k = (k << 3) | a->accessory;
  return k ^ ((uint64_t)(a->cyber >> 3) << 58); /* limbs and finish on top: older looks keep their keys */
}

/* ---- services ------------------------------------------------------------------------- */

void sr_poi_hours(const SrCity *c, int building, int *open_hour, int *hours) {
  SrRng r = sr_rng_stream(c->seed, "hours", (uint64_t)building, 0);
  int p = c->buildings[building].poi;
  switch (p) {
    case SR_POI_FOOD:
      *open_hour = sr_rng_range(&r, 12, 16);
      *hours = 16;
      break;
    case SR_POI_BAR:
      *open_hour = 18;
      *hours = 12;
      break;
    case SR_POI_FIXER:
      *open_hour = 20;
      *hours = 8;
      break;
    case SR_POI_GUNSHOP:
    case SR_POI_ELECTRONICS:
    case SR_POI_CLOTHING:
    case SR_POI_GROCERY:
    case SR_POI_FENCE:
    case SR_POI_RIPPERDOC:
      *open_hour = sr_rng_range(&r, 9, 14);
      *hours = 12;
      break;
    default:
      *open_hour = 0;
      *hours = 24;
      break;
  }
}

bool sr_poi_open(const SrCity *c, int building, int64_t t) {
  int open, hours;
  sr_poi_hours(c, building, &open, &hours);
  if (hours >= 24) return true;
  int h = (int)((t % 86400) / 3600);
  return ((h - open + 24) % 24) < hours;
}

/* ---- notables ------------------------------------------------------------------------- */

static int random_building(const SrCity *c, SrRng *r, const int *types, int ntypes, int poi) {
  for (int tries = 0; tries < 5000; tries++) {
    int b = sr_rng_range(r, 0, c->building_count - 1);
    if (poi >= 0 && c->buildings[b].poi != poi) continue;
    int t = c->districts[c->blocks[c->buildings[b].block].district].type;
    for (int i = 0; i < ntypes; i++)
      if (types[i] == t) return b;
  }
  return c->home_building; /* GCOVR_EXCL_LINE reason: every city has matching buildings */
}

static int nearest_poi_or(const SrCity *c, SrPoi poi, int from, int fallback) {
  int b = sr_city_find_poi_near(c, poi, from, NULL);
  return b >= 0 ? b : fallback;
}

static SrNpc *add_npc(SrPopulation *p, const SrCity *c, SrRole role, int faction, int home, int work, int shift_start,
                      int shift_len) {
  if (p->count >= SR_MAX_NPCS) return NULL;
  SrNpc *n = &p->npcs[p->count];
  memset(n, 0, sizeof *n);
  n->id = (uint32_t)p->count;
  sr_name_person(c->seed, 20000u + n->id, n->name, sizeof n->name);
  int marker = faction >= 0 ? c->factions.f[faction].marker : 0;
  n->look = sr_appearance_generate(sr_hash_combine(c->seed, n->id), role, marker);
  int frame = sr_name_frame(n->name); /* the body matches the name */
  if (frame >= 0) sr_look_set_body(&n->look, sr_look_build(&n->look), frame == 1, sr_look_height(&n->look));
  n->role = (uint8_t)role;
  n->faction = (int8_t)faction;
  SrRng r = sr_rng_stream(c->seed, "npc", n->id, 0);
  n->bravery = (uint8_t)(role == SR_ROLE_GUARD || role == SR_ROLE_COP || role == SR_ROLE_GANG ? sr_rng_range(&r, 2, 3)
                                                                                              : sr_rng_range(&r, 0, 3));
  n->status = SR_NPC_ALIVE;
  n->home = home;
  n->work = work;
  n->shift_start = (uint8_t)shift_start;
  n->shift_len = (uint8_t)shift_len;
  n->eat = nearest_poi_or(c, SR_POI_FOOD, work, work);
  n->leisure = nearest_poi_or(c, SR_POI_BAR, home, home);
  p->count++;
  return n;
}

void sr_population_generate(SrPopulation *p, const SrCity *c) {
  memset(p, 0, sizeof *p);
  SrRng r = sr_rng_stream(c->seed, "population", 0, 0);
  static const int homes[] = {SR_D_RESIDENTIAL, SR_D_OLDQUARTER, SR_D_UNDERCITY};
  static const int works[] = {SR_D_COMMERCIAL, SR_D_INDUSTRIAL,    SR_D_SPIRE, SR_D_DOCKS,
                              SR_D_CIVIC,      SR_D_ENTERTAINMENT, SR_D_CAMPUS};
  /* staff for every service */
  for (int b = 0; b < c->building_count; b++) {
    int poi = c->buildings[b].poi;
    if (poi == SR_POI_NONE || poi >= SR_POI_FIRST_SPECIAL) continue;
    int open, hours;
    sr_poi_hours(c, b, &open, &hours);
    SrRole role = poi == SR_POI_FIXER ? SR_ROLE_FIXER : poi == SR_POI_POLICE ? SR_ROLE_COP : SR_ROLE_VENDOR;
    int faction = poi == SR_POI_POLICE ? c->factions.police : -1;
    int home = random_building(c, &r, homes, 3, SR_POI_NONE);
    if (hours >= 24) {
      add_npc(p, c, role, faction, home, b, open, 12);
      add_npc(p, c, role, faction, random_building(c, &r, homes, 3, SR_POI_NONE), b, (open + 12) % 24, 12);
    } else {
      add_npc(p, c, role, faction, home, b, open, hours);
    }
  }
  /* corporate guards and executives */
  for (int f = 0; f < c->factions.corp_count; f++) {
    const SrFaction *fa = &c->factions.f[f];
    int sites[9];
    int ns = 0;
    sites[ns++] = fa->hq_building;
    for (int i = 0; i < fa->facility_count; i++) sites[ns++] = fa->facilities[i];
    for (int s = 0; s < ns; s++) {
      for (int g = 0; g < 2; g++)
        add_npc(p, c, SR_ROLE_GUARD, f, random_building(c, &r, homes, 3, SR_POI_NONE), sites[s], g * 12, 12);
    }
    add_npc(p, c, SR_ROLE_EXEC, f, fa->hq_building, fa->hq_building, 9, 10);
  }
  /* gang members */
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++) {
    const SrFaction *fa = &c->factions.f[g];
    for (int m = 0; m < 8; m++) add_npc(p, c, SR_ROLE_GANG, g, fa->hq_building, fa->hq_building, 18 + m % 4, 10);
  }
  /* residents and workers */
  int residents = 0;
  while ((p->count < 1500 || residents < 600) && p->count < SR_MAX_NPCS) {
    residents++;
    int home = random_building(c, &r, homes, 3, SR_POI_NONE);
    int work = random_building(c, &r, works, 7, -1);
    static const int shifts[3] = {6, 14, 22};
    SrRole role = sr_rng_chance(&r, 60) ? SR_ROLE_DRIFTER : SR_ROLE_WORKER;
    add_npc(p, c, role, -1, home, work, shifts[sr_rng_range(&r, 0, 2)], 8);
  }
}

int sr_population_staff_of(const SrPopulation *p, int building) {
  for (int i = 0; i < p->count; i++)
    if (p->npcs[i].work == building &&
        (p->npcs[i].role == SR_ROLE_VENDOR || p->npcs[i].role == SR_ROLE_FIXER || p->npcs[i].role == SR_ROLE_COP))
      return i;
  return -1;
}

static void building_pos(const SrCity *c, int b, int32_t *x, int32_t *y) { sr_building_center(&c->buildings[b], x, y); }

SrPlace sr_npc_place(const SrCity *c, const SrNpc *n, int64_t t) {
  SrPlace pl;
  memset(&pl, 0, sizeof pl);
  if (n->status != SR_NPC_ALIVE) {
    pl.activity = SR_ACT_SLEEP;
    pl.building = pl.to_building = -1;
    pl.x = pl.y = -1;
    return pl;
  }
  int64_t day = t / 86400;
  int jitter = 0;
  if (n->role == SR_ROLE_WORKER || n->role == SR_ROLE_DRIFTER) {
    jitter = (int)(sr_hash_combine(n->id, (uint64_t)day) % 31) - 15;
  }
  int64_t start = (int64_t)n->shift_start * 3600 + jitter * 60;
  int64_t rel = ((t % 86400) - start + 2 * 86400) % 86400;
  int64_t L = (int64_t)n->shift_len * 3600;
  int32_t a = n->work, b = n->work;
  float prog = 0.0f;
  SrActivity act;
  if (rel < L) {
    act = SR_ACT_WORK;
  } else if (rel < L + 1800) {
    act = SR_ACT_COMMUTE;
    a = n->work;
    b = n->eat;
    prog = (float)(rel - L) / 1800.0f;
  } else if (rel < L + 5400) {
    act = SR_ACT_EAT;
    a = b = n->eat;
  } else if (rel < L + 7200) {
    act = SR_ACT_COMMUTE;
    a = n->eat;
    b = n->leisure;
    prog = (float)(rel - L - 5400) / 1800.0f;
  } else if (rel < L + 14400) {
    act = SR_ACT_LEISURE;
    a = b = n->leisure;
  } else if (rel < L + 16200) {
    act = SR_ACT_COMMUTE;
    a = n->leisure;
    b = n->home;
    prog = (float)(rel - L - 14400) / 1800.0f;
  } else if (rel < 86400 - 1800) {
    act = SR_ACT_SLEEP;
    a = b = n->home;
  } else {
    act = SR_ACT_COMMUTE;
    a = n->home;
    b = n->work;
    prog = (float)(rel - (86400 - 1800)) / 1800.0f;
  }
  pl.activity = act;
  pl.building = a;
  pl.to_building = b;
  pl.progress = prog;
  int32_t ax, ay, bx, by;
  building_pos(c, a, &ax, &ay);
  building_pos(c, b, &bx, &by);
  pl.x = ax + (int32_t)((float)(bx - ax) * prog);
  pl.y = ay + (int32_t)((float)(by - ay) * prog);
  return pl;
}

int sr_population_find_by_key(const SrPopulation *p, uint64_t key) {
  for (int i = 0; i < p->count; i++)
    if (p->npcs[i].promoted && p->npcs[i].ambient_key == key) return i;
  return -1;
}

int sr_population_promote(SrPopulation *p, const SrCity *c, const SrAmbient *a, int64_t t) {
  int existing = sr_population_find_by_key(p, a->key);
  if (existing >= 0) return existing;
  SrRng r = sr_rng_stream(c->seed, "promote", a->key, 0);
  static const int homes[] = {SR_D_RESIDENTIAL, SR_D_OLDQUARTER, SR_D_UNDERCITY};
  int block = sr_city_block_at(c, a->x, a->y);
  int work = block >= 0 && c->blocks[block].building_count > 0 ? c->blocks[block].first_building : c->home_building;
  /* the citizen is where we met them, so their shift covers the current hour */
  int hour = (int)((t % 86400) / 3600);
  SrNpc *n = add_npc(p, c, (SrRole)a->role, a->faction, random_building(c, &r, homes, 3, SR_POI_NONE), work,
                     (hour + 23) % 24, 8);
  if (!n) return -1;
  n->look = a->look;
  n->bravery = a->bravery;
  n->promoted = true;
  n->ambient_key = a->key;
  return (int)n->id;
}

/* ---- ambient crowds --------------------------------------------------------------------- */

int sr_crowd_density(SrDistrictType t, int hour) {
  static const int base[SR_D_COUNT] = {45, 30, 65, 80, 40, 45, 20, 15, 15, 35, 8};
  int pct;
  if (t == SR_D_ENTERTAINMENT) {
    if (hour >= 20 || hour <= 4)
      pct = 100;
    else if (hour <= 8)
      pct = 30;
    else if (hour <= 16)
      pct = 15;
    else
      pct = 50;
  } else if (t == SR_D_COMMERCIAL) {
    pct = (hour >= 17 && hour <= 23) ? 100 : 55;
  } else {
    /* rush at shift changes (06, 14, 22) */
    int m = hour % 8;
    pct = (m == 5 || m == 6) ? 100 : 55;
  }
  int v = base[(unsigned)t < SR_D_COUNT ? t : 0] * pct / 100;
  return v > SR_MAX_AMBIENT_PER_BLOCK ? SR_MAX_AMBIENT_PER_BLOCK : v;
}

int sr_ambient_generate(const SrCity *c, int block, int64_t t, SrAmbient *out, int max) {
  if (block < 0 || block >= c->grid * c->grid || c->blocks[block].water) return 0;
  SrDistrictType type = c->districts[c->blocks[block].district].type;
  int hour = (int)((t % 86400) / 3600);
  int n = sr_crowd_density(type, hour);
  if (n > max) n = max;
  int64_t slot = t / 1800;
  int32_t ox, oy;
  sr_city_block_origin(block, c->grid, &ox, &oy);
  /* gang members on turf */
  int gang = -1;
  for (int g = c->factions.gang_first; g < c->factions.gang_first + c->factions.gang_count; g++)
    if (c->factions.f[g].turf & (1u << c->blocks[block].district)) gang = g;
  for (int i = 0; i < n; i++) {
    SrAmbient *a = &out[i];
    a->key = sr_hash_combine(sr_hash_combine(c->seed, (uint64_t)block * 100000u + (uint64_t)i), (uint64_t)slot);
    SrRng r = sr_rng_stream(a->key, "ambient", 0, 0);
    int roll = sr_rng_range(&r, 0, 99);
    SrRole role = SR_ROLE_WORKER;
    a->faction = -1;
    if (gang >= 0 && roll < 15) {
      role = SR_ROLE_GANG;
      a->faction = (int8_t)gang;
    } else if (type == SR_D_SPIRE && roll < 25) {
      role = SR_ROLE_EXEC;
    } else if (roll > 94) {
      role = SR_ROLE_DRIFTER;
    }
    a->role = (uint8_t)role;
    int marker = a->faction >= 0 ? c->factions.f[a->faction].marker : 0;
    a->look = sr_appearance_generate(a->key, role, marker);
    /* uniqueness within the crowd: vary details until the combination is new */
    for (int tries = 0; tries < 64; tries++) {
      bool dup = false;
      uint64_t k = sr_appearance_key(&a->look);
      for (int j = 0; j < i && !dup; j++) dup = sr_appearance_key(&out[j].look) == k;
      if (!dup) break;
      a->look.accessory = (uint8_t)((a->look.accessory + 1) % 8);
      if (tries % 8 == 7) a->look.hair_color = (uint8_t)((a->look.hair_color + 1) % 12);
    }
    a->bravery = (uint8_t)sr_rng_range(&r, 0, 3);
    a->umbrella = sr_rng_chance(&r, 400);
    /* on the sidewalk ring around the block */
    int side = sr_rng_range(&r, 0, 3);
    int32_t along = sr_rng_range(&r, SR_STREET_CM, SR_BLOCK_CM - 100);
    int32_t walk = sr_rng_range(&r, SR_STREET_CM - 400, SR_STREET_CM - 100);
    switch (side) {
      case 0:
        a->x = ox + along;
        a->y = oy + walk;
        break;
      case 1:
        a->x = ox + along;
        a->y = oy + SR_BLOCK_CM - 100;
        break;
      case 2:
        a->x = ox + walk;
        a->y = oy + along;
        break;
      default:
        a->x = ox + SR_BLOCK_CM - 100;
        a->y = oy + along;
        break;
    }
  }
  return n;
}

/* ---- transit ------------------------------------------------------------------------------ */

int64_t sr_metro_next_train(const SrCity *c, int station, int direction, int64_t t) {
  const int64_t interval = 240;
  int64_t offset =
      (int64_t)(sr_hash_combine(c->seed, (uint64_t)station * 2u + (uint64_t)direction) % (uint64_t)interval);
  int64_t k = (t - offset + interval - 1) / interval;
  if (t - offset < 0) k = 0;
  int64_t next = offset + k * interval;
  return next < t ? next + interval : next;
}

/* ---- reactions ------------------------------------------------------------------------------ */

SrResponse sr_react_gunfire(uint8_t bravery, SrDistrictType district, uint64_t key) {
  uint64_t h = sr_mix64(key) % 100;
  if (bravery <= 1) return h < 70 ? SR_RESP_FLEE : SR_RESP_COVER;
  if (bravery == 2) return SR_RESP_COVER;
  /* the brave or the jaded: in rough districts people stop and watch */
  bool rough = district == SR_D_UNDERCITY || district == SR_D_BARRENS || district == SR_D_DOCKS;
  return (rough || h < 50) ? SR_RESP_GAWK : SR_RESP_COVER;
}

bool sr_witness_reports(SrDistrictType district, uint8_t bravery, uint64_t key) {
  if (district == SR_D_UNDERCITY || district == SR_D_BARRENS) return false;
  if (district == SR_D_SPIRE || district == SR_D_CIVIC || district == SR_D_CAMPUS) return true;
  return bravery >= 1 && (sr_mix64(key) % 100) < 70;
}

bool sr_bark(const SrCity *c, const SrEventLog *log, int district, int64_t t, uint64_t key, char *out, size_t cap) {
  for (int i = 0; i < sr_events_size(log); i++) {
    const SrWorldEvent *ev = sr_events_recent(log, i);
    if (t - ev->time > 86400) break;
    if (!ev->significant || ev->district != district) continue;
    snprintf(out, cap, "Heard about %s? Crazy night.", ev->text);
    return true;
  }
  static const char *const generic[] = {"Rain again. Always rain.", "My shift starts in an hour.",
                                        "Rent went up again.", "Don't go into the Sump after midnight.",
                                        "You look like you need a job, choom."};
  snprintf(out, cap, "%s", generic[sr_mix64(key) % SR_ARRAY_LEN(generic)]);
  (void)c;
  return false;
}

void sr_npc_remember(SrNpc *n, int delta) { n->attitude = (int8_t)SR_CLAMP(n->attitude + delta, -100, 100); }

const char *sr_npc_greeting(const SrNpc *n) {
  if (n->attitude <= -30) return "You again. Get lost before I call someone.";
  if (n->attitude < 10) return "What do you want?";
  if (n->attitude < 40) return "Good to see you again.";
  return "My favourite customer! The usual?";
}
