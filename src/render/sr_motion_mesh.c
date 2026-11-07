/* Motion: the neon flicker the shader applies (mirrored here for tests), the wind, drones over the
 * rich districts, birds over the docks and the barrens, litter drifting in the wind, fans turning on
 * the roofs, sparks from broken signs, and police light bars. All of it follows game time. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

/* Same formula as the shader: pad 0x80 | seed on an emissive surface. Seeds 0..4 are broken neon
 * (about 8 % of signs), 56..63 blink like tower beacons, the rest burn steady. */
float sr_neon_level(uint8_t pad, float t) {
  if (pad < 0x80u || pad >= 0xC0u) return 1.0f;
  int s = pad & 63;
  if (s < 5) { /* broken: mostly on, dropping out in short bursts */
    float k = floorf(t * 12.0f);
    float h = fmodf(sinf(k * 12.9898f + (float)s * 78.233f) * 43758.5453f, 1.0f);
    if (h < 0) h += 1.0f;
    return h < 0.28f ? 0.12f : 1.0f;
  }
  if (s >= 56) { /* beacons: on for 0.3 s in every 1.5 s */
    float ph = fmodf(t + (float)(s - 56) * 0.17f, 1.5f);
    return ph < 0.3f ? 1.0f : 0.15f;
  }
  return 1.0f;
}

SrVec3 sr_wind(uint64_t seed, int64_t t, float rain) {
  uint64_t h = sr_mix64(seed ^ ((uint64_t)(t / 7200) * 0x9E3779B97F4A7C15ull));
  float a = (float)(h % 6283u) / 1000.0f, s = 0.6f + 1.6f * rain;
  return sr_v3(cosf(a) * s, 0, sinf(a) * s);
}

uint8_t sr_sign_pad(uint64_t seed, uint32_t id) {
  uint64_t h = sr_mix64(seed ^ ((uint64_t)id * 0xC2B2AE3D27D4EB4Full) ^ 0x51u);
  return (uint8_t)(0x80u | (h % 56u)); /* seeds 0..55: 5 of 56 broken, never a beacon */
}

bool sr_vehicle_is_patrol(const SrVehicle *v) { return v->key % 9u == 0; }

void sr_build_police_lights(const SrCrowd *cr, SrVec3 eye, float time_s, int heat, SrMesh *out) {
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < cr->vehicle_count; i++) {
    const SrVehicle *v = &cr->vehicles[i];
    if (!sr_vehicle_is_patrol(v)) continue;
    float dx = v->pos.x - eye.x, dz = v->pos.z - eye.z;
    if (dx * dx + dz * dz > 120.0f * 120.0f) continue;
    if (heat < 3) continue; /* the lights go on when the police are out for the player */
    bool red = fmodf(time_s * 4.0f + (float)(v->key % 7u), 2.0f) < 1.0f;
    SrVec3 side = sr_v3(-v->dir.z * 0.5f, 0, v->dir.x * 0.5f);
    SrVec3 l = sr_v3_add(sr_v3(v->pos.x, 1.55f, v->pos.z), side), r = sr_v3_sub(sr_v3(v->pos.x, 1.55f, v->pos.z), side);
    sr_mesh_sprite(out, &bb, red ? l : r, 2.2f, 2.2f, SR_SPR_GLOW, SR_SPRITE_FACING, 0,
                   red ? (SrColor){255, 30, 40, 255} : (SrColor){40, 80, 255, 255}, 255);
  }
}

int sr_build_flyers(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out) {
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return 0;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  int n = 0;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      int b = by * c->grid + bx;
      if (c->blocks[b].water) continue;
      SrDistrictType t = c->districts[c->blocks[b].district].type;
      int32_t ox, oy;
      sr_city_block_origin(b, c->grid, &ox, &oy);
      float cx = (float)ox / 100.0f + 48.0f, cz = (float)oy / 100.0f + 48.0f;
      if (t == SR_D_SPIRE || t == SR_D_CAMPUS || t == SR_D_CIVIC) { /* security drones on short loops */
        for (int k = 0; k < 2; k++) {
          float a = time_s * (0.35f + 0.1f * (float)k) + (float)(b * 7 + k * 3);
          SrVec3 p = sr_v3(cx + cosf(a) * 22.0f, 14.0f + 4.0f * (float)k + sinf(a * 2.0f), cz + sinf(a) * 22.0f);
          sr_mesh_sprite(out, &bb, p, 0.9f, 0.5f, SR_SPR_DRONE, SR_SPRITE_FACING, 0, (SrColor){60, 60, 70, 255}, 255);
          bool on = fmodf(time_s + (float)k * 0.5f, 1.0f) < 0.2f;
          if (on)
            sr_mesh_sprite(out, &bb, sr_v3(p.x, p.y - 0.25f, p.z), 0.6f, 0.6f, SR_SPR_GLOW, SR_SPRITE_FACING, 0,
                           (SrColor){255, 40, 40, 255}, 255);
          n++;
        }
      }
      if (t == SR_D_DOCKS || t == SR_D_BARRENS) { /* a flock wheeling overhead */
        for (int k = 0; k < 7; k++) {
          float a = time_s * 0.25f + (float)k * 0.31f + (float)b;
          SrVec3 p = sr_v3(cx + cosf(a) * (30.0f + (float)k), 26.0f + sinf(a * 3.0f + (float)k) * 2.0f,
                           cz + sinf(a) * (30.0f + (float)k));
          int frame = sr_anim_frame((double)time_s, 5.0f, 2, (uint32_t)(k * 13 + b));
          sr_mesh_sprite(out, &bb, p, 0.8f, 0.5f, SR_SPR_BIRD + frame, SR_SPRITE_FACING, 0, (SrColor){30, 30, 34, 255},
                         255);
          n++;
        }
      }
    }
  return n;
}

