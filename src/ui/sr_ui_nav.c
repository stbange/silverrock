/* Navigation and guidance in the UI: the city map, the help screen, the destination on the
 * compass, the next turn, the current goal, tips and the minimap. */
#include "ui/sr_ui.h"

#include "world/sr_nav.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const SrColor k_white = {235, 240, 255, 255};
static const SrColor k_cyan = {60, 230, 255, 255};
static const SrColor k_amber = {255, 190, 60, 255};
static const SrColor k_red = {255, 60, 70, 255};
static const SrColor k_dim = {150, 160, 180, 255};
static const SrColor k_panel = {8, 10, 18, 215};

static void text_cap(SrUi *ui, float x, float y, const char *t, SrColor c, int s, float max_w) {
  /* shrinks the scale until the text fits (down to 1) */
  while (s > 1 && (float)sr_ui_text_width(t, s) > max_w) s--;
  char buf[160];
  snprintf(buf, sizeof buf, "%s", t);
  int maxc = (int)(max_w / (6.0f * (float)s));
  if (maxc >= 0 && maxc < (int)sizeof buf && (int)strlen(buf) > maxc) buf[maxc] = '\0';
  sr_ui_text(ui, x, y, buf, c, s);
}

/* A line of width w as a quad. */
static void line(SrUi *ui, float x0, float y0, float x1, float y1, float w, SrColor c) {
  float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
  if (l < 0.01f) return;
  float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
  sr_mesh_quad(&ui->mesh, sr_v3(x0 + nx, y0 + ny, 0), sr_v3(x1 + nx, y1 + ny, 0), sr_v3(x1 - nx, y1 - ny, 0),
               sr_v3(x0 - nx, y0 - ny, 0), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
}

/* A filled triangle (a degenerate quad). */
static void tri(SrUi *ui, float ax, float ay, float bx, float by, float cx, float cy, SrColor c) {
  sr_mesh_quad(&ui->mesh, sr_v3(ax, ay, 0), sr_v3(bx, by, 0), sr_v3(cx, cy, 0), sr_v3(cx, cy, 0), 0, 0, 1, 1, c,
               SR_LAYER_WHITE, 255, 0);
}

static void diamond(SrUi *ui, float x, float y, float r, SrColor c) {
  tri(ui, x, y - r, x + r, y, x, y + r, c);
  tri(ui, x, y - r, x - r, y, x, y + r, c);
}

/* Clips a segment to a box (Liang-Barsky); false when nothing is left. */
static bool clip(float *x0, float *y0, float *x1, float *y1, float bx0, float by0, float bx1, float by1) {
  float t0 = 0, t1 = 1, dx = *x1 - *x0, dy = *y1 - *y0;
  float p[4] = {-dx, dx, -dy, dy}, q[4] = {*x0 - bx0, bx1 - *x0, *y0 - by0, by1 - *y0};
  for (int i = 0; i < 4; i++) {
    if (fabsf(p[i]) < 1e-6f) {
      if (q[i] < 0) return false;
      continue;
    }
    float r = q[i] / p[i];
    if (p[i] < 0) {
      if (r > t1) return false;
      if (r > t0) t0 = r;
    } else {
      if (r < t0) return false;
      if (r < t1) t1 = r;
    }
  }
  float ox = *x0, oy = *y0;
  *x0 = ox + t0 * dx;
  *y0 = oy + t0 * dy;
  *x1 = ox + t1 * dx;
  *y1 = oy + t1 * dy;
  return true;
}

static SrColor dim(SrColor c, int pct) {
  return (SrColor){(uint8_t)(c.r * pct / 100), (uint8_t)(c.g * pct / 100), (uint8_t)(c.b * pct / 100), c.a};
}

/* ---- the city map ------------------------------------------------------------------------ */

#define MAP_BUTTONS 5
static const char *const k_map_buttons[MAP_BUTTONS] = {"ZOOM +", "ZOOM -", "FILTER", "CLEAR", "CLOSE"};

static void map_button_rect(const SrUi *ui, int i, float *x, float *y, float *w, float *h) {
  float bw = (float)ui->w / (float)MAP_BUTTONS;
  *w = bw - 6;
  *h = (float)ui->h * 0.09f;
  *x = (float)i * bw + 3;
  *y = (float)ui->h - *h - 3;
}

int sr_ui_map_button_at(const SrUi *ui, float x, float y) {
  for (int i = 0; i < MAP_BUTTONS; i++) {
    float bx, by, bw, bh;
    map_button_rect(ui, i, &bx, &by, &bw, &bh);
    if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) return i + 1;
  }
  return 0;
}

