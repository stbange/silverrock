/* Game-side glue for interiors, rooftops, gadgets, cyberware, the stash, the wardrobe and shops. */
#include "core/sr_log.h"
#include "core/sr_rng.h"
#include "core/sr_str.h"
#include "game/sr_game.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define EYE          1.7f
#define RADIUS       0.35f
#define REACH        1.7f
#define GRAPPLE_MAX  30.0f
#define JAILBREAK    400
#define ABILITY_CD   20.0f
#define APPROACH_M   25.0f
#define CONSOLE_RANK 2

static const SrBuilding *bld(const SrGame *g, int b) { return &g->city->buildings[b]; }

static int site_job_for(const SrGame *g, int building) {
  for (int j = 0; j < g->jobs.count; j++) {
    const SrJob *jb = &g->jobs.jobs[j];
    if (jb->state == SR_JS_ACCEPTED && !jb->executed && jb->target_building == building &&
        sr_job_is_site((SrJobType)jb->type))
      return jb->id;
  }
  return -1;
}

bool sr_game_enterable(const SrGame *g, int b) {
  if (b < 0 || b >= g->city->building_count) return false;
  if (b == g->player.home) return true;
  if (bld(g, b)->flags & SR_BF_MISSION) return true;
  return site_job_for(g, b) >= 0;
}

bool sr_game_inside(const SrGame *g) { return g->site.active; }

static void interior_for(SrGame *g, int b, SrInterior *out) {
  bool home = b == g->player.home;
  if (g->approach_building == b && g->approach.apartment == home && (!home || g->approach.tier == g->player.tier)) {
    *out = g->approach;
    return;
  }
  if (home)
    sr_interior_apartment(g->city, b, g->player.tier, out);
  else
    sr_interior_generate(g->city, b, out);
  g->approach = *out;
  g->approach_building = b;
}

static SrVec3 roof_point(const SrGame *g, int b) {
  const SrBuilding *d = bld(g, b);
  return sr_v3((float)(d->x0 + d->x1) / 200.0f, (float)d->height_cm / 100.0f + EYE, (float)(d->y0 + d->y1) / 200.0f);
}

static bool roof_entry(SrEntryType t) { return t == SR_ENTRY_ROOF || t == SR_ENTRY_VENT || t == SR_ENTRY_WINDOW; }

SrResult sr_game_enter(SrGame *g, int b, SrEntryType via) {
  if (g->site.active || !sr_game_enterable(g, b)) return SR_ERR_DENIED;
  if (b == g->player.home && !sr_player_can_use_home(&g->player)) {
    sr_game_toast(g, "Locked. You owe the landlord.");
    return SR_ERR_DENIED;
  }
  SrInterior in;
  interior_for(g, b, &in);
  int entry = -1;
  for (int e = 0; e < in.entry_count && entry < 0; e++)
    if (in.entries[e].type == via) entry = e;
  if (entry < 0 && roof_entry(via)) return SR_ERR_NOT_FOUND;
  if (entry < 0) entry = 0;
  const SrBuilding *d = bld(g, b);
  sr_site_open(&g->site, &in, b, (float)d->x0 / 100.0f, (float)d->y0 / 100.0f, &g->deltas, entry, site_job_for(g, b));
  g->site.entered_at = sr_game_time(g);
  float yaw = 0;
  g->player.pos = sr_site_entry_pos(&g->site, entry, &yaw);
  g->player.yaw = yaw;
  g->player.pitch = 0;
  g->roof_building = -1;
  g->floor_y = SR_SITE_FLOOR_Y;
  g->on_ground = true;
  g->vel_y = 0;
  sr_game_toast(g, in.apartment ? "Home." : g->site.job >= 0 ? "You're in. Find the objective." : "You step inside.");
  return SR_OK;
}

static void site_consequences(SrGame *g) {
  SrSite *s = &g->site;
  if (!s->alarm && !s->kills && !s->camera_seen && !(s->witnessed && s->hostile)) return;
  SrCrime cr;
  memset(&cr, 0, sizeof cr);
  cr.jurisdiction = (int8_t)sr_heat_jurisdiction(g->city, s->building);
  cr.severity = (uint8_t)(s->kills ? 5 : s->alarm ? 3 : 1);
  cr.witnessed = s->witnessed;
  cr.camera = s->camera_seen;
  cr.evidence_left = s->kills > 0;
  cr.look = sr_appearance_key(&g->player.look);
  cr.time = sr_game_time(g);
  sr_heat_crime(&g->heat, &cr);
  int owner = bld(g, s->building)->owner;
  if (owner >= 0 && (s->witnessed || s->camera_seen)) sr_progress_standing(&g->player.prog, owner, -15, true);
}

