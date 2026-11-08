/* Interior meshes: one mesh per room (floor, ceiling, the walls it owns, furniture) so every
 * room gets its own light selection, plus a per-frame mesh for guards, cameras and effects. */
#include "core/sr_rng.h"
#include "render/sr_render.h"

#include <math.h>
#include <string.h>

#define CELL ((float)SR_INT_CELL_CM / 100.0f)
#define WT   (SR_SITE_WALL_HALF * 2.0f)
#define Y0   SR_SITE_FLOOR_Y
#define H    SR_SITE_WALL_H

static SrColor rgb(int r, int g, int b) { return (SrColor){(uint8_t)r, (uint8_t)g, (uint8_t)b, 255}; }

static SrColor deco(int i) {
  static const SrColor k[8] = {{255, 60, 140, 255}, {60, 220, 255, 255}, {255, 200, 60, 255},  {140, 90, 255, 255},
                               {80, 255, 140, 255}, {255, 110, 40, 255}, {230, 230, 240, 255}, {255, 40, 60, 255}};
  return k[i & 7];
}

static SrColor room_floor(SrRoomType t) {
  switch (t) {
    case SR_ROOM_LOBBY: return rgb(120, 115, 125);
    case SR_ROOM_SECURITY: return rgb(70, 90, 80);
    case SR_ROOM_TARGET:
    case SR_ROOM_SERVER: return rgb(60, 70, 95);
    case SR_ROOM_HOME: return rgb(115, 90, 75);
    default: return rgb(95, 95, 105);
  }
}

static void box(SrMesh *m, float x0, float y0, float z0, float x1, float y1, float z1, SrColor c, int layer, int em) {
  sr_mesh_box(m, sr_v3(x0, y0, z0), sr_v3(x1, y1, z1), c, layer, layer == SR_LAYER_WHITE ? 0.0f : 0.5f, em);
}

/* Walls are split into 1.5 m pieces so per-vertex lighting from the room lights reaches them. */
static void wall_piece_x(SrMesh *m, float x0, float x1, float y0, float y1, float z, SrColor c) {
  int n = (int)((x1 - x0) / 1.5f) + 1;
  for (int i = 0; i < n; i++) {
    float a = x0 + (x1 - x0) * (float)i / (float)n, b = x0 + (x1 - x0) * (float)(i + 1) / (float)n;
    box(m, a, y0, z - WT / 2, b, y1, z + WT / 2, c, SR_LAYER_CONCRETE, 0);
  }
}

static void wall_piece_z(SrMesh *m, float z0, float z1, float y0, float y1, float x, SrColor c) {
  int n = (int)((z1 - z0) / 1.5f) + 1;
  for (int i = 0; i < n; i++) {
    float a = z0 + (z1 - z0) * (float)i / (float)n, b = z0 + (z1 - z0) * (float)(i + 1) / (float)n;
    box(m, x - WT / 2, y0, a, x + WT / 2, y1, b, c, SR_LAYER_CONCRETE, 0);
  }
}

/* Wall along x (horizontal boundary) at z, from x0 to x1, with an optional door gap at xc. */
static void wall_x(SrMesh *m, float x0, float x1, float z, bool gap, float xc, bool slab, SrColor c) {
  float d = SR_SITE_DOOR_HALF;
  if (!gap) {
    wall_piece_x(m, x0, x1, Y0, Y0 + H, z, c);
    return;
  }
  wall_piece_x(m, x0, xc - d, Y0, Y0 + H, z, c);
  wall_piece_x(m, xc + d, x1, Y0, Y0 + H, z, c);
  box(m, xc - d, Y0 + 2.2f, z - WT / 2, xc + d, Y0 + H, z + WT / 2, c, SR_LAYER_CONCRETE, 0);
  if (slab) box(m, xc - d, Y0, z - 0.04f, xc + d, Y0 + 2.2f, z + 0.04f, rgb(150, 50, 50), SR_LAYER_METAL, 0);
}

