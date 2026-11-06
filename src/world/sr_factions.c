/* Faction roster: megacorps, gangs, government, police and minor groups. */
#include "core/sr_rng.h"
#include "world/sr_world.h"

#include <stdio.h>
#include <string.h>

bool sr_faction_name_free(const SrFactions *fs, const char *name, int self) {
  for (int j = 0; j < fs->count; j++) {
    SrFactionKind k = fs->f[j].kind;
    if (j == self || (k != SR_FK_CORP && k != SR_FK_GANG) || !fs->f[j].name[0]) continue;
    if (sr_name_similar(fs->f[j].name, name)) return false;
  }
  return true;
}

const char *sr_sector_name(SrSector s) {
  static const char *const n[SR_SEC_COUNT] = {
      "Arms & Security",     "Cyberware", "Biotech & Synthfood",   "Media & Entertainment",      "AI & Netware",
      "Finance & Insurance", "Energy",    "Logistics & Transport", "Construction & Real Estate", "Orbital & Aerospace"};
  return (unsigned)s < SR_SEC_COUNT ? n[s] : "?";
}

const char *sr_doctrine_name(SrDoctrine d) {
  static const char *const n[SR_DOC_COUNT] = {"Shark", "Fortress", "Spider", "Prophet", "Vulture"};
  return (unsigned)d < SR_DOC_COUNT ? n[d] : "?";
}

static int district_type_of_building(const SrCity *c, int b) {
  return c->districts[c->blocks[c->buildings[b].block].district].type;
}

static bool free_building(const SrCity *c, int b) {
  return c->buildings[b].poi == SR_POI_NONE && c->buildings[b].owner < 0 && (int)b != c->home_building;
}

static int add_faction(SrFactions *fs, SrFactionKind kind) {
  int i = fs->count++;
  memset(&fs->f[i], 0, sizeof fs->f[i]);
  fs->f[i].kind = kind;
  fs->f[i].owner = -1;
  fs->f[i].hq_building = -1;
  return i;
}

static void claim(SrCity *c, int b, int faction, SrPoi poi) {
  c->buildings[b].owner = (int8_t)faction;
  c->buildings[b].poi = (uint8_t)poi;
  c->buildings[b].flags |= SR_BF_ENTERABLE | SR_BF_NETWORKED | SR_BF_MISSION;
}

static int random_free_in(SrCity *c, SrRng *r, int district_type, uint16_t district_mask) {
  for (int tries = 0; tries < 20000; tries++) {
    int b = sr_rng_range(r, 0, c->building_count - 1);
    if (!free_building(c, b)) continue;
    int d = c->blocks[c->buildings[b].block].district;
    if (district_type >= 0 && district_type_of_building(c, b) != district_type) continue;
    if (district_mask && !(district_mask & (1u << d))) continue;
    return b;
  }
  return -1;
}

