/* Fuzz targets for every parser of external input: save files, settings, scenario scripts,
 * seed codes and key bindings. Built either with libFuzzer (clang -fsanitize=fuzzer) or with
 * the stand-alone driver in fuzz_driver.c (any compiler, random mutation). */
#include "game/sr_save.h"
#include "game/sr_script.h"

#include <stdlib.h>
#include <string.h>

static SrGame *g_game;
static SrWriter g_valid;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (!g_game) {
    g_game = sr_game_create();
    sr_game_new(g_game, 3, NULL, SR_BG_STREET_KID, "Fuzz");
    sr_writer_init(&g_valid);
    sr_save_write(g_game, &g_valid, SR_SAVE_VERSION, 1);
  }
  if (size == 0) return 0;
  char *text = (char *)malloc(size + 1);
  memcpy(text, data, size);
  text[size] = '\0';
  switch (data[0] % 5) {
    case 0: { /* save file: mutate a valid save in place so the fuzzer gets past the header */
      uint8_t *buf = (uint8_t *)malloc(g_valid.len);
      memcpy(buf, g_valid.data, g_valid.len);
      for (size_t i = 1; i + 1 < size; i += 2)
        buf[(data[i] * 131u + data[i + 1] * 7u + i) % g_valid.len] ^= data[i + 1];
      sr_save_read(g_game, buf, g_valid.len);
      sr_save_read(g_game, data, size);
      free(buf);
      break;
    }
    case 1: {
      SrSettings s;
      sr_settings_default(&s);
      sr_settings_parse(&s, text + 1);
      break;
    }
    case 2: {
      uint64_t seed;
      sr_seed_from_code(text + 1, &seed);
      break;
    }
    case 3: {
      SrInput in;
      sr_input_init(&in, false);
      sr_input_bindings_load(&in, text + 1);
      break;
    }
    default: {
      /* scenario commands against a running game (no newgame: keeps the run fast) */
      char out[256];
      char *line = strtok(text + 1, "\n");
      for (int k = 0; line && k < 4; k++, line = strtok(NULL, "\n")) {
        if (strstr(line, "autoplay") || strstr(line, "advance") || strstr(line, "sleep") || strstr(line, "steps"))
          continue;
        sr_game_command(g_game, line, out, sizeof out);
      }
      break;
    }
  }
  free(text);
  return 0;
}