static void wall_z(SrMesh *m, float z0, float z1, float x, bool gap, float zc, bool slab, SrColor c) {
  float d = SR_SITE_DOOR_HALF;
  if (!gap) {
    wall_piece_z(m, z0, z1, Y0, Y0 + H, x, c);
    return;
  }
  wall_piece_z(m, z0, zc - d, Y0, Y0 + H, x, c);
  wall_piece_z(m, zc + d, z1, Y0, Y0 + H, x, c);
  box(m, x - WT / 2, Y0 + 2.2f, zc - d, x + WT / 2, Y0 + H, zc + d, c, SR_LAYER_CONCRETE, 0);
  if (slab) box(m, x - 0.04f, Y0, zc - d, x + 0.04f, Y0 + 2.2f, zc + d, rgb(150, 50, 50), SR_LAYER_METAL, 0);
}

static void prop_mesh(const SrSite *s, const SrProp *p, SrMesh *m) {
  float x = s->ox + (float)p->x / 100.0f, z = s->oz + (float)p->z / 100.0f;
  SrColor c = deco(p->color);
  switch ((SrPropType)p->type) {
    case SR_PROP_EXIT: {
      bool ceiling = s->in.entries[p->variant].type == SR_ENTRY_ROOF;
      if (ceiling) { /* ladder up to the roof hatch */
        box(m, x - 0.3f, Y0, z - 0.05f, x + 0.3f, Y0 + H, z + 0.05f, rgb(120, 120, 130), SR_LAYER_METAL, 0);
      } else {
        bool along_x = p->facing == 0 || p->facing == 2;
        float hw = s->in.entries[p->variant].type == SR_ENTRY_VENT ? 0.4f : 0.6f;
        float hh = s->in.entries[p->variant].type == SR_ENTRY_VENT ? 0.7f : 2.1f;
        if (along_x)
          box(m, x - hw, Y0, z - 0.06f, x + hw, Y0 + hh, z + 0.06f, rgb(60, 70, 80), SR_LAYER_METAL, 0);
        else
          box(m, x - 0.06f, Y0, z - hw, x + 0.06f, Y0 + hh, z + hw, rgb(60, 70, 80), SR_LAYER_METAL, 0);
      }
      box(m, x - 0.25f, Y0 + 2.35f, z - 0.25f, x + 0.25f, Y0 + 2.5f, z + 0.25f, rgb(40, 255, 90), SR_LAYER_WHITE, 255);
      break;
    }
    case SR_PROP_BED:
      box(m, x - 0.5f, Y0, z - 1.0f, x + 0.5f, Y0 + 0.45f, z + 1.0f, rgb(70, 60, 80), SR_LAYER_WHITE, 0);
      box(m, x - 0.48f, Y0 + 0.45f, z - 0.98f, x + 0.48f, Y0 + 0.6f, z + 0.6f, c, SR_LAYER_WHITE, 0);
      box(m, x - 0.4f, Y0 + 0.45f, z + 0.65f, x + 0.4f, Y0 + 0.62f, z + 0.95f, rgb(220, 220, 230), SR_LAYER_WHITE, 0);
      break;
    case SR_PROP_STASH:
      box(m, x - 0.4f, Y0, z - 0.3f, x + 0.4f, Y0 + 1.6f, z + 0.3f, rgb(90, 100, 110), SR_LAYER_METAL, 0);
      box(m, x - 0.05f, Y0 + 0.8f, z - 0.32f, x + 0.05f, Y0 + 0.9f, z + 0.32f, rgb(255, 180, 40), SR_LAYER_WHITE, 200);
      break;
    case SR_PROP_TERMINAL:
    case SR_PROP_CONSOLE:
    case SR_PROP_DESK: {
      box(m, x - 0.6f, Y0 + 0.7f, z - 0.4f, x + 0.6f, Y0 + 0.78f, z + 0.4f, rgb(80, 70, 65), SR_LAYER_WHITE, 0);
      box(m, x - 0.55f, Y0, z - 0.35f, x - 0.45f, Y0 + 0.7f, z + 0.35f, rgb(60, 55, 50), SR_LAYER_WHITE, 0);
      box(m, x + 0.45f, Y0, z - 0.35f, x + 0.55f, Y0 + 0.7f, z + 0.35f, rgb(60, 55, 50), SR_LAYER_WHITE, 0);
      if (p->type != SR_PROP_DESK) {
        SrColor screen = p->type == SR_PROP_CONSOLE ? rgb(60, 255, 120) : rgb(60, 200, 255);
        box(m, x - 0.3f, Y0 + 0.78f, z - 0.05f, x + 0.3f, Y0 + 1.2f, z + 0.05f, screen, SR_LAYER_WHITE, 255);
      }
      break;
    }
    case SR_PROP_WARDROBE:
      box(m, x - 0.5f, Y0, z - 0.3f, x + 0.5f, Y0 + 2.0f, z + 0.3f, rgb(100, 75, 60), SR_LAYER_WHITE, 0);
      box(m, x - 0.02f, Y0 + 0.2f, z - 0.31f, x + 0.02f, Y0 + 1.9f, z + 0.31f, rgb(40, 30, 25), SR_LAYER_WHITE, 0);
      break;
    case SR_PROP_OBJECTIVE:
      box(m, x - 0.3f, Y0, z - 0.3f, x + 0.3f, Y0 + 0.9f, z + 0.3f, rgb(70, 75, 90), SR_LAYER_METAL, 0);
      if (!s->objective_done)
        box(m, x - 0.15f, Y0 + 0.95f, z - 0.15f, x + 0.15f, Y0 + 1.25f, z + 0.15f, rgb(60, 240, 255), SR_LAYER_WHITE,
            255);
      break;
    case SR_PROP_SHELF:
      box(m, x - 0.5f, Y0, z - 0.2f, x + 0.5f, Y0 + 1.8f, z + 0.2f, rgb(85, 70, 60), SR_LAYER_WHITE, 0);
      for (int k = 0; k < 3; k++)
        box(m, x - 0.4f + 0.25f * (float)k, Y0 + 0.4f + 0.5f * (float)k, z - 0.15f, x - 0.25f + 0.25f * (float)k,
            Y0 + 0.7f + 0.5f * (float)k, z + 0.15f, deco(p->color + k), SR_LAYER_WHITE, 0);
      break;
    case SR_PROP_PLANT:
      box(m, x - 0.2f, Y0, z - 0.2f, x + 0.2f, Y0 + 0.4f, z + 0.2f, rgb(110, 70, 50), SR_LAYER_WHITE, 0);
      box(m, x - 0.3f, Y0 + 0.4f, z - 0.3f, x + 0.3f, Y0 + 1.1f + 0.1f * (float)p->variant, z + 0.3f,
          rgb(40, 140 + 20 * (p->variant & 1), 60), SR_LAYER_WHITE, 0);
      break;
    case SR_PROP_POSTER: {
      bool on_north = p->facing == 2;
      if (on_north)
        box(m, x - 0.4f, Y0 + 1.2f, z - 0.01f, x + 0.4f, Y0 + 2.3f, z + 0.03f, c, SR_LAYER_SIGN, 90);
      else
        box(m, x - 0.01f, Y0 + 1.2f, z - 0.4f, x + 0.03f, Y0 + 2.3f, z + 0.4f, c, SR_LAYER_SIGN, 90);
      break;
    }
    case SR_PROP_LAMP:
      box(m, x - 0.04f, Y0, z - 0.04f, x + 0.04f, Y0 + 1.5f, z + 0.04f, rgb(50, 50, 55), SR_LAYER_WHITE, 0);
      box(m, x - 0.18f, Y0 + 1.5f, z - 0.18f, x + 0.18f, Y0 + 1.75f, z + 0.18f, c, SR_LAYER_WHITE, 255);
      break;
    case SR_PROP_CRATE:
      box(m, x - 0.45f, Y0, z - 0.45f, x + 0.45f, Y0 + 0.8f + 0.2f * (float)(p->variant & 1), z + 0.45f,
          rgb(110, 95, 60), SR_LAYER_METAL, 0);
      break;
    case SR_PROP_RUG:
      sr_mesh_quad(m, sr_v3(x - 1.2f, Y0 + 0.01f, z + 0.8f), sr_v3(x + 1.2f, Y0 + 0.01f, z + 0.8f),
                   sr_v3(x + 1.2f, Y0 + 0.01f, z - 0.8f), sr_v3(x - 1.2f, Y0 + 0.01f, z - 0.8f), 0, 0, 1, 1,
                   (SrColor){(uint8_t)(c.r / 2), (uint8_t)(c.g / 2), (uint8_t)(c.b / 2), 255}, SR_LAYER_WHITE, 0, 0);
      break;
    case SR_PROP_SERVER:
      box(m, x - 0.35f, Y0, z - 0.4f, x + 0.35f, Y0 + 2.1f, z + 0.4f, rgb(30, 35, 45), SR_LAYER_METAL, 0);
      for (int k = 0; k < 6; k++)
        box(m, x - 0.3f, Y0 + 0.3f + 0.3f * (float)k, z - 0.41f, x - 0.2f, Y0 + 0.34f + 0.3f * (float)k, z + 0.41f,
            k % 2 ? rgb(60, 255, 120) : rgb(255, 80, 60), SR_LAYER_WHITE, 255);
      break;
    default: break; /* GCOVR_EXCL_LINE reason: every prop type is handled above */
  }
}

