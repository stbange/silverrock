/* Planted bug for the fuzz pipeline self-test: crashes on inputs starting with "BUG". */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static char small[4];
  if (size >= 3 && data[1] == 'B') memcpy(small, data, size + 8); /* overflow on purpose */
  return 0;
}
