#include "ui/sr_ui.h"

#include "core/sr_str.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const SrColor k_white = {235, 240, 255, 255};
static const SrColor k_amber = {255, 190, 70, 255};
static const SrColor k_cyan = {80, 230, 255, 255};
static const SrColor k_panel = {8, 10, 20, 210};
static const SrColor k_red = {255, 70, 70, 255};

void sr_ui_init(SrUi *ui) {
  memset(ui, 0, sizeof *ui);
  sr_mesh_init(&ui->mesh);
  ui->scale = 2;
  ui->px_per_css = 1.0f;
}

void sr_ui_free(SrUi *ui) { sr_mesh_free(&ui->mesh); }

int sr_ui_auto_scale(float css_h) {
  int s = (int)(css_h / 300.0f);
  return s < 2 ? 2 : s;
}

void sr_ui_begin(SrUi *ui, int w, int h, int scale) {
  sr_mesh_clear(&ui->mesh);
  ui->w = w;
  ui->h = h;
  ui->scale = scale > 0 ? scale : 2;
}

void sr_ui_rect(SrUi *ui, float x, float y, float w, float h, SrColor c) {
  sr_mesh_quad(&ui->mesh, sr_v3(x, y + h, 0), sr_v3(x + w, y + h, 0), sr_v3(x + w, y, 0), sr_v3(x, y, 0), 0, 0, 1, 1, c,
               SR_LAYER_WHITE, 255, 0);
}

int sr_ui_text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale; }
int sr_ui_glyph_height(int scale) { return 7 * scale; }

void sr_ui_text(SrUi *ui, float x, float y, const char *s, SrColor c, int scale) {
  const float inv = 1.0f / SR_TEX_SIZE;
  for (; *s; s++, x += 6.0f * (float)scale) {
    if (*s == ' ') continue;
    int cx, cy;
    sr_font_cell(*s, &cx, &cy);
    float u0 = (float)cx * inv, v0 = (float)cy * inv, u1 = (float)(cx + 5) * inv, v1 = (float)(cy + 7) * inv;
    float w = 5.0f * (float)scale, h = 7.0f * (float)scale;
    sr_mesh_quad(&ui->mesh, sr_v3(x, y + h, 0), sr_v3(x + w, y + h, 0), sr_v3(x + w, y, 0), sr_v3(x, y, 0), u0, v1, u1,
                 v0, c, SR_LAYER_FONT, 255, 0);
  }
}

void sr_ui_icon(SrUi *ui, float x, float y, float size, int poi, SrColor c) {
  SrAtlasCell ic = sr_icon_cell(poi);
  sr_mesh_quad(&ui->mesh, sr_v3(x, y + size, 0), sr_v3(x + size, y + size, 0), sr_v3(x + size, y, 0), sr_v3(x, y, 0),
               ic.u0, ic.v1, ic.u1, ic.v0, c, ic.layer, 255, 0);
}

const char *sr_ui_context_label(SrContextKind k) {
  switch (k) {
    case SR_CTX_FOOD: return "EAT";
    case SR_CTX_SHOP: return "SHOP";
    case SR_CTX_FIXER: return "TALK TO FIXER";
    case SR_CTX_HOME: return "GO HOME";
    case SR_CTX_METRO: return "RIDE METRO";
    case SR_CTX_CAPSULE: return "RENT CAPSULE";
    case SR_CTX_JOB_TARGET: return "DO THE JOB";
    case SR_CTX_JOB_REPORT: return "REPORT BACK";
    case SR_CTX_CLINIC: return "CLINIC";
    case SR_CTX_LANDLORD: return "PAY LANDLORD";
    case SR_CTX_CLOSED: return "CLOSED";
    case SR_CTX_EXIT: return "LEAVE";
    case SR_CTX_BED: return "SLEEP";
    case SR_CTX_STASH: return "STASH";
    case SR_CTX_TERMINAL: return "TERMINAL";
    case SR_CTX_WARDROBE: return "CHANGE OUTFIT";
    case SR_CTX_OBJECTIVE: return "TAKE THE OBJECTIVE";
    case SR_CTX_CONSOLE: return "HACK CONSOLE";
    case SR_CTX_LOCKED: return "PICK THE LOCK";
    case SR_CTX_TAKEDOWN: return "TAKEDOWN";
    case SR_CTX_ENTER: return "GO INSIDE";
    case SR_CTX_ROOF_HATCH: return "DROP IN";
    case SR_CTX_CLIMB_DOWN: return "CLIMB DOWN";
    case SR_CTX_NODE: return "ACCESS NODE";
    case SR_CTX_HOST: return "ENTER HOST";
    case SR_CTX_QUICK: return "QUICK HACK";
    default: return "";
  }
}

int sr_ui_context_icon(const SrGame *g, const SrContext *c) {
  if (c->kind == SR_CTX_NONE) return -1;
  if (c->building >= 0 && c->kind != SR_CTX_EXIT && c->kind != SR_CTX_CLIMB_DOWN && c->kind < SR_CTX_EXIT) {
    int poi = g->city->buildings[c->building].poi;
    if (poi != SR_POI_NONE) return poi;
  }
  switch (c->kind) {
    case SR_CTX_FOOD: return SR_POI_FOOD;
    case SR_CTX_FIXER:
    case SR_CTX_JOB_REPORT: return SR_POI_FIXER;
    case SR_CTX_CAPSULE: return SR_POI_CAPSULE;
    case SR_CTX_CLINIC: return SR_POI_CLINIC;
    case SR_CTX_METRO:
    case SR_CTX_TERMINAL:
    case SR_CTX_CONSOLE:
    case SR_CTX_NODE:
    case SR_CTX_HOST:
    case SR_CTX_QUICK: return SR_POI_MATRIX_BOOTH;
    case SR_CTX_LOCKED:
    case SR_CTX_LANDLORD: return SR_POI_POLICE;
    case SR_CTX_TAKEDOWN: return SR_POI_GANG_HIDEOUT;
    case SR_CTX_JOB_TARGET:
    case SR_CTX_OBJECTIVE:
    case SR_CTX_ENTER:
    case SR_CTX_ROOF_HATCH: return SR_POI_CORP_FACILITY;
    case SR_CTX_SHOP: return SR_POI_ELECTRONICS;
    case SR_CTX_WARDROBE: return SR_POI_CLOTHING;
    default: return SR_POI_HOME; /* home, bed, stash, exits, closed doors */
  }
}

static int fit_scale(const char *t, int s, float max_w);

static void centered_text(SrUi *ui, float y, const char *txt, SrColor c, int s) {
  s = fit_scale(txt, s, (float)ui->w - 8.0f * (float)ui->scale);
  sr_ui_text(ui, (float)ui->w / 2 - (float)sr_ui_text_width(txt, s) / 2, y, txt, c, s);
}

static void bar(SrUi *ui, float x, float y, float w, float h, float frac, SrColor c) {
  sr_ui_rect(ui, x, y, w, h, k_panel);
  sr_ui_rect(ui, x + 1, y + 1, (w - 2) * SR_CLAMP(frac, 0.0f, 1.0f), h - 2, c);
}

