/* The vertical layers: highway decks with barriers and lamps (and traffic moving on them), glass
 * skyways between towers, and the elevated metro whose trains run on the metro timetable. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>

#define DECK_Y0 11.0f
#define DECK_Y1 12.0f

void sr_build_highway(const SrCity *c, int blk, SrMesh *m) {
  int32_t ox, oy;
  sr_city_block_origin(blk, c->grid, &ox, &oy);
  float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f, s = (float)SR_BLOCK_CM / 100.0f,
        st = (float)SR_STREET_CM / 100.0f;
  float za = z0 + 2.0f, zb = z0 + st - 2.0f;
  SrColor deck = {95, 95, 100, 255}, rail = {120, 120, 125, 255};
  sr_mesh_box(m, sr_v3(x0, DECK_Y0, za), sr_v3(x0 + s, DECK_Y1, zb), deck, SR_LAYER_CONCRETE, 0.2f, 0);
  /* the road surface on top with its lines, barriers along both edges */
  sr_mesh_quad(m, sr_v3(x0, DECK_Y1 + 0.01f, zb), sr_v3(x0 + s, DECK_Y1 + 0.01f, zb),
               sr_v3(x0 + s, DECK_Y1 + 0.01f, za), sr_v3(x0, DECK_Y1 + 0.01f, za), 0, 0, s / (zb - za), 1,
               (SrColor){255, 255, 255, 255}, SR_LAYER_ASPHALT, 0, 1);
  sr_mesh_box(m, sr_v3(x0, DECK_Y1, za), sr_v3(x0 + s, DECK_Y1 + 0.9f, za + 0.35f), rail, SR_LAYER_CONCRETE, 0.3f, 0);
  sr_mesh_box(m, sr_v3(x0, DECK_Y1, zb - 0.35f), sr_v3(x0 + s, DECK_Y1 + 0.9f, zb), rail, SR_LAYER_CONCRETE, 0.3f, 0);
  for (int k = 0; k < 4; k++) { /* lamps on the deck every 24 m */
    float lx = x0 + 12.0f + 24.0f * (float)k;
    sr_mesh_box(m, sr_v3(lx - 0.08f, DECK_Y1, za + 0.1f), sr_v3(lx + 0.08f, DECK_Y1 + 5.0f, za + 0.26f),
                (SrColor){60, 62, 68, 255}, SR_LAYER_METAL, 0, 0);
    sr_mesh_box(m, sr_v3(lx - 0.3f, DECK_Y1 + 4.8f, za + 0.2f), sr_v3(lx + 0.3f, DECK_Y1 + 5.0f, za + 1.2f),
                (SrColor){255, 170, 70, 255}, SR_LAYER_WHITE, 0, 255);
  }
}

void sr_build_highway_traffic(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out) {
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int dy = -2; dy <= 2; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      int b = by * c->grid + bx;
      if (!c->blocks[b].highway) continue;
      int32_t ox, oy;
      sr_city_block_origin(b, c->grid, &ox, &oy);
      float x0 = (float)ox / 100.0f, z0 = (float)oy / 100.0f, st = (float)SR_STREET_CM / 100.0f;
      for (int car = 0; car < 4; car++) { /* head lights one way, tail lights the other */
        bool east = car % 2 == 0;
        float speed = 22.0f + 3.0f * (float)car;
        float x = fmodf(time_s * speed + (float)(car * 37 + b * 13), 96.0f);
        float px = east ? x0 + x : x0 + 96.0f - x;
        float pz = z0 + st * 0.5f + (east ? 2.0f : -2.0f);
        SrColor col = east ? (SrColor){230, 230, 200, 255} : (SrColor){230, 40, 30, 255};
        for (int side = -1; side <= 1; side += 2)
          sr_mesh_sprite(out, &bb, sr_v3(px, DECK_Y1 + 0.8f, pz + (float)side * 0.7f), 0.9f, 0.9f, SR_SPR_GLOW,
                         SR_SPRITE_FACING, 0, col, 255);
      }
    }
}