void sr_ui_map_screen(SrUi *ui, const SrGame *g, const SrInput *in) {
  const SrCity *c = g->city;
  const SrMapView *v = &g->map;
  float W = (float)ui->w, H = (float)ui->h, aspect = W / H;
  int s = ui->scale;
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  sr_ui_rect(ui, 0, 0, W, H, (SrColor){6, 8, 14, 255});
  float B = (float)SR_BLOCK_CM / 100.0f, ST = (float)SR_STREET_CM / 100.0f;
#define TO_SCREEN(wx, wz, sx, sy)                                                                                      \
  do {                                                                                                                 \
    float nx_, ny_;                                                                                                    \
    sr_map_screen_of(v, aspect, (wx), (wz), &nx_, &ny_);                                                               \
    *(sx) = nx_ * W;                                                                                                   \
    *(sy) = ny_ * H;                                                                                                   \
  } while (0)
  /* blocks: streets dark, lots in the district's colour, water blue, highways amber */
  for (int bi = 0; bi < c->grid * c->grid; bi++) {
    int32_t ox, oy;
    sr_city_block_origin(bi, c->grid, &ox, &oy);
    float x0, y0, x1, y1, lx, ly;
    TO_SCREEN((float)ox / 100.0f, (float)oy / 100.0f, &x0, &y0);
    TO_SCREEN((float)ox / 100.0f + B, (float)oy / 100.0f + B, &x1, &y1);
    if (x1 < 0 || y1 < 0 || x0 > W || y0 > H) continue;
    const SrBlock *b = &c->blocks[bi];
    if (b->water) {
      sr_ui_rect(ui, x0, y0, x1 - x0, y1 - y0, (SrColor){14, 30, 60, 255});
      continue;
    }
    sr_ui_rect(ui, x0, y0, x1 - x0, y1 - y0, (SrColor){26, 28, 36, 255});
    TO_SCREEN((float)ox / 100.0f + ST, (float)oy / 100.0f + ST, &lx, &ly);
    const SrPalette *pal = sr_palette(c->districts[b->district].type);
    SrColor lot = dim(pal->neon[0], 22);
    lot.r = (uint8_t)SR_MIN(255, lot.r + 18);
    lot.g = (uint8_t)SR_MIN(255, lot.g + 18);
    lot.b = (uint8_t)SR_MIN(255, lot.b + 22);
    sr_ui_rect(ui, lx, ly, x1 - lx, y1 - ly, lot);
    if (b->highway) sr_ui_rect(ui, x0, y0 + (ly - y0) * 0.35f, x1 - x0, (ly - y0) * 0.3f, dim(k_amber, 70));
    if (v->span < 1400.0f) /* buildings once zoomed in */
      for (int i = 0; i < b->building_count; i++) {
        const SrBuilding *bd = &c->buildings[b->first_building + i];
        float bx0, by0, bx1, by1;
        TO_SCREEN((float)bd->x0 / 100.0f, (float)bd->y0 / 100.0f, &bx0, &by0);
        TO_SCREEN((float)bd->x1 / 100.0f, (float)bd->y1 / 100.0f, &bx1, &by1);
        sr_ui_rect(ui, bx0 + 0.5f, by0 + 0.5f, bx1 - bx0 - 1, by1 - by0 - 1, dim(lot, 140));
      }
  }
  /* the metro: lines between stations */
  for (int e = 0; e < c->metro_edge_count; e++) {
    float p[3][2];
    sr_metro_route(c, e, p);
    for (int k = 0; k < 2; k++) {
      float ax, ay, bx, by;
      TO_SCREEN(p[k][0], p[k][1], &ax, &ay);
      TO_SCREEN(p[k + 1][0], p[k + 1][1], &bx, &by);
      line(ui, ax, ay, bx, by, 2.0f * (float)s, (SrColor){150, 70, 230, 200});
    }
  }
  /* district names when zoomed out */
  if (v->span > 1200.0f)
    for (int d = 0; d < c->district_count; d++) {
      float sx = 0, sz = 0;
      int n = 0;
      for (int bi = 0; bi < c->grid * c->grid; bi++)
        if (c->blocks[bi].district == d && !c->blocks[bi].water) {
          int32_t ox, oy;
          sr_city_block_origin(bi, c->grid, &ox, &oy);
          sx += (float)ox / 100.0f + B * 0.5f;
          sz += (float)oy / 100.0f + B * 0.5f;
          n++;
        }
      if (!n) continue;
      float x, y;
      TO_SCREEN(sx / (float)n, sz / (float)n, &x, &y);
      const char *name = c->districts[d].name;
      float nw = (float)sr_ui_text_width(name, s);
      sr_ui_rect(ui, x - nw * 0.5f - 2, y - 2, nw + 4, 7.0f * (float)s + 4, (SrColor){6, 8, 14, 170});
      sr_ui_text(ui, x - nw * 0.5f, y, name, dim(k_white, 85), s);
    }
  /* places of the filter, home, stations: icons up close, coloured dots for everything far out */
  float isz = (v->span < 900.0f ? 9.0f : 6.0f) * (float)s;
  bool dots = v->span > 1600.0f && v->filter == SR_MAPF_ALL;
  for (int b = 0; b < c->building_count; b++) {
    const SrBuilding *bd = &c->buildings[b];
    bool home = b == g->player.home;
    if (!home && !sr_map_shows_poi(v->filter, bd->poi)) continue;
    int32_t cx, cy;
    sr_building_center(bd, &cx, &cy);
    float x, y;
    TO_SCREEN((float)cx / 100.0f, (float)cy / 100.0f, &x, &y);
    if (x < -isz || y < -isz || x > W + isz || y > H + isz) continue;
    if (dots && !home) {
      SrColor dc = sr_map_shows_poi(SR_MAPF_FOOD, bd->poi)       ? (SrColor){240, 170, 60, 230}
                   : sr_map_shows_poi(SR_MAPF_WORK, bd->poi)     ? (SrColor){240, 70, 70, 230}
                   : sr_map_shows_poi(SR_MAPF_SERVICES, bd->poi) ? (SrColor){80, 220, 240, 230}
                                                                 : (SrColor){180, 180, 190, 200};
      sr_ui_rect(ui, x - (float)s, y - (float)s, 2.0f * (float)s, 2.0f * (float)s, dc);
      continue;
    }
    sr_ui_icon(ui, x - isz / 2, y - isz / 2, isz, home ? SR_POI_HOME : bd->poi, home ? k_amber : k_white);
  }
  if (v->filter == SR_MAPF_ALL || v->filter == SR_MAPF_TRAVEL)
    for (int st = 0; st < c->station_count; st++) {
      SrVec3 e = sr_station_entrance(g, st);
      float x, y;
      TO_SCREEN(e.x, e.z, &x, &y);
      sr_ui_rect(ui, x - isz / 2, y - isz / 2, isz, isz, (SrColor){150, 70, 230, 255});
      sr_ui_text(ui, x - 2.5f * (float)s, y - 3.5f * (float)s, "M", k_white, s);
    }
  /* the active job's target */
  for (int i = 0; i < g->jobs.count && g->active_job >= 0; i++) {
    const SrJob *j = &g->jobs.jobs[i];
    if (j->id != g->active_job) continue;
    int32_t cx, cy;
    sr_building_center(&c->buildings[j->executed ? j->client_building : j->target_building], &cx, &cy);
    float x, y;
    TO_SCREEN((float)cx / 100.0f, (float)cy / 100.0f, &x, &y);
    diamond(ui, x, y, 6.0f * (float)s, k_red);
    sr_ui_text(ui, x + 7.0f * (float)s, y - 3.5f * (float)s, "JOB", k_red, s);
  }
  /* the route and the destination */
  const SrRoute *r = &g->route;
  for (int i = 1; i < r->n; i++) {
    float ax, ay, bx, by;
    TO_SCREEN(r->pts[i - 1].x, r->pts[i - 1].z, &ax, &ay);
    TO_SCREEN(r->pts[i].x, r->pts[i].z, &bx, &by);
    line(ui, ax, ay, bx, by, 2.5f * (float)s, k_cyan);
  }
  if (r->kind != SR_DEST_NONE) {
    float x, y;
    TO_SCREEN(r->target.x, r->target.z, &x, &y);
    diamond(ui, x, y, 5.0f * (float)s, k_cyan);
  }
  /* the player: an arrow in the direction of view */
  {
    float x, y;
    TO_SCREEN(g->player.pos.x, g->player.pos.z, &x, &y);
    float fx = -sinf(g->player.yaw), fz = -cosf(g->player.yaw), L = 8.0f * (float)s;
    tri(ui, x + fx * L, y + fz * L, x - fz * L * 0.5f - fx * L * 0.4f, y + fx * L * 0.5f - fz * L * 0.4f,
        x + fz * L * 0.5f - fx * L * 0.4f, y - fx * L * 0.5f - fz * L * 0.4f, k_amber);
  }
#undef TO_SCREEN
  /* the cursor (keyboard and gamepad pick at the centre) */
  if (!touch) {
    sr_ui_rect(ui, W / 2 - 6.0f * (float)s, H / 2 - 0.5f * (float)s, 12.0f * (float)s, (float)s, k_white);
    sr_ui_rect(ui, W / 2 - 0.5f * (float)s, H / 2 - 6.0f * (float)s, (float)s, 12.0f * (float)s, k_white);
  }
  /* header and footer */
  float m = 6.0f * (float)s;
  char buf[160];
  sr_ui_rect(ui, 0, 0, W, 12.0f * (float)s, k_panel);
  snprintf(buf, sizeof buf, "CITY MAP  //  %s  //  %d M ACROSS", sr_map_filter_name(v->filter),
           (int)(v->span * aspect));
  text_cap(ui, m, 3.0f * (float)s, buf, k_cyan, s, W - 2 * m);
  if (r->kind != SR_DEST_NONE) {
    snprintf(buf, sizeof buf, "DESTINATION: %s, %d M", r->label, (int)sr_route_distance(g));
    sr_ui_rect(ui, 0, 12.0f * (float)s, W, 10.0f * (float)s, k_panel);
    text_cap(ui, m, 13.5f * (float)s, buf, k_white, s, W - 2 * m);
  }
  if (touch) {
    for (int i = 0; i < MAP_BUTTONS; i++) {
      float bx, by, bw, bh;
      map_button_rect(ui, i, &bx, &by, &bw, &bh);
      sr_ui_rect(ui, bx, by, bw, bh, k_panel);
      text_cap(ui, bx + 4, by + bh / 2 - 3.5f * (float)s, k_map_buttons[i], k_white, s, bw - 8);
    }
  } else {
    sr_ui_rect(ui, 0, H - 11.0f * (float)s, W, 11.0f * (float)s, k_panel);
    text_cap(ui, m, H - 9.0f * (float)s, "WASD PAN  SPACE/C ZOOM  E GO HERE  R FILTER  Q CLEAR ROUTE  M CLOSE", k_amber,
             s, W - 2 * m);
  }
}