void sr_factions_generate(SrCity *c, uint64_t seed) {
  SrFactions *fs = &c->factions;
  memset(fs, 0, sizeof *fs);
  SrRng r = sr_rng_stream(seed, "factions", 0, 0);

  /* ---- megacorps ---- */
  int n = c->opt.corp_count >= 5 && c->opt.corp_count <= 10 ? c->opt.corp_count : sr_rng_range(&r, 5, 10);
  fs->corp_count = n;
  int hues[12];
  int hue_start = sr_rng_range(&r, 0, 1);
  for (int i = 0; i < 12; i++) hues[i] = hue_start + 2 * i;
  for (int i = 11; i > 0; i--) {
    int j = sr_rng_range(&r, 0, i);
    int t = hues[i];
    hues[i] = hues[j];
    hues[j] = t;
  }
  int sectors[SR_SEC_COUNT];
  for (int i = 0; i < SR_SEC_COUNT; i++) sectors[i] = i;
  for (int i = SR_SEC_COUNT - 1; i > 2; i--) { /* keep Arms and Cyber in the first slots */
    int j = sr_rng_range(&r, 2, i);
    int t = sectors[i];
    sectors[i] = sectors[j];
    sectors[j] = t;
  }
  /* Spire buildings ordered by height for headquarters. */
  int spire[SR_MAX_BUILDINGS / 4];
  int ns = 0;
  for (int b = 0; b < c->building_count && ns < (int)SR_ARRAY_LEN(spire); b++)
    if (district_type_of_building(c, b) == SR_D_SPIRE && free_building(c, b)) spire[ns++] = b;
  for (int i = 1; i < ns; i++) {
    int v = spire[i], j = i - 1;
    while (j >= 0 && c->buildings[spire[j]].height_cm < c->buildings[v].height_cm) {
      spire[j + 1] = spire[j];
      j--;
    }
    spire[j + 1] = v;
  }
  for (int k = 0; k < n; k++) {
    int f = add_faction(fs, SR_FK_CORP);
    SrFaction *fa = &fs->f[f];
    for (uint64_t salt = 0;; salt++) {
      sr_name_corp(seed, (uint64_t)k * 100u + salt, fa->name, sizeof fa->name);
      sr_name_blackops(seed, (uint64_t)k * 100u + salt, fa->blackops, sizeof fa->blackops);
      bool dup = !sr_faction_name_free(fs, fa->name, f);
      for (int j = 0; j < k; j++) dup |= strcmp(fs->f[j].blackops, fa->blackops) == 0;
      if (!dup || salt > 400) break;
    }
    fa->sector1 = (uint8_t)sectors[k];
    fa->sector2 = (uint8_t)sectors[(k + 1 + sr_rng_range(&r, 0, 3)) % SR_SEC_COUNT];
    if (fa->sector2 == fa->sector1) fa->sector2 = (uint8_t)((fa->sector1 + 1) % SR_SEC_COUNT);
    fa->doctrine = (uint8_t)sr_rng_range(&r, 0, SR_DOC_COUNT - 1);
    fa->hue = (uint8_t)hues[k];
    /* logos are unique by construction: the glyph is the corp index */
    fa->logo_shape = (uint8_t)sr_rng_range(&r, 0, 5);
    fa->logo_glyph = (uint8_t)k;
    fa->logo_frame = (uint8_t)sr_rng_range(&r, 0, 3);
    if (k < ns) {
      int hq = spire[k];
      claim(c, hq, f, SR_POI_CORP_HQ);
      c->buildings[hq].flags |= SR_BF_LANDMARK | SR_BF_ELEVATOR;
      /* silhouette: tiers / taper / crown / width; index k guarantees uniqueness */
      c->buildings[hq].silhouette[0] = (uint8_t)(1 + k % 4);
      c->buildings[hq].silhouette[1] = (uint8_t)((k / 4) % 3);
      c->buildings[hq].silhouette[2] = (uint8_t)sr_rng_range(&r, 0, 5);
      c->buildings[hq].silhouette[3] = (uint8_t)sr_rng_range(&r, 0, 2);
      snprintf(c->buildings[hq].name, sizeof c->buildings[hq].name, "%s Tower", fa->name);
      fa->hq_building = hq;
    }
  }
  /* facilities: at least three, spread over two districts */
  for (int k = 0; k < n; k++) {
    SrFaction *fa = &fs->f[k];
    int want = sr_rng_range(&r, 3, 5);
    int first_district = -1;
    for (int tries = 0; fa->facility_count < want && tries < 200; tries++) {
      static const int types[] = {SR_D_INDUSTRIAL, SR_D_COMMERCIAL,    SR_D_DOCKS,      SR_D_CAMPUS,
                                  SR_D_CIVIC,      SR_D_ENTERTAINMENT, SR_D_RESIDENTIAL};
      int b = random_free_in(c, &r, types[sr_rng_range(&r, 0, 6)], 0);
      if (b < 0) continue;
      int d = c->blocks[c->buildings[b].block].district;
      if (fa->facility_count == want - 1 && d == first_district) continue;
      if (first_district < 0) first_district = d;
      claim(c, b, k, SR_POI_CORP_FACILITY);
      snprintf(c->buildings[b].name, sizeof c->buildings[b].name, "%s Site %d", fa->name, fa->facility_count + 1);
      fa->facilities[fa->facility_count++] = (int16_t)b;
    }
  }

  /* ---- gangs ---- */
  int ng = sr_rng_range(&r, 3, 6);
  fs->gang_first = fs->count;
  fs->gang_count = ng;
  uint16_t eligible = 0;
  for (int d = 0; d < c->district_count; d++) {
    SrDistrictType t = c->districts[d].type;
    if (t != SR_D_SPIRE && t != SR_D_CAMPUS) eligible |= (uint16_t)(1u << d);
  }
  for (int g = 0; g < ng; g++) {
    int f = add_faction(fs, SR_FK_GANG);
    SrFaction *fa = &fs->f[f];
    for (uint64_t salt = 0;; salt++) {
      sr_name_gang(seed, (uint64_t)g * 100u + salt, fa->name, sizeof fa->name);
      if (sr_faction_name_free(fs, fa->name, f) || salt > 400) break;
    }
    fa->theme = (uint8_t)sr_rng_range(&r, 0, 5);
    fa->hue = (uint8_t)sr_rng_range(&r, 0, 23);
    /* turf: prefer Undercity, Residential, Industrial, Docks, Strip, Barrens, Old Quarter */
    for (int tries = 0; !fa->turf && tries < 500; tries++) {
      int d = sr_rng_range(&r, 0, c->district_count - 1);
      SrDistrictType t = c->districts[d].type;
      bool pref = t == SR_D_UNDERCITY || t == SR_D_RESIDENTIAL || t == SR_D_INDUSTRIAL || t == SR_D_DOCKS ||
                  t == SR_D_ENTERTAINMENT || t == SR_D_BARRENS || t == SR_D_OLDQUARTER;
      if (pref && (eligible & (1u << d))) fa->turf = (uint16_t)(1u << d);
    }
    if (sr_rng_chance(&r, 400)) {
      int d = sr_rng_range(&r, 0, c->district_count - 1);
      if (eligible & (1u << d)) fa->turf |= (uint16_t)(1u << d);
    }
    int hide = random_free_in(c, &r, -1, fa->turf);
    if (hide >= 0) {
      claim(c, hide, f, SR_POI_GANG_HIDEOUT);
      snprintf(c->buildings[hide].name, sizeof c->buildings[hide].name, "%s Hideout", fa->name);
      fa->hq_building = hide;
    }
  }

  /* ---- government, police, minor groups ---- */
  fs->gov = add_faction(fs, SR_FK_GOV);
  snprintf(fs->f[fs->gov].name, SR_NAME_LEN, "City of %s", c->name);
  fs->police = add_faction(fs, SR_FK_POLICE);
  snprintf(fs->f[fs->police].name, SR_NAME_LEN, "%s Police Department", c->name);
  fs->collective = add_faction(fs, SR_FK_COLLECTIVE);
  snprintf(fs->f[fs->collective].name, SR_NAME_LEN, "The Lattice");
  fs->union_ = add_faction(fs, SR_FK_UNION);
  snprintf(fs->f[fs->union_].name, SR_NAME_LEN, "United Dockworkers");
  fs->church = add_faction(fs, SR_FK_CHURCH);
  snprintf(fs->f[fs->church].name, SR_NAME_LEN, "Church of the Deep Signal");
  fs->media_first = fs->count;
  fs->media_count = 2;
  for (int m = 0; m < 2; m++) {
    int f = add_faction(fs, SR_FK_MEDIA);
    fs->f[f].owner = (int8_t)(m == 0 ? sr_rng_range(&r, 0, n - 1) : -1);
    snprintf(fs->f[f].name, SR_NAME_LEN, m == 0 ? "Channel %d Pulse" : "Free Signal %d", sr_rng_range(&r, 2, 99));
    int b = random_free_in(c, &r, -1, 0);
    if (b >= 0) {
      claim(c, b, f, SR_POI_MEDIA);
      fs->f[f].hq_building = b;
    }
  }
  sr_name_person(seed, 9000, fs->mayor, sizeof fs->mayor);
  fs->mayor_patron = (int8_t)sr_rng_range(&r, 0, n - 1);
  fs->council_count = 5 + 2 * sr_rng_range(&r, 0, 2);
  for (int i = 0; i < fs->council_count; i++) {
    sr_name_person(seed, 9100u + (uint64_t)i, fs->council[i].name, SR_NAME_LEN);
    fs->council[i].lean = (int8_t)sr_rng_range(&r, -2, 2);
    fs->council[i].patron = (int8_t)(sr_rng_chance(&r, 600) ? sr_rng_range(&r, 0, n - 1) : -1);
  }
  /* police precincts: existing police places, plus one per three districts */
  fs->precinct_count = 0;
  for (int b = 0; b < c->building_count && fs->precinct_count < SR_MAX_DISTRICTS; b++)
    if (c->buildings[b].poi == SR_POI_POLICE) fs->precinct_building[fs->precinct_count++] = b;
  int want_prec = (c->district_count + 2) / 3;
  while (fs->precinct_count < want_prec) {
    int b = random_free_in(c, &r, -1, 0);
    if (b < 0) break; /* GCOVR_EXCL_LINE reason: cities always have free buildings */
    c->buildings[b].poi = SR_POI_POLICE;
    c->buildings[b].flags |= SR_BF_ENTERABLE | SR_BF_NETWORKED | SR_BF_MISSION;
    fs->precinct_building[fs->precinct_count++] = b;
  }
  for (int d = 0; d < c->district_count; d++) {
    int best = 0;
    int64_t bd = INT64_MAX;
    for (int p = 0; p < fs->precinct_count; p++) {
      int32_t x, y;
      sr_building_center(&c->buildings[fs->precinct_building[p]], &x, &y);
      int64_t dd = (int64_t)(x - c->districts[d].cx) * (x - c->districts[d].cx) +
                   (int64_t)(y - c->districts[d].cy) * (y - c->districts[d].cy);
      if (dd < bd) {
        bd = dd;
        best = p;
      }
    }
    fs->precinct_of_district[d] = (int8_t)best;
  }
  /* one fixer per district */
  for (int d = 0; d < c->district_count; d++) {
    fs->fixer_building[d] = -1;
    for (int b = 0; b < c->building_count; b++) {
      if (c->buildings[b].poi == SR_POI_FIXER && c->blocks[c->buildings[b].block].district == d) {
        fs->fixer_building[d] = b;
        break;
      }
    }
    if (fs->fixer_building[d] < 0) {
      int b = random_free_in(c, &r, -1, (uint16_t)(1u << d));
      if (b >= 0) {
        c->buildings[b].poi = SR_POI_FIXER;
        c->buildings[b].flags |= SR_BF_ENTERABLE | SR_BF_NETWORKED;
        sr_name_business(seed, (uint64_t)b, SR_POI_FIXER, c->buildings[b].name, SR_NAME_LEN);
        fs->fixer_building[d] = b;
      }
    }
  }

  /* ---- icons, markers, relationships ---- */
  int icons[40];
  for (int i = 0; i < 40; i++) icons[i] = i;
  for (int i = 39; i > 0; i--) {
    int j = sr_rng_range(&r, 0, i);
    int t = icons[i];
    icons[i] = icons[j];
    icons[j] = t;
  }
  for (int f = 0; f < fs->count; f++) {
    fs->f[f].icon = (uint8_t)icons[f];
    fs->f[f].marker = (uint8_t)(f + 1); /* marker 0 = none (civilian) */
  }
  for (int a = 0; a < fs->count; a++) {
    for (int b = 0; b < fs->count; b++) { fs->rel[a][b] = (int8_t)(a == b ? 100 : sr_rng_range(&r, -40, 40)); }
  }
  for (int k = 0; k < n; k++) {
    int rival = (k + 1 + sr_rng_range(&r, 0, n - 2)) % n;
    fs->rel[k][rival] = fs->rel[rival][k] = (int8_t)sr_rng_range(&r, -90, -60);
  }
}

