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
  sr_mesh_quad(&ui->mesh, sr_v3(x, y + h, 0), sr_v3(x + w, y + h, 0), sr_v3(x + w, y, 0),
               sr_v3(x, y, 0), 0, 0, 1, 1, c, SR_LAYER_WHITE, 255, 0);
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
    sr_mesh_quad(&ui->mesh, sr_v3(x, y + h, 0), sr_v3(x + w, y + h, 0), sr_v3(x + w, y, 0),
                 sr_v3(x, y, 0), u0, v1, u1, v0, c, SR_LAYER_FONT, 255, 0);
  }
}

void sr_ui_icon(SrUi *ui, float x, float y, float size, int poi, SrColor c) {
  sr_mesh_quad(&ui->mesh, sr_v3(x, y + size, 0), sr_v3(x + size, y + size, 0), sr_v3(x + size, y, 0),
               sr_v3(x, y, 0), 0, 1, 1, 0, c, SR_LAYER_ICON_FIRST + poi, 255, 0);
}

const char *sr_ui_context_label(SrContextKind k) {
  switch (k) {
  case SR_CTX_FOOD: return "EAT";
  case SR_CTX_SHOP: return "SHOP";
  case SR_CTX_FIXER: return "TALK TO FIXER";
  case SR_CTX_HOME: return "SLEEP";
  case SR_CTX_METRO: return "RIDE METRO";
  case SR_CTX_CAPSULE: return "RENT CAPSULE";
  case SR_CTX_JOB_TARGET: return "DO THE JOB";
  case SR_CTX_JOB_REPORT: return "REPORT BACK";
  case SR_CTX_CLINIC: return "CLINIC";
  case SR_CTX_LANDLORD: return "PAY LANDLORD";
  case SR_CTX_CLOSED: return "CLOSED";
  default: return "";
  }
}

static void bar(SrUi *ui, float x, float y, float w, float h, float frac, SrColor c) {
  sr_ui_rect(ui, x, y, w, h, k_panel);
  sr_ui_rect(ui, x + 1, y + 1, (w - 2) * SR_CLAMP(frac, 0.0f, 1.0f), h - 2, c);
}

static void hud(SrUi *ui, const SrGame *g) {
  const SrPlayer *p = &g->player;
  int s = ui->scale;
  float m = 6.0f * (float)s;
  char buf[160];
  int64_t t = sr_game_time(g);
  sr_time_format(t, buf, sizeof buf);
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
  /* district and weather, top right */
  int d = sr_city_district_at(g->city, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  snprintf(buf, sizeof buf, "%s", d >= 0 ? g->city->districts[d].name : "Outside the city");
  sr_ui_text(ui, (float)ui->w - m - (float)sr_ui_text_width(buf, s), m, buf, k_white, s);
  snprintf(buf, sizeof buf, "%s", sr_weather_name(sr_weather_at(g->seed, t).to));
  sr_ui_text(ui, (float)ui->w - m - (float)sr_ui_text_width(buf, s), m + 10.0f * (float)s, buf, k_cyan, s);
  /* compass with megacorp landmarks */
  float cw = (float)ui->w * 0.4f, cx = (float)ui->w / 2;
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
  /* crosshair */
  sr_ui_rect(ui, (float)ui->w / 2 - (float)s, (float)ui->h / 2 - (float)s, 2.0f * (float)s, 2.0f * (float)s, k_white);
  /* context prompt */
  const char *label = sr_ui_context_label(g->ctx.kind);
  if (label[0]) {
    snprintf(buf, sizeof buf, "[E] %s", label);
    float tw = (float)sr_ui_text_width(buf, s);
    sr_ui_rect(ui, (float)ui->w / 2 - tw / 2 - 4, (float)ui->h * 0.62f - 4, tw + 8, 7.0f * (float)s + 8, k_panel);
    sr_ui_text(ui, (float)ui->w / 2 - tw / 2, (float)ui->h * 0.62f, buf, k_amber, s);
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
      sr_ui_text(ui, m, (float)ui->h - m - 7.0f * (float)s, buf, k_white, s);
    }
  }
  if (g->toast_time > 0) {
    float tw = (float)sr_ui_text_width(g->toast, s);
    sr_ui_rect(ui, (float)ui->w / 2 - tw / 2 - 4, (float)ui->h * 0.25f - 4, tw + 8, 7.0f * (float)s + 8, k_panel);
    sr_ui_text(ui, (float)ui->w / 2 - tw / 2, (float)ui->h * 0.25f, g->toast, k_white, s);
  }
}

static void centered(SrUi *ui, float y, const char *txt, SrColor c, int s) {
  sr_ui_text(ui, (float)ui->w / 2 - (float)sr_ui_text_width(txt, s) / 2, y, txt, c, s);
}

static void terminal(SrUi *ui, const SrGame *g) {
  int s = ui->scale;
  float m = 10.0f * (float)s, line = 10.0f * (float)s;
  sr_ui_rect(ui, m, m, (float)ui->w - 2 * m, (float)ui->h - 2 * m, (SrColor){4, 14, 8, 235});
  float x = m + 6.0f * (float)s, y = m + 6.0f * (float)s;
  SrColor green = {80, 255, 120, 255};
  char buf[200];
  snprintf(buf, sizeof buf, "TERMINAL // %s // %s", g->player.name, g->city->name);
  sr_ui_text(ui, x, y, buf, green, s);
  y += line * 1.5f;
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
    snprintf(buf, sizeof buf, "%s: %s", g->player.msgs[i].from, g->player.msgs[i].text);
    buf[(ui->w - 2 * (int)x) / (6 * s)] = '\0';
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
  sr_ui_text(ui, x, (float)ui->h - m - line, "[ESC] CLOSE", green, s);
}