void sr_build_interior_room(const SrSite *s, int room, SrMesh *m) {
  sr_mesh_clear(m);
  const SrInterior *in = &s->in;
  int rx = room % in->w, ry = room / in->w;
  float x0 = s->ox + (float)rx * CELL, z0 = s->oz + (float)ry * CELL, x1 = x0 + CELL, z1 = z0 + CELL;
  SrColor wall = in->apartment ? rgb(175, 165, 160) : rgb(125, 128, 135);
  const int k = 4; /* floor and ceiling tiles for vertex lighting */
  float t = CELL / (float)k;
  for (int i = 0; i < k; i++)
    for (int j = 0; j < k; j++) {
      float a = x0 + t * (float)i, b = z0 + t * (float)j;
      sr_mesh_quad(m, sr_v3(a, Y0, b + t), sr_v3(a + t, Y0, b + t), sr_v3(a + t, Y0, b), sr_v3(a, Y0, b), 0, 0, t / 2,
                   t / 2, room_floor((SrRoomType)in->rooms[room]), SR_LAYER_SIDEWALK, 0, 0);
      sr_mesh_quad(m, sr_v3(a, Y0 + H, b), sr_v3(a + t, Y0 + H, b), sr_v3(a + t, Y0 + H, b + t),
                   sr_v3(a, Y0 + H, b + t), 0, 0, 1, 1, rgb(70, 70, 76), SR_LAYER_CONCRETE, 0, 0);
    }
  /* ceiling light panel */
  box(m, x0 + CELL / 2 - 0.6f, Y0 + H - 0.05f, z0 + CELL / 2 - 0.3f, x0 + CELL / 2 + 0.6f, Y0 + H, z0 + CELL / 2 + 0.3f,
      s->lights_off ? rgb(40, 40, 45) : rgb(255, 240, 210), SR_LAYER_WHITE, s->lights_off ? 0 : 255);
  /* walls this room owns: east and south always, west and north on the outer edge */
  if (rx == 0) wall_z(m, z0, z1, x0, false, 0, false, wall);
  if (ry == 0) wall_x(m, x0, x1, z0, false, 0, false, wall);
  bool e_door = rx + 1 < in->w && (in->doors[room] & 1);
  wall_z(m, z0, z1, x1, e_door, z0 + CELL / 2, false, wall); /* door leaves swing in sr_build_doors */
  bool s_door = ry + 1 < in->h && (in->doors[room] & 2);
  wall_x(m, x0, x1, z1, s_door, x0 + CELL / 2, false, wall);
  /* vent grilles */
  if (in->vents[room] & 1)
    box(m, x1 - 0.12f, Y0 + 0.05f, z0 + CELL / 2 + SR_SITE_VENT_OFFSET - 0.4f, x1 + 0.12f, Y0 + 0.65f,
        z0 + CELL / 2 + SR_SITE_VENT_OFFSET + 0.4f, rgb(40, 45, 50), SR_LAYER_METAL, 0);
  if (in->vents[room] & 2)
    box(m, x0 + CELL / 2 + SR_SITE_VENT_OFFSET - 0.4f, Y0 + 0.05f, z1 - 0.12f,
        x0 + CELL / 2 + SR_SITE_VENT_OFFSET + 0.4f, Y0 + 0.65f, z1 + 0.12f, rgb(40, 45, 50), SR_LAYER_METAL, 0);
  for (int i = 0; i < in->prop_count; i++)
    if (in->props[i].room == room) prop_mesh(s, &in->props[i], m);
}

