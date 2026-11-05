/* Stand-alone fuzz driver for compilers without libFuzzer: feeds random and mutated inputs to
 * LLVMFuzzerTestOneInput. Usage: fuzz_driver [iterations] [seed] [file...]. A crash (caught by
 * AddressSanitizer) aborts with the reproducing input written to crash-input.bin. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static uint8_t g_buf[4096];
static size_t g_len;

static void save_current(void) {
  FILE *f = fopen("crash-input.bin", "wb");
  if (f) {
    fwrite(g_buf, 1, g_len, f);
    fclose(f);
  }
}

int main(int argc, char **argv) {
  long iters = argc > 1 ? atol(argv[1]) : 20000;
  uint64_t s = argc > 2 ? strtoull(argv[2], NULL, 10) : 1;
  for (int i = 3; i < argc; i++) { /* replay files */
    FILE *f = fopen(argv[i], "rb");
    if (!f) continue;
    g_len = fread(g_buf, 1, sizeof g_buf, f);
    fclose(f);
    LLVMFuzzerTestOneInput(g_buf, g_len);
  }
  static const char *const seeds[] = {"\x01volume=5\ncrt=1\n",
                                      "\x02"
                                      "00000-00000-0000Z",
                                      "\x03jump=C;fire=W", "\x04get player.cash\nteleport home", "\x00"};
  for (long it = 0; it < iters; it++) {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    const char *base = seeds[(s >> 33) % 5];
    g_len = strlen(base) + 1;
    memcpy(g_buf, base, g_len);
    size_t extra = (s >> 40) % 64;
    for (size_t k = 0; k < extra && g_len < sizeof g_buf; k++) {
      s = s * 6364136223846793005ull + 1442695040888963407ull;
      g_buf[g_len++] = (uint8_t)(s >> 56);
    }
    for (int m = 0; m < 4; m++) {
      s = s * 6364136223846793005ull + 1442695040888963407ull;
      g_buf[(s >> 20) % g_len] ^= (uint8_t)(s >> 48);
    }
    save_current(); /* if this input crashes, it is on disk */
    LLVMFuzzerTestOneInput(g_buf, g_len);
  }
  remove("crash-input.bin");
  printf("fuzz: %ld inputs, no crash\n", iters);
  return 0;
}
