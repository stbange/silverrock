/* The route in the world: chevrons glowing on the pavement along the next 60 m of the route, a pulse
 * running along them toward the destination, and a beacon of light standing over the destination.
 * Presentation only; drawn in the additive glow pass. */
#include "game/sr_guide.h"
#include "render/sr_render.h"

#include <math.h>

#define AR_REACH   60.0f /* metres of route shown ahead */
#define AR_SPACING 2.5f  /* metres between chevrons */
#define AR_Y       0.14f /* just above the pavement */

/* A flat ">" pointing along (dx, dz): two thin bars meeting at the tip. */
static void chevron(SrMesh *m, SrVec3 p, float dx, float dz, float size, SrColor c) {
  float px = -dz, pz = dx, w = size * 0.16f;
  SrVec3 tip = sr_v3(p.x + dx * size * 0.4f, AR_Y, p.z + dz * size * 0.4f);
  for (int side = -1; side <= 1; side += 2) {
    SrVec3 tail = sr_v3(p.x - dx * size * 0.4f + px * size * 0.5f * (float)side, AR_Y,
                        p.z - dz * size * 0.4f + pz * size * 0.5f * (float)side);
    float ex = tip.x - tail.x, ez = tip.z - tail.z, el = sqrtf(ex * ex + ez * ez);
    float nx = -ez / el * w, nz = ex / el * w;
    SrVec3 a = sr_v3(tail.x - nx, AR_Y, tail.z - nz), b = sr_v3(tail.x + nx, AR_Y, tail.z + nz);
    SrVec3 cc = sr_v3(tip.x + nx, AR_Y, tip.z + nz), d = sr_v3(tip.x - nx, AR_Y, tip.z - nz);
    /* both windings, so the bar shows whichever way it turned out */
    sr_mesh_quad(m, a, b, cc, d, 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
    sr_mesh_quad(m, d, cc, b, a, 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
  }
}

int sr_build_route_ar(const SrRoute *r, SrVec3 eye, double t, SrMesh *chevrons, SrMesh *beacon) {
  int drawn = 0;
  if (r->kind == SR_DEST_NONE || r->n < 1) return 0;
  if (chevrons) {
    /* walk the route from the player, a chevron every few metres, brightest where the pulse is */
    float along = 0;
    float next = 3.0f; /* the first one a step ahead */
    for (int i = 1; i < r->n && along < AR_REACH; i++) {
      SrVec3 a = r->pts[i - 1], b = r->pts[i];
      float dx = b.x - a.x, dz = b.z - a.z, len = sqrtf(dx * dx + dz * dz);
      if (len < 0.01f) continue;
      dx /= len;
      dz /= len;
      while (next <= along + len && next < AR_REACH) {
        float k = next - along;
        SrVec3 p = sr_v3(a.x + dx * k, AR_Y, a.z + dz * k);
        float pulse = fmodf(next - (float)t * 8.0f, 12.0f);
        if (pulse < 0) pulse += 12.0f;
        float lvl = 0.35f + 0.65f * expf(-pulse * 0.6f);
        float fade = 1.0f - next / AR_REACH; /* thinner ahead */
        lvl *= 0.4f + 0.6f * fade;
        SrColor c = {(uint8_t)(30 * lvl), (uint8_t)(200 * lvl), (uint8_t)(255 * lvl), 255};
        chevron(chevrons, p, dx, dz, 1.1f, c);
        drawn++;
        next += AR_SPACING;
      }
      along += len;
    }
  }
  /* the beacon: a shaft of light over the destination, seen over the roofs */
  float bx = r->target.x - eye.x, bz = r->target.z - eye.z, dist = sqrtf(bx * bx + bz * bz);
  if (beacon && dist > 4.0f && dist < 1500.0f) {
    float w = SR_CLAMP(dist * 0.004f, 0.35f, 4.0f); /* stays a few pixels wide far off */
    float rx = -bz / dist * w, rz = bx / dist * w;
    float pulse = 0.85f + 0.15f * sinf((float)t * 2.0f);
    for (int k = 0; k < 8; k++) { /* fading upward in steps, past most roofs */
      float y0 = (float)k * 22.0f, y1 = y0 + 22.0f;
      float lvl = (0.9f - 0.1f * (float)k) * pulse;
      SrColor c = {(uint8_t)(40 * lvl), (uint8_t)(170 * lvl), (uint8_t)(230 * lvl), 255};
      SrVec3 a = sr_v3(r->target.x - rx, y0, r->target.z - rz), b = sr_v3(r->target.x + rx, y0, r->target.z + rz);
      SrVec3 cc = sr_v3(r->target.x + rx, y1, r->target.z + rz), d = sr_v3(r->target.x - rx, y1, r->target.z - rz);
      sr_mesh_quad(beacon, a, b, cc, d, 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
      sr_mesh_quad(beacon, d, cc, b, a, 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
    }
    drawn++;
  }
  return drawn;
}