uint64_t sr_site_mesh_key(const SrSite *s, const SrDeltas *d) {
  uint64_t h = sr_hash_combine((uint64_t)s->building, sr_interior_digest(&s->in));
  for (int i = 0; i < s->in.device_count; i++)
    h = sr_hash_combine(h, (uint64_t)s->in.devices[i].locked * 2u + (uint64_t)sr_site_device_disabled(s, d, i));
  return sr_hash_combine(h, (uint64_t)s->lights_off * 3u + (uint64_t)s->objective_done);
}

void sr_build_site_dynamic(const SrSite *s, const SrDeltas *d, float t, SrMesh *m) {
  sr_mesh_clear(m);
  /* cameras and turrets: red eye while live */
  for (int i = 0; i < s->in.device_count; i++) {
    SrDeviceType ty = s->in.devices[i].type;
    if (ty != SR_DEV_CAMERA && ty != SR_DEV_TURRET) continue;
    SrVec3 p = sr_site_camera_pos(s, i);
    bool live = !sr_site_device_disabled(s, d, i);
    float sz = ty == SR_DEV_TURRET ? 0.3f : 0.18f;
    box(m, p.x - sz, p.y - sz, p.z - sz, p.x + sz, p.y + sz, p.z + sz, rgb(50, 50, 55), SR_LAYER_METAL, 0);
    if (live)
      box(m, p.x - 0.05f, p.y - 0.05f, p.z + sz, p.x + 0.05f, p.y + 0.05f, p.z + sz + 0.04f, rgb(255, 30, 30),
          SR_LAYER_WHITE, 255);
  }
  for (int i = 0; i < s->effect_count; i++) {
    const SrSiteEffect *e = &s->effects[i];
    if (!e->active) continue;
    SrVec3 p = e->pos;
    switch (e->type) {
      case SR_FX_SMOKE:
        for (int k = 0; k < 6; k++) {
          float a = (float)k * 1.05f + t * 0.2f, r = e->radius * 0.6f;
          float cx = p.x + cosf(a) * r * 0.5f, cz = p.z + sinf(a) * r * 0.5f;
          box(m, cx - r * 0.5f, Y0, cz - r * 0.5f, cx + r * 0.5f, Y0 + 2.6f, cz + r * 0.5f,
              (SrColor){170, 170, 175, 200}, SR_LAYER_WHITE, 40);
        }
        break;
      case SR_FX_DECOY: { /* a flickering hologram of the player */
        SrAppearance a;
        memset(&a, 0, sizeof a);
        a.outfit_color = 15;
        int v0 = m->nv;
        sr_build_character(&a, sr_v3(p.x, Y0, p.z), sr_v3(0, 0, -1), false, m);
        for (int v = v0; v < m->nv; v++) {
          m->v[v].emissive = 200;
          m->v[v].b = 255;
          m->v[v].a = (uint8_t)(150 + 60 * (int)(sinf(t * 20.0f) > 0));
        }
        break;
      }
      default:
        box(m, p.x - 0.2f, Y0, p.z - 0.2f, p.x + 0.2f, Y0 + 0.3f, p.z + 0.2f, rgb(60, 60, 70), SR_LAYER_METAL, 0);
        box(m, p.x - 0.05f, Y0 + 0.3f, p.z - 0.05f, p.x + 0.05f, Y0 + 0.4f, p.z + 0.05f, rgb(255, 200, 40),
            SR_LAYER_WHITE, 255);
        break;
    }
  }
}

