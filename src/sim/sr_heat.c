#include "sim/sr_heat.h"

#include <string.h>

#define DECAY_DELAY (3600)
#define DECAY_STEP (8 * 3600)

void sr_heat_init(SrHeat *h) { memset(h, 0, sizeof *h); }

int sr_heat_jurisdiction(const SrCity *c, int building) {
  if (building >= 0 && building < c->building_count) {
    int owner = c->buildings[building].owner;
    if (owner >= 0 && owner < c->factions.corp_count) return owner;
  }
  return c->factions.police;
}

static void add_evidence(SrHeat *h, SrEvidenceType type, const SrCrime *cr) {
  SrEvidence *e = &h->evidence[h->evidence_count % SR_MAX_EVIDENCE];
  e->type = (uint8_t)type;
  e->jurisdiction = cr->jurisdiction;
  e->points_to = -2;
  e->severity = cr->severity;
  e->time = cr->time;
  e->look = cr->look;
  if (h->evidence_count < SR_MAX_EVIDENCE) h->evidence_count++;
}

int sr_heat_crime(SrHeat *h, const SrCrime *cr) {
  int j = cr->jurisdiction;
  if (j < 0 || j >= SR_MAX_FACTIONS) return 0;
  if (!cr->witnessed && !cr->camera && !cr->evidence_left) return 0;
  if (cr->witnessed) add_evidence(h, SR_EVID_WITNESS, cr);
  if (cr->camera) add_evidence(h, SR_EVID_CAMERA, cr);
  if (cr->evidence_left) add_evidence(h, SR_EVID_BODY, cr);
  int add = cr->severity >= 4 ? 2 : 1;
  int before = h->level[j];
  h->level[j] = (int8_t)SR_MIN(5, h->level[j] + add);
  h->last_seen[j] = cr->time;
  h->decay_from[j] = cr->time;
  if (cr->witnessed || cr->camera) h->known_look[j] = cr->look;
  return h->level[j] - before;
}

int sr_heat_response_time(int level, SrDistrictType district) {
  static const int base[SR_D_COUNT] = {30, 45, 90, 120, 150, 140, 160, 200, 40, 400, 600};
  int b = base[(unsigned)district < SR_D_COUNT ? district : SR_D_RESIDENTIAL];
  if (level <= 0) return -1;
  return b * 6 / (5 + level);
}

void sr_heat_seen(SrHeat *h, int j, int64_t t) {
  if (j < 0 || j >= SR_MAX_FACTIONS || h->level[j] == 0) return;
  h->last_seen[j] = t;
  h->decay_from[j] = t;
}

void sr_heat_update(SrHeat *h, int64_t t, bool at_home) {
  int64_t step = at_home ? DECAY_STEP / 2 : DECAY_STEP;
  for (int j = 0; j < SR_MAX_FACTIONS; j++) {
    if (h->level[j] == 0) continue;
    if (t - h->last_seen[j] < DECAY_DELAY) continue;
    while (h->level[j] > 0 && t - h->decay_from[j] >= DECAY_DELAY + step) {
      h->level[j]--;
      h->decay_from[j] += step;
    }
    if (h->level[j] == 0) h->known_look[j] = 0;
  }
}

bool sr_heat_recognises(const SrHeat *h, int j, uint64_t look) {
  return j >= 0 && j < SR_MAX_FACTIONS && h->level[j] > 0 && h->known_look[j] == look;
}

int32_t sr_heat_bribe_cost(const SrHeat *h, int j) {
  return j >= 0 && j < SR_MAX_FACTIONS ? 400 * h->level[j] : 0;
}

bool sr_heat_bribe(SrHeat *h, int j, int police, int64_t *cash) {
  if (j != police || h->level[j] == 0) return false;
  int32_t cost = sr_heat_bribe_cost(h, j);
  if (*cash < cost) return false;
  *cash -= cost;
  h->level[j]--;
  return true;
}

SrArrest sr_heat_arrest_terms(const SrHeat *h, int j, int police) {
  SrArrest a;
  memset(&a, 0, sizeof a);
  if (j != police || h->level[j] < 1 || h->level[j] > 2) return a;
  a.allowed = true;
  a.fine = 250 * h->level[j];
  a.jail_seconds = (int64_t)h->level[j] * 12 * 3600;
  return a;
}

void sr_heat_surrender(SrHeat *h, int j) {
  if (j < 0 || j >= SR_MAX_FACTIONS) return;
  h->level[j] = 0;
  h->known_look[j] = 0;
}

void sr_heat_erase_records(SrHeat *h, int j) {
  int w = 0;
  for (int i = 0; i < h->evidence_count; i++)
    if (h->evidence[i].jurisdiction != j) h->evidence[w++] = h->evidence[i];
  h->evidence_count = w;
  if (j >= 0 && j < SR_MAX_FACTIONS) {
    h->level[j] = (int8_t)(h->level[j] > 1 ? h->level[j] - 2 : 0);
    if (h->level[j] < 0) h->level[j] = 0; /* GCOVR_EXCL_LINE reason: defensive */
    h->known_look[j] = 0;
  }
}

int sr_heat_attribution(const SrEvidence *ev, int n) {
  int player = 0, best_faction = -1, best_count = 0;
  int counts[SR_MAX_FACTIONS] = {0};
  for (int i = 0; i < n; i++) {
    if (ev[i].points_to == -2) player++;
    else if (ev[i].points_to >= 0 && ev[i].points_to < SR_MAX_FACTIONS) {
      int c = ++counts[ev[i].points_to];
      if (c > best_count) {
        best_count = c;
        best_faction = ev[i].points_to;
      }
    }
  }
  if (player > 0 && player >= best_count) return -2;
  return best_faction;
}

bool sr_disguise_authorised(const SrDisguise *d, int site_faction, SrAreaType area) {
  if (area == SR_AREA_PUBLIC) return true;
  return !d->blown && d->faction == site_faction && d->clearance >= area;
}

void sr_disguise_action(SrDisguise *d, bool suspicious) {
  if (suspicious) d->blown = true;
}

bool sr_is_trespassing(const SrDisguise *d, int site_faction, SrAreaType area) {
  return !sr_disguise_authorised(d, site_faction, area);
}