void sr_game_leave(SrGame *g) {
  SrSite *s = &g->site;
  if (!s->active) return;
  site_consequences(g);
  int b = s->building;
  bool roof = roof_entry((SrEntryType)s->in.entries[s->entry].type);
  sr_site_close(s);
  g->combat_count = 0;
  g->hostiles_near = 0;
  if (roof) {
    g->roof_building = b;
    g->floor_y = (float)bld(g, b)->height_cm / 100.0f;
    g->player.pos = roof_point(g, b);
  } else {
    sr_game_teleport_building(g, b);
  }
}

SrResult sr_game_climb_down(SrGame *g) {
  if (g->roof_building < 0) return SR_ERR_INVALID;
  int b = g->roof_building;
  sr_game_skip(g, 30, false);
  sr_game_teleport_building(g, b);
  sr_game_toast(g, "You take the fire escape down.");
  return SR_OK;
}

bool sr_game_blocked_here(const SrGame *g, float x, float z) {
  if (g->site.active) return sr_site_blocked(&g->site, x, z, RADIUS, g->player.crouched);
  if (g->roof_building >= 0) {
    const SrBuilding *d = bld(g, g->roof_building);
    float in = 0.4f;
    return x < (float)d->x0 / 100.0f + in || x > (float)d->x1 / 100.0f - in || z < (float)d->y0 / 100.0f + in ||
           z > (float)d->y1 / 100.0f - in;
  }
  return sr_game_blocked(g, x, z);
}

/* 2D ray against a footprint; returns the hit distance or -1. */
static float ray_box(float ox, float oz, float dx, float dz, const SrBuilding *b) {
  float x0 = (float)b->x0 / 100.0f, x1 = (float)b->x1 / 100.0f, z0 = (float)b->y0 / 100.0f, z1 = (float)b->y1 / 100.0f;
  float tmin = 0, tmax = 1e9f;
  float o[2] = {ox, oz}, d[2] = {dx, dz}, lo[2] = {x0, z0}, hi[2] = {x1, z1};
  for (int k = 0; k < 2; k++) {
    if (fabsf(d[k]) < 1e-6f) {
      if (o[k] < lo[k] || o[k] > hi[k]) return -1;
      continue;
    }
    float t1 = (lo[k] - o[k]) / d[k], t2 = (hi[k] - o[k]) / d[k];
    if (t1 > t2) {
      float t = t1;
      t1 = t2;
      t2 = t;
    }
    tmin = SR_MAX(tmin, t1);
    tmax = SR_MIN(tmax, t2);
    if (tmin > tmax) return -1;
  }
  return tmin;
}

SrResult sr_game_grapple(SrGame *g) {
  if (g->site.active) return SR_ERR_DENIED;
  SrPlayer *p = &g->player;
  SrVec3 f = sr_dir_from_yaw_pitch(p->yaw, 0);
  int blk = sr_city_block_at(g->city, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  if (blk < 0) return SR_ERR_NOT_FOUND;
  int bx = blk % g->city->grid, by = blk / g->city->grid;
  int best = -1;
  float bt = 1e9f;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int b2 = sr_city_block_index(g->city, bx + dx, by + dy);
      if (b2 < 0) continue;
      const SrBlock *bk = &g->city->blocks[b2];
      for (int i = 0; i < bk->building_count; i++) {
        int bi = bk->first_building + i;
        if (bi == g->roof_building) continue;
        float t = ray_box(p->pos.x, p->pos.z, f.x, f.z, bld(g, bi));
        if (t < 0 || t >= bt) continue;
        float rise = (float)bld(g, bi)->height_cm / 100.0f - (p->pos.y - EYE);
        if (sqrtf(t * t + rise * rise) > GRAPPLE_MAX) continue;
        bt = t;
        best = bi;
      }
    }
  if (best < 0) {
    sr_game_toast(g, "No grapple point in range.");
    return SR_ERR_NOT_FOUND;
  }
  const SrBuilding *d = bld(g, best);
  float x = p->pos.x + f.x * (bt + 1.0f), z = p->pos.z + f.z * (bt + 1.0f);
  x = sr_clampf(x, (float)d->x0 / 100.0f + 0.6f, (float)d->x1 / 100.0f - 0.6f);
  z = sr_clampf(z, (float)d->y0 / 100.0f + 0.6f, (float)d->y1 / 100.0f - 0.6f);
  g->roof_building = best;
  g->floor_y = (float)d->height_cm / 100.0f;
  p->pos = sr_v3(x, g->floor_y + EYE, z);
  g->on_ground = true;
  g->vel_y = 0;
  sr_progress_use_skill(&p->prog, SR_SK_ATHLETICS, SR_USE_SUCCESS, 5);
  sr_game_toast(g, "The line pulls you up onto the roof.");
  return SR_OK;
}