static void touch_overlay(SrUi *ui, const SrInput *in) {
  const SrTouchLayout *l = &in->layout;
  float k = ui->px_per_css;
  for (int i = 0; i < SR_TOUCH_BUTTONS; i++) {
    const SrTouchButton *b = &l->buttons[i];
    float sz = b->size * k;
    SrColor c = in->touch_down[b->action] ? (SrColor){80, 230, 255, 200} : (SrColor){20, 30, 50, 150};
    sr_ui_rect(ui, b->x * k - sz / 2, b->y * k - sz / 2, sz, sz, c);
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
    sr_ui_rect(ui, cx + in->stick_x * r - r / 3, cy - in->stick_y * r - r / 3, r / 1.5f, r / 1.5f, (SrColor){80, 230, 255, 200});
  }
  if (in->wheel_open) {
    float cx = (float)ui->w * 0.7f, cy = (float)ui->h * 0.5f;
    for (int i = 0; i < 4; i++)
      sr_ui_rect(ui, cx + (float)(i % 2) * 60 * k, cy + (float)(i / 2) * 60 * k, 56 * k, 56 * k,
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
    sr_ui_text(ui, x, y, buf, b == g->create_bg ? k_amber : k_white, s);
    y += line;
  }
  const SrBackgroundDef *d = sr_background_def(g->create_bg);
  y += line * 0.5f;
  for (int a = 0; a < SR_ATTR_COUNT; a++) {
    sr_ui_text(ui, x, y, sr_attr_name((SrAttr)a), k_white, s);
    bar(ui, x + 50.0f * (float)s, y, 60.0f * (float)s, 6.0f * (float)s, d->attr[a] / 10.0f, k_cyan);
    y += line * 0.8f;
  }
  snprintf(buf, sizeof buf, "TRAINED: %s, %s", sr_skill_name((SrSkill)d->trained[0]), sr_skill_name((SrSkill)d->trained[1]));
  sr_ui_text(ui, x, y + line * 0.3f, buf, k_cyan, s);
  /* appearance preview: a portrait built from the generated parts */
  SrAppearance a = sr_game_create_preview(g);
  static const SrColor skin[8] = {{250, 210, 180, 255}, {230, 180, 140, 255}, {200, 150, 110, 255},
                                  {170, 120, 80, 255}, {140, 95, 60, 255}, {110, 70, 45, 255},
                                  {80, 52, 35, 255}, {60, 40, 28, 255}};
  float px = (float)ui->w * 0.7f, py = (float)ui->h * 0.25f, u = 6.0f * (float)s;
  SrColor hair = sr_corp_color(a.hair_color * 2);
  SrColor coat = sr_corp_color(a.outfit_color + 8);
  sr_ui_rect(ui, px - 3 * u, py + 9 * u, 6 * u + (float)a.body * u, 9 * u, coat);
  sr_ui_rect(ui, px - 2 * u, py + 2 * u, 4 * u, 6 * u, skin[a.skin & 7]);
  if (a.hair % 12 < 4) sr_ui_rect(ui, px - u / 2, py - (float)(a.hair % 4) * u, u, 2 * u + (float)(a.hair % 4) * u, hair);
  else if (a.hair % 12 < 9) sr_ui_rect(ui, px - 2 * u, py + u, 4 * u, 1.5f * u, hair);
  if (a.cyber) sr_ui_rect(ui, px - 1.5f * u, py + 4 * u, 3 * u, 0.6f * u, k_red);
  bool touch = in && in->scheme == SR_SCHEME_TOUCH;
  sr_ui_text(ui, x, (float)ui->h - line * 1.5f,
             touch ? "TAP LEFT: BACKGROUND  RIGHT: LOOK  BOTTOM RIGHT: START"
                   : "1-5 BACKGROUND   R RANDOMISE LOOK   ENTER START", k_amber, s);
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
    centered(ui, (float)ui->h * 0.7f, in && in->scheme == SR_SCHEME_TOUCH ? "TAP TO START" : "PRESS ENTER TO START", k_amber, s);
    break;
  case SR_SCREEN_PLAY: hud(ui, g); break;
  case SR_SCREEN_PAUSE:
    hud(ui, g);
    sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){0, 0, 0, 160});
    centered(ui, (float)ui->h * 0.4f, "PAUSED", k_cyan, s * 2);
    centered(ui, (float)ui->h * 0.4f + 22.0f * (float)s,
             in && in->scheme == SR_SCHEME_TOUCH ? "TAP TO RESUME" : "PRESS ESC TO RESUME", k_white, s);
    break;
  case SR_SCREEN_TERMINAL: terminal(ui, g); break;
  case SR_SCREEN_CREATE: create_screen(ui, g, in); break;
  case SR_SCREEN_DEAD:
    sr_ui_rect(ui, 0, 0, (float)ui->w, (float)ui->h, (SrColor){30, 0, 0, 240});
    centered(ui, (float)ui->h * 0.4f, "FLATLINED", k_red, s * 2);
    centered(ui, (float)ui->h * 0.4f + 22.0f * (float)s, "Hardcore: your save is gone.", k_white, s);
    break;
  }
  if (in && in->scheme == SR_SCHEME_TOUCH && g->screen == SR_SCREEN_PLAY) touch_overlay(ui, in);
}