/* Largest text scale <= s at which a string fits in max_w pixels (at least 1). */
static int fit_scale(const char *t, int s, float max_w) {
  while (s > 1 && (float)sr_ui_text_width(t, s) > max_w) s--;
  return s;
}

/* Text that never runs past max_w: shrinks first, then cuts the end. */
static void text_fit(SrUi *ui, float x, float y, const char *t, SrColor c, int s, float max_w) {
  int fs = fit_scale(t, s, max_w);
  char buf[256];
  snprintf(buf, sizeof buf, "%s", t);
  int max_chars = (int)(max_w / (6.0f * (float)fs));
  if (max_chars >= 0 && max_chars < (int)sizeof buf) buf[max_chars] = '\0';
  sr_ui_text(ui, x, y, buf, c, fs);
}

static void netsight_overlay(SrUi *ui, const SrGame *g);

static void hud(SrUi *ui, const SrGame *g, const SrInput *in) {
  const SrPlayer *p = &g->player;
  int s = ui->scale;
  float m = 6.0f * (float)s;
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  float k = ui->px_per_css;
  /* right edge of the free area: the touch buttons own the right-hand side */
  float rx = touch ? sr_touch_buttons_left(&in->layout) * k - m : (float)ui->w - m;
  char buf[160];
  int64_t t = sr_game_time(g);
  sr_time_format(t, buf, sizeof buf);
  float time_right = m + (float)sr_ui_text_width(buf, s);
  /* a dark backing keeps the HUD column readable over bright walls and windows */
  float col_h = touch ? 64.0f * (float)s : 46.0f * (float)s;
  sr_ui_rect(ui, m / 2, m / 2, SR_MAX(time_right, m + 62.0f * (float)s) + m / 2, col_h, (SrColor){4, 6, 14, 120});
  sr_ui_text(ui, m, m, buf, k_cyan, s);
  snprintf(buf, sizeof buf, "%lld CRED", (long long)p->cred);
  sr_ui_text(ui, m, m + 10.0f * (float)s, buf, k_amber, s);
  float bw = 60.0f * (float)s, bh = 4.0f * (float)s;
  float by = m + 20.0f * (float)s;
  bar(ui, m, by, bw, bh, p->health / p->max_health, k_red);
  bar(ui, m, by + bh + 2, bw, bh, p->satiety / 100.0f, (SrColor){90, 220, 90, 255});
  bar(ui, m, by + 2 * (bh + 2), bw, bh, 1.0f - p->fatigue / 100.0f, (SrColor){90, 140, 255, 255});
  /* Heat stars */
  int heat = 0;
  for (int j = 0; j < SR_MAX_FACTIONS; j++) heat = SR_MAX(heat, g->heat.level[j]);
  for (int i = 0; i < 5; i++)
    sr_ui_rect(ui, m + (float)i * 6.0f * (float)s, by + 3 * (bh + 2) + 2, 4.0f * (float)s, 4.0f * (float)s,
               i < heat ? k_red : (SrColor){60, 60, 70, 200});
  /* district and weather: top right, or under the bars on touch (the buttons own the top right) */
  int d = sr_city_district_at(g->city, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  char weather[48];
  snprintf(buf, sizeof buf, "%s", d >= 0 ? g->city->districts[d].name : "Outside the city");
  snprintf(weather, sizeof weather, "%s", sr_weather_name(sr_game_weather(g).to));
  float left_w = (float)ui->w * 0.3f - m;
  if (touch) {
    float ty = by + 3 * (bh + 2) + 6.0f * (float)s;
    text_fit(ui, m, ty, buf, k_white, s, left_w);
    text_fit(ui, m, ty + 9.0f * (float)s, weather, k_cyan, s, left_w);
  } else {
    sr_ui_text(ui, (float)ui->w - m - (float)sr_ui_text_width(buf, s), m, buf, k_white, s);
    sr_ui_text(ui, (float)ui->w - m - (float)sr_ui_text_width(weather, s), m + 10.0f * (float)s, weather, k_cyan, s);
  }
  /* compass with megacorp landmarks, between the HUD column and the top-right corner */
  float cw = (float)ui->w * 0.4f, cx = (float)ui->w / 2;
  if (touch) {
    float lo = SR_MAX((float)ui->w * 0.3f, time_right + 2 * m), hi = ((float)ui->w - (8.0f + 3 * 56.0f) * k) - m;
    cw = SR_MAX(hi - lo, 40.0f);
    cx = (lo + hi) / 2;
  }
  sr_ui_rect(ui, cx - cw / 2, m, cw, 9.0f * (float)s, k_panel);
  for (int f = 0; f < g->city->factions.corp_count; f++) {
    const SrBuilding *b = &g->city->buildings[g->city->factions.f[f].hq_building];
    float dx = (float)(b->x0 + b->x1) / 200.0f - p->pos.x, dz = (float)(b->y0 + b->y1) / 200.0f - p->pos.z;
    float ang = atan2f(-dx, -dz) - p->yaw;
    ang = sr_wrap_angle(ang);
    if (fabsf(ang) > 1.2f) continue;
    float x = cx - ang / 1.2f * cw / 2;
    sr_ui_rect(ui, x - 2.0f * (float)s, m + 2.0f * (float)s, 4.0f * (float)s, 5.0f * (float)s,
               sr_corp_color(g->city->factions.f[f].hue));
  }
  sr_ui_rect(ui, cx - 1, m, 2, 9.0f * (float)s, k_white);
  /* the district's billboards rotate the latest headlines */
  const char *head = !g->site.active && d >= 0 ? sr_news_billboard(&g->chess.news, d, t) : NULL;
  if (head) {
    float hy = m + 11.0f * (float)s;
    sr_ui_rect(ui, cx - cw / 2, hy - (float)s, cw, 9.0f * (float)s, (SrColor){40, 6, 30, 170});
    int hs = fit_scale(head, s, cw - 4);
    text_fit(ui, cx - SR_MIN((float)sr_ui_text_width(head, hs), cw - 4) / 2, hy, head, (SrColor){255, 80, 200, 255}, hs,
             cw - 4);
  }
  /* crosshair */
  sr_ui_rect(ui, (float)ui->w / 2 - (float)s, (float)ui->h / 2 - (float)s, 2.0f * (float)s, 2.0f * (float)s, k_white);
  /* context prompt with the interaction's icon */
  const char *label = sr_ui_context_label(g->ctx.kind);
  if (label[0] && touch) { /* on touch the label sits right above the interact button */
    for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
      const SrTouchButton *b = &in->layout.buttons[i];
      if (b->action != SR_ACT_INTERACT) continue;
      int ls = fit_scale(label, s, (float)ui->w * 0.45f);
      float tw = (float)sr_ui_text_width(label, ls);
      float x = SR_MIN(b->x * k + b->size * k / 2, (float)ui->w - m) - tw;
      float y = (b->y - b->size / 2) * k - 7.0f * (float)ls - 8;
      sr_ui_rect(ui, x - 4, y - 4, tw + 8, 7.0f * (float)ls + 8, k_panel);
      sr_ui_text(ui, x, y, label, k_amber, ls);
    }
  } else if (label[0]) {
    snprintf(buf, sizeof buf, "[E] %s", label);
    float tw = (float)sr_ui_text_width(buf, s), ic = 9.0f * (float)s;
    float x0 = (float)ui->w / 2 - (tw + ic + 4) / 2;
    sr_ui_rect(ui, x0 - 4, (float)ui->h * 0.62f - 4 - (float)s, tw + ic + 12, ic + 8, k_panel);
    sr_ui_icon(ui, x0, (float)ui->h * 0.62f - (float)s, ic, sr_ui_context_icon(g, &g->ctx), k_amber);
    sr_ui_text(ui, x0 + ic + 4, (float)ui->h * 0.62f, buf, k_amber, s);
  }
  if (g->net.netsight && g->site.active) netsight_overlay(ui, g);
  /* inside: guard awareness and camera contact */
  if (g->site.active && !g->site.in.apartment) {
    float aw = SR_MAX(g->site.awareness, g->site.cam_detect);
    sr_ui_rect(ui, cx - 30.0f * (float)s, m + 12.0f * (float)s, 60.0f * (float)s, 3.0f * (float)s, k_panel);
    sr_ui_rect(ui, cx - 30.0f * (float)s, m + 12.0f * (float)s, 60.0f * (float)s * aw, 3.0f * (float)s,
               g->site.alarm ? k_red : k_amber);
    if (g->site.alarm) centered_text(ui, m + 17.0f * (float)s, "ALARM", k_red, s);
  }
  /* cyberware and gadget states */
  float sy = (float)ui->h - m - 18.0f * (float)s;
  if (g->thermal) sr_ui_text(ui, rx - (float)sr_ui_text_width("THERMAL", s), sy, "THERMAL", k_red, s);
  if (g->buff_camo > 0)
    sr_ui_text(ui, rx - (float)sr_ui_text_width("CAMO", s), sy - 9.0f * (float)s, "CAMO", k_cyan, s);
  if (g->buff_reflex > 0)
    sr_ui_text(ui, rx - (float)sr_ui_text_width("REFLEX", s), sy - 18.0f * (float)s, "REFLEX", k_amber, s);
  if (g->glitch_time > 0) { /* neural glitch: static bars over the view */
    for (int i = 0; i < 12; i++) {
      float y = (float)((g->steps * 37u + (uint64_t)i * 101u) % (uint64_t)ui->h);
      sr_ui_rect(ui, 0, y, (float)ui->w, 2.0f * (float)s, (SrColor){(uint8_t)(i * 20), 255, 255, 90});
    }
    centered_text(ui, (float)ui->h * 0.4f, "NEURAL GLITCH", k_red, s);
  }
  /* active job objective */
  if (g->active_job >= 0) {
    const SrJob *j = NULL;
    for (int i = 0; i < g->jobs.count; i++)
      if (g->jobs.jobs[i].id == g->active_job) j = &g->jobs.jobs[i];
    if (j) {
      const SrBuilding *tb = &g->city->buildings[j->executed ? j->client_building : j->target_building];
      float dist = sqrtf(powf((float)(tb->x0 + tb->x1) / 200.0f - p->pos.x, 2) +
                         powf((float)(tb->y0 + tb->y1) / 200.0f - p->pos.z, 2));
      snprintf(buf, sizeof buf, "%s: %s %.0fm", j->executed ? "REPORT" : "JOB", j->title, (double)dist);
      text_fit(ui, m, (float)ui->h - m - 7.0f * (float)s, buf, k_white, s, rx - m);
    }
  }
  if (g->toast_time > 0) { /* centred in the free area, shrunk to fit */
    float area = rx - m, mid = (m + rx) / 2;
    int ts = fit_scale(g->toast, s, area - 8);
    float tw = SR_MIN((float)sr_ui_text_width(g->toast, ts), area - 8);
    float ty = touch ? SR_MAX((float)ui->h * 0.3f, m / 2 + col_h + m) : (float)ui->h * 0.25f;
    sr_ui_rect(ui, mid - tw / 2 - 4, ty - 4, tw + 8, 7.0f * (float)ts + 8, k_panel);
    text_fit(ui, mid - tw / 2, ty, g->toast, k_white, ts, area - 8);
  }
}