/* ---- cyberware ------------------------------------------------------------------------------ */

static int cyber_index(const SrGame *g, int type) {
  for (int i = 0; i < g->player.cyber_count; i++)
    if (g->player.cyber[i].kind == SR_IK_CYBER && g->player.cyber[i].type == type) return i;
  return -1;
}

bool sr_game_has_cyber(const SrGame *g, int type) {
  int i = cyber_index(g, type);
  return i >= 0 && !(g->cyber_offline & (1u << i));
}

int sr_game_cyber_load(const SrGame *g) { return sr_cyber_load(g->player.cyber, g->player.cyber_count); }

int sr_game_neural_capacity(const SrGame *g) {
  return sr_neural_capacity(g->player.prog.attr[SR_ATTR_BODY], g->player.prog.attr[SR_ATTR_TECH]);
}

static void apply_cyber_stats(SrGame *g) {
  SrPlayer *p = &g->player;
  p->armor = sr_game_has_cyber(g, 10) ? 0.3f : 0.0f;          /* Subdermal Armour */
  p->max_health = sr_game_has_cyber(g, 13) ? 130.0f : 100.0f; /* Second Heart */
  if (p->health > p->max_health) p->health = p->max_health;
}

bool sr_game_install_cyber(SrGame *g, int idx) {
  SrPlayer *p = &g->player;
  if (idx < 0 || idx >= p->inv.count || p->inv.items[idx].kind != SR_IK_CYBER || p->cyber_count >= SR_MAX_CYBER)
    return false;
  SrItem it = p->inv.items[idx];
  it.count = 1;
  int slot = sr_cyber_def(it.type)->slot;
  for (int i = 0; i < p->cyber_count; i++)
    if (sr_cyber_def(p->cyber[i].type)->slot == slot) return false;
  p->cyber[p->cyber_count++] = it;
  sr_inv_remove_at(&p->inv, idx, 1);
  apply_cyber_stats(g);
  if (sr_game_cyber_load(g) > sr_game_neural_capacity(g))
    sr_player_message(p, "Ripperdoc", "That's more chrome than your nerves can take. Expect glitches.",
                      sr_game_time(g));
  return true;
}

bool sr_game_ability(SrGame *g) {
  if (g->glitch_time > 0) return false;
  bool used = false;
  if (sr_game_has_cyber(g, 2)) { /* Thermal Optics toggle */
    g->thermal = !g->thermal;
    used = true;
  }
  if (g->ability_cd <= 0) {
    bool active = false;
    if (sr_game_has_cyber(g, 0)) { /* Reflex Booster */
      g->buff_reflex = SR_MAX(g->buff_reflex, 6.0f);
      active = true;
    }
    if (sr_game_has_cyber(g, 11)) { /* Chameleon Skin */
      g->buff_camo = SR_MAX(g->buff_camo, 10.0f);
      active = true;
    }
    if (active) g->ability_cd = ABILITY_CD;
    used = used || active;
  }
  return used;
}

/* ---- gadgets -------------------------------------------------------------------------------- */

static bool reusable(int gd) {
  return gd == SR_GD_GRAPPLE || gd == SR_GD_LANCE || gd == SR_GD_DRONE || gd == SR_GD_SPIDER || gd == SR_GD_CAMO ||
         gd == SR_GD_HOLOMASK;
}

static void street_noise(SrGame *g, SrVec3 at, float radius) {
  int reacted = sr_crowd_gunfire(&g->crowd, g->city, at, radius, &g->events, sr_game_time(g));
  const SrWorldEvent *ev = sr_events_recent(&g->events, 0);
  if (reacted > 0 && ev && ev->type == SR_EV_CRIME_REPORT && ev->time == sr_game_time(g)) {
    SrCrime cr;
    memset(&cr, 0, sizeof cr);
    cr.jurisdiction = (int8_t)g->city->factions.police;
    cr.severity = 2;
    cr.witnessed = true;
    cr.look = sr_appearance_key(&g->player.look);
    cr.time = sr_game_time(g);
    sr_heat_crime(&g->heat, &cr);
  }
}

