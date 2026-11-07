/* Signs and adverts: shop names in the bitmap font, blade signs in a made-up neon script, billboards
 * for the city's corporations that follow the board, holo-ads, LED news tickers and wanted posters. */
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "render/sr_render.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- text ------------------------------------------------------------------------------------- */

void sr_mesh_text(SrMesh *m, SrVec3 o, SrVec3 right, SrVec3 up, float h, const char *s, SrColor col, int em) {
  const float inv = 1.0f / SR_TEX_SIZE, gw = h * 5.0f / 7.0f, adv = h * 6.0f / 7.0f;
  for (int i = 0; s[i]; i++) {
    if (s[i] == ' ') continue;
    int cx, cy;
    sr_font_cell(s[i], &cx, &cy);
    float u0 = (float)cx * inv, v0 = (float)cy * inv, u1 = (float)(cx + 5) * inv, v1 = (float)(cy + 7) * inv;
    SrVec3 a = sr_v3_add(o, sr_v3_scale(right, adv * (float)i));
    SrVec3 b = sr_v3_add(a, sr_v3_scale(right, gw));
    SrVec3 cc = sr_v3_add(b, sr_v3_scale(up, h)), d = sr_v3_add(a, sr_v3_scale(up, h));
    sr_mesh_quad(m, a, b, cc, d, u0, v1, u1, v0, col, SR_LAYER_FONT, em, 0);
  }
}

float sr_text_width(const char *s, float h) { return h * 6.0f / 7.0f * (float)strlen(s); }

/* ---- shop names and blade signs -------------------------------------------------------------- */