static void centered(SrUi *ui, float y, const char *txt, SrColor c, int s);

/* Screen position of a world point for the player's view (same camera as the scene). */
static bool project(const SrUi *ui, const SrGame *g, SrVec3 p, float *sx, float *sy) {
  SrMat4 view = sr_m4_fps_view(g->player.pos, g->player.yaw, g->player.pitch);
  SrMat4 proj = sr_m4_perspective(70.0f * SR_PI / 180.0f, (float)ui->w / (float)SR_MAX(1, ui->h), 0.1f, 4000.0f);
  SrMat4 vp = sr_m4_mul(proj, view);
  float x = vp.m[0] * p.x + vp.m[4] * p.y + vp.m[8] * p.z + vp.m[12];
  float y = vp.m[1] * p.x + vp.m[5] * p.y + vp.m[9] * p.z + vp.m[13];
  float w = vp.m[3] * p.x + vp.m[7] * p.y + vp.m[11] * p.z + vp.m[15];
  if (w < 0.1f) return false;
  *sx = (x / w * 0.5f + 0.5f) * (float)ui->w;
  *sy = (0.5f - y / w * 0.5f) * (float)ui->h;
  return *sx >= 0 && *sy >= 0 && *sx <= (float)ui->w && *sy <= (float)ui->h;
}

/* Netsight: every networked device in view, with its type; owned devices in green. */
static void netsight_overlay(SrUi *ui, const SrGame *g) {
  static const char *const names[] = {"DOOR", "CAM", "TURRET", "ALARM", "LIGHTS", "LIFT"};
  int s = ui->scale;
  for (int i = 0; i < g->site.in.device_count; i++) {
    float x, y;
    SrVec3 p = sr_game_device_pos(g, i);
    if (sr_v3_len(sr_v3_sub(p, g->player.pos)) > 30.0f || !project(ui, g, p, &x, &y)) continue;
    bool own = sr_game_owns_device(g, g->site.in.devices[i].id);
    SrColor c = own ? (SrColor){80, 255, 140, 230} : (SrColor){80, 200, 255, 200};
    sr_ui_rect(ui, x - 3.0f * (float)s, y - 3.0f * (float)s, 6.0f * (float)s, 6.0f * (float)s, c);
    sr_ui_text(ui, x + 5.0f * (float)s, y - 3.0f * (float)s, names[g->site.in.devices[i].type], c, s);
  }
}

