/* Name generators. Several sound palettes keep the city diverse without leaning on
 * real-world stereotypes; every result is checked against a blocklist of real brands,
 * companies and public figures and re-rolled on a hit. */
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "world/sr_world.h"

#include <stdio.h>
#include <string.h>

#define PICK(rng, arr) (arr)[sr_rng_range((rng), 0, (int32_t)SR_ARRAY_LEN(arr) - 1)]

/* Normalised (lowercase alphanumeric) names that must never be generated. */
static const char *const k_blocklist[] = {"apple",        "google",
                                          "alphabet",     "microsoft",
                                          "amazon",       "meta",
                                          "facebook",     "sony",
                                          "samsung",      "nike",
                                          "tesla",        "spacex",
                                          "nvidia",       "intel",
                                          "ibm",          "oracle",
                                          "disney",       "toyota",
                                          "honda",        "nintendo",
                                          "valve",        "netflix",
                                          "boeing",       "lockheed",
                                          "raytheon",     "palantir",
                                          "blackrock",    "goldman",
                                          "jpmorgan",     "pfizer",
                                          "bayer",        "siemens",
                                          "philips",      "arasaka",
                                          "militech",     "biotechnica",
                                          "tyrell",       "weyland",
                                          "yutani",       "weylandyutani",
                                          "umbrella",     "aperture",
                                          "cyberdyne",    "omnicorp",
                                          "ocp",          "rosen",
                                          "sarif",        "versalife",
                                          "renraku",      "saederkrupp",
                                          "aztechnology", "mitsuhama",
                                          "shiawase",     "zetatech",
                                          "kangtao",      "helion",
                                          "lumen",        "meridianbioscience",
                                          "tesseract",    "obsidian",
                                          "ninthcircuit", "elonmusk",
                                          "musk",         "bezos",
                                          "zuckerberg",   "gates",
                                          "jobs",         "trump",
                                          "biden",        "putin"};

bool sr_name_blocked(const char *name) {
  char norm[64];
  sr_str_normalize(name, norm, sizeof norm);
  for (size_t i = 0; i < SR_ARRAY_LEN(k_blocklist); i++) {
    if (strcmp(norm, k_blocklist[i]) == 0) return true;
    /* also block names that contain a blocked word of 5+ letters */
    if (strlen(k_blocklist[i]) >= 5 && strstr(norm, k_blocklist[i])) return true;
  }
  return false;
}

/* ---- palettes ------------------------------------------------------------ */

static const char *const k_given[] = {"Ada",   "Bren",    "Cass",    "Dario",  "Eun",     "Farid",  "Greta", "Hiro",
                                      "Ilse",  "Joaquin", "Kaya",    "Lior",   "Mara",    "Nnamdi", "Odile", "Pavel",
                                      "Quinn", "Rafa",    "Sunniva", "Tomas",  "Uma",     "Vesna",  "Wren",  "Xiomara",
                                      "Yusuf", "Zora",    "Ines",    "Mateo",  "Noor",    "Oskar",  "Priya", "Ren",
                                      "Sol",   "Teo",     "Anouk",   "Bashir", "Celeste", "Dmitri", "Emeka", "Freya"};
static const char *const k_family[] = {
    "Achterberg", "Bastos",    "Calloway",   "Delacroix", "Eze",      "Falk",     "Gutierrez", "Haddad",
    "Iwasaki",    "Jankowski", "Kessler",    "Lindqvist", "Mbeki",    "Navarro",  "Okonkwo",   "Petrov",
    "Quintero",   "Rahman",    "Santangelo", "Takeda",    "Ulloa",    "Varga",    "Whitlock",  "Xu",
    "Yilmaz",     "Zamora",    "Brandt",     "Castellan", "Dumont",   "Ferreira", "Halloway",  "Kurogane",
    "Moreau",     "Nakamura",  "Ostrander",  "Reyes",     "Sorensen", "Vance",    "Winter",    "Adeyemi"};
static const char *const k_corp_a[] = {"Aurelian", "Verdance", "Hexis",   "Corvid", "Umbra",   "Solace",
                                       "Kestrel",  "Halcyon",  "Obelisk", "Vantor", "Myriad",  "Paragon",
                                       "Sable",    "Zenith",   "Argent",  "Cinder", "Lattice", "Nadir",
                                       "Orison",   "Quill",    "Rubicon", "Talon",  "Vigil",   "Ashgrove"};
static const char *const k_corp_b[] = {"Dynamics",    "Biosystems",   "Netware",     "Mutual",   "Atmospherics",
                                       "Media Group", "Arms",         "Industries",  "Holdings", "Systems",
                                       "Logistics",   "Orbital",      "Cybernetics", "Capital",  "Securities",
                                       "Labs",        "Consolidated", "Works",       "Pharma",   "Grid"};
static const char *const k_gang_a[] = {"Static", "Rust",   "Velvet", "Hollow", "Neon",   "Iron",  "Gutter", "Chrome",
                                       "Black",  "Silent", "Red",    "Ghost",  "Broken", "Paper", "Glass",  "Ash"};