void sr_build_doors(const SrSite *s, float *angle, float dt, bool snap, SrMesh *m) {
  /* each door leaf swings about its hinge toward its state over 0.3 s (angle 0 shut, 1 open) */
  const SrInterior *in = &s->in;
  float d = SR_SITE_DOOR_HALF, step = dt / 0.3f;
  for (int room = 0; room < in->w * in->h; room++) {
    int rx = room % in->w, ry = room / in->w;
    float x0 = s->ox + (float)rx * CELL, z0 = s->oz + (float)ry * CELL, x1 = x0 + CELL, z1 = z0 + CELL;
    for (int dir = 0; dir < 2; dir++) {
      bool has = dir == 0 ? rx + 1 < in->w && (in->doors[room] & 1) : ry + 1 < in->h && (in->doors[room] & 2);
      if (!has) continue;
      float want = sr_site_door_open(s, room, dir) ? 1.0f : 0.0f, *a = &angle[room * 2 + dir];
      *a = snap ? want : *a + sr_clampf(want - *a, -step, step);
      float sw = *a * SR_PI * 0.5f;
      if (dir == 0) /* a door in the east wall: hinged at its north edge, opening east */
        sr_mesh_part(m, sr_v3(x1, Y0, z0 + CELL / 2 - d), -SR_PI * 0.5f + sw, sr_v3(0, 0, 0), 0, sr_v3(0, 0, -0.04f),
                     sr_v3(2 * d, 2.2f, 0.04f), rgb(150, 50, 50), SR_LAYER_METAL, 0);
      else /* a door in the south wall: hinged at its west edge, opening south */
        sr_mesh_part(m, sr_v3(x0 + CELL / 2 - d, Y0, z1), -sw, sr_v3(0, 0, 0), 0, sr_v3(0, 0, -0.04f),
                     sr_v3(2 * d, 2.2f, 0.04f), rgb(150, 50, 50), SR_LAYER_METAL, 0);
    }
  }
}