/* Breach puzzle: concentric rings with their gaps; the lock mark sits at the top. */
static void breach_overlay(SrUi *ui, const SrBreach *b) {
  int s = ui->scale;
  float cx = (float)ui->w / 2, cy = (float)ui->h / 2;
  float base = (float)SR_MIN(ui->w, ui->h) * 0.08f, step = (float)SR_MIN(ui->w, ui->h) * 0.055f;
  sr_ui_rect(ui, cx - base - step * (float)b->rings - 10, cy - base - step * (float)b->rings - 10,
             2 * (base + step * (float)b->rings) + 20, 2 * (base + step * (float)b->rings) + 20,
             (SrColor){0, 4, 12, 200});
  sr_ui_rect(ui, cx - (float)s, cy - base - step * (float)b->rings - 8, 2.0f * (float)s, step * (float)b->rings + 8,
             (SrColor){80, 255, 140, 200}); /* the lock mark */
  for (int r = 0; r < b->rings; r++) {
    float rad = base + step * (float)r;
    SrColor c = b->locked[r]  ? (SrColor){80, 255, 140, 255}
                : r == b->sel ? (SrColor){255, 200, 60, 255}
                              : (SrColor){60, 160, 255, 220};
    for (int k = 0; k < 48; k++) {
      float a = (float)k / 48.0f * 2.0f * SR_PI;
      bool gap = fabsf(sr_wrap_angle(a - b->angle[r])) < SR_BREACH_TOLERANCE * 1.2f;
      for (int q = 0; q < b->decoys[r] && !gap; q++)
        gap = fabsf(sr_wrap_angle(a - b->angle[r] + b->decoy_at[r][q])) < SR_BREACH_TOLERANCE * 1.2f;
      if (gap) continue;
      float x = cx + sinf(a) * rad, y = cy - cosf(a) * rad;
      sr_ui_rect(ui, x - 1.5f * (float)s, y - 1.5f * (float)s, 3.0f * (float)s, 3.0f * (float)s, c);
    }
    /* the true gap carries a small amber tick on its edge; decoys do not */
    float a = b->angle[r] + SR_BREACH_TOLERANCE * 1.5f;
    sr_ui_rect(ui, cx + sinf(a) * rad - (float)s, cy - cosf(a) * rad - (float)s, 2.0f * (float)s, 2.0f * (float)s,
               (SrColor){255, 150, 40, 255});
  }
  float bw = 2 * (base + step * (float)b->rings);
  sr_ui_rect(ui, cx - bw / 2, cy + base + step * (float)b->rings + 6, bw * b->time_left / b->time_limit,
             3.0f * (float)s, (SrColor){255, 80, 80, 255});
}

static void matrix_hud(SrUi *ui, const SrGame *g, const SrInput *in) {
  const SrDive *d = &g->dive;
  int s = ui->scale;
  float m = 6.0f * (float)s, line = 10.0f * (float)s;
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  float k = ui->px_per_css;
  float rx = touch ? sr_touch_buttons_left(&in->layout) * k - m : (float)ui->w - m;
  SrColor cyan = {80, 230, 255, 255}, red = {255, 70, 70, 255}, amber = {255, 190, 70, 255};
  char buf[128];
  sr_ui_rect(ui, m / 2, m / 2, (float)ui->w * 0.34f, line * 5.4f, (SrColor){0, 6, 16, 150});
  if (d->in_host)
    snprintf(buf, sizeof buf, "HOST %s  SEC %d", g->city->buildings[d->building].name, d->host.security);
  else
    snprintf(buf, sizeof buf, "THE GRID");
  sr_ui_text(ui, m, m, buf, cyan, s);
  float bw = 60.0f * (float)s;
  sr_ui_rect(ui, m, m + line, bw, 4.0f * (float)s, (SrColor){10, 20, 40, 220});
  sr_ui_rect(ui, m, m + line, bw * d->persona.integrity / SR_MAX(1.0f, d->persona.max_integrity), 4.0f * (float)s,
             cyan);
  for (int i = 0; i < 3; i++) /* host alert */
    sr_ui_rect(ui, m + bw + 6 + (float)i * 6.0f * (float)s, m + line, 4.0f * (float)s, 4.0f * (float)s,
               i < d->alert ? red : (SrColor){40, 40, 60, 220});
  if (d->trace > 0) {
    snprintf(buf, sizeof buf, "TRACE %d%%", (int)(d->trace * 100));
    sr_ui_text(ui, m, m + line * 2, buf, red, s);
  }
  if (d->download_node >= 0) {
    snprintf(buf, sizeof buf, "DOWNLOAD %d%%", (int)(100 * (1 - d->download_left / SR_MAX(0.01f, d->download_total))));
    sr_ui_text(ui, m, m + line * 3, buf, amber, s);
  }
  if (d->body_alert && (g->steps / 15) % 2)
    centered(ui, m + line * 4.5f, "BODY ALERT - JACK OUT", red, s);
  else
    sr_ui_text(ui, m, m + line * 4, g->site.active ? "BODY: QUIET" : "BODY: SAFE", (SrColor){120, 200, 140, 255}, s);
  /* programs on the quick slots */
  const SrLoadout *l = &g->net.programs;
  for (int i = 0; i < l->count && i < 4; i++) {
    float y = (float)ui->h - m - (float)(4 - i) * line;
    snprintf(buf, sizeof buf, "%d %s", i + 1, sr_program_def(l->prog[i])->name);
    sr_ui_text(ui, m, y, buf, l->cooldown[i] > 0 ? (SrColor){90, 100, 130, 255} : cyan, s);
    if (l->cooldown[i] > 0)
      sr_ui_rect(ui, m, y + 8.0f * (float)s, 40.0f * (float)s * l->cooldown[i] / sr_program_def(l->prog[i])->cooldown,
                 2.0f * (float)s, amber);
  }
  sr_ui_rect(ui, (float)ui->w / 2 - (float)s, (float)ui->h / 2 - (float)s, 2.0f * (float)s, 2.0f * (float)s, cyan);
  const char *label = sr_ui_context_label(g->ctx.kind);
  if (label[0] && d->breach_node < 0) {
    if (g->ctx.kind == SR_CTX_NODE && g->ctx.prop >= 0 && g->ctx.prop < d->host.node_count)
      snprintf(buf, sizeof buf, "%s: %s%s", label, sr_node_type_name((SrNodeType)d->host.nodes[g->ctx.prop].type),
               sr_game_node_locked(g, g->ctx.prop) ? " [BREACH]" : "");
    else
      snprintf(buf, sizeof buf, "%s", label);
    centered(ui, (float)ui->h * 0.62f, buf, amber, s);
  }
  if (d->feed_device >= 0) centered(ui, m, "CAMERA FEED  //  MOVE TO RETURN", red, s);
  if (d->breach_node >= 0) breach_overlay(ui, &d->breach);
  (void)rx;
  if (g->toast_time > 0) centered(ui, (float)ui->h * 0.3f, g->toast, (SrColor){235, 240, 255, 255}, s);
}

