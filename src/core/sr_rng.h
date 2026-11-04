/* Hashing and PCG32 random streams. There is no global generator: every system derives
 * named streams from the seed so adding a random call never shifts another system. */
#ifndef SR_RNG_H
#define SR_RNG_H

#include "core/sr_types.h"

typedef struct SrRng {
  uint64_t state;
  uint64_t inc;
} SrRng;

uint64_t sr_mix64(uint64_t x);
uint64_t sr_hash_combine(uint64_t h, uint64_t v);
uint64_t sr_hash_str(const char *s);
uint64_t sr_hash_bytes(uint64_t h, const void *data, size_t len);
/* Stream derived from seed, a domain tag and two ids. */
SrRng sr_rng_stream(uint64_t seed, const char *tag, uint64_t a, uint64_t b);
void sr_rng_seed(SrRng *r, uint64_t seed, uint64_t stream);
uint32_t sr_rng_u32(SrRng *r);
/* Uniform integer in [lo, hi] inclusive, unbiased. */
int32_t sr_rng_range(SrRng *r, int32_t lo, int32_t hi);
/* True with probability permille/1000. */
bool sr_rng_chance(SrRng *r, int32_t permille);
/* Float in [0, 1). Simulation only, never world generation. */
float sr_rng_float(SrRng *r);
/* Weighted pick: returns index i with probability weights[i]/sum. */
int sr_rng_weighted(SrRng *r, const int32_t *weights, int count);

#endif