bool sr_game_use_gadget(SrGame *g, int gd) {
  SrPlayer *p = &g->player;
  int idx = sr_inv_find(&p->inv, SR_IK_GADGET, gd);
  if (idx < 0) return false;
  const SrGadgetDef *d = sr_gadget_def(gd);
  bool used = false;
  switch (gd) {
    case SR_GD_MEDKIT:
    case SR_GD_TRAUMA:
    case SR_GD_PAINKILLER:
      p->health = SR_MIN(p->max_health, p->health - d->damage);
      if (gd == SR_GD_PAINKILLER) g->buff_pain = d->duration;
      used = true;
      break;
    case SR_GD_REFLEXSTIM: g->buff_reflex = SR_MAX(g->buff_reflex, d->duration), used = true; break;
    case SR_GD_FOCUSSTIM: g->buff_focus = d->duration, used = true; break;
    case SR_GD_CAMO: g->buff_camo = SR_MAX(g->buff_camo, d->duration), used = true; break;
    case SR_GD_HOLOMASK: g->buff_mask = d->duration, used = true; break;
    case SR_GD_FAKEID: sr_game_toast(g, "Keep the ID on you; it works at front desks."); return false;
    case SR_GD_GRAPPLE: used = sr_game_grapple(g) == SR_OK; break;
    default:
      if (g->site.active) {
        SrSiteCtx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.deltas = &g->deltas;
        ctx.toxin_filter = sr_game_has_cyber(g, 12);
        used = sr_site_gadget(&g->site, &ctx, gd, p->pos, p->yaw);
        p->health -= ctx.player_damage;
      } else if (d->category == SR_GC_THROWABLE && gd != SR_GD_STICKYCAM && gd != SR_GD_DATASPIKE) {
        SrVec3 at = sr_v3_add(p->pos, sr_v3_scale(sr_dir_from_yaw_pitch(p->yaw, 0), 10.0f));
        street_noise(g, at, gd == SR_GD_LURE ? 6.0f : d->radius * 3.0f);
        used = true;
      }
      if (!used) sr_game_toast(g, "Nothing to use that on here.");
      break;
  }
  if (used && !reusable(gd)) sr_inv_remove_at(&p->inv, idx, 1);
  return used;
}

/* ---- stash, wardrobe, shops ----------------------------------------------------------------- */

static bool keep_on_person(const SrPlayer *p, int i) {
  const SrItem *it = &p->inv.items[i];
  if (i == p->equipped || it->kind == SR_IK_AMMO || it->kind == SR_IK_JOB || it->kind == SR_IK_PAYDATA) return true;
  if (it->kind == SR_IK_GADGET)
    for (int q = 0; q < 4; q++)
      if (p->quick[q] == it->type) return true;
  return false;
}

SrResult sr_game_stash(SrGame *g) {
  SrPlayer *p = &g->player;
  int cap = sr_housing_def(p->tier)->stash_slots;
  int moved = 0;
  for (int i = p->inv.count - 1; i >= 0; i--) {
    if (keep_on_person(p, i) || p->stash.count >= cap) continue;
    if (!sr_inv_add(&p->stash, p->inv.items[i])) continue; /* GCOVR_EXCL_LINE reason: capped above */
    sr_inv_remove_at(&p->inv, i, p->inv.items[i].count);
    if (p->equipped > i) p->equipped--;
    moved++;
  }
  if (moved) {
    sr_game_toast(g, "Stashed.");
    return SR_OK;
  }
  if (p->stash.count == 0) {
    sr_game_toast(g, "Nothing to stash.");
    return SR_ERR_NOT_FOUND;
  }
  sr_inv_transfer_all(&p->stash, &p->inv);
  sr_game_toast(g, "You take your things from the stash.");
  return SR_OK;
}

void sr_game_change_outfit(SrGame *g) {
  SrPlayer *p = &g->player;
  p->outfit = (uint8_t)((p->outfit + 1) % 8);
  p->look.outfit = p->outfit;
  p->look.outfit_color = (uint8_t)((p->look.outfit_color + 5) % 16);
  sr_game_toast(g, "New look. Witnesses will have a harder time.");
}

