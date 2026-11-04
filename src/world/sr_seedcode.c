#include "world/sr_world.h"

static const char k_alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; /* Crockford base32 */

static int symbol_value(char ch) {
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  if (ch == 'O') ch = '0';
  if (ch == 'I' || ch == 'L') ch = '1';
  for (int i = 0; i < 32; i++) {
    if (k_alphabet[i] == ch) return i;
  }
  return -1;
}

/* Weighted sum modulo a prime larger than any single-symbol change (13 * 31 < 1021),
 * so every single-character typo changes the checksum. */
static int checksum(const int v[13]) {
  int s = 0;
  for (int i = 0; i < 13; i++) s += v[i] * (i + 1);
  return s % 1021;
}

void sr_seed_to_code(uint64_t seed, char out[SR_SEED_CODE_LEN]) {
  int v[15];
  for (int i = 12; i >= 0; i--) {
    v[i] = (int)(seed & 31u);
    seed >>= 5;
  }
  int cs = checksum(v);
  v[13] = cs / 32;
  v[14] = cs % 32;
  int o = 0;
  for (int i = 0; i < 15; i++) {
    if (i == 5 || i == 10) out[o++] = '-';
    out[o++] = k_alphabet[v[i]];
  }
  out[o] = '\0';
}

bool sr_seed_from_code(const char *code, uint64_t *seed_out) {
  int v[15];
  int n = 0;
  for (; *code; code++) {
    if (*code == '-' || *code == ' ') continue;
    int val = symbol_value(*code);
    if (val < 0 || n >= 15) return false;
    v[n++] = val;
  }
  if (n != 15) return false;
  if (v[0] > 15) return false; /* 13 symbols carry 65 bits; the top bit must be clear */
  if (checksum(v) != v[13] * 32 + v[14]) return false;
  uint64_t seed = 0;
  for (int i = 0; i < 13; i++) seed = (seed << 5) | (uint64_t)v[i];
  *seed_out = seed;
  return true;
}