/* ---- help -------------------------------------------------------------------------------- */

int sr_ui_help_row_at(const SrUi *ui, float y) {
  float top = (float)ui->h * 0.18f, row = (float)ui->h * 0.08f;
  int r = (int)((y - top) / row);
  return y >= top && r >= 0 && r < SR_HELP_PAGES ? r : -1;
}

/* Draws text wrapped at spaces to a width; returns the y under the last line. */
static float wrapped(SrUi *ui, bool draw, float x, float y, const char *txt, SrColor c, int s, float width) {
  char buf[200];
  const char *p = txt;
  float lh = 11.0f * (float)s;
  while (*p) {
    int n = (int)strlen(p), take, last_space = -1;
    for (int i = 0; i <= n && i < (int)sizeof buf - 1; i++) {
      if (i == n || p[i] == ' ') {
        memcpy(buf, p, (size_t)i);
        buf[i] = '\0';
        if ((float)sr_ui_text_width(buf, s) > width && last_space > 0) break;
        last_space = i;
      }
    }
    take = last_space > 0 ? last_space : SR_MIN(n, (int)sizeof buf - 1);
    memcpy(buf, p, (size_t)take);
    buf[take] = '\0';
    if (draw) text_cap(ui, x, y, buf, c, s, width);
    y += lh;
    p += take;
    while (*p == ' ') p++;
  }
  return y;
}