void sr_game_shop_open(SrGame *g, int b) {
  SrMenu *m = &g->menu;
  memset(m, 0, sizeof *m);
  m->building = b;
  m->ripperdoc = bld(g, b)->poi == SR_POI_RIPPERDOC;
  m->count = sr_vendor_stock(g->city, b, sr_game_time(g) / SR_DAY_S, m->items, SR_MENU_MAX);
  for (int i = 0; i < m->count; i++) {
    m->price[i] = sr_price_at(g->city, b, &m->items[i]);
    m->jailbreak[i] = -1;
  }
  if (m->ripperdoc)
    for (int c = 0; c < g->player.cyber_count && m->count < SR_MENU_MAX; c++)
      if (g->player.cyber[c].flags & SR_IF_BACKDOOR) {
        m->items[m->count] = g->player.cyber[c];
        m->price[m->count] = JAILBREAK;
        m->jailbreak[m->count] = (int8_t)c;
        m->count++;
      }
  g->screen = SR_SCREEN_SHOP;
}

void sr_game_shop_close(SrGame *g) {
  if (g->screen == SR_SCREEN_SHOP) g->screen = SR_SCREEN_PLAY;
}

void sr_game_housing_open(SrGame *g) {
  SrMenu *m = &g->menu;
  memset(m, 0, sizeof *m);
  m->building = g->player.home;
  m->housing = true;
  for (int tier = 0; tier < SR_HOUSE_COUNT && m->count < SR_MENU_MAX; tier++) {
    if (tier == g->player.tier) continue;
    m->items[m->count] = sr_item_simple(SR_IK_NONE, tier, 1);
    m->price[m->count] = sr_housing_def(tier)->rent;
    m->jailbreak[m->count] = -1;
    m->count++;
  }
  g->screen = SR_SCREEN_SHOP;
}

/* A free building for a housing tier: in the preferred district type, nearest to the old home. */
static int find_home(const SrGame *g, int tier) {
  const SrHousingDef *h = sr_housing_def(tier);
  const SrBuilding *old = bld(g, g->player.home);
  int best = -1;
  int64_t bd = INT64_MAX;
  for (int b = 0; b < g->city->building_count; b++) {
    const SrBuilding *d = bld(g, b);
    if (b == g->player.home || d->poi != SR_POI_NONE || (d->flags & SR_BF_MISSION)) continue;
    int dist = g->city->blocks[d->block].district;
    if (h->district_type >= 0 && (int)g->city->districts[dist].type != h->district_type) continue;
    int64_t dx = (d->x0 - old->x0) / 100, dz = (d->y0 - old->y0) / 100;
    if (dx * dx + dz * dz < bd) {
      bd = dx * dx + dz * dz;
      best = b;
    }
  }
  return best;
}

SrResult sr_game_move_home(SrGame *g, int tier) {
  int b = tier >= 0 && tier < SR_HOUSE_COUNT ? find_home(g, tier) : -1;
  if (b < 0) return SR_ERR_NOT_FOUND;
  bool was_home = g->site.active && g->site.in.apartment;
  SrResult r = sr_player_move(&g->player, tier, b, sr_game_time(g));
  if (r != SR_OK) {
    sr_game_toast(g, "The landlord wants the deposit first.");
    return r;
  }
  g->approach_building = -1;
  if (was_home) {
    sr_site_close(&g->site);
    sr_game_skip(g, 2 * 3600, false); /* moving day */
    sr_game_enter(g, b, SR_ENTRY_FRONT);
  }
  sr_player_message(&g->player, "Landlord", "Keys are yours. Your things are already in the new place.",
                    sr_game_time(g));
  sr_game_toast(g, "You moved. The new rent starts with the next due date.");
  return SR_OK;
}