/* The animation input of a guard: weapon, stance by alert state, hits, falls and status poses. */
void sr_guard_anim(const SrSite *s, const SrGuard *g, const SrAppearance *a, double now, SrVec3 player,
                   SrAnimInput *in) {
  memset(in, 0, sizeof *in);
  in->key = 0x6A4Du ^ ((uint64_t)g->id << 8);
  in->look = a;
  in->role = SR_ROLE_GUARD;
  in->pos = sr_v3(g->e.pos.x, Y0, g->e.pos.z);
  in->dir = sr_dir_from_yaw_pitch(g->e.yaw, 0);
  in->speed = g->speed;
  in->walk = g->walk;
  in->once_t0 = -1;
  bool rifle = g->id % 3u != 0;
  in->prop_r = (uint8_t)(rifle ? SR_HP_RIFLE : SR_HP_PISTOL);
  float yaw = atan2f(in->dir.x, in->dir.z), rel = sr_wrap_angle(g->hit_dir - yaw);
  if (g->e.dead || g->e.ko) {
    if (g->e.restrained) {
      in->base = SR_CLIP_RESTRAINED;
      in->prop_r = SR_HP_NONE;
      return;
    }
    /* topple away from the hit: shot from the front falls back, from behind forward */
    in->once = (uint8_t)(g->e.ko             ? SR_CLIP_SLUMP
                         : fabsf(rel) < 0.8f ? SR_CLIP_DIE_BACK
                         : fabsf(rel) > 2.3f ? SR_CLIP_DIE_FRONT
                                             : SR_CLIP_DIE_SIDE);
    in->mirror = rel < 0;
    in->base = in->once;
    in->once_t0 = g->down_time > 0 ? (double)g->down_time : -1.0;
    in->prop_r = SR_HP_NONE;
    return;
  }
  if (g->e.surrendered) {
    in->base = SR_CLIP_SURRENDER;
    in->prop_r = SR_HP_NONE;
    return;
  }
  if (g->stun > 0) {
    in->base = SR_CLIP_STUNNED;
    in->prop_r = SR_HP_NONE;
    return;
  }
  in->base = SR_CLIP_IDLE;
  switch (g->e.alert) {
    case SR_ALERT_UNAWARE:
      in->upper = rifle ? SR_CLIP_SLUNG : SR_CLIP_NONE;
      if (!rifle) in->prop_r = SR_HP_NONE; /* holstered */
      if (g->pause > 0) in->base = SR_CLIP_LOOK_AROUND;
      break;
    case SR_ALERT_SUSPICIOUS:
      in->upper = SR_CLIP_READY;
      in->look_at = true;
      in->target = g->e.investigate;
      break;
    case SR_ALERT_SEARCHING:
      in->upper = SR_CLIP_READY;
      if (g->speed < 0.3f) in->base = SR_CLIP_LOOK_AROUND;
      break;
    default: /* combat: aim, strafe, or crouch and peek out from behind cover */
      in->upper = rifle ? SR_CLIP_AIM : SR_CLIP_AIM_PISTOL;
      in->look_at = true;
      in->target = player;
      if (g->speed < 0.3f && g->id % 2u == 0) {
        in->base = rifle ? SR_CLIP_PEEK : SR_CLIP_CROUCH_AIM;
        in->upper = SR_CLIP_NONE;
      }
      break;
  }
  if (g->hit_kind && now - g->hit_time < 1.0) { /* flinch away from the shot */
    in->once = g->hit_kind == 3 ? SR_CLIP_HEADSHOT : g->hit_kind == 2 ? SR_CLIP_STAGGER : SR_CLIP_FLINCH;
    in->once_t0 = g->hit_time;
    in->mirror = rel > 0;
  }
  (void)s;
}