static const char *const k_gang_b[] = {"Saints", "Dogs",  "Knives",   "Choir", "Wolves", "Kings",  "Angels", "Rats",
                                       "Monks",  "Hands", "Lanterns", "Teeth", "Crows",  "Sirens", "Vipers", "Owls"};
static const char *const k_place_a[] = {"Hollis",     "Kessling", "Vesper",  "Orrin",   "Marrow",  "Calder",  "Tamsin",
                                        "Bellwether", "Corran",   "Dunmore", "Elsby",   "Faraday", "Gallow",  "Harrow",
                                        "Ivel",       "Juniper",  "Kilroy",  "Lanyard", "Mercer",  "Northam", "Osk",
                                        "Pell",       "Quayle",   "Rook",    "Sable",   "Tern"};
static const char *const k_city_prefix[] = {"Port", "New", "Saint", "Fort", "East", "Lower"};
static const char *const k_blackops[] = {"Grey Ledger",  "Division Nine",      "The Gardeners",    "The Quiet Room",
                                         "Null Office",  "Actuarial Services", "Eclipse Group",    "Black Meridian",
                                         "Section Zero", "The Archive",        "Project Nocturne", "Cold Harbour",
                                         "The Tailors",  "Glass Office",       "Night Desk",       "Special Accounts",
                                         "Long Room",    "The Orchard",        "Silent Partners",  "Unit Lantern"};

static const char *district_suffix(SrDistrictType t, SrRng *r) {
  static const char *const spire[] = {"Spire", "Heights", "Crown"};
  static const char *const civic[] = {"Civic Center", "Forum", "Capitol"};
  static const char *const comm[] = {"Market", "Bazaar", "Exchange"};
  static const char *const ent[] = {"Strip", "Neon Mile", "Lights"};
  static const char *const res[] = {"Stacks", "Blocks", "Commons", "Towers"};
  static const char *const old[] = {"Old Quarter", "Old Town", "Lanes"};
  static const char *const ind[] = {"Works", "Foundry", "Yards"};
  static const char *const docks[] = {"Docks", "Wharf", "Port"};
  static const char *const campus[] = {"Campus", "Park", "Institute"};
  static const char *const under[] = {"Sump", "Underway", "Depths"};
  static const char *const barrens[] = {"Barrens", "Edge", "Scrap"};
  switch (t) {
    case SR_D_SPIRE: return PICK(r, spire);
    case SR_D_CIVIC: return PICK(r, civic);
    case SR_D_COMMERCIAL: return PICK(r, comm);
    case SR_D_ENTERTAINMENT: return PICK(r, ent);
    case SR_D_RESIDENTIAL: return PICK(r, res);
    case SR_D_OLDQUARTER: return PICK(r, old);
    case SR_D_INDUSTRIAL: return PICK(r, ind);
    case SR_D_DOCKS: return PICK(r, docks);
    case SR_D_CAMPUS: return PICK(r, campus);
    case SR_D_UNDERCITY: return PICK(r, under);
    case SR_D_BARRENS:
    case SR_D_COUNT: break;
  }
  return PICK(r, barrens);
}

/* Each generator retries with a new attempt index until the name passes the blocklist. */
#define GEN_LOOP(tag, body)                                                                                            \
  for (uint64_t attempt = 0;; attempt++) {                                                                             \
    SrRng r = sr_rng_stream(seed, tag, id, attempt);                                                                   \
    body;                                                                                                              \
    if (!sr_name_blocked(out) || attempt > 32) return;                                                                 \
  }

int sr_name_frame(const char *full) {
  /* the given names the generator uses, as read by most people: 1 female, 0 male, -1 either */
  static const char *const female[] = {"Ada",     "Greta", "Ilse", "Kaya", "Mara",  "Odile", "Sunniva", "Uma",  "Vesna",
                                       "Xiomara", "Zora",  "Ines", "Noor", "Priya", "Anouk", "Celeste", "Freya"};
  static const char *const male[] = {"Dario", "Farid", "Hiro",  "Joaquin", "Nnamdi", "Pavel", "Rafa", "Tomas",
                                     "Yusuf", "Mateo", "Oskar", "Bashir",  "Dmitri", "Emeka", "Teo"};
  if (!full) return -1;
  size_t n = strcspn(full, " ");
  for (size_t i = 0; i < SR_ARRAY_LEN(female); i++)
    if (strlen(female[i]) == n && strncmp(full, female[i], n) == 0) return 1;
  for (size_t i = 0; i < SR_ARRAY_LEN(male); i++)
    if (strlen(male[i]) == n && strncmp(full, male[i], n) == 0) return 0;
  return -1;
}

void sr_name_person(uint64_t seed, uint64_t id, char *out, size_t cap) {
  GEN_LOOP("name.person", {
    const char *a = PICK(&r, k_given);
    const char *b = PICK(&r, k_family);
    snprintf(out, cap, "%s %s", a, b);
  })
}

