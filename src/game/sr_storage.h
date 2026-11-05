/* Abstract per-user storage. The platform layer supplies a file-backed implementation
 * (atomic temp-file + rename; IndexedDB-backed on the web); tests use the memory store. */
#ifndef SR_STORAGE_H
#define SR_STORAGE_H

#include "core/sr_types.h"

typedef struct SrStorage {
  void *user;
  /* Must be atomic: either the whole file is replaced or the old one stays. */
  SrResult (*write)(void *user, const char *name, const void *data, size_t len);
  /* Allocates *data with malloc; caller frees. */
  SrResult (*read)(void *user, const char *name, void **data, size_t *len);
  SrResult (*remove)(void *user, const char *name);
  bool (*exists)(void *user, const char *name);
} SrStorage;

#define SR_MEM_FILES 16
#define SR_MEM_NAME  32

typedef struct SrMemFile {
  char name[SR_MEM_NAME];
  void *data;
  size_t len;
  bool used;
} SrMemFile;

typedef struct SrMemStore {
  SrMemFile files[SR_MEM_FILES];
  int fail_writes; /* test hook: next N writes are interrupted */
  int writes;
} SrMemStore;

void sr_memstore_init(SrMemStore *m, SrStorage *out);
void sr_memstore_free(SrMemStore *m);
SrMemFile *sr_memstore_get(SrMemStore *m, const char *name);

#endif
