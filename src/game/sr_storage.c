#include "game/sr_storage.h"

#include "core/sr_str.h"

#include <stdlib.h>
#include <string.h>

SrMemFile *sr_memstore_get(SrMemStore *m, const char *name) {
  for (int i = 0; i < SR_MEM_FILES; i++)
    if (m->files[i].used && strcmp(m->files[i].name, name) == 0) return &m->files[i];
  return NULL;
}

static SrResult mem_write(void *user, const char *name, const void *data, size_t len) {
  SrMemStore *m = (SrMemStore *)user;
  m->writes++;
  if (m->fail_writes > 0) {
    m->fail_writes--;
    return SR_ERR_IO; /* interrupted: the old file stays untouched */
  }
  SrMemFile *f = sr_memstore_get(m, name);
  if (!f) {
    for (int i = 0; i < SR_MEM_FILES && !f; i++)
      if (!m->files[i].used) f = &m->files[i];
    if (!f) return SR_ERR_NOMEM;
    f->used = true;
    sr_strlcpy(f->name, name, sizeof f->name);
    f->data = NULL;
  }
  void *copy = malloc(len ? len : 1);
  if (!copy) return SR_ERR_NOMEM; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  memcpy(copy, data, len);
  free(f->data);
  f->data = copy;
  f->len = len;
  return SR_OK;
}

static SrResult mem_read(void *user, const char *name, void **data, size_t *len) {
  SrMemFile *f = sr_memstore_get((SrMemStore *)user, name);
  if (!f) return SR_ERR_NOT_FOUND;
  *data = malloc(f->len ? f->len : 1);
  if (!*data) return SR_ERR_NOMEM; /* GCOVR_EXCL_LINE reason: malloc failure not injectable */
  memcpy(*data, f->data, f->len);
  *len = f->len;
  return SR_OK;
}

static SrResult mem_remove(void *user, const char *name) {
  SrMemFile *f = sr_memstore_get((SrMemStore *)user, name);
  if (!f) return SR_ERR_NOT_FOUND;
  free(f->data);
  memset(f, 0, sizeof *f);
  return SR_OK;
}

static bool mem_exists(void *user, const char *name) { return sr_memstore_get((SrMemStore *)user, name) != NULL; }

void sr_memstore_init(SrMemStore *m, SrStorage *out) {
  memset(m, 0, sizeof *m);
  out->user = m;
  out->write = mem_write;
  out->read = mem_read;
  out->remove = mem_remove;
  out->exists = mem_exists;
}

void sr_memstore_free(SrMemStore *m) {
  for (int i = 0; i < SR_MEM_FILES; i++) free(m->files[i].data);
  memset(m, 0, sizeof *m);
}
