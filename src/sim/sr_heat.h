/* Law and Heat: evidence, Heat per jurisdiction, escalating responses, decay, recognition,
 * bribes, arrests, disguises and restricted areas. */
#ifndef SR_HEAT_H
#define SR_HEAT_H

#include "world/sr_world.h"

#define SR_MAX_EVIDENCE 64

typedef enum SrEvidenceType { SR_EVID_WITNESS, SR_EVID_CAMERA, SR_EVID_TRACE, SR_EVID_BODY, SR_EVID_PLANTED } SrEvidenceType;

typedef struct SrEvidence {
  uint8_t type;
  int8_t jurisdiction;
  int8_t points_to; /* -2 player, faction index if planted */
  uint8_t severity;
  int64_t time;
  uint64_t look; /* appearance key of the perpetrator */
} SrEvidence;

typedef struct SrHeat {
  int8_t level[SR_MAX_FACTIONS];
  int64_t last_seen[SR_MAX_FACTIONS];
  int64_t decay_from[SR_MAX_FACTIONS];
  uint64_t known_look[SR_MAX_FACTIONS];
  int evidence_count;
  SrEvidence evidence[SR_MAX_EVIDENCE];
} SrHeat;

typedef struct SrCrime {
  int8_t jurisdiction;
  uint8_t severity;     /* 1 trespass .. 5 murder */
  bool witnessed;       /* a witness saw it and reported */
  bool camera;          /* a working camera recorded it and the footage was reviewed */
  bool evidence_left;
  uint64_t look;
  int64_t time;
} SrCrime;

typedef struct SrArrest {
  bool allowed;
  int32_t fine;
  int64_t jail_seconds;
} SrArrest;

void sr_heat_init(SrHeat *h);
/* Jurisdiction of a location: owning corp of the building, else the police. */
int sr_heat_jurisdiction(const SrCity *c, int building);
/* Returns Heat added (0 if undetected). */
int sr_heat_crime(SrHeat *h, const SrCrime *cr);
/* Response time in seconds for the first responders. */
int sr_heat_response_time(int level, SrDistrictType district);
void sr_heat_seen(SrHeat *h, int jurisdiction, int64_t t);
/* Decays Heat for jurisdictions that have not seen the player. */
void sr_heat_update(SrHeat *h, int64_t t, bool hiding_at_home);
bool sr_heat_recognises(const SrHeat *h, int jurisdiction, uint64_t look);
int32_t sr_heat_bribe_cost(const SrHeat *h, int jurisdiction);
bool sr_heat_bribe(SrHeat *h, int jurisdiction, int police, int64_t *cash);
SrArrest sr_heat_arrest_terms(const SrHeat *h, int jurisdiction, int police);
void sr_heat_surrender(SrHeat *h, int jurisdiction);
/* Removes evidence of a jurisdiction and lowers its Heat (records erased in the Matrix). */
void sr_heat_erase_records(SrHeat *h, int jurisdiction);
/* Who the evidence for a crime points to: -2 player, faction index if framed, -1 nobody. */
int sr_heat_attribution(const SrEvidence *ev, int n);

/* Disguises */
typedef enum SrAreaType { SR_AREA_PUBLIC, SR_AREA_MAINTENANCE, SR_AREA_SECURE, SR_AREA_EXEC } SrAreaType;
typedef struct SrDisguise {
  int8_t faction;   /* uniform of this faction, -1 none */
  uint8_t clearance; /* highest SrAreaType it covers */
  bool blown;
} SrDisguise;
/* True if the player in this area is treated as authorised. */
bool sr_disguise_authorised(const SrDisguise *d, int site_faction, SrAreaType area);
/* Out-of-role actions (drawing a weapon, picking a lock) blow the disguise. */
void sr_disguise_action(SrDisguise *d, bool suspicious_action);
bool sr_is_trespassing(const SrDisguise *d, int site_faction, SrAreaType area);

#endif