static float help_settings_top(const SrUi *ui, const SrGame *g, bool touch) {
  const char *const *lines = sr_help_lines(g->help_page, touch);
  float W = (float)ui->w, m = 8.0f * (float)ui->scale, split = W * 0.3f, y = (float)ui->h * 0.18f;
  for (int i = 0; lines[i]; i++)
    y = wrapped((SrUi *)ui, false, split + m, y, lines[i], k_white, ui->scale, W - split - 2 * m);
  return y + 6.0f * (float)ui->scale;
}

int sr_ui_help_setting_at(const SrUi *ui, const SrGame *g, bool touch, float y) {
  if (g->help_page != SR_HELP_PAGES - 1) return -1;
  float top = help_settings_top(ui, g, touch), line = 11.0f * (float)ui->scale;
  int r = (int)((y - top + 2) / line);
  return y >= top - 2 && r < 3 ? r : -1;
}

void sr_ui_help_screen(SrUi *ui, const SrGame *g, const SrInput *in) {
  float W = (float)ui->w, H = (float)ui->h;
  int s = ui->scale;
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  sr_ui_rect(ui, 0, 0, W, H, (SrColor){4, 6, 12, 245});
  float m = 8.0f * (float)s, top = H * 0.18f, row = H * 0.08f, split = W * 0.3f;
  text_cap(ui, m, H * 0.06f, "HELP AND GUIDANCE", k_cyan, s * 2, W - 2 * m);
  for (int p = 0; p < SR_HELP_PAGES; p++) {
    bool sel = p == g->help_page;
    if (sel) sr_ui_rect(ui, m - 2, top + row * (float)p - 2, split - m, row - 2, (SrColor){20, 60, 80, 220});
    text_cap(ui, m, top + row * (float)p + row * 0.25f, sr_help_title(p), sel ? k_cyan : k_dim, s, split - m - 6);
  }
  const char *const *lines = sr_help_lines(g->help_page, touch);
  float y = top;
  for (int i = 0; lines[i]; i++) y = wrapped(ui, true, split + m, y, lines[i], k_white, s, W - split - 2 * m);
  if (g->help_page == SR_HELP_PAGES - 1) { /* the settings and their state */
    char buf[96];
    y = help_settings_top(ui, g, touch);
    snprintf(buf, sizeof buf, "TIPS AND GOAL: %s", g->settings.guidance ? "ON" : "OFF");
    text_cap(ui, split + m, y, buf, g->settings.guidance ? k_cyan : k_dim, s, W - split - 2 * m);
    snprintf(buf, sizeof buf, "MINIMAP: %s", g->settings.minimap ? "ON" : "OFF");
    text_cap(ui, split + m, y + 11.0f * (float)s, buf, g->settings.minimap ? k_cyan : k_dim, s, W - split - 2 * m);
    snprintf(buf, sizeof buf, "ROUTE ON THE PAVEMENT: %s", g->settings.nav_path ? "ON" : "OFF");
    text_cap(ui, split + m, y + 22.0f * (float)s, buf, g->settings.nav_path ? k_cyan : k_dim, s, W - split - 2 * m);
  }
  text_cap(ui, m, H - 12.0f * (float)s,
           touch ? "TAP A TOPIC  //  ON THE LAST PAGE TAP A SETTING  //  TAP THE TITLE TO GO BACK"
                 : "UP/DOWN TOPIC  G TIPS  N MINIMAP  P ROUTE  R RESET TIPS  ESC BACK",
           k_amber, s, W - 2 * m);
}