SrResult sr_game_shop_buy(SrGame *g, int row) {
  SrMenu *m = &g->menu;
  SrPlayer *p = &g->player;
  if (row < 0 || row >= m->count) return SR_ERR_INVALID;
  m->sel = row;
  int64_t t = sr_game_time(g);
  if (m->node_menu) {
    SrResult r = sr_game_node_act(g, m->node, (SrNodeAction)m->action[row]);
    sr_game_shop_close(g);
    return r;
  }
  if (m->housing) {
    SrResult r = sr_game_move_home(g, m->items[row].type);
    if (r == SR_OK) sr_game_shop_close(g);
    return r;
  }
  if (m->jailbreak[row] >= 0) {
    if (p->cred < m->price[row]) return SR_ERR_DENIED;
    sr_cyber_jailbreak(&p->cyber[m->jailbreak[row]]);
    sr_player_tx(p, t, -m->price[row], "jailbreak");
    sr_game_toast(g, "Jailbroken. Nobody else is listening in.");
    sr_game_shop_open(g, m->building);
    m->sel = SR_MIN(row, m->count - 1);
    return SR_OK;
  }
  SrResult r = sr_player_buy(p, g->city, m->building, &m->items[row], t);
  if (r != SR_OK) {
    sr_game_toast(g, r == SR_ERR_DENIED ? "Not enough cred." : "Inventory full.");
    return r;
  }
  const SrItem *it = &m->items[row];
  if (it->kind == SR_IK_CYBER && m->ripperdoc) {
    int idx = sr_inv_find(&p->inv, SR_IK_CYBER, it->type);
    if (!sr_game_install_cyber(g, idx))
      sr_game_toast(g, "Slot taken. It's in your bag for later.");
    else
      sr_game_toast(g, "Installed.");
  } else if (it->kind == SR_IK_GADGET) {
    bool bound = false;
    for (int q = 0; q < 4; q++) bound = bound || p->quick[q] == it->type;
    for (int q = 0; q < 4 && !bound; q++)
      if (p->quick[q] == 255) {
        p->quick[q] = it->type;
        bound = true;
      }
    sr_game_toast(g, "Bought.");
  } else if (it->kind == SR_IK_WEAPON && p->equipped < 0) {
    p->equipped = (int8_t)sr_inv_find(&p->inv, SR_IK_WEAPON, it->type);
    sr_game_toast(g, "Bought and equipped.");
  } else {
    sr_game_toast(g, "Bought.");
  }
  return SR_OK;
}

/* ---- per-step upkeep ------------------------------------------------------------------------ */

static void cooldown(float *v, float dt) {
  *v -= dt;
  if (*v < 0) *v = 0;
}

static void approach_cache(SrGame *g) {
  SrPlayer *p = &g->player;
  int blk = sr_city_block_at(g->city, (int32_t)(p->pos.x * 100), (int32_t)(p->pos.z * 100));
  if (blk < 0) return;
  int bx = blk % g->city->grid, by = blk / g->city->grid, best = -1;
  float bd = APPROACH_M;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int b2 = sr_city_block_index(g->city, bx + dx, by + dy);
      if (b2 < 0) continue;
      for (int i = 0; i < g->city->blocks[b2].building_count; i++) {
        int bi = g->city->blocks[b2].first_building + i;
        if (!sr_game_enterable(g, bi)) continue;
        const SrBuilding *d = bld(g, bi);
        float cx = (float)(d->x0 + d->x1) / 200.0f, cz = (float)d->y1 / 100.0f;
        float dist = sqrtf((cx - p->pos.x) * (cx - p->pos.x) + (cz - p->pos.z) * (cz - p->pos.z));
        if (dist < bd) {
          bd = dist;
          best = bi;
        }
      }
    }
  if (best >= 0 && best != g->approach_building) {
    SrInterior tmp;
    interior_for(g, best, &tmp);
  }
}

