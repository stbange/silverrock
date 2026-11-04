/* Arena allocator and growable byte buffers with fault injection for tests. */
#ifndef SR_MEM_H
#define SR_MEM_H

#include "core/sr_types.h"

typedef struct SrArena {
  uint8_t *base;
  size_t cap;
  size_t used;
  int fail_after; /* -1: never fail; n >= 0: the (n+1)-th allocation fails */
} SrArena;

void sr_arena_init(SrArena *a, void *memory, size_t cap);
void *sr_arena_alloc(SrArena *a, size_t size, size_t align);
void sr_arena_reset(SrArena *a);

/* Little-endian binary writer that grows on the heap. */
typedef struct SrWriter {
  uint8_t *data;
  size_t len;
  size_t cap;
  bool failed;
  size_t limit; /* 0 = unlimited; used to inject allocation failures */
} SrWriter;

void sr_writer_init(SrWriter *w);
void sr_writer_free(SrWriter *w);
void sr_put_bytes(SrWriter *w, const void *p, size_t n);
void sr_put_u8(SrWriter *w, uint8_t v);
void sr_put_u16(SrWriter *w, uint16_t v);
void sr_put_u32(SrWriter *w, uint32_t v);
void sr_put_u64(SrWriter *w, uint64_t v);
void sr_put_i32(SrWriter *w, int32_t v);
void sr_put_i64(SrWriter *w, int64_t v);
void sr_put_f32(SrWriter *w, float v);
void sr_put_str(SrWriter *w, const char *s);
/* Overwrites 4 bytes at offset (for back-patching lengths). */
void sr_patch_u32(SrWriter *w, size_t offset, uint32_t v);

typedef struct SrReader {
  const uint8_t *data;
  size_t len;
  size_t pos;
  bool failed;
} SrReader;

void sr_reader_init(SrReader *r, const void *data, size_t len);
bool sr_get_bytes(SrReader *r, void *out, size_t n);
uint8_t sr_get_u8(SrReader *r);
uint16_t sr_get_u16(SrReader *r);
uint32_t sr_get_u32(SrReader *r);
uint64_t sr_get_u64(SrReader *r);
int32_t sr_get_i32(SrReader *r);
int64_t sr_get_i64(SrReader *r);
float sr_get_f32(SrReader *r);
/* Reads a string into out (always terminated); fails if it does not fit. */
bool sr_get_str(SrReader *r, char *out, size_t cap);

uint32_t sr_crc32(const void *data, size_t len);

#endif