void sr_name_corp(uint64_t seed, uint64_t id, char *out, size_t cap) {
  GEN_LOOP("name.corp", {
    int form = sr_rng_range(&r, 0, 3);
    if (form == 0) {
      const char *a = PICK(&r, k_family);
      const char *b = PICK(&r, k_family);
      snprintf(out, cap, "%s-%s", a, b);
    } else {
      const char *a = PICK(&r, k_corp_a);
      const char *b = PICK(&r, k_corp_b);
      snprintf(out, cap, "%s %s", a, b);
    }
  })
}

void sr_name_gang(uint64_t seed, uint64_t id, char *out, size_t cap) {
  GEN_LOOP("name.gang", {
    const char *a = PICK(&r, k_gang_a);
    const char *b = PICK(&r, k_gang_b);
    snprintf(out, cap, "%s %s", a, b);
  })
}

void sr_name_district(uint64_t seed, uint64_t id, SrDistrictType t, char *out, size_t cap) {
  GEN_LOOP("name.district", {
    const char *a = PICK(&r, k_place_a);
    const char *b = district_suffix(t, &r);
    snprintf(out, cap, "%s %s", a, b);
  })
}

void sr_name_city(uint64_t seed, char *out, size_t cap) {
  uint64_t id = 0;
  GEN_LOOP("name.city", {
    const char *a = PICK(&r, k_city_prefix);
    const char *b = PICK(&r, k_place_a);
    snprintf(out, cap, "%s %s", a, b);
  })
}

/* Splits a name into lower-case words (spaces and hyphens separate them). */
static int words_of(const char *name, char w[6][24]) {
  int n = 0, len = 0;
  for (const char *p = name;; p++) {
    bool sep = *p == ' ' || *p == '-' || *p == '\0';
    if (!sep && len < 23) {
      char ch = *p;
      w[n][len++] = (char)(ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch);
    }
    if (sep && len > 0) {
      w[n][len] = '\0';
      len = 0;
      if (++n == 6) break;
    }
    if (*p == '\0') break;
  }
  return n;
}

static bool numeric(const char *w) { return w[0] >= '0' && w[0] <= '9'; }

bool sr_name_similar(const char *a, const char *b) {
  char wa[6][24], wb[6][24];
  int na = words_of(a, wa), nb = words_of(b, wb);
  for (int i = 0; i < na; i++)
    for (int j = 0; j < nb; j++) {
      if (numeric(wa[i]) || numeric(wb[j])) continue;
      if (strcmp(wa[i], wb[j]) == 0) return true;
      /* Umbra / Umbral, Castellan / Castellane: the same first five letters read alike */
      if (strlen(wa[i]) >= 5 && strlen(wb[j]) >= 5 && strncmp(wa[i], wb[j], 5) == 0) return true;
    }
  return false;
}

void sr_name_blackops(uint64_t seed, uint64_t id, char *out, size_t cap) {
  GEN_LOOP("name.blackops", snprintf(out, cap, "%s", PICK(&r, k_blackops)))
}

void sr_name_business(uint64_t seed, uint64_t id, SrPoi poi, char *out, size_t cap) {
  static const char *const adj[] = {"Lucky",  "Golden", "Neon",  "Midnight", "Red",  "Blue",
                                    "Silver", "Seven",  "Happy", "Electric", "Rain", "Jade"};
  static const char *const food[] = {"Noodle", "Dumplings", "Grill", "Kitchen", "Ramen"};
  GEN_LOOP("name.business", {
    const char *fam = PICK(&r, k_family);
    switch (poi) {
      case SR_POI_FOOD: {
        const char *a = PICK(&r, adj);
        const char *b = PICK(&r, food);
        snprintf(out, cap, "%s %s", a, b);
        break;
      }
      case SR_POI_BAR: snprintf(out, cap, "The %s Lantern", PICK(&r, adj)); break;
      case SR_POI_CLINIC: snprintf(out, cap, "%s Clinic", fam); break;
      case SR_POI_RIPPERDOC: snprintf(out, cap, "Chrome & Bone %s", fam); break;
      case SR_POI_GUNSHOP: snprintf(out, cap, "%s's Guns & Gifts", fam); break;
      case SR_POI_ELECTRONICS: snprintf(out, cap, "%s Circuits", PICK(&r, adj)); break;
      case SR_POI_CLOTHING: snprintf(out, cap, "%s Threads", PICK(&r, adj)); break;
      case SR_POI_FENCE: snprintf(out, cap, "%s Pawn", fam); break;
      case SR_POI_CAPSULE: snprintf(out, cap, "%s Capsules", PICK(&r, adj)); break;
      case SR_POI_MATRIX_BOOTH: snprintf(out, cap, "Jack-In %d", sr_rng_range(&r, 10, 99)); break;
      case SR_POI_GROCERY: snprintf(out, cap, "%s Mart", PICK(&r, adj)); break;
      case SR_POI_UV_BOOTH: snprintf(out, cap, "SunStop %d", sr_rng_range(&r, 1, 9)); break;
      default: snprintf(out, cap, "%s Building", fam); break;
    }
  })
}
