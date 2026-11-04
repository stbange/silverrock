#include "core/sr_mem.h"

#include <stdlib.h>
#include <string.h>

void sr_arena_init(SrArena *a, void *memory, size_t cap) {
  a->base = (uint8_t *)memory;
  a->cap = cap;
  a->used = 0;
  a->fail_after = -1;
}

void *sr_arena_alloc(SrArena *a, size_t size, size_t align) {
  if (a->fail_after == 0) return NULL;
  if (a->fail_after > 0) a->fail_after--;
  if (align == 0) align = 1;
  size_t start = (a->used + (align - 1)) & ~(align - 1);
  if (start > a->cap || size > a->cap - start) return NULL;
  a->used = start + size;
  memset(a->base + start, 0, size);
  return a->base + start;
}

void sr_arena_reset(SrArena *a) { a->used = 0; }

void sr_writer_init(SrWriter *w) { memset(w, 0, sizeof *w); }

void sr_writer_free(SrWriter *w) {
  free(w->data);
  memset(w, 0, sizeof *w);
}

void sr_put_bytes(SrWriter *w, const void *p, size_t n) {
  if (w->failed) return;
  if (w->len + n > w->cap) {
    size_t ncap = w->cap ? w->cap * 2 : 256;
    while (ncap < w->len + n) ncap *= 2;
    if (w->limit && ncap > w->limit) {
      w->failed = true;
      return;
    }
    uint8_t *nd = (uint8_t *)realloc(w->data, ncap);
    if (!nd) { /* GCOVR_EXCL_START reason: realloc failure not injectable */
      w->failed = true;
      return;
    } /* GCOVR_EXCL_STOP */
    w->data = nd;
    w->cap = ncap;
  }
  if (n) memcpy(w->data + w->len, p, n);
  w->len += n;
}

void sr_put_u8(SrWriter *w, uint8_t v) { sr_put_bytes(w, &v, 1); }
void sr_put_u16(SrWriter *w, uint16_t v) {
  uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
  sr_put_bytes(w, b, 2);
}
void sr_put_u32(SrWriter *w, uint32_t v) {
  uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
  sr_put_bytes(w, b, 4);
}
void sr_put_u64(SrWriter *w, uint64_t v) {
  sr_put_u32(w, (uint32_t)v);
  sr_put_u32(w, (uint32_t)(v >> 32));
}
void sr_put_i32(SrWriter *w, int32_t v) { sr_put_u32(w, (uint32_t)v); }
void sr_put_i64(SrWriter *w, int64_t v) { sr_put_u64(w, (uint64_t)v); }
void sr_put_f32(SrWriter *w, float v) {
  uint32_t u;
  memcpy(&u, &v, 4);
  sr_put_u32(w, u);
}
void sr_put_str(SrWriter *w, const char *s) {
  size_t n = strlen(s);
  sr_put_u16(w, (uint16_t)n);
  sr_put_bytes(w, s, n);
}
void sr_patch_u32(SrWriter *w, size_t off, uint32_t v) {
  if (w->failed || off + 4 > w->len) return;
  w->data[off] = (uint8_t)v;
  w->data[off + 1] = (uint8_t)(v >> 8);
  w->data[off + 2] = (uint8_t)(v >> 16);
  w->data[off + 3] = (uint8_t)(v >> 24);
}

void sr_reader_init(SrReader *r, const void *data, size_t len) {
  r->data = (const uint8_t *)data;
  r->len = len;
  r->pos = 0;
  r->failed = false;
}

bool sr_get_bytes(SrReader *r, void *out, size_t n) {
  if (r->failed || n > r->len - r->pos) {
    r->failed = true;
    if (out && n) memset(out, 0, n);
    return false;
  }
  if (out && n) memcpy(out, r->data + r->pos, n);
  r->pos += n;
  return true;
}

uint8_t sr_get_u8(SrReader *r) {
  uint8_t b = 0;
  sr_get_bytes(r, &b, 1);
  return b;
}
uint16_t sr_get_u16(SrReader *r) {
  uint8_t b[2];
  sr_get_bytes(r, b, 2);
  return (uint16_t)(b[0] | (b[1] << 8));
}
uint32_t sr_get_u32(SrReader *r) {
  uint8_t b[4];
  sr_get_bytes(r, b, 4);
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
uint64_t sr_get_u64(SrReader *r) {
  uint64_t lo = sr_get_u32(r);
  uint64_t hi = sr_get_u32(r);
  return lo | (hi << 32);
}
int32_t sr_get_i32(SrReader *r) { return (int32_t)sr_get_u32(r); }
int64_t sr_get_i64(SrReader *r) { return (int64_t)sr_get_u64(r); }
float sr_get_f32(SrReader *r) {
  uint32_t u = sr_get_u32(r);
  float f;
  memcpy(&f, &u, 4);
  return f;
}
bool sr_get_str(SrReader *r, char *out, size_t cap) {
  uint16_t n = sr_get_u16(r);
  if (r->failed || (size_t)n + 1 > cap) {
    r->failed = true;
    if (cap) out[0] = '\0';
    return false;
  }
  if (!sr_get_bytes(r, out, n)) {
    out[0] = '\0';
    return false;
  }
  out[n] = '\0';
  return true;
}

uint32_t sr_crc32(const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= p[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