void sr_build_skyways(const SrCity *c, SrMesh *m) {
  SrSkywayTube t[SR_MAX_TUBES];
  int n = sr_skyway_tubes(c, t, SR_MAX_TUBES);
  for (int i = 0; i < n; i++) {
    /* a glass tube lit from inside, with a dark frame top and bottom */
    sr_mesh_box(m, sr_v3(t[i].x0, t[i].y, t[i].z0), sr_v3(t[i].x1, t[i].y + 4.0f, t[i].z1),
                (SrColor){120, 170, 200, 255}, SR_LAYER_WINMOD_FIRST + SR_WIN_CURTAIN, 0.25f, 60);
    sr_mesh_box(m, sr_v3(t[i].x0, t[i].y - 0.4f, t[i].z0 - 0.2f), sr_v3(t[i].x1, t[i].y, t[i].z1 + 0.2f),
                (SrColor){50, 54, 62, 255}, SR_LAYER_METAL, 0.2f, 0);
    bool along_x = (t[i].x1 - t[i].x0) > (t[i].z1 - t[i].z0);
    float cx = (t[i].x0 + t[i].x1) * 0.5f, cz = (t[i].z0 + t[i].z1) * 0.5f;
    if (along_x) /* a strip of light along the floor */
      sr_mesh_box(m, sr_v3(t[i].x0, t[i].y + 0.1f, cz - 0.1f), sr_v3(t[i].x1, t[i].y + 0.2f, cz + 0.1f),
                  (SrColor){150, 220, 255, 255}, SR_LAYER_WHITE, 0, 255);
    else
      sr_mesh_box(m, sr_v3(cx - 0.1f, t[i].y + 0.1f, t[i].z0), sr_v3(cx + 0.1f, t[i].y + 0.2f, t[i].z1),
                  (SrColor){150, 220, 255, 255}, SR_LAYER_WHITE, 0, 255);
  }
}

void sr_build_metro_tracks(const SrCity *c, SrMesh *m) {
  SrColor beam = {85, 85, 92, 255}, pillar = {100, 100, 104, 255};
  for (int e = 0; e < c->metro_edge_count; e++) {
    float p[3][2];
    sr_metro_route(c, e, p);
    for (int s = 0; s < 2; s++) {
      float ax = p[s][0], az = p[s][1], bx = p[s + 1][0], bz = p[s + 1][1];
      if (fabsf(ax - bx) + fabsf(az - bz) < 0.5f) continue;
      float lo_x = SR_MIN(ax, bx) - 1.2f, hi_x = SR_MAX(ax, bx) + 1.2f, lo_z = SR_MIN(az, bz) - 1.2f,
            hi_z = SR_MAX(az, bz) + 1.2f;
      sr_mesh_box(m, sr_v3(lo_x, SR_METRO_Y - 1.2f, lo_z), sr_v3(hi_x, SR_METRO_Y, hi_z), beam, SR_LAYER_CONCRETE,
                  0.15f, 0);
    }
    float pl[64][2]; /* pillars every 32 m (they also stand in the street dressing, and collide) */
    int npl = sr_metro_pillars(c, e, pl, 64);
    for (int k = 0; k < npl; k++)
      sr_mesh_box(m, sr_v3(pl[k][0] - 0.6f, 0, pl[k][1] - 0.6f),
                  sr_v3(pl[k][0] + 0.6f, SR_METRO_Y - 1.2f, pl[k][1] + 0.6f), pillar, SR_LAYER_CONCRETE, 0.2f, 0);
  }
}

#define TRAIN_TRAVEL_S 90