static void drone_mesh(SrMesh *m, const SrGuard *g, double now) {
  /* a hovering drone: bobs, tilts toward its travel, spinning rotor discs */
  float bob = 0.1f * sinf((float)now * 3.1f + (float)g->id);
  float tilt = SR_MIN(0.35f, g->speed * 0.12f);
  SrVec3 p = sr_v3(g->e.pos.x, Y0 + 1.9f + bob, g->e.pos.z);
  float yaw = g->e.yaw + SR_PI;
  sr_mesh_part(m, p, yaw, sr_v3(0, 0, 0), tilt, sr_v3(-0.25f, -0.1f, -0.25f), sr_v3(0.25f, 0.1f, 0.25f),
               rgb(60, 62, 70), SR_LAYER_METAL, 0);
  for (int k = 0; k < 4; k++) {
    float spin = (float)now * 40.0f + (float)k;
    float rx = k & 1 ? 0.32f : -0.32f, rz = k & 2 ? 0.32f : -0.32f;
    SrVec3 c = sr_v3(p.x + rx * cosf(yaw) + rz * sinf(yaw), p.y + 0.12f, p.z - rx * sinf(yaw) + rz * cosf(yaw));
    sr_mesh_part(m, c, spin, sr_v3(0, 0, 0), 0, sr_v3(-0.16f, 0, -0.02f), sr_v3(0.16f, 0.01f, 0.02f),
                 rgb(150, 150, 160), SR_LAYER_WHITE, 40);
  }
  sr_mesh_part(m, p, yaw, sr_v3(0, 0, 0), 0, sr_v3(-0.05f, -0.14f, 0.2f), sr_v3(0.05f, -0.08f, 0.26f),
               g->e.alert == SR_ALERT_COMBAT ? rgb(255, 40, 40) : rgb(40, 220, 255), SR_LAYER_WHITE, 255);
}

void sr_build_guards(const SrSite *s, SrAnimCache *anim, double now, SrVec3 player, SrMesh *m) {
  sr_mesh_clear(m);
  for (int i = 0; i < s->guard_count; i++) {
    const SrGuard *g = &s->guards[i];
    if (g->e.drone) {
      if (!g->e.dead) drone_mesh(m, g, now);
      continue;
    }
    SrAppearance a = sr_appearance_generate(g->id, SR_ROLE_GUARD, 0);
    SrAnimInput in;
    sr_guard_anim(s, g, &a, now, player, &in);
    sr_anim_character(anim, &in, m);
    SrVec3 foot = in.pos;
    if (g->e.dead && g->down_time != 0) { /* the blood pool spreads over two seconds */
      float grow = g->down_time > 0 ? sr_clampf((float)(now - g->down_time) / 2.0f, 0.15f, 1.0f) : 1.0f;
      float r = 0.6f * grow;
      SrVec3 back = sr_v3_scale(in.dir, -0.9f);
      box(m, foot.x + back.x - r, Y0 + 0.005f, foot.z + back.z - r, foot.x + back.x + r, Y0 + 0.01f,
          foot.z + back.z + r, rgb(90, 10, 15), SR_LAYER_WHITE, 0);
    }
    if (!(g->e.dead || g->e.ko) && (g->tagged || g->e.alert == SR_ALERT_COMBAT)) { /* marker above the head */
      SrColor c = g->e.alert == SR_ALERT_COMBAT ? rgb(255, 40, 40) : rgb(255, 200, 40);
      box(m, foot.x - 0.08f, Y0 + 2.2f, foot.z - 0.08f, foot.x + 0.08f, Y0 + 2.4f, foot.z + 0.08f, c, SR_LAYER_WHITE,
          255);
    }
  }
}