static void centered(SrUi *ui, float y, const char *txt, SrColor c, int s) {
  s = fit_scale(txt, s, (float)ui->w - 8.0f * (float)ui->scale);
  sr_ui_text(ui, (float)ui->w / 2 - (float)sr_ui_text_width(txt, s) / 2, y, txt, c, s);
}

static void terminal_home(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  SrColor green = {80, 255, 120, 255};
  char buf[200];
  snprintf(buf, sizeof buf, "RENT %d DUE %s%s", g->player.rent, "SUN 23:59", g->player.evicted ? "  EVICTED" : "");
  sr_ui_text(ui, x, y, buf, g->player.evicted ? k_red : green, s);
  y += line;
  snprintf(buf, sizeof buf, "STREET CRED %d (%s)  LEVEL %d", g->player.prog.street_cred,
           sr_cred_tier_name(sr_cred_tier(g->player.prog.street_cred)), g->player.prog.level);
  sr_ui_text(ui, x, y, buf, green, s);
  y += line * 1.5f;
  sr_ui_text(ui, x, y, "MESSAGES", k_amber, s);
  y += line;
  for (int i = g->player.msg_count - 1; i >= 0 && y < (float)ui->h * 0.6f; i--) {
    const SrMessage *mg = &g->player.msgs[i];
    if (mg->repeat > 1)
      snprintf(buf, sizeof buf, "%s: %s (x%d)", mg->from, mg->text, mg->repeat);
    else
      snprintf(buf, sizeof buf, "%s: %s", mg->from, mg->text);
    buf[SR_CLAMP((ui->w - 2 * (int)x) / (6 * s), 1, 199)] = '\0';
    sr_ui_text(ui, x, y, buf, green, s);
    y += line;
  }
  y += line * 0.5f;
  sr_ui_text(ui, x, y, "JOBS", k_amber, s);
  y += line;
  for (int i = 0; i < g->jobs.count && y < (float)ui->h - 3 * line; i++) {
    const SrJob *j = &g->jobs.jobs[i];
    if (j->state != SR_JS_ACCEPTED) continue;
    snprintf(buf, sizeof buf, "[T%d] %s  %d CRED", j->tier, j->title, j->pay);
    sr_ui_text(ui, x, y, buf, green, s);
    y += line;
  }
}

static void job_row(const SrGame *g, const SrJob *j, char *buf, size_t cap);

static void terminal_jobs(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  static const char *const heads[4] = {"YOUR HANDLER", "GANG CONTACTS", "PUBLIC BOARD", "ACCEPTED"};
  int32_t ids[32];
  int n = sr_game_terminal_jobs(g, ids, 32), group = -1;
  if (g->chess.camp.hunter >= 0) { /* bounties on the player */
    char b[96];
    snprintf(b, sizeof b, "BOUNTY: %s want you dead", g->city->factions.f[g->chess.camp.hunter].name);
    sr_ui_text(ui, x, y, b, k_red, s);
    y += line * 1.3f;
  }
  if (n == 0) sr_ui_text(ui, x, y, "NO OFFERS. VISIT A FIXER OR WAIT FOR A CALL.", k_white, s);
  for (int i = 0; i < n && y < (float)ui->h - 3 * line; i++) {
    const SrJob *j = NULL;
    for (int k = 0; k < g->jobs.count; k++)
      if (g->jobs.jobs[k].id == ids[i]) j = &g->jobs.jobs[k];
    if (!j) continue; /* GCOVR_EXCL_LINE reason: the ids come from the board */
    bool handler = j->client_building == g->player.home;
    int grp = j->state == SR_JS_ACCEPTED ? 3 : handler ? 0 : j->source == SR_SRC_CONTACT ? 1 : 2;
    if (grp != group) {
      sr_ui_text(ui, x, y, heads[grp], k_amber, s);
      y += line;
      group = grp;
    }
    char row[200], b[210];
    job_row(g, j, row, sizeof row);
    snprintf(b, sizeof b, "%s %s", i == g->term_sel ? ">" : " ", row);
    text_fit(ui, x, y, b,
             sr_game_job_against_patron(g, j) ? k_red
             : i == g->term_sel               ? k_cyan
                                              : k_white,
             s, (float)ui->w - 2 * x);
    y += line;
  }
}