int sr_metro_train_state(const SrCity *c, int edge, int64_t t, SrVec3 *pos, SrVec3 *dir) {
  const SrEdge *e = &c->metro_edges[edge];
  float p[3][2];
  sr_metro_route(c, edge, p);
  /* standing at station a with the doors open before the next departure */
  int64_t next = sr_metro_next_train(c, e->a, 0, t);
  if (next >= t && next - t < SR_TRAIN_DWELL_S) {
    *pos = sr_v3(p[0][0], SR_METRO_Y, p[0][1]);
    float dx = p[1][0] - p[0][0], dz = p[1][1] - p[0][1], dl = fabsf(dx) + fabsf(dz);
    *dir = dl > 0 ? sr_v3(dx / dl, 0, dz / dl) : sr_v3(1, 0, 0);
    return 2;
  }
  /* the last train that left station a toward b, from the metro timetable */
  int64_t dep = sr_metro_next_train(c, e->a, 0, t - 240);
  if (dep > t || t - dep >= TRAIN_TRAVEL_S) return 0;
  float f = (float)(t - dep) / (float)TRAIN_TRAVEL_S;
  f = f * f * (3.0f - 2.0f * f); /* pulls away and brakes into the next station */
  float l0 = fabsf(p[1][0] - p[0][0]) + fabsf(p[1][1] - p[0][1]),
        l1 = fabsf(p[2][0] - p[1][0]) + fabsf(p[2][1] - p[1][1]);
  float d = f * (l0 + l1);
  int s = d <= l0 ? 0 : 1;
  float ls = s == 0 ? l0 : l1, u = ls > 0 ? (s == 0 ? d : d - l0) / ls : 0;
  float x = p[s][0] + (p[s + 1][0] - p[s][0]) * u, z = p[s][1] + (p[s + 1][1] - p[s][1]) * u;
  *pos = sr_v3(x, SR_METRO_Y, z);
  float dx = p[s + 1][0] - p[s][0], dz = p[s + 1][1] - p[s][1], dl = fabsf(dx) + fabsf(dz);
  *dir = dl > 0 ? sr_v3(dx / dl, 0, dz / dl) : sr_v3(1, 0, 0);
  return 1;
}

bool sr_metro_train_pos(const SrCity *c, int edge, int64_t t, SrVec3 *pos, SrVec3 *dir) {
  return sr_metro_train_state(c, edge, t, pos, dir) != 0;
}

int sr_build_trains(const SrCity *c, int64_t t, SrVec3 eye, SrMesh *out) {
  int n = 0;
  for (int e = 0; e < c->metro_edge_count; e++) {
    SrVec3 p, d;
    int st = sr_metro_train_state(c, e, t, &p, &d);
    if (!st) continue;
    if (fabsf(p.x - eye.x) > 400.0f || fabsf(p.z - eye.z) > 400.0f) continue;
    float yaw = atan2f(d.x, d.z);
    for (int car = 0; car < 3; car++) { /* three cars, lit windows along both sides */
      SrVec3 cp = sr_v3_sub(p, sr_v3_scale(d, 13.0f * (float)car));
      sr_mesh_part(out, cp, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(-1.4f, 0.0f, -6.0f), sr_v3(1.4f, 3.0f, 6.0f),
                   (SrColor){150, 155, 165, 255}, SR_LAYER_METAL, 0);
      sr_mesh_part(out, cp, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(-1.45f, 1.3f, -5.5f), sr_v3(1.45f, 2.3f, 5.5f),
                   (SrColor){255, 230, 170, 255}, SR_LAYER_WHITE, 220);
      for (int k = -1; k <= 1; k += 2) { /* doors: slid open at the station, light spilling out */
        float slide = st == 2 ? 0.75f : 0.0f;
        for (int side = -1; side <= 1; side += 2) {
          float x = 1.47f * (float)side;
          sr_mesh_part(out, cp, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(x - 0.03f, 0.1f, (float)k * 2.5f - 0.7f - slide),
                       sr_v3(x + 0.03f, 2.6f, (float)k * 2.5f - slide), (SrColor){120, 125, 135, 255}, SR_LAYER_METAL,
                       0);
          sr_mesh_part(out, cp, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(x - 0.03f, 0.1f, (float)k * 2.5f + slide),
                       sr_v3(x + 0.03f, 2.6f, (float)k * 2.5f + 0.7f + slide), (SrColor){120, 125, 135, 255},
                       SR_LAYER_METAL, 0);
          if (st == 2)
            sr_mesh_part(out, cp, yaw, sr_v3(0, 0, 0), 0.0f, sr_v3(x - 0.02f, 0.1f, (float)k * 2.5f - 0.7f),
                         sr_v3(x + 0.02f, 2.5f, (float)k * 2.5f + 0.7f), (SrColor){255, 235, 190, 255}, SR_LAYER_WHITE,
                         240);
        }
      }
    }
    n++;
  }
  return n;
}
