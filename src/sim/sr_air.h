/* Aerodyne traffic: flying cars on fixed sky lanes above the major streets. Lanes along x and
 * along z fly at different altitudes so they never meet; each aerodyne's position is a pure
 * function of time, so traffic needs no state, costs nothing off screen and is identical in
 * every run. */
#ifndef SR_AIR_H
#define SR_AIR_H

#include "core/sr_math.h"
#include "world/sr_world.h"

#define SR_AIR_MAX_LANES 24
#define SR_AIR_PER_LANE  6
#define SR_AIR_SPEED     22.0f /* m/s */
#define SR_AIR_ALT_X     70.0f /* lanes along x */
#define SR_AIR_ALT_Z     95.0f /* lanes along z */

typedef struct SrAirLane {
  bool along_x;
  float line;     /* street centre line (z for x lanes, x for z lanes), metres */
  float from, to; /* extent along the lane */
  float alt;
  int8_t dir; /* +1 or -1 */
} SrAirLane;

typedef struct SrAirTraffic {
  int lane_count;
  SrAirLane lanes[SR_AIR_MAX_LANES];
} SrAirTraffic;

enum { SR_AIR_CRUISE, SR_AIR_DESCEND, SR_AIR_LANDED, SR_AIR_LIFT };

typedef struct SrAerodyne {
  SrVec3 pos, dir;
  int lane; /* -1 for an aerodyne landing on a roof */
  uint8_t color;
  uint8_t mode;      /* SR_AIR_* */
  float roll, pitch; /* bank into the weave, nose up when settling */
} SrAerodyne;

#define SR_AIR_LAND_TIME 8.0f /* seconds to descend onto (or lift off) a helipad */

void sr_air_build(SrAirTraffic *a, const SrCity *c);
/* Aerodyne k of a lane at time t (seconds). */
SrAerodyne sr_air_vehicle(const SrAirTraffic *a, int lane, int k, double t);
/* Aerodynes within radius of the camera; returns how many were written. */
int sr_air_near(const SrAirTraffic *a, double t, SrVec3 cam, float radius, SrAerodyne *out, int max);
/* The landing cycle of the helipad on building b at time t: descend, wait, lift off, away. Returns
 * false while the pad is empty. */
bool sr_air_lander(const SrCity *c, int building, double t, SrAerodyne *out);
/* Aerodynes landing on, standing on or leaving helipads within radius; returns how many. */
int sr_air_landers(const SrCity *c, double t, SrVec3 cam, float radius, SrAerodyne *out, int max);

#endif