/* ---- HUD --------------------------------------------------------------------------------- */

void sr_ui_nav_compass(SrUi *ui, const SrGame *g, float cx, float cw, float y, int s) {
  const SrRoute *r = &g->route;
  if (r->kind == SR_DEST_NONE) return;
  SrVec3 aim = r->target; /* toward the next route point that is not underfoot */
  for (int i = 1; i < r->n; i++) {
    float ex = r->pts[i].x - g->player.pos.x, ez = r->pts[i].z - g->player.pos.z;
    if (ex * ex + ez * ez > 12.0f * 12.0f) {
      aim = r->pts[i];
      break;
    }
  }
  float dx = aim.x - g->player.pos.x, dz = aim.z - g->player.pos.z;
  float ang = sr_wrap_angle(atan2f(-dx, -dz) - g->player.yaw);
  float half = cw / 2, x = cx - SR_CLAMP(ang / 1.2f, -1.0f, 1.0f) * half;
  diamond(ui, x, y + 4.5f * (float)s, 4.0f * (float)s, k_cyan);
  char buf[96], ins[64];
  snprintf(buf, sizeof buf, "%d M", (int)sr_route_distance(g));
  sr_ui_text(ui, x - (float)sr_ui_text_width(buf, s) / 2, y + 11.0f * (float)s, buf, k_cyan, s);
  sr_route_instruction(g, ins, sizeof ins);
  if (ins[0]) {
    int w = sr_ui_text_width(ins, s);
    sr_ui_rect(ui, cx - (float)w / 2 - 3, y + 30.0f * (float)s, (float)w + 6, 9.0f * (float)s, k_panel);
    sr_ui_text(ui, cx - (float)w / 2, y + 31.0f * (float)s, ins, k_white, s);
  }
}