void sr_build_shop_name(SrMesh *m, const SrBuilding *b, SrColor neon, bool lit) {
  char name[19];
  snprintf(name, sizeof name, "%s", b->name);
  for (char *p = name; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
  float h = 0.42f, w = sr_text_width(name, h), cx = (float)(b->x0 + b->x1) / 200.0f, z = (float)b->y1 / 100.0f;
  float maxw = (float)(b->x1 - b->x0) / 100.0f - 1.0f;
  if (w > maxw) { /* shrink long names to fit the front, never below legibility */
    h = SR_MAX(0.3f, h * maxw / w);
    w = sr_text_width(name, h);
  }
  float y = 5.6f;
  sr_mesh_quad(m, sr_v3(cx - w / 2 - 0.25f, y - 0.15f, z + 0.12f), sr_v3(cx + w / 2 + 0.25f, y - 0.15f, z + 0.12f),
               sr_v3(cx + w / 2 + 0.25f, y + h + 0.15f, z + 0.12f), sr_v3(cx - w / 2 - 0.25f, y + h + 0.15f, z + 0.12f),
               0, 0, 1, 1, (SrColor){16, 16, 20, 255}, SR_LAYER_WHITE, 0, 0); /* dark plate */
  sr_mesh_text(m, sr_v3(cx - w / 2, y, z + 0.16f), sr_v3(1, 0, 0), sr_v3(0, 1, 0), h, name, neon, lit ? 255 : 40);
}

/* Vertical signs in the neon script on the front corners of some buildings. */
void sr_build_blade_signs(const SrCity *c, int bi, SrMesh *m) {
  const SrBuilding *b = &c->buildings[bi];
  SrDistrictType t = c->districts[c->blocks[b->block].district].type;
  if (t != SR_D_COMMERCIAL && t != SR_D_ENTERTAINMENT && t != SR_D_OLDQUARTER) return;
  if (b->height_cm < 900) return;
  const SrPalette *pal = sr_palette(t);
  bool poor = t == SR_D_OLDQUARTER;
  for (int corner = 0; corner < 4; corner++) { /* front and back faces, both ends */
    uint64_t h = sr_mix64(c->seed ^ ((uint64_t)b->id * 0x9E37u) ^ (uint64_t)(corner * 77 + 5));
    if ((h % 100u) >= (t == SR_D_ENTERTAINMENT ? 45u : 28u)) continue;
    bool front = corner < 2;
    float x = (corner % 2) ? (float)b->x1 / 100.0f - 0.6f : (float)b->x0 / 100.0f + 0.6f;
    float face = front ? (float)b->y1 / 100.0f : (float)b->y0 / 100.0f, dir = front ? 1.0f : -1.0f;
    float z = face + dir * 0.75f; /* the plate sticks out from the facade into the street */
    int glyphs = 3 + (int)((h >> 8) % 4u);
    float gh = 1.1f, y0 = 4.8f, top = y0 + gh * (float)glyphs + 0.3f;
    if (top > (float)b->height_cm / 100.0f - 0.5f) continue;
    SrColor col = pal->neon[(h >> 12) % 3u];
    sr_mesh_box(m, sr_v3(x - 0.08f, y0 - 0.2f, z - 0.6f), sr_v3(x + 0.08f, top, z + 0.6f), (SrColor){20, 20, 26, 255},
                SR_LAYER_WHITE, 0, 0);
    int dead = (int)((h >> 20) % 100u) < (poor ? 20 : 8) ? (int)((h >> 28) % (uint64_t)glyphs) : -1; /* broken neon */
    uint8_t seed = (uint8_t)(0x80u | ((h >> 34) & 63u));
    for (int g = 0; g < glyphs; g++) {
      SrAtlasCell a = sr_script_cell((int)((h >> (g * 6)) & 63u));
      float y = top - 0.3f - gh * (float)(g + 1);
      int em = g == dead ? 25 : 255;
      for (int side = 0; side < 2; side++) { /* readable from both directions along the street */
        float sx = side ? x + 0.1f : x - 0.1f;
        int first = m->nv;
        if (side)
          sr_mesh_quad(m, sr_v3(sx, y, z + 0.45f), sr_v3(sx, y, z - 0.45f), sr_v3(sx, y + gh * 0.9f, z - 0.45f),
                       sr_v3(sx, y + gh * 0.9f, z + 0.45f), a.u0, a.v1, a.u1, a.v0, col, a.layer, em, 0);
        else
          sr_mesh_quad(m, sr_v3(sx, y, z - 0.45f), sr_v3(sx, y, z + 0.45f), sr_v3(sx, y + gh * 0.9f, z + 0.45f),
                       sr_v3(sx, y + gh * 0.9f, z - 0.45f), a.u0, a.v1, a.u1, a.v0, col, a.layer, em, 0);
        for (int v = first; v < m->nv; v++) m->v[v].pad = seed; /* flicker seed (motion) */
      }
    }
  }
}

/* ---- adverts ---------------------------------------------------------------------------------- */

void sr_product_name(const SrCity *c, int corp, uint32_t seed, char *out, size_t cap) {
  static const char *const pre[16] = {"NOVA", "HEX",  "LUMI", "KYRO", "VANTA", "OPTI", "SYNTH", "HALO",
                                      "ZETA", "PULS", "AURA", "VOLT", "NEXA",  "ORBI", "FLUX",  "CORE"};
  static const char *const suf[16] = {"COLA",  "GEL",  "LINK",  "DREAM", "SLEEP", "SHIELD", "CHROME", "RUSH",
                                      "GRAIN", "CURE", "SIGHT", "BLISS", "GRIP",  "SPARK",  "VAULT",  "GLOW"};
  uint64_t h = sr_mix64(c->seed ^ ((uint64_t)(unsigned)corp << 40) ^ seed);
  snprintf(out, cap, "%s%s", pre[h % 16u], suf[(h >> 8) % 16u]);
}

void sr_ads_context(SrAdContext *a, const SrChess *ch, const SrCity *c, int64_t now) {
  memset(a, 0, sizeof *a);
  a->c = c;
  a->now = now;
  for (int i = 0; ch && i < SR_MAX_NEWS && i < ch->news.count; i++) {
    const SrNews *n = &ch->news.items[i];
    if (n->scandal && n->subject >= 0 && n->subject < c->factions.corp_count && n->time >= now - 7 * 86400 &&
        n->time <= now)
      a->scandal_mask |= 1u << n->subject;
  }
  a->leader = -1;
  if (ch) {
    int leader = -1;
    float share = sr_chess_dominance(ch, c, &leader);
    if (share > 0.25f) a->leader = leader;
  }
  int count[SR_MAX_DISTRICTS][SR_MAX_FACTIONS];
  memset(count, 0, sizeof count);
  uint64_t own = 0;
  for (int i = 0; i < c->building_count; i++) {
    const SrBuilding *b = &c->buildings[i];
    if (b->owner < 0 || b->owner >= c->factions.corp_count) continue;
    int d = c->blocks[b->block].district;
    if (d < 0 || d >= SR_MAX_DISTRICTS) continue;
    count[d][b->owner]++;
    a->owners[d] |= 1u << b->owner;
    own = own * 31u + (uint64_t)(i * 7 + b->owner);
  }
  for (int d = 0; d < SR_MAX_DISTRICTS; d++) {
    a->main_owner[d] = -1;
    int best = 0;
    for (int f = 0; f < c->factions.corp_count; f++)
      if (count[d][f] > best) {
        best = count[d][f];
        a->main_owner[d] = (int8_t)f;
      }
  }
  a->key = sr_mix64(own ^ ((uint64_t)a->scandal_mask << 20) ^ (uint64_t)(a->leader + 1) << 52);
}

SrAd sr_ad_for_slot(const SrAdContext *a, const SrBillboardSlot *s, int district) {
  SrAd ad = {-1, false, true};
  const SrCity *c = a->c;
  if (c->factions.corp_count <= 0) return ad;
  int corp = (int)(s->seed % (uint32_t)c->factions.corp_count);
  bool empty = s->empty != 0;
  if (empty && a->leader >= 0) { /* a dominant corp buys up the empty slots */
    corp = a->leader;
    empty = false;
  }
  if (empty) return ad;
  /* a corp with nothing left in the district (it lost its facilities) loses its slots there */
  if (district >= 0 && district < SR_MAX_DISTRICTS && a->owners[district] && !(a->owners[district] & (1u << corp)))
    corp = a->main_owner[district];
  if (corp < 0) return ad;
  ad.corp = corp;
  ad.empty = false;
  ad.vandal = (a->scandal_mask & (1u << corp)) && ((s->seed >> 7) & 1u); /* about half after a scandal */
  return ad;
}

void sr_build_billboard(SrMesh *m, const SrCity *c, const SrBillboardSlot *s, SrAd ad) {
  float cy = cosf(s->yaw), sy = sinf(s->yaw);
  SrVec3 right = sr_v3(cy, 0, -sy), up = sr_v3(0, 1, 0), out = sr_v3(sy, 0, cy);
  SrVec3 o = sr_v3_add(sr_v3(s->x, s->y, s->z), sr_v3_scale(right, -s->w * 0.5f)); /* bottom left */
  SrColor frame = {40, 42, 48, 255};
  if (s->kind == SR_SLOT_ROOF) { /* legs down to the roof */
    for (int k = 0; k < 2; k++) {
      SrVec3 lp = sr_v3_add(o, sr_v3_scale(right, s->w * (k ? 0.85f : 0.15f)));
      sr_mesh_box(m, sr_v3(lp.x - 0.1f, s->y - 1.5f, lp.z - 0.1f), sr_v3(lp.x + 0.1f, s->y, lp.z + 0.1f), frame,
                  SR_LAYER_METAL, 0, 0);
    }
  }
  SrVec3 a = sr_v3_add(o, sr_v3_scale(out, 0.05f)), b = sr_v3_add(a, sr_v3_scale(right, s->w));
  SrVec3 cc = sr_v3_add(b, sr_v3_scale(up, s->h)), d = sr_v3_add(a, sr_v3_scale(up, s->h));
  if (ad.empty) { /* "space for rent": a dark panel */
    sr_mesh_quad(m, a, b, cc, d, 0, 1, 1, 0, (SrColor){30, 30, 34, 255}, SR_LAYER_CONCRETE, 0, 0);
    return;
  }
  SrColor corp = sr_corp_color(c->factions.f[ad.corp].hue);
  SrAtlasCell lay = sr_ad_cell(SR_LAYER_ADS, (int)((s->seed >> 3) % SR_AD_LAYOUTS));
  int em = ad.vandal ? 120 : 210;
  sr_mesh_quad(m, a, b, cc, d, lay.u0, lay.v1, lay.u1, lay.v0, corp, lay.layer, em, 0);
  /* the corp's logo on the left, the product's name along the bottom */
  float ls = s->h * 0.55f;
  SrAtlasCell ic = sr_icon_cell(SR_POI_CORP_HQ);
  SrVec3 la = sr_v3_add(sr_v3_add(a, sr_v3_scale(out, 0.03f)),
                        sr_v3_add(sr_v3_scale(right, s->h * 0.15f), sr_v3_scale(up, s->h * 0.35f)));
  SrVec3 lb = sr_v3_add(la, sr_v3_scale(right, ls)), lc = sr_v3_add(lb, sr_v3_scale(up, ls)),
         ld = sr_v3_add(la, sr_v3_scale(up, ls));
  sr_mesh_quad(m, la, lb, lc, ld, ic.u0, ic.v1, ic.u1, ic.v0, (SrColor){255, 255, 255, 255}, ic.layer, em, 0);
  char name[24];
  sr_product_name(c, ad.corp, s->seed, name, sizeof name);
  float th = SR_MIN(s->h * 0.18f, (s->w * 0.9f) / ((float)strlen(name) * 6.0f / 7.0f));
  SrVec3 to = sr_v3_add(sr_v3_add(a, sr_v3_scale(out, 0.04f)),
                        sr_v3_add(sr_v3_scale(right, s->w * 0.05f), sr_v3_scale(up, s->h * 0.08f)));
  sr_mesh_text(m, to, right, up, th, name, (SrColor){255, 255, 255, 255}, em);
  if (ad.vandal) { /* sprayed over, torn or cracked */
    SrAtlasCell v = sr_ad_cell(SR_LAYER_ADS, SR_AD_VANDAL + (int)((s->seed >> 11) % 3u));
    SrVec3 va = sr_v3_add(a, sr_v3_scale(out, 0.06f)), vb = sr_v3_add(va, sr_v3_scale(right, s->w));
    SrVec3 vc = sr_v3_add(vb, sr_v3_scale(up, s->h)), vd = sr_v3_add(va, sr_v3_scale(up, s->h));
    sr_mesh_quad(m, va, vb, vc, vd, v.u0, v.v1, v.u1, v.v0, (SrColor){200, 40, 160, 255}, v.layer, 90, 0);
  }
  /* a lamp over the panel */
  SrVec3 lamp = sr_v3_add(sr_v3_add(d, sr_v3_scale(right, s->w * 0.5f)), sr_v3_scale(out, 0.6f));
  sr_mesh_box(m, sr_v3(lamp.x - 0.3f, lamp.y, lamp.z - 0.3f), sr_v3(lamp.x + 0.3f, lamp.y + 0.15f, lamp.z + 0.3f),
              (SrColor){255, 240, 210, 255}, SR_LAYER_WHITE, 0, 200);
}

/* ---- holo-ads, tickers, wanted posters -------------------------------------------------------- */

void sr_build_holo_ads(const SrCity *c, const SrAdContext *ads, SrVec3 eye, float time_s, SrMesh *out) {
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return;
  SrBillboard bb = sr_billboard(eye, 0, 0);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      int b = by * c->grid + bx;
      if (c->blocks[b].water) continue;
      SrDistrictType t = c->districts[c->blocks[b].district].type;
      if (t != SR_D_SPIRE && t != SR_D_COMMERCIAL && t != SR_D_ENTERTAINMENT) continue;
      int32_t ox, oy;
      sr_city_block_origin(b, c->grid, &ox, &oy);
      uint64_t h = sr_mix64(c->seed ^ ((uint64_t)b * 0x51AFu));
      SrBillboardSlot slot = {0};
      slot.seed = (uint32_t)(h >> 16);
      SrAd ad = sr_ad_for_slot(ads, &slot, c->blocks[b].district);
      if (ad.empty) continue;
      SrColor col = sr_corp_color(c->factions.f[ad.corp].hue);
      float bob = sinf(time_s * 0.8f + (float)(h % 7u)) * 0.3f;
      SrVec3 p = sr_v3((float)ox / 100.0f + (float)SR_STREET_CM / 100.0f + 3.0f, 9.0f + bob,
                       (float)oy / 100.0f + (float)SR_STREET_CM / 100.0f + 3.0f);
      SrColor dimc = {(uint8_t)(col.r * 3 / 5), (uint8_t)(col.g * 3 / 5), (uint8_t)(col.b * 3 / 5), 255};
      sr_mesh_sprite(out, &bb, p, 5.0f, 3.6f, SR_SPR_HOLO, SR_SPRITE_FACING, 0, dimc, 255);
      /* the logo floating inside it, turning slowly */
      float ang = time_s * 0.6f + (float)(h % 11u), s = 1.2f, cx = cosf(ang) * s, cz = sinf(ang) * s;
      SrAtlasCell ic = sr_icon_cell(SR_POI_CORP_HQ);
      sr_mesh_quad(out, sr_v3(p.x - cx, p.y - s, p.z - cz), sr_v3(p.x + cx, p.y - s, p.z + cz),
                   sr_v3(p.x + cx, p.y + s, p.z + cz), sr_v3(p.x - cx, p.y + s, p.z - cz), ic.u0, ic.v1, ic.u1, ic.v0,
                   col, ic.layer, 255, 0);
    }
}