void sr_game_site_step(SrGame *g, float dt) {
  SrPlayer *p = &g->player;
  cooldown(&g->buff_camo, dt);
  cooldown(&g->buff_reflex, dt);
  cooldown(&g->buff_mask, dt);
  cooldown(&g->buff_focus, dt);
  cooldown(&g->buff_pain, dt);
  cooldown(&g->ability_cd, dt);
  cooldown(&g->glitch_time, dt);
  /* daily malfunctions of street-grade chrome */
  int64_t day = sr_game_time(g) / SR_DAY_S;
  if (day != g->cyber_day) {
    g->cyber_day = day;
    g->cyber_offline = 0;
    for (int i = 0; i < p->cyber_count; i++)
      if (sr_cyber_malfunctions(&p->cyber[i], g->seed, day)) {
        g->cyber_offline |= 1u << i;
        char msg[96];
        snprintf(msg, sizeof msg, "Your %s is acting up today.", sr_cyber_def(p->cyber[i].type)->name);
        sr_player_message(p, "Body monitor", msg, sr_game_time(g));
      }
    apply_cyber_stats(g);
  }
  /* glitches while over neural capacity, at the configured rate */
  if (sr_cyber_glitching(sr_game_cyber_load(g), sr_game_neural_capacity(g)) && g->glitch_time <= 0) {
    uint32_t roll = (uint32_t)(sr_mix64(sr_hash_combine(g->seed ^ 0x611C7ull, g->steps)) % 1000000u);
    if ((float)roll < SR_GLITCH_PER_MINUTE / 60.0f * dt * 1e6f) {
      g->glitch_time = SR_GLITCH_TIME;
      g->glitches++;
      g->thermal = false;
    }
  }
  if (g->thermal && !sr_game_has_cyber(g, 2)) g->thermal = false;
  if (!g->site.active) {
    if (g->steps % 30 == 0) approach_cache(g);
    return;
  }
  SrSiteCtx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.deltas = &g->deltas;
  g->site.authorised = sr_game_has_creds(g, g->site.building) && !g->site.hostile && !g->site.alarm;
  ctx.player = p->pos;
  ctx.player_speed = g->speed * (sr_game_has_cyber(g, 9) ? 0.5f : 1.0f); /* Silent Steps */
  ctx.crouched = p->crouched;
  ctx.camo = g->buff_camo > 0;
  ctx.masked = g->buff_mask > 0;
  ctx.has_fake_id = sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_FAKEID) >= 0;
  ctx.toxin_filter = sr_game_has_cyber(g, 12);
  ctx.difficulty = g->difficulty;
  ctx.key = sr_hash_combine(g->seed, g->steps);
  sr_site_update(&g->site, &ctx, dt);
  float dmg = ctx.player_damage * (1.0f - p->armor) * (g->buff_pain > 0 ? 0.75f : 1.0f);
  p->health -= dmg;
  int combat = 0;
  for (int i = 0; i < g->site.guard_count; i++) {
    const SrGuard *gd = &g->site.guards[i];
    combat += !gd->e.dead && !gd->e.ko && gd->e.alert == SR_ALERT_COMBAT;
  }
  g->combat_count = combat;
  g->hostiles_near = combat;
}

/* ---- context and interaction inside ---------------------------------------------------------- */

static bool behind_unaware(const SrGuard *gd, SrVec3 eye) {
  if (gd->e.dead || gd->e.ko || gd->e.alert == SR_ALERT_COMBAT) return false;
  SrVec3 to = sr_v3_sub(sr_v3(eye.x, gd->e.pos.y, eye.z), gd->e.pos);
  if (sr_v3_len(to) > 2.0f) return false;
  return sr_v3_dot(sr_v3_norm(to), sr_dir_from_yaw_pitch(gd->e.yaw, 0)) <= -0.3f;
}

SrContext sr_game_site_context(SrGame *g) {
  SrContext ctx = {SR_CTX_NONE, -1, -1, -1, 1e9f, -1};
  const SrPlayer *p = &g->player;
  if (g->roof_building >= 0) {
    int b = g->roof_building;
    ctx.building = b;
    SrVec3 c = roof_point(g, b);
    float d = sqrtf((c.x - p->pos.x) * (c.x - p->pos.x) + (c.z - p->pos.z) * (c.z - p->pos.z));
    ctx.distance = d;
    ctx.kind = SR_CTX_CLIMB_DOWN;
    if (d < 4.0f && sr_game_enterable(g, b)) {
      SrInterior in;
      interior_for(g, b, &in);
      for (int e = 0; e < in.entry_count; e++)
        if (roof_entry((SrEntryType)in.entries[e].type)) ctx.kind = SR_CTX_ROOF_HATCH;
    }
    return ctx;
  }
  if (!g->site.active) return ctx;
  const SrSite *s = &g->site;
  ctx.building = s->building;
  ctx.job = s->job;
  int gi = sr_site_guard_ahead(s, p->pos, p->yaw, 0, 2.0f, 0.5f);
  if (gi >= 0 && behind_unaware(&s->guards[gi], p->pos)) {
    ctx.kind = SR_CTX_TAKEDOWN;
    ctx.prop = gi;
    return ctx;
  }
  int pr = sr_site_prop_near(s, p->pos, p->yaw, REACH);
  if (pr >= 0) {
    static const SrContextKind k[SR_PROP_COUNT] = {SR_CTX_EXIT,     SR_CTX_BED,       SR_CTX_STASH,   SR_CTX_TERMINAL,
                                                   SR_CTX_WARDROBE, SR_CTX_OBJECTIVE, SR_CTX_CONSOLE, SR_CTX_NONE};
    SrContextKind kind = k[s->in.props[pr].type];
    if (kind == SR_CTX_OBJECTIVE && (s->job < 0 || s->objective_done)) kind = SR_CTX_NONE;
    if (kind != SR_CTX_NONE) {
      ctx.kind = kind;
      ctx.prop = pr;
      ctx.distance = sr_v3_len(sr_v3_sub(sr_site_prop_pos(s, pr), sr_v3(p->pos.x, SR_SITE_FLOOR_Y, p->pos.z)));
      return ctx;
    }
  }
  int door = sr_site_locked_door_ahead(s, p->pos, p->yaw, 2.2f);
  if (door >= 0) {
    ctx.kind = SR_CTX_LOCKED;
    ctx.prop = door;
  }
  return ctx;
}

