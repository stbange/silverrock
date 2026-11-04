/* Small string helpers. */
#ifndef SR_STR_H
#define SR_STR_H

#include "core/sr_types.h"

/* Copies with truncation; always terminates. Returns strlen(src). */
size_t sr_strlcpy(char *dst, const char *src, size_t cap);
/* Lowercase letters and digits only: "Sony-Corp." -> "sonycorp". */
void sr_str_normalize(const char *src, char *dst, size_t cap);
bool sr_str_starts_with(const char *s, const char *prefix);
/* Parses a decimal integer (optional sign); false on junk. */
bool sr_parse_i64(const char *s, int64_t *out);
/* Splits s in place on whitespace; returns number of tokens (max n). */
int sr_str_tokenize(char *s, char **tokens, int n);
void sr_str_trim(char *s);

#endif
