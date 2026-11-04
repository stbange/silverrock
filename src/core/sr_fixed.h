/* Q16.16 fixed-point and integer helpers. World generation uses only these, never floats,
 * so a seed produces bit-identical cities on every compiler and platform. */
#ifndef SR_FIXED_H
#define SR_FIXED_H

#include "core/sr_types.h"

typedef int32_t SrFix;

#define SR_FIX_ONE ((SrFix)65536)
#define SR_FIX(i) ((SrFix)((i) * 65536))

SrFix sr_fix_mul(SrFix a, SrFix b);
SrFix sr_fix_div(SrFix a, SrFix b);
SrFix sr_fix_lerp(SrFix a, SrFix b, SrFix t);
int32_t sr_fix_to_int(SrFix a); /* rounds toward negative infinity */
/* Integer square root of a non-negative 64-bit value (floor). */
uint32_t sr_isqrt64(uint64_t v);
/* Integer distance helpers on centimetre coordinates. */
int64_t sr_dist2_i(int32_t ax, int32_t ay, int32_t bx, int32_t by);
int32_t sr_dist_i(int32_t ax, int32_t ay, int32_t bx, int32_t by);
int32_t sr_manhattan_i(int32_t ax, int32_t ay, int32_t bx, int32_t by);

#endif