static void complete_objective(SrGame *g) {
  SrSite *s = &g->site;
  SrJob *j = sr_jobs_get(&g->jobs, s->job);
  s->objective_done = true;
  sr_deltas_set(&g->deltas, sr_site_objective_id(s->job), 1);
  if (!j) return; /* GCOVR_EXCL_LINE reason: the site job exists while the site is open */
  SrMissionResult r = sr_site_result(s, sr_game_time(g));
  sr_job_apply_result(j, &g->player, &r);
  sr_progress_use_skill(&g->player.prog, SR_SK_STEALTH, r.alarm ? SR_USE_HOSTILE : SR_USE_SUCCESS, 10 + 5 * j->tier);
  sr_game_toast(g, "Objective complete. Get out and report back.");
}

SrResult sr_game_site_interact(SrGame *g, const SrContext *c) {
  SrPlayer *p = &g->player;
  SrSite *s = &g->site;
  switch (c->kind) {
    case SR_CTX_EXIT: sr_game_leave(g); return SR_OK;
    case SR_CTX_BED: return sr_game_sleep(g, 8, SR_SLEEP_HOME);
    case SR_CTX_STASH: return sr_game_stash(g);
    case SR_CTX_TERMINAL: g->screen = SR_SCREEN_TERMINAL; return SR_OK;
    case SR_CTX_WARDROBE: sr_game_change_outfit(g); return SR_OK;
    case SR_CTX_OBJECTIVE: complete_objective(g); return SR_OK;
    case SR_CTX_CONSOLE:
      if (!sr_game_has_cyber(g, 7) && p->prog.skill_rank[SR_SK_ELECTRONICS] < CONSOLE_RANK) {
        sr_game_toast(g, "Access denied. You need Electronics 2 or a data jack.");
        return SR_ERR_DENIED;
      }
      sr_site_console(s, &g->deltas);
      sr_progress_use_skill(&p->prog, SR_SK_ELECTRONICS, SR_USE_SUCCESS, 10);
      sr_game_toast(g, "Cameras off, doors open, alarm silenced.");
      return SR_OK;
    case SR_CTX_LOCKED: {
      int tool = sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_MULTITOOL);
      if (sr_game_has_cyber(g, 6) || sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_LANCE) >= 0) {
        sr_site_unlock(s, &g->deltas, c->prop);
      } else if (tool >= 0) {
        sr_site_unlock(s, &g->deltas, c->prop);
        sr_inv_remove_at(&p->inv, tool, 1);
      } else {
        sr_game_toast(g, "Locked. A multitool or the security console would open it.");
        return SR_ERR_DENIED;
      }
      sr_game_toast(g, "Unlocked.");
      return SR_OK;
    }
    case SR_CTX_TAKEDOWN: {
      bool lethal = sr_game_has_cyber(g, 5); /* Mono-Claws */
      if (p->equipped >= 0 && p->equipped < p->inv.count) {
        SrWeaponStats w = sr_weapon_stats(g->seed, &p->inv.items[p->equipped]);
        if (sr_archetype(p->inv.items[p->equipped].type)->category == SR_WC_MELEE) lethal = lethal || w.lethal;
      }
      if (!sr_site_takedown(s, &g->deltas, p->pos, p->yaw, lethal)) return SR_ERR_DENIED;
      if (lethal)
        p->kills++;
      else
        p->knockouts++;
      sr_progress_use_skill(&p->prog, SR_SK_STEALTH, SR_USE_SUCCESS, 8);
      return SR_OK;
    }
    case SR_CTX_ROOF_HATCH: {
      SrResult r = sr_game_enter(g, g->roof_building, SR_ENTRY_ROOF);
      if (r == SR_ERR_NOT_FOUND) r = sr_game_enter(g, c->building, SR_ENTRY_VENT);
      return r;
    }
    case SR_CTX_CLIMB_DOWN: return sr_game_climb_down(g);
    default: return SR_ERR_NOT_FOUND;
  }
}