/* A scrolling LED band showing text; `ox` is the band's left end, it faces +z turned by yaw. */
static void ticker(SrMesh *m, SrVec3 o, float yaw, float w, const char *text, float time_s) {
  float cy = cosf(yaw), sy = sinf(yaw);
  SrVec3 right = sr_v3(cy, 0, -sy), up = sr_v3(0, 1, 0), out = sr_v3(sy, 0, cy);
  SrAtlasCell led = sr_ad_cell(SR_LAYER_ADS, SR_AD_LED);
  SrVec3 a = o, b = sr_v3_add(o, sr_v3_scale(right, w)), c = sr_v3_add(b, sr_v3_scale(up, 0.7f)),
         d = sr_v3_add(a, sr_v3_scale(up, 0.7f));
  sr_mesh_quad(m, a, b, c, d, led.u0, led.v1, led.u0 + (led.u1 - led.u0) * w, led.v0, (SrColor){40, 26, 10, 255},
               led.layer, 120, 0);
  char line[160];
  snprintf(line, sizeof line, "%s   +++   ", text[0] ? text : "NIGHT CITY NEWS");
  for (char *p = line; *p; p++)
    if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 'a' + 'A');
  int len = (int)strlen(line);
  float gh = 0.45f, adv = gh * 6.0f / 7.0f;
  int visible = (int)(w / adv) - 1;
  int start = (int)(time_s * 4.0f) % len; /* four characters a second */
  char win[96];
  for (int i = 0; i < visible && i < 95; i++) win[i] = line[(start + i) % len];
  win[SR_MIN(visible, 95)] = '\0';
  sr_mesh_text(m,
               sr_v3_add(sr_v3_add(a, sr_v3_scale(out, 0.03f)),
                         sr_v3_add(sr_v3_scale(right, adv * 0.5f), sr_v3_scale(up, 0.12f))),
               right, up, gh, win, (SrColor){255, 170, 50, 255}, 255);
}