static void minimap(SrUi *ui, const SrGame *g, float x0, float y0, float size, int s) {
  const SrCity *c = g->city;
  float radius = 110.0f, sc = size * 0.5f / radius, mx = x0 + size / 2, my = y0 + size / 2;
  float cy_ = cosf(g->player.yaw), sy_ = sinf(g->player.yaw);
  sr_ui_rect(ui, x0, y0, size, size, k_panel);
  /* heading up: the world turns about the player */
#define MM(wx, wz, ox, oy)                                                                                             \
  do {                                                                                                                 \
    float dx_ = (wx) - g->player.pos.x, dz_ = (wz) - g->player.pos.z;                                                  \
    float rx_ = dx_ * cy_ - dz_ * sy_, rz_ = dx_ * sy_ + dz_ * cy_;                                                    \
    *(ox) = mx + rx_ * sc;                                                                                             \
    *(oy) = my + rz_ * sc;                                                                                             \
  } while (0)
  float B = (float)SR_BLOCK_CM / 100.0f, half = (float)SR_STREET_CM / 200.0f;
  int bx = (int)(g->player.pos.x / B), bz = (int)(g->player.pos.z / B);
  for (int k = -2; k <= 3; k++) { /* street centre lines */
    float lx = (float)(bx + k) * B + half, lz = (float)(bz + k) * B + half;
    float ax, ay, bxx, byy;
    MM(lx, g->player.pos.z - radius * 1.5f, &ax, &ay);
    MM(lx, g->player.pos.z + radius * 1.5f, &bxx, &byy);
    if (clip(&ax, &ay, &bxx, &byy, x0, y0, x0 + size, y0 + size))
      line(ui, ax, ay, bxx, byy, 3.0f * (float)s, (SrColor){48, 52, 66, 255});
    MM(g->player.pos.x - radius * 1.5f, lz, &ax, &ay);
    MM(g->player.pos.x + radius * 1.5f, lz, &bxx, &byy);
    if (clip(&ax, &ay, &bxx, &byy, x0, y0, x0 + size, y0 + size))
      line(ui, ax, ay, bxx, byy, 3.0f * (float)s, (SrColor){48, 52, 66, 255});
  }
  /* places nearby */
  int b0 = sr_city_block_at(c, (int32_t)(g->player.pos.x * 100.0f), (int32_t)(g->player.pos.z * 100.0f));
  for (int dy = -2; dy <= 2 && b0 >= 0; dy++)
    for (int dx = -2; dx <= 2; dx++) {
      int b = sr_city_block_index(c, b0 % c->grid + dx, b0 / c->grid + dy);
      if (b < 0) continue;
      const SrBlock *blk = &c->blocks[b];
      for (int i = 0; i < blk->building_count; i++) {
        int bi = blk->first_building + i;
        const SrBuilding *bd = &c->buildings[bi];
        if (bd->poi == SR_POI_NONE && bi != g->player.home) continue;
        int32_t px, pz;
        sr_building_center(bd, &px, &pz);
        float ox, oy;
        MM((float)px / 100.0f, (float)pz / 100.0f, &ox, &oy);
        if (ox < x0 + 2 || oy < y0 + 2 || ox > x0 + size - 2 || oy > y0 + size - 2) continue;
        sr_ui_rect(ui, ox - 1.5f * (float)s, oy - 1.5f * (float)s, 3.0f * (float)s, 3.0f * (float)s,
                   bi == g->player.home      ? k_amber
                   : bd->poi == SR_POI_FIXER ? k_red
                                             : k_dim);
      }
    }
  /* the route and the destination, held to the edge when outside */
  const SrRoute *r = &g->route;
  for (int i = 1; i < r->n; i++) {
    float ax, ay, bxx, byy;
    MM(r->pts[i - 1].x, r->pts[i - 1].z, &ax, &ay);
    MM(r->pts[i].x, r->pts[i].z, &bxx, &byy);
    if (clip(&ax, &ay, &bxx, &byy, x0, y0, x0 + size, y0 + size)) line(ui, ax, ay, bxx, byy, 2.0f * (float)s, k_cyan);
  }
  if (r->kind != SR_DEST_NONE) {
    float ox, oy;
    MM(r->target.x, r->target.z, &ox, &oy);
    ox = SR_CLAMP(ox, x0 + 4, x0 + size - 4);
    oy = SR_CLAMP(oy, y0 + 4, y0 + size - 4);
    diamond(ui, ox, oy, 3.0f * (float)s, k_cyan);
  }
#undef MM
  tri(ui, mx, my - 5.0f * (float)s, mx - 3.5f * (float)s, my + 3.5f * (float)s, mx + 3.5f * (float)s,
      my + 3.5f * (float)s, k_amber);
  /* north, turning with the view */
  float nx = mx + sinf(g->player.yaw) * (size * 0.5f - 5.0f * (float)s),
        ny = my - cosf(g->player.yaw) * (size * 0.5f - 5.0f * (float)s);
  sr_ui_text(ui, nx - 2.5f * (float)s, ny - 3.5f * (float)s, "N", k_white, s);
}