int sr_build_litter(const SrCity *c, SrVec3 eye, float time_s, SrVec3 wind, SrMesh *out) {
  SrBillboard bb = sr_billboard(eye, 0, 0);
  int n = 0;
  float speed = sqrtf(wind.x * wind.x + wind.z * wind.z);
  for (int k = 0; k < 14; k++) { /* scraps carried along the street by the wind, wrapping round the camera */
    uint64_t h = sr_mix64(c->seed ^ (uint64_t)(k * 7919));
    float ox = (float)(h % 4000u) / 100.0f - 20.0f, oz = (float)((h >> 16) % 4000u) / 100.0f - 20.0f;
    float x = fmodf(ox + wind.x * time_s * 0.8f + 2000.0f, 40.0f) - 20.0f;
    float z = fmodf(oz + wind.z * time_s * 0.8f + 2000.0f, 40.0f) - 20.0f;
    SrVec3 p = sr_v3(eye.x + x, 0.08f + 0.25f * fabsf(sinf(time_s * 3.0f + (float)k)) * SR_MIN(1.0f, speed), eye.z + z);
    if (sr_point_covered(c, sr_v3(p.x, 1.0f, p.z))) continue;
    sr_mesh_sprite(out, &bb, p, 0.35f, 0.3f, SR_SPR_LITTER + (int)(h >> 40 & 3u), SR_SPRITE_UPRIGHT, 0,
                   (SrColor){160, 155, 140, 255}, 30);
    n++;
  }
  return n;
}

void sr_build_sparks(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out) {
  /* broken blade signs spit sparks now and then */
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  const SrBlock *b = &c->blocks[blk];
  for (int i = 0; i < b->building_count; i++) {
    const SrBuilding *bd = &c->buildings[b->first_building + i];
    uint64_t h = sr_mix64(c->seed ^ ((uint64_t)bd->id * 0x9E37u) ^ 5u);
    if ((h >> 20) % 100u >= 8u) continue; /* the broken ones */
    float burst = fmodf(time_s + (float)(h % 13u), 6.0f);
    if (burst > 0.5f) continue;
    float x = (float)bd->x0 / 100.0f + 0.6f, z = (float)bd->y1 / 100.0f + 0.75f;
    for (int k = 0; k < 6; k++) {
      float t = burst + (float)k * 0.05f;
      SrVec3 p = sr_v3(x + 0.3f * sinf((float)k * 2.1f), 7.0f - 9.0f * t * t, z + 0.3f * cosf((float)k * 1.7f));
      sr_mesh_sprite(out, &bb, p, 0.25f, 0.25f, SR_SPR_SPARK + (k % 4), SR_SPRITE_FACING, 0,
                     (SrColor){255, 200, 90, 255}, 255);
    }
  }
}

void sr_build_fans(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out) {
  /* the fans of rooftop air conditioners, turning (seen from the roofs and from above) */
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      const SrBlock *b = &c->blocks[by * c->grid + bx];
      for (int i = 0; i < b->building_count; i++) {
        int bi = b->first_building + i;
        SrFacade f;
        sr_facade_for(c, bi, &f);
        if (f.roof != SR_ROOF_HVAC) continue;
        SrVolume v[SR_MAX_VOLUMES];
        int nv = sr_building_volumes(c, bi, &f, v), top = 0;
        for (int k = 0; k < nv; k++)
          if (v[k].flags & SR_VOL_ROOF) top = k;
        if (eye.y < v[top].y1 - 2.0f) continue; /* only seen from above */
        uint32_t seed = (uint32_t)(sr_mix64(c->seed ^ ((uint64_t)c->buildings[bi].id << 17)) >> 24);
        int units = 2 + (int)(seed % 3u);
        float w = v[top].x1 - v[top].x0, d = v[top].z1 - v[top].z0;
        for (int u = 0; u < units; u++) {
          float cx = v[top].x0 + w * (0.2f + 0.2f * (float)u), cz = v[top].z0 + d * (0.3f + 0.15f * (float)(u % 2));
          float a = time_s * 6.0f + (float)u, s = 0.6f, y = v[top].y1 + 1.32f;
          float ca = cosf(a) * s, sa = sinf(a) * s;
          for (int blade = 0; blade < 2; blade++) {
            float bxx = blade ? -sa : ca, bzz = blade ? ca : sa;
            sr_mesh_quad(out, sr_v3(cx - bxx - bzz * 0.1f, y, cz - bzz + bxx * 0.1f),
                         sr_v3(cx + bxx - bzz * 0.1f, y, cz + bzz + bxx * 0.1f),
                         sr_v3(cx + bxx + bzz * 0.1f, y, cz + bzz - bxx * 0.1f),
                         sr_v3(cx - bxx + bzz * 0.1f, y, cz - bzz - bxx * 0.1f), 0, 0, 1, 1, (SrColor){40, 42, 46, 255},
                         SR_LAYER_WHITE, 0, 0);
          }
        }
      }
    }
}