/* Enemy fire: a flash at the muzzle for a tenth of a second and a tracer every third round. */
int sr_build_enemy_fire(const SrSite *s, double now, SrVec3 player, SrVec3 eye, SrMesh *fx, SrLight *lights, int nl,
                        int max_lights) {
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int i = 0; i < s->guard_count; i++) {
    const SrGuard *g = &s->guards[i];
    if (g->shot_time <= 0 || now - g->shot_time > 0.1 || g->e.dead || g->e.ko) continue;
    SrVec3 fwd = sr_dir_from_yaw_pitch(g->e.yaw, 0);
    SrVec3 muzzle = sr_v3(g->e.pos.x + fwd.x * 0.75f, Y0 + 1.35f, g->e.pos.z + fwd.z * 0.75f);
    sr_mesh_sprite(fx, &bb, muzzle, 0.6f, 0.6f, SR_SPR_FLARE, SR_SPRITE_FACING, 0, rgb(255, 210, 120), 255);
    if (g->shot_tracer) {
      SrVec3 d = sr_v3_sub(player, muzzle);
      float len = sr_v3_len(d);
      for (int k = 1; k < 8 && len > 0.5f; k++) {
        SrVec3 p = sr_v3_add(muzzle, sr_v3_scale(d, (float)k / 8.0f));
        sr_mesh_sprite(fx, &bb, p, 0.12f, 0.12f, SR_SPR_GLOW, SR_SPRITE_FACING, 0, rgb(255, 190, 90), 255);
      }
    }
    if (nl < max_lights) { /* the flash lights the room for a moment */
      SrLight *l = &lights[nl++];
      l->pos = muzzle;
      l->radius = 7.0f;
      l->r = 1.6f;
      l->g = 1.2f;
      l->b = 0.6f;
    }
  }
  return nl;
}

int sr_site_collect_lights(const SrSite *s, SrLight *out, int max) {
  int n = 0;
  for (int r = 0; r < s->in.w * s->in.h && n < max; r++) {
    if (s->lights_off) break;
    SrVec3 c = sr_site_room_center(s, r);
    SrLight *l = &out[n++];
    l->pos = sr_v3(c.x, Y0 + H - 0.3f, c.z);
    l->radius = 9.5f;
    SrRoomType t = (SrRoomType)s->in.rooms[r];
    l->r = t == SR_ROOM_TARGET || t == SR_ROOM_SERVER ? 0.5f : 1.0f;
    l->g = t == SR_ROOM_SECURITY ? 1.0f : 0.85f;
    l->b = t == SR_ROOM_TARGET || t == SR_ROOM_SERVER ? 1.0f : 0.65f;
  }
  /* lamps, screens and posters add coloured light */
  for (int i = 0; i < s->in.prop_count && n < max; i++) {
    const SrProp *p = &s->in.props[i];
    if (p->type != SR_PROP_LAMP && p->type != SR_PROP_TERMINAL && p->type != SR_PROP_POSTER) continue;
    SrColor c = p->type == SR_PROP_TERMINAL ? rgb(60, 200, 255) : deco(p->color);
    SrLight *l = &out[n++];
    l->pos = sr_v3(s->ox + (float)p->x / 100.0f, Y0 + 1.6f, s->oz + (float)p->z / 100.0f);
    l->radius = p->type == SR_PROP_LAMP ? 5.0f : 3.0f;
    l->r = (float)c.r / 255.0f * 0.8f;
    l->g = (float)c.g / 255.0f * 0.8f;
    l->b = (float)c.b / 255.0f * 0.8f;
  }
  return n;
}

void sr_mesh_thermal(SrMesh *m, int from) {
  for (int v = from; v < m->nv; v++) {
    SrVertex *x = &m->v[v];
    int heat = (x->r + x->g + x->b) / 3;
    x->r = 255;
    x->g = (uint8_t)(60 + heat / 2);
    x->b = 20;
    x->a = 255;
    x->layer = SR_LAYER_WHITE;
    x->emissive = 255;
  }
}