void sr_ui_nav_overlay(SrUi *ui, const SrGame *g, const SrInput *in) {
  float W = (float)ui->w, H = (float)ui->h;
  int s = ui->scale;
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  float m = 6.0f * (float)s;
  /* the current goal, under the compass */
  char goal[160];
  sr_guide_objective(g, goal, sizeof goal);
  if (goal[0] && !g->dive.active) {
    float gw = SR_MIN((float)sr_ui_text_width(goal, s), W * 0.5f);
    text_cap(ui, W / 2 - gw / 2, m + 21.0f * (float)s, goal, k_amber, s, W * 0.5f);
  }
  /* the minimap: bottom left on desktop, top left on touch (the left thumb owns the bottom) */
  if (g->settings.minimap && !g->site.active && !g->dive.active) {
    float size = H * (touch ? 0.2f : 0.24f);
    float y0 = touch ? H * 0.3f : H - size - m;
    minimap(ui, g, m, y0, size, SR_MAX(1, s / 2));
  }
  /* a tip, fading in and out, low and centred */
  float alpha;
  const char *tip = sr_guide_tip_text(g, touch, &alpha);
  if (tip && alpha > 0) {
    float tw = W * 0.62f, th = 22.0f * (float)s, tx = W / 2 - tw / 2, ty = H * (touch ? 0.58f : 0.7f);
    SrColor panel = k_panel, text = k_white, edge = k_cyan;
    panel.a = (uint8_t)(panel.a * alpha);
    text.a = (uint8_t)(255 * alpha);
    edge.a = (uint8_t)(255 * alpha);
    sr_ui_rect(ui, tx, ty, tw, th, panel);
    sr_ui_rect(ui, tx, ty, 2.0f * (float)s, th, edge);
    /* two lines at most: split at a space near the middle when it does not fit */
    int scale = s;
    char a[160], b[160];
    snprintf(a, sizeof a, "%s", tip);
    b[0] = '\0';
    if ((float)sr_ui_text_width(a, scale) > tw - 12) {
      int len = (int)strlen(a), cut = len / 2;
      while (cut < len && a[cut] != ' ') cut++;
      if (cut < len) {
        snprintf(b, sizeof b, "%s", a + cut + 1);
        a[cut] = '\0';
      }
    }
    text_cap(ui, tx + 4.0f * (float)s, ty + 3.0f * (float)s, a, text, scale, tw - 6.0f * (float)s);
    if (b[0]) text_cap(ui, tx + 4.0f * (float)s, ty + 12.0f * (float)s, b, text, scale, tw - 6.0f * (float)s);
  }
}
