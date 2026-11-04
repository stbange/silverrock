#include "core/sr_str.h"

#include <string.h>

size_t sr_strlcpy(char *dst, const char *src, size_t cap) {
  size_t n = strlen(src);
  if (cap) {
    size_t c = n < cap - 1 ? n : cap - 1;
    memcpy(dst, src, c);
    dst[c] = '\0';
  }
  return n;
}

void sr_str_normalize(const char *src, char *dst, size_t cap) {
  size_t o = 0;
  for (; *src && o + 1 < cap; src++) {
    char c = *src;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) dst[o++] = c;
  }
  if (cap) dst[o] = '\0';
}

bool sr_str_starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool sr_parse_i64(const char *s, int64_t *out) {
  int64_t v = 0;
  bool neg = false;
  if (*s == '-' || *s == '+') {
    neg = *s == '-';
    s++;
  }
  if (!*s) return false;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + (*s - '0');
  }
  *out = neg ? -v : v;
  return true;
}

static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

int sr_str_tokenize(char *s, char **tokens, int n) {
  int count = 0;
  while (*s && count < n) {
    while (is_space(*s)) s++;
    if (!*s) break;
    tokens[count++] = s;
    while (*s && !is_space(*s)) s++;
    if (*s) *s++ = '\0';
  }
  return count;
}

void sr_str_trim(char *s) {
  size_t n = strlen(s);
  while (n && is_space(s[n - 1])) s[--n] = '\0';
  size_t i = 0;
  while (is_space(s[i])) i++;
  if (i) memmove(s, s + i, n - i + 1);
}
