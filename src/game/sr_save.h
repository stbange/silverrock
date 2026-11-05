/* Save format: "SRSV", format version, generator version, seed, then sections
 * (type, version, length, payload, CRC32). Saves hold the seed plus all changes, never the
 * generated world. Older versions are migrated; newer ones are refused. */
#ifndef SR_SAVE_H
#define SR_SAVE_H

#include "core/sr_mem.h"
#include "game/sr_game.h"

#define SR_SAVE_VERSION   2u
#define SR_AUTOSAVE_SLOTS 3
#define SR_MANUAL_SLOTS   3

typedef enum SrSection {
  SR_SEC_END = 0,
  SR_SEC_CLOCK = 1,
  SR_SEC_PLAYER = 2,
  SR_SEC_NPCS = 3,
  SR_SEC_JOBS = 4,
  SR_SEC_HEAT = 5,
  SR_SEC_EVENTS = 6,
  SR_SEC_DELTAS = 7,
  SR_SEC_META = 8
} SrSection;

typedef struct SrSaveInfo {
  uint32_t version;
  uint32_t gen_version;
  uint64_t seed;
  int64_t time;
  int64_t saved_at; /* monotonic save counter, used for autosave rotation */
} SrSaveInfo;

/* Serialises the game. `version` lets tests write older formats (1 or 2). */
SrResult sr_save_write(const SrGame *g, SrWriter *w, uint32_t version, int64_t counter);
SrResult sr_save_read(SrGame *g, const void *data, size_t len);
SrResult sr_save_peek(const void *data, size_t len, SrSaveInfo *info);

/* Storage-level helpers */
SrResult sr_save_to(const SrGame *g, SrStorage *s, const char *name, int64_t counter);
SrResult sr_load_from(SrGame *g, SrStorage *s, const char *name);
/* Writes the next autosave slot (oldest one replaced). Returns slot index or -1. */
int sr_autosave(const SrGame *g, SrStorage *s);
/* Manual save; refused during combat. */
SrResult sr_manual_save(const SrGame *g, SrStorage *s, int slot);
/* Hardcore: delete the character's save. */
SrResult sr_save_delete_hardcore(SrStorage *s);
void sr_autosave_name(int slot, char *out, size_t cap);

#endif
