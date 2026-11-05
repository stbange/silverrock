#include "core/sr_rng.h"

uint64_t sr_mix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

uint64_t sr_hash_combine(uint64_t h, uint64_t v) { return sr_mix64(h ^ sr_mix64(v)); }

uint64_t sr_hash_bytes(uint64_t h, const void *data, size_t len) {
  const unsigned char *p = (const unsigned char *)data;
  if (h == 0) h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < len; i++) {
    h ^= p[i];
    h *= 0x100000001B3ull;
  }
  return h;
}

uint64_t sr_hash_str(const char *s) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 0x100000001B3ull;
  }
  return h;
}

void sr_rng_seed(SrRng *r, uint64_t seed, uint64_t stream) {
  r->state = 0;
  r->inc = (stream << 1u) | 1u;
  (void)sr_rng_u32(r);
  r->state += seed;
  (void)sr_rng_u32(r);
}

SrRng sr_rng_stream(uint64_t seed, const char *tag, uint64_t a, uint64_t b) {
  uint64_t h = sr_hash_combine(sr_hash_combine(sr_hash_combine(seed, sr_hash_str(tag)), a), b);
  SrRng r;
  sr_rng_seed(&r, h, sr_mix64(h ^ 0xA5A5A5A5u));
  return r;
}

uint32_t sr_rng_u32(SrRng *r) {
  uint64_t old = r->state;
  r->state = old * 6364136223846793005ull + r->inc;
  uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
  uint32_t rot = (uint32_t)(old >> 59u);
  return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
}

int32_t sr_rng_range(SrRng *r, int32_t lo, int32_t hi) {
  if (hi <= lo) return lo;
  uint32_t span = (uint32_t)((int64_t)hi - lo + 1);
  uint32_t threshold = (0u - span) % span;
  for (;;) {
    uint32_t x = sr_rng_u32(r);
    if (x >= threshold) return (int32_t)((int64_t)lo + (x % span));
  }
}

bool sr_rng_chance(SrRng *r, int32_t permille) { return sr_rng_range(r, 0, 999) < permille; }

float sr_rng_float(SrRng *r) { return (float)(sr_rng_u32(r) >> 8) * (1.0f / 16777216.0f); }

int sr_rng_weighted(SrRng *r, const int32_t *weights, int count) {
  int32_t sum = 0;
  for (int i = 0; i < count; i++) sum += weights[i] > 0 ? weights[i] : 0;
  if (sum <= 0) return 0;
  int32_t x = sr_rng_range(r, 0, sum - 1);
  for (int i = 0; i < count; i++) {
    int32_t w = weights[i] > 0 ? weights[i] : 0;
    if (x < w) return i;
    x -= w;
  }
  return count - 1; /* GCOVR_EXCL_LINE reason: unreachable, loop always returns */
}