int sr_build_tickers(const SrCity *c, const char *headline, SrVec3 eye, float time_s, SrMesh *out) {
  int n = 0;
  for (int i = 0; i < c->building_count; i++) { /* media buildings */
    const SrBuilding *b = &c->buildings[i];
    if (b->poi != SR_POI_MEDIA) continue;
    float cx = (float)(b->x0 + b->x1) / 200.0f, z = (float)b->y1 / 100.0f + 0.1f;
    if (fabsf(cx - eye.x) > 80.0f || fabsf(z - eye.z) > 80.0f) continue;
    float w = SR_MIN(14.0f, (float)(b->x1 - b->x0) / 100.0f - 1.0f);
    ticker(out, sr_v3(cx - w / 2, 6.8f, z), 0.0f, w, headline, time_s);
    n++;
  }
  for (int s = 0; s < c->station_count; s++) { /* over the metro entrances */
    int32_t ox, oy;
    sr_city_block_origin(c->stations[s].block, c->grid, &ox, &oy);
    float x = (float)ox / 100.0f + 4.0f, z = (float)oy / 100.0f + 4.0f;
    if (fabsf(x - eye.x) > 80.0f || fabsf(z - eye.z) > 80.0f) continue;
    SrColor post = {50, 52, 58, 255};
    sr_mesh_box(out, sr_v3(x - 3.1f, 0, z - 0.1f), sr_v3(x - 2.9f, 4.6f, z + 0.1f), post, SR_LAYER_METAL, 0, 0);
    sr_mesh_box(out, sr_v3(x + 2.9f, 0, z - 0.1f), sr_v3(x + 3.1f, 4.6f, z + 0.1f), post, SR_LAYER_METAL, 0, 0);
    ticker(out, sr_v3(x - 3.0f, 4.0f, z + 0.12f), 0.0f, 6.0f, headline, time_s);
    sr_mesh_text(out, sr_v3(x - 1.3f, 4.75f, z + 0.12f), sr_v3(1, 0, 0), sr_v3(0, 1, 0), 0.5f, "METRO",
                 (SrColor){90, 200, 255, 255}, 255);
    n++;
  }
  return n;
}