static void terminal_news(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  char buf[160];
  int n = sr_news_size(&g->chess.news);
  if (n == 0) sr_ui_text(ui, x, y, "NO NEWS YET. THE CITY HOLDS ITS BREATH.", k_white, s);
  for (int i = 0; i < n && y < (float)ui->h - 3 * line; i++) {
    const SrNews *it = sr_news_latest(&g->chess.news, i);
    const char *outlet = it->outlet >= 0 ? g->city->factions.f[it->outlet].name : "Pirate feed";
    snprintf(buf, sizeof buf, "D%lld %s: %s", (long long)(it->time / 86400), outlet, it->headline);
    text_fit(ui, x, y, buf, it->scandal ? k_red : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
}

static void terminal_market(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  char buf[160];
  const SrMarket *m = &g->chess.market;
  snprintf(buf, sizeof buf, "CRED %lld   PORTFOLIO %lld", (long long)g->player.cred,
           (long long)sr_market_portfolio(m, g->city));
  sr_ui_text(ui, x, y, buf, k_amber, s);
  y += line * 1.5f;
  for (int f = 0; f < g->city->factions.corp_count && y < (float)ui->h - 3 * line; f++) {
    snprintf(buf, sizeof buf, "%s %-28s %5d  HELD %d", f == g->term_sel ? ">" : " ", g->city->factions.f[f].name,
             m->price[f], m->shares[f]);
    text_fit(ui, x, y, buf, f == g->term_sel ? k_cyan : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
}

static void terminal_board(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  static const char *const verb[SR_LINK_COUNT] = {"funds", "owns", "bribes seat", "feuds with"};
  char buf[160];
  const SrChess *ch = &g->chess;
  int known = 0;
  for (int i = 0; i < ch->link_count && y < (float)ui->h - 3 * line; i++) {
    const SrLink *l = &ch->links[i];
    if (!l->known) continue;
    known++;
    if (l->type == SR_LINK_BRIBES)
      snprintf(buf, sizeof buf, "%s %s %d (%s)", g->city->factions.f[l->a].name, verb[l->type], l->b + 1,
               ch->pol.seats[l->b].name);
    else
      snprintf(buf, sizeof buf, "%s %s %s", g->city->factions.f[l->a].name, verb[l->type],
               g->city->factions.f[l->b].name);
    text_fit(ui, x, y, buf, k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  if (known == 0) sr_ui_text(ui, x, y, "NOTHING KNOWN. STEAL FILES TO MAP THE BOARD.", k_white, s);
  y += line;
  int leader = 0;
  float dom = sr_chess_dominance(ch, g->city, &leader);
  snprintf(buf, sizeof buf, "LEADING: %s (%d%%)", g->city->factions.f[leader].name, (int)(dom * 100.0f));
  sr_ui_text(ui, x, y, buf, k_amber, s);
}

static void terminal_story(SrUi *ui, const SrGame *g, float x, float y, float line, int s) {
  static const char *const acts[] = {"PROLOGUE", "ACT I: PAWN", "ACT II: KNIGHT", "ACT III: PROMOTION", "EPILOGUE"};
  char buf[160];
  const SrStory *st = &g->chess.story;
  sr_ui_text(ui, x, y, acts[SR_MIN(st->act, 4)], k_amber, s);
  y += line;
  if (st->retainer && st->patron >= 0) {
    snprintf(buf, sizeof buf, "PATRON %s  HANDLER %s  TRUST %d", g->city->factions.f[st->patron].name,
             st->cast[SR_CAST_HANDLER].name, st->trust);
    text_fit(ui, x, y, buf, k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  if (st->hostile >= 0) {
    snprintf(buf, sizeof buf, "HUNTED BY %s", g->city->factions.f[st->hostile].name);
    sr_ui_text(ui, x, y, buf, k_red, s);
    y += line;
  }
  if (st->ending != SR_END_NONE) {
    snprintf(buf, sizeof buf, "ENDING: %s. THE CITY GOES ON.", sr_ending_name((SrEnding)st->ending));
    sr_ui_text(ui, x, y, buf, k_cyan, s);
    y += line;
  }
  /* what trust unlocks, and what the endings need */
  if (st->retainer) {
    snprintf(buf, sizeof buf, "TRUST %d: 40 OPENS ACT II, 60 OPENS BLACK OPS, 0 MEANS THEY HUNT YOU", st->trust);
    text_fit(ui, x, y, buf, k_cyan, s, (float)ui->w - 2 * x);
    y += line;
  }
  if (st->act >= SR_ACT_KNIGHT && st->ending == SR_END_NONE) {
    text_fit(ui, x, y,
             "ENDINGS: LOYAL (STAY), DEFECTOR (SWITCH ONCE), BURN (LEAK OR BE HUNTED), KINGMAKER, SUNRISE (?)", k_cyan,
             s, (float)ui->w - 2 * x);
    y += line;
  }
  int options = 0, okind = 0;
  while (sr_game_story_option(g, options, &okind)) options++;
  y += line * 0.5f;
  sr_ui_text(ui, x, y, "STORY SO FAR", k_amber, s);
  y += line;
  for (int i = 0; i < 6 && y < (float)ui->h - (4 + options) * line; i++) {
    const SrLogEntry *e = sr_story_log_entry(&g->chess, i);
    if (!e) break;
    snprintf(buf, sizeof buf, "D%lld %s", (long long)(e->time / 86400), e->text);
    text_fit(ui, x, y, buf, k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  y += line * 0.5f;
  int kind = 0;
  for (int i = 0; sr_game_story_option(g, i, &kind) && y < (float)ui->h - 3 * line; i++) {
    snprintf(buf, sizeof buf, "%s %s", i == g->term_sel ? ">" : " ", sr_game_story_option(g, i, &kind));
    text_fit(ui, x, y, buf, i == g->term_sel ? k_cyan : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
}

static void terminal(SrUi *ui, const SrGame *g) {
  static const char *const tabs[SR_TAB_COUNT] = {"HOME", "JOBS", "NEWS", "MARKET", "BOARD", "STORY"};
  int s = ui->scale;
  float m = 10.0f * (float)s, line = 10.0f * (float)s;
  sr_ui_rect(ui, m, m, (float)ui->w - 2 * m, (float)ui->h - 2 * m, (SrColor){4, 14, 8, 235});
  float x = m + 6.0f * (float)s, y = m + 6.0f * (float)s;
  SrColor green = {80, 255, 120, 255};
  char buf[200];
  snprintf(buf, sizeof buf, "TERMINAL // %s // %s", g->player.name, g->city->name);
  text_fit(ui, x, y, buf, green, s, (float)ui->w - 2 * x);
  y += line * 1.3f;
  float tx = x;
  for (int t = 0; t < SR_TAB_COUNT; t++) {
    snprintf(buf, sizeof buf, t == g->term_tab ? "[%s]" : " %s ", tabs[t]);
    sr_ui_text(ui, tx, y, buf, t == g->term_tab ? k_amber : green, s);
    tx += (float)sr_ui_text_width(buf, s) + 4.0f * (float)s;
  }
  y += line * 1.5f;
  switch ((SrTermTab)g->term_tab) {
    case SR_TAB_JOBS: terminal_jobs(ui, g, x, y, line, s); break;
    case SR_TAB_NEWS: terminal_news(ui, g, x, y, line, s); break;
    case SR_TAB_MARKET: terminal_market(ui, g, x, y, line, s); break;
    case SR_TAB_BOARD: terminal_board(ui, g, x, y, line, s); break;
    case SR_TAB_STORY: terminal_story(ui, g, x, y, line, s); break;
    default: terminal_home(ui, g, x, y, line, s); break;
  }
  const char *hint = g->term_tab == SR_TAB_JOBS
                         ? "LEFT/RIGHT TABS  UP/DOWN CHOOSE  [E] ACCEPT  [R] ABANDON  [ESC] CLOSE"
                     : g->term_tab == SR_TAB_MARKET ? "LEFT/RIGHT TABS  [E] BUY 10  [R] SELL 10  [ESC] CLOSE"
                     : g->term_tab == SR_TAB_STORY  ? "LEFT/RIGHT TABS  UP/DOWN CHOOSE  [E] CONFIRM  [ESC] CLOSE"
                     : g->term_tab == SR_TAB_HOME && g->site.active && g->site.in.apartment
                         ? "LEFT/RIGHT TABS  [E] APARTMENTS  [ESC] CLOSE"
                         : "LEFT/RIGHT TABS  [ESC] CLOSE";
  text_fit(ui, x, (float)ui->h - m - line, hint, green, s, (float)ui->w - 2 * x);
}

static void touch_overlay(SrUi *ui, const SrGame *g, const SrInput *in) {
  const SrTouchLayout *l = &in->layout;
  float k = ui->px_per_css;
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
    const SrTouchButton *b = &l->buttons[i];
    float sz = b->size * k;
    SrColor c = in->touch_down[b->action] ? (SrColor){80, 230, 255, 200} : (SrColor){20, 30, 50, 150};
    sr_ui_rect(ui, b->x * k - sz / 2, b->y * k - sz / 2, sz, sz, c);
    int icon = b->action == SR_ACT_INTERACT ? sr_ui_context_icon(g, &g->ctx) : -1;
    if (icon >= 0) { /* the interact button shows what it will do */
      sr_ui_icon(ui, b->x * k - sz * 0.35f, b->y * k - sz * 0.35f, sz * 0.7f, icon, k_amber);
      continue;
    }
    char label[4] = {0};
    const char *n = sr_action_name(b->action);
    label[0] = (char)(n[0] - 32);
    label[1] = (char)(n[1] - 32);
    int s = SR_MAX(1, ui->scale);
    sr_ui_text(ui, b->x * k - (float)sr_ui_text_width(label, s) / 2, b->y * k - 3.5f * (float)s, label, k_white, s);
  }
  for (int i = 0; i < SR_MAX_FINGERS; i++) {
    const SrFinger *f = &in->fingers[i];
    if (!f->active || f->role != 1) continue;
    float cx = f->start_x * l->width * k, cy = f->start_y * l->height * k, r = l->stick_radius * k;
    sr_ui_rect(ui, cx - r, cy - r, 2 * r, 2 * r, (SrColor){30, 40, 70, 110});
    sr_ui_rect(ui, cx + in->stick_x * r - r / 3, cy - in->stick_y * r - r / 3, r / 1.5f, r / 1.5f,
               (SrColor){80, 230, 255, 200});
  }
  if (in->wheel_open) {
    float cx = (float)ui->w * 0.7f, cy = (float)ui->h * 0.5f;
    for (int i = 0; i < 4; i++)
      sr_ui_rect(ui, cx + (float)(i % 2) * 60 * k, cy + (float)(i >> 1) * 60 * k, 56 * k, 56 * k,
                 i == in->wheel_choice ? k_cyan : k_panel);
  }
}

static void create_screen(SrUi *ui, const SrGame *g, const SrInput *in) {
  int s = ui->scale;
  float line = 11.0f * (float)s, x = 12.0f * (float)s, y = 10.0f * (float)s;
  sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){5, 3, 12, 255});
  sr_ui_text(ui, x, y, "WHO ARE YOU?", k_cyan, s * 2);
  y += line * 2;
  char buf[96];
  for (int b = 0; b < SR_BG_COUNT; b++) {
    const SrBackgroundDef *d = sr_background_def(b);
    snprintf(buf, sizeof buf, "%d %s  %d CRED", b + 1, d->name, d->cash);
    text_fit(ui, x, y, buf, b == g->create_bg ? k_amber : k_white, s, (float)ui->w * 0.62f - x);
    y += line;
  }
  const SrBackgroundDef *d = sr_background_def(g->create_bg);
  y += line * 0.5f;
  for (int a = 0; a < SR_ATTR_COUNT; a++) {
    sr_ui_text(ui, x, y, sr_attr_name((SrAttr)a), k_white, s);
    bar(ui, x + 50.0f * (float)s, y, 60.0f * (float)s, 6.0f * (float)s, d->attr[a] / 10.0f, k_cyan);
    y += line * 0.8f;
  }
  snprintf(buf, sizeof buf, "TRAINED: %s, %s", sr_skill_name((SrSkill)d->trained[0]),
           sr_skill_name((SrSkill)d->trained[1]));
  text_fit(ui, x, y + line * 0.3f, buf, k_cyan, s, (float)ui->w * 0.62f - x);
  /* appearance preview: a portrait built from the generated parts */
  SrAppearance a = sr_game_create_preview(g);
  static const SrColor skin[8] = {{250, 210, 180, 255}, {230, 180, 140, 255}, {200, 150, 110, 255}, {170, 120, 80, 255},
                                  {140, 95, 60, 255},   {110, 70, 45, 255},   {80, 52, 35, 255},    {60, 40, 28, 255}};
  float px = (float)ui->w * 0.7f, py = (float)ui->h * 0.25f, u = 6.0f * (float)s;
  SrColor hair = sr_corp_color(a.hair_color * 2);
  SrColor coat = sr_corp_color(a.outfit_color + 8);
  sr_ui_rect(ui, px - 3 * u, py + 9 * u, 6 * u + (float)a.body * u, 9 * u, coat);
  sr_ui_rect(ui, px - 2 * u, py + 2 * u, 4 * u, 6 * u, skin[a.skin & 7]);
  if (a.hair % 12 < 4)
    sr_ui_rect(ui, px - u / 2, py - (float)(a.hair % 4) * u, u, 2 * u + (float)(a.hair % 4) * u, hair);
  else if (a.hair % 12 < 9)
    sr_ui_rect(ui, px - 2 * u, py + u, 4 * u, 1.5f * u, hair);
  if (a.cyber) sr_ui_rect(ui, px - 1.5f * u, py + 4 * u, 3 * u, 0.6f * u, k_red);
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  text_fit(ui, x, (float)ui->h - line * 1.5f,
           touch ? "TAP LEFT: BACKGROUND  RIGHT: LOOK  BOTTOM RIGHT: START"
                 : "1-5 BACKGROUND   R RANDOMISE LOOK   ENTER START",
           k_amber, s, (float)ui->w - 2 * x);
}

/* One line for a job offer: tier, type and target, district, pay, hours left, approaches, warnings. */
static void job_row(const SrGame *g, const SrJob *j, char *buf, size_t cap) {
  const SrCity *c = g->city;
  const SrBuilding *b = &c->buildings[j->target_building];
  const char *who =
      j->target_faction >= 0 ? c->factions.f[j->target_faction].name : (b->name[0] ? b->name : "a client");
  const char *client = j->client_faction >= 0 ? c->factions.f[j->client_faction].name : "private";
  int64_t left = (j->deadline - sr_game_time(g)) / 3600;
  char ap[5] = "----";
  static const char letters[4] = {'G', 'F', 'R', 'H'};
  for (int a = 0; a < 4; a++)
    if (j->approaches & (1u << a)) ap[a] = letters[a];
  snprintf(buf, cap, "[T%d] %s: %s (%s) for %s  %d CR  %lldh  %s%s", j->tier, sr_job_type_name((SrJobType)j->type), who,
           c->districts[c->blocks[b->block].district].name, client, j->pay, (long long)(left > 0 ? left : 0), ap,
           sr_game_job_against_patron(g, j) ? "  PATRON" : "");
}

static void shop_screen(SrUi *ui, const SrGame *g, const SrInput *in) {
  int s = ui->scale;
  float m = 10.0f * (float)s, line = 11.0f * (float)s;
  sr_ui_rect(ui, m, m, (float)ui->w - 2 * m, (float)ui->h - 2 * m, (SrColor){6, 8, 20, 235});
  float x = m + 6.0f * (float)s, y = m + 6.0f * (float)s;
  char buf[128];
  const SrMenu *mn = &g->menu;
  if (mn->node_menu)
    snprintf(buf, sizeof buf, "%s // %s", g->city->buildings[mn->building].name,
             sr_node_type_name((SrNodeType)g->dive.host.nodes[mn->node].type));
  else if (mn->job_menu)
    snprintf(buf, sizeof buf, "FIXER // %s  //  WORK ON OFFER", g->city->buildings[mn->building].name);
  else if (mn->encounter)
    snprintf(buf, sizeof buf, "%s // %s HITTERS", g->enc.raid ? "RAID" : "AMBUSH",
             g->enc.faction >= 0 ? g->city->factions.f[g->enc.faction].name : "UNKNOWN");
  else
    snprintf(buf, sizeof buf, "%s  //  %lld CRED", g->city->buildings[mn->building].name, (long long)g->player.cred);
  sr_ui_text(ui, x, y, buf, k_cyan, s);
  y += line * 1.5f;
  for (int i = 0; i < mn->count && mn->job_menu; i++) {
    if (mn->job[i] == SR_JOB_HEAT_CLEANUP) {
      int worst = 0;
      for (int f = 1; f < g->city->factions.count; f++)
        if (g->heat.level[f] > g->heat.level[worst]) worst = f;
      snprintf(buf, sizeof buf, "Make a call: Heat with %s -1  %d CR", g->city->factions.f[worst].name,
               500 * g->heat.level[worst]);
      if (i == mn->sel) sr_ui_rect(ui, x - 4, y - 2, (float)ui->w - 2 * x + 8, line, (SrColor){40, 60, 110, 200});
      text_fit(ui, x, y, buf, i == mn->sel ? k_amber : k_cyan, s, (float)ui->w - 2 * x);
      y += line;
      continue;
    }
    const SrJob *j = NULL;
    for (int k = 0; k < g->jobs.count; k++)
      if (g->jobs.jobs[k].id == mn->job[i]) j = &g->jobs.jobs[k];
    if (!j) continue;
    char row[200];
    job_row(g, j, row, sizeof row);
    bool warn = sr_game_job_against_patron(g, j);
    if (i == mn->sel) sr_ui_rect(ui, x - 4, y - 2, (float)ui->w - 2 * x + 8, line, (SrColor){40, 60, 110, 200});
    text_fit(ui, x, y, row, warn ? k_red : i == mn->sel ? k_amber : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  if (mn->job_menu && mn->sel >= 0 && mn->sel < mn->count) { /* the briefing of the selected offer */
    const SrJob *j = NULL;
    for (int k = 0; k < g->jobs.count; k++)
      if (g->jobs.jobs[k].id == mn->job[mn->sel]) j = &g->jobs.jobs[k];
    if (j) {
      char brief[400];
      sr_job_briefing(g->city, j, brief, sizeof brief);
      text_fit(ui, x, (float)ui->h - m - line * 3.0f, brief, k_cyan, s, (float)ui->w - 2 * x);
    }
  }
  for (int i = 0; i < mn->count && mn->encounter; i++) {
    if (i == SR_ENC_PAY)
      snprintf(buf, sizeof buf, "Pay them off  %d CRED", g->enc.payoff);
    else
      snprintf(buf, sizeof buf, "%s", i == SR_ENC_FIGHT ? "Fight" : "Run");
    if (i == mn->sel) sr_ui_rect(ui, x - 4, y - 2, (float)ui->w - 2 * x + 8, line, (SrColor){110, 30, 30, 200});
    text_fit(ui, x, y, buf, i == mn->sel ? k_amber : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  for (int i = 0; i < mn->count && !mn->job_menu && !mn->encounter; i++) {
    char name[SR_NAME_LEN];
    if (mn->node_menu)
      snprintf(name, sizeof name, "%s", sr_node_action_name((SrNodeAction)mn->action[i]));
    else if (mn->housing)
      snprintf(name, sizeof name, "%s, rent/week", sr_housing_def(mn->items[i].type)->name);
    else
      sr_item_name(g->city, &mn->items[i], name, sizeof name);
    if (mn->node_menu)
      snprintf(buf, sizeof buf, "%s", name);
    else
      snprintf(buf, sizeof buf, "%s%s  %d", mn->jailbreak[i] >= 0 ? "JAILBREAK " : "", name, mn->price[i]);
    if (i == mn->sel) sr_ui_rect(ui, x - 4, y - 2, (float)ui->w - 2 * x + 8, line, (SrColor){40, 60, 110, 200});
    text_fit(ui, x, y, buf, i == mn->sel ? k_amber : k_white, s, (float)ui->w - 2 * x);
    y += line;
  }
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  const char *hint = mn->job_menu ? (touch ? "TAP TO SELECT, TAP AGAIN TO ACCEPT  //  TAP HERE TO LEAVE"
                                           : "W/S SELECT  E ACCEPT  ESC LEAVE    G GHOST F FACE R RUNNER H HAMMER")
                     : touch      ? "TAP A ROW TO BUY  //  TAP HERE TO LEAVE"
                                  : "W/S SELECT  E BUY  ESC LEAVE";
  text_fit(ui, x, (float)ui->h - m - line, hint, k_amber, s, (float)ui->w - 2 * x);
}

int sr_ui_shop_row_at(const SrUi *ui, float y) {
  float m = 10.0f * (float)ui->scale, line = 11.0f * (float)ui->scale;
  float top = m + 6.0f * (float)ui->scale + line * 1.5f - 2;
  if (y < top) return -1;
  return (int)((y - top) / line);
}

void sr_ui_build(SrUi *ui, const SrGame *g, const SrInput *in, bool portrait) {
  int s = ui->scale;
  if (portrait && in && in->scheme == SR_SCHEME_TOUCH) {
    sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){0, 0, 0, 230});
    centered(ui, (float)ui->h * 0.45f, "PLEASE ROTATE", k_cyan, s);
    centered(ui, (float)ui->h * 0.45f + 12.0f * (float)s, "YOUR DEVICE", k_cyan, s);
    return;
  }
  switch (g->screen) {
    case SR_SCREEN_TITLE:
      sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){5, 3, 12, 255});
      centered(ui, (float)ui->h * 0.3f, "SILVERROCK", k_cyan, s * 2);
      centered(ui, (float)ui->h * 0.3f + 20.0f * (float)s, "The sun hasn't risen in forty years.", k_white, s);
      centered(ui, (float)ui->h * 0.3f + 30.0f * (float)s, "Rent is due on Friday.", k_white, s);
      centered(ui, (float)ui->h * 0.7f, in && in->scheme == SR_SCHEME_TOUCH ? "TAP TO START" : "PRESS ENTER TO START",
               k_amber, s);
      break;
    case SR_SCREEN_PLAY:
      if (g->dive.active)
        matrix_hud(ui, g, in);
      else
        hud(ui, g, in);
      break;
    case SR_SCREEN_PAUSE:
      hud(ui, g, in);
      sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){0, 0, 0, 160});
      centered(ui, (float)ui->h * 0.4f, "PAUSED", k_cyan, s * 2);
      centered(ui, (float)ui->h * 0.4f + 22.0f * (float)s,
               in && in->scheme == SR_SCHEME_TOUCH ? "TAP TO RESUME" : "PRESS ESC TO RESUME", k_white, s);
      break;
    case SR_SCREEN_TERMINAL: terminal(ui, g); break;
    case SR_SCREEN_SHOP: shop_screen(ui, g, in); break;
    case SR_SCREEN_CREATE: create_screen(ui, g, in); break;
    case SR_SCREEN_DEAD:
      sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){30, 0, 0, 240});
      centered(ui, (float)ui->h * 0.4f, "FLATLINED", k_red, s * 2);
      centered(ui, (float)ui->h * 0.4f + 22.0f * (float)s, "Hardcore: your save is gone.", k_white, s);
      break;
  }
  if (in && in->scheme == SR_SCHEME_TOUCH && g->screen == SR_SCREEN_PLAY) touch_overlay(ui, g, in);
}