int sr_factions_validate(const SrCity *c) {
  const SrFactions *fs = &c->factions;
  if (fs->corp_count < 5 || fs->corp_count > 10) return 1;
  bool arms = false, cyber = false;
  for (int k = 0; k < fs->corp_count; k++) {
    const SrFaction *fa = &fs->f[k];
    if (fa->hq_building < 0) return 2;
    if (district_type_of_building(c, fa->hq_building) != SR_D_SPIRE) return 2;
    if (fa->facility_count < 3) return 3;
    int d0 = c->blocks[c->buildings[fa->facilities[0]].block].district;
    bool spread = false;
    for (int i = 1; i < fa->facility_count; i++)
      spread |= c->blocks[c->buildings[fa->facilities[i]].block].district != d0;
    if (!spread) return 3;
    arms |= fa->sector1 == SR_SEC_ARMS || fa->sector2 == SR_SEC_ARMS;
    cyber |= fa->sector1 == SR_SEC_CYBER || fa->sector2 == SR_SEC_CYBER;
    bool rival = false;
    for (int j = 0; j < fs->corp_count; j++) {
      if (j == k) continue;
      if (strcmp(fs->f[j].name, fa->name) == 0 || strcmp(fs->f[j].blackops, fa->blackops) == 0) return 4;
      int dh = fs->f[j].hue > fa->hue ? fs->f[j].hue - fa->hue : fa->hue - fs->f[j].hue;
      if (dh > 12) dh = 24 - dh;
      if (dh < 2) return 4;
      rival |= fs->rel[k][j] <= -60;
    }
    if (!rival) return 5;
  }
  if (!arms || !cyber) return 6;
  if (fs->gang_count < 3 || fs->gang_count > 6) return 7;
  for (int g = fs->gang_first; g < fs->gang_first + fs->gang_count; g++) {
    const SrFaction *fa = &fs->f[g];
    if (!fa->turf || fa->hq_building < 0) return 8;
    for (int d = 0; d < c->district_count; d++) {
      if (!(fa->turf & (1u << d))) continue;
      SrDistrictType t = c->districts[d].type;
      if (t == SR_D_SPIRE || t == SR_D_CAMPUS) return 8;
    }
    int hd = c->blocks[c->buildings[fa->hq_building].block].district;
    if (!(fa->turf & (1u << hd))) return 8;
  }
  if (fs->council_count != 5 && fs->council_count != 7 && fs->council_count != 9) return 9;
  for (int d = 0; d < c->district_count; d++) {
    if (fs->precinct_of_district[d] < 0 || fs->precinct_of_district[d] >= fs->precinct_count) return 10;
    if (fs->fixer_building[d] < 0) return 11;
  }
  if (fs->media_count < 2) return 12;
  bool owned = false;
  for (int m = fs->media_first; m < fs->media_first + fs->media_count; m++) owned |= fs->f[m].owner >= 0;
  if (!owned) return 12;
  for (int a = 0; a < fs->count; a++)
    for (int b = 0; b < a; b++)
      if (fs->f[a].icon == fs->f[b].icon || fs->f[a].marker == fs->f[b].marker) return 13;
  return 0;
}
