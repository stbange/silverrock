#include "core/sr_fixed.h"

SrFix sr_fix_mul(SrFix a, SrFix b) { return (SrFix)(((int64_t)a * (int64_t)b) >> 16); }

SrFix sr_fix_div(SrFix a, SrFix b) {
  if (b == 0) { return a >= 0 ? INT32_MAX : INT32_MIN; }
  return (SrFix)(((int64_t)a * 65536) / b);
}

SrFix sr_fix_lerp(SrFix a, SrFix b, SrFix t) { return a + sr_fix_mul(b - a, t); }

int32_t sr_fix_to_int(SrFix a) { return a >> 16; }

uint32_t sr_isqrt64(uint64_t v) {
  uint64_t res = 0;
  uint64_t bit = (uint64_t)1 << 62;
  while (bit > v) bit >>= 2;
  while (bit != 0) {
    if (v >= res + bit) {
      v -= res + bit;
      res = (res >> 1) + bit;
    } else {
      res >>= 1;
    }
    bit >>= 2;
  }
  return (uint32_t)res;
}

int64_t sr_dist2_i(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int64_t dx = (int64_t)ax - bx, dy = (int64_t)ay - by;
  return dx * dx + dy * dy;
}

int32_t sr_dist_i(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  return (int32_t)sr_isqrt64((uint64_t)sr_dist2_i(ax, ay, bx, by));
}

int32_t sr_manhattan_i(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
  int32_t dx = ax > bx ? ax - bx : bx - ax;
  int32_t dy = ay > by ? ay - by : by - ay;
  return dx + dy;
}