int sr_build_wanted(const SrCity *c, int district, int heat, SrColor outfit, SrVec3 eye, SrMesh *out) {
  if (heat < 3 || district < 0) return 0;
  int blk = sr_city_block_at(c, (int32_t)(eye.x * 100.0f), (int32_t)(eye.z * 100.0f));
  if (blk < 0) return 0;
  int n = 0;
  SrAtlasCell w = sr_atlas_cell(SR_LAYER_DECALS, SR_DECAL_WANTED);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int bx = blk % c->grid + dx, by = blk / c->grid + dy;
      if (bx < 0 || by < 0 || bx >= c->grid || by >= c->grid) continue;
      const SrBlock *b = &c->blocks[by * c->grid + bx];
      if (b->water || b->district != district) continue;
      for (int i = 0; i < b->building_count; i++) {
        const SrBuilding *bd = &c->buildings[b->first_building + i];
        if (bd->poi != SR_POI_NONE) continue;
        float z = (float)bd->y1 / 100.0f + 0.07f;
        for (int k = 0; k < 2; k++) { /* two posters on the ground floor's front */
          float x = (float)bd->x0 / 100.0f + 1.5f + 1.3f * (float)k;
          sr_mesh_quad(out, sr_v3(x, 1.2f, z), sr_v3(x + 0.8f, 1.2f, z), sr_v3(x + 0.8f, 2.3f, z), sr_v3(x, 2.3f, z),
                       w.u0, w.v1, w.u1, w.v0, outfit, w.layer, 40, 0);
          n++;
        }
      }
    }
  return n;
}
