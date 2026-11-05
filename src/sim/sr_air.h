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

typedef struct SrAerodyne {
  SrVec3 pos, dir;
  int lane;
  uint8_t color;
} SrAerodyne;

void sr_air_build(SrAirTraffic *a, const SrCity *c);
/* Aerodyne k of a lane at time t (seconds). */
SrAerodyne sr_air_vehicle(const SrAirTraffic *a, int lane, int k, double t);
/* Aerodynes within radius of the camera; returns how many were written. */
int sr_air_near(const SrAirTraffic *a, double t, SrVec3 cam, float radius, SrAerodyne *out, int max);

#endif
