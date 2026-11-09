/* Diving: access points, persona flight on the grid and in hosts, breaching, node actions and
 * their effect on the physical world, ICE, traces, the body left behind, ownership tokens,
 * Netsight quick actions, backdoors, forged credentials and host countermeasures. */
#include "core/sr_rng.h"
#include "game/sr_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define NODE_REACH  3.0f
#define HOST_REACH  2.5f
#define FLY_SPEED   9.0f
#define GRID_SPEED  6.0f
#define QUICK_RANGE 12.0f

uint32_t sr_net_elevator_id(uint32_t device_id) { return 0x20000000u | device_id; }

const char *sr_node_action_name(SrNodeAction a) {
  static const char *const n[SR_NA_COUNT] = {"",
                                             "Jack out",
                                             "Plant worm (backdoor)",
                                             "Unlock door",
                                             "Lock door",
                                             "Loop camera",
                                             "Disable",
                                             "Restore",
                                             "View feed",
                                             "Switch friend-or-foe",
                                             "Cancel alarm",
                                             "False alarm in the far wing",
                                             "Lights off",
                                             "Lights on",
                                             "Send elevator to next floor",
                                             "Download",
                                             "Erase logs",
                                             "Erase my warrant",
                                             "Forge credentials",
                                             "Alter account",
                                             "District blackout",
                                             "Take over every device",
                                             "Enter host",
                                             "Plant a story",
                                             "Kill the next story",
                                             "Alter the vote count",
                                             "Leak bribery evidence"};
  return (unsigned)a < SR_NA_COUNT ? n[a] : "?";
}

/* ---- deck, persona -------------------------------------------------------------------------- */

int sr_game_deck_grade(const SrGame *g) {
  int best = 0;
  for (int i = 0; i < g->player.inv.count; i++)
    if (g->player.inv.items[i].kind == SR_IK_DECK) best = SR_MAX(best, g->player.inv.items[i].type + 1);
  return SR_MIN(best, SR_DECK_GRADES - 1);
}

SrPersona sr_game_persona(const SrGame *g) {
  return sr_persona_stats(sr_game_deck_grade(g), g->player.prog.attr[SR_ATTR_MIND],
                          g->player.prog.skill_rank[SR_SK_NETRUNNING]);
}

bool sr_game_diving(const SrGame *g) { return g->dive.active; }

/* ---- network state helpers ------------------------------------------------------------------ */

static int64_t now(const SrGame *g) { return sr_game_time(g); }

bool sr_game_owns_device(const SrGame *g, uint32_t device) {
  for (int i = 0; i < g->net.token_count; i++)
    if (g->net.tokens[i].device == device && g->net.tokens[i].expires > now(g)) return true;
  return false;
}

static void grant_token(SrGame *g, uint32_t device, int building, int security) {
  SrNet *n = &g->net;
  int64_t exp = now(g) + (int64_t)sr_discovery_time(security);
  for (int i = 0; i < n->token_count; i++)
    if (n->tokens[i].device == device) {
      n->tokens[i].expires = exp;
      return;
    }
  if (n->token_count >= SR_NET_MAX_TOKENS) memmove(n->tokens, n->tokens + 1, sizeof(SrToken) * --n->token_count);
  n->tokens[n->token_count++] = (SrToken){device, building, exp};
}

bool sr_game_has_backdoor(const SrGame *g, int building) {
  for (int i = 0; i < g->net.backdoor_count; i++)
    if (g->net.backdoors[i].building == building) return true;
  return false;
}

bool sr_game_has_creds(const SrGame *g, int building) {
  for (int i = 0; i < g->net.cred_count; i++)
    if (g->net.creds[i].building == building && g->net.creds[i].expires > now(g)) return true;
  return false;
}

int sr_game_host_alert(const SrGame *g, int building) {
  for (int i = 0; i < g->net.alert_count; i++)
    if (g->net.alerts[i].building == building && g->net.alerts[i].until > now(g)) return g->net.alerts[i].level;
  return 0;
}

static void raise_host_alert(SrGame *g, int building, int level) {
  SrNet *n = &g->net;
  int64_t until = now(g) + 6 * 3600;
  for (int i = 0; i < n->alert_count; i++)
    if (n->alerts[i].building == building) {
      n->alerts[i].level = (uint8_t)SR_MAX(n->alerts[i].level, level);
      n->alerts[i].until = until;
      return;
    }
  if (n->alert_count >= SR_NET_MAX_ALERTS) memmove(n->alerts, n->alerts + 1, sizeof(SrHostAlert) * --n->alert_count);
  n->alerts[n->alert_count++] = (SrHostAlert){building, (uint8_t)level, until};
}

static void remove_backdoors(SrGame *g, int building) {
  SrNet *n = &g->net;
  for (int i = n->backdoor_count - 1; i >= 0; i--)
    if (n->backdoors[i].building == building)
      memmove(&n->backdoors[i], &n->backdoors[i + 1], sizeof(SrBackdoor) * (size_t)(--n->backdoor_count - i));
}

static void revoke_tokens(SrGame *g, int building) {
  SrNet *n = &g->net;
  for (int i = n->token_count - 1; i >= 0; i--)
    if (n->tokens[i].building == building)
      memmove(&n->tokens[i], &n->tokens[i + 1], sizeof(SrToken) * (size_t)(--n->token_count - i));
}

/* ---- device control: the bridge between nodes and the physical world ------------------------ */

static SrInterior *interior_of(SrGame *g, int building, SrInterior *tmp) {
  if (g->dive.active && g->dive.in_host && g->dive.building == building) return &g->dive.in;
  if (g->site.active && g->site.building == building) return &g->site.in;
  if (building == g->player.home)
    sr_interior_apartment(g->city, building, g->player.tier, tmp);
  else
    sr_interior_generate(g->city, building, tmp);
  return tmp;
}

/* Whether a door is locked as generated (before any player change). */
static bool original_locked(const SrGame *g, int building, int device) {
  SrInterior fresh;
  if (building == g->player.home)
    sr_interior_apartment(g->city, building, g->player.tier, &fresh);
  else
    sr_interior_generate(g->city, building, &fresh);
  return device < fresh.device_count && fresh.devices[device].locked;
}

/* Sets and clears delta bits of a device, records the tamper for host countermeasures and
 * mirrors the change into the live site, if the player's body is in that building. */
static void set_device(SrGame *g, int building, int device, uint8_t set, uint8_t clear, int security) {
  SrInterior tmp;
  SrInterior *in = interior_of(g, building, &tmp);
  if (device < 0 || device >= in->device_count) return;
  uint32_t id = in->devices[device].id;
  int before = sr_deltas_get(&g->deltas, id);
  uint8_t v = (uint8_t)(((before < 0 ? 0 : before) | set) & ~clear);
  sr_deltas_set(&g->deltas, id, v);
  SrNet *n = &g->net;
  bool known = false;
  for (int i = 0; i < n->tamper_count; i++) known = known || n->tamper[i].device == id;
  if (!known && n->tamper_count < SR_NET_MAX_TAMPER)
    n->tamper[n->tamper_count++] =
        (SrTamper){id, building, (uint8_t)(before < 0 ? 255 : before), g->dive.active && g->dive.logs_erased,
                   now(g) + (int64_t)sr_discovery_time(security)};
  grant_token(g, id, building, security);
  bool locked = original_locked(g, building, device) && !(v & SR_DV_UNLOCKED);
  if (g->site.active && g->site.building == building) {
    SrDevice *d = &g->site.in.devices[device];
    if (d->type == SR_DEV_DOOR) d->locked = locked;
    if (d->type == SR_DEV_LIGHTS) g->site.lights_off = (v & SR_DV_DISABLED) != 0;
  }
  if (g->dive.active && g->dive.in_host && g->dive.building == building &&
      g->dive.in.devices[device].type == SR_DEV_DOOR)
    g->dive.in.devices[device].locked = locked;
}

static int device_of_type(const SrInterior *in, SrDeviceType t) {
  for (int d = 0; d < in->device_count; d++)
    if (in->devices[d].type == t) return d;
  return -1;
}

/* Runs a device's default quick action (Netsight) or a node action. */
static SrResult device_action(SrGame *g, int building, int device, SrNodeAction a, int security) {
  SrInterior tmp;
  SrInterior *in = interior_of(g, building, &tmp);
  if (device < 0 || device >= in->device_count) return SR_ERR_INVALID;
  switch (a) {
    case SR_NA_UNLOCK: set_device(g, building, device, SR_DV_UNLOCKED, 0, security); break;
    case SR_NA_LOCK: set_device(g, building, device, 0, SR_DV_UNLOCKED, security); break;
    case SR_NA_LOOP: set_device(g, building, device, SR_DV_LOOPED, 0, security); break;
    case SR_NA_DISABLE: set_device(g, building, device, SR_DV_DISABLED, SR_DV_LOOPED, security); break;
    case SR_NA_RESTORE: set_device(g, building, device, 0, SR_DV_DISABLED | SR_DV_LOOPED | SR_DV_FOF, security); break;
    case SR_NA_FOF: set_device(g, building, device, SR_DV_FOF, 0, security); break;
    case SR_NA_LIGHTS_OFF: set_device(g, building, device, SR_DV_DISABLED, 0, security); break;
    case SR_NA_LIGHTS_ON: set_device(g, building, device, 0, SR_DV_DISABLED, security); break;
    case SR_NA_FEED:
      g->dive.feed_device = device;
      grant_token(g, in->devices[device].id, building, security);
      break;
    case SR_NA_ELEVATOR: {
      uint32_t eid = sr_net_elevator_id(in->devices[device].id);
      int floors = SR_MAX(2, g->city->buildings[building].floors);
      int cur = sr_deltas_get(&g->deltas, eid);
      sr_deltas_set(&g->deltas, eid, (uint8_t)(((cur < 0 ? 0 : cur) + 1) % floors));
      grant_token(g, in->devices[device].id, building, security);
      break;
    }
    case SR_NA_CANCEL_ALARM:
      if (g->site.active && g->site.building == building) {
        g->site.alarm = false;
        g->site.reinforce_at = 0;
        for (int i = 0; i < g->site.guard_count; i++) g->site.guards[i].e.called_reinforcements = false;
      }
      grant_token(g, in->devices[device].id, building, security);
      break;
    case SR_NA_FALSE_ALARM:
      if (g->site.active && g->site.building == building) {
        /* the room farthest from the player's body */
        int far = 0;
        float fd = -1;
        for (int r = 0; r < g->site.in.w * g->site.in.h; r++) {
          SrVec3 c = sr_site_room_center(&g->site, r);
          float d = sr_v3_len(sr_v3_sub(c, g->player.pos));
          if (d > fd) fd = d, far = r;
        }
        sr_site_false_alarm(&g->site, far);
      }
      grant_token(g, in->devices[device].id, building, security);
      break;
    default: return SR_ERR_INVALID;
  }
  return SR_OK;
}

/* ---- access ---------------------------------------------------------------------------------- */

static void persona_at_node(SrGame *g, int node, float back) {
  const SrNode *n = &g->dive.host.nodes[node];
  g->dive.pos = sr_v3(n->x, n->y + 1.5f, n->z + back);
  g->dive.yaw = 0;
  g->dive.pitch = 0;
}

static void enter_host(SrGame *g, int building, bool past_gateway) {
  SrDive *d = &g->dive;
  SrInterior tmp;
  SrInterior *in = interior_of(g, building, &tmp);
  d->in = *in;
  sr_host_generate(g->city, building, &d->in, &d->host);
  d->in_host = true;
  d->building = building;
  memset(d->open, 0, sizeof d->open);
  d->alert = sr_game_host_alert(g, building);
  d->ice_count = sr_ice_populate(&d->host, g->city->seed, d->ice, SR_MX_MAX_ICE);
  if (d->alert > 0)
    d->ice_count += sr_ice_reinforce(&d->host, d->alert, g->city->seed ^ (uint64_t)now(g), d->ice + d->ice_count,
                                     SR_MX_MAX_ICE - d->ice_count);
  d->breach_node = -1;
  d->download_node = -1;
  d->trace = 0;
  d->logs_erased = false;
  bool bypass = past_gateway || sr_game_has_backdoor(g, building) || sr_game_has_creds(g, building);
  if (bypass) d->open[d->host.gateway] = 1;
  persona_at_node(g, d->host.gateway, bypass ? 0.0f : 4.0f);
  if (bypass) d->pos.y += 1.0f;
}

SrResult sr_game_dive_enter_host(SrGame *g, int building) {
  if (!g->dive.active || g->dive.in_host || !sr_building_networked(g->city, building)) return SR_ERR_INVALID;
  enter_host(g, building, false);
  return SR_OK;
}

/* Which access point the player is at, and for wireless and ports, which building. */
static SrAccess pick_access(SrGame *g, int *building) {
  *building = -1;
  if (g->site.active) {
    if (g->site.in.apartment) return SR_ACCESS_RIG;
    for (int i = 0; i < g->site.effect_count; i++)
      if (g->site.effects[i].active && g->site.effects[i].type == SR_FX_LOOP) {
        *building = g->site.building;
        return SR_ACCESS_SPIKE;
      }
    int pr = sr_site_prop_near(&g->site, g->player.pos, g->player.yaw, 2.0f);
    if (pr >= 0 && g->site.in.props[pr].type == SR_PROP_BOOTH &&
        g->city->buildings[g->site.building].poi == SR_POI_MATRIX_BOOTH) {
      *building = g->site.building;
      return SR_ACCESS_BOOTH;
    }
    if (pr >= 0 && (g->site.in.props[pr].type == SR_PROP_CONSOLE || g->site.in.props[pr].type == SR_PROP_TERMINAL)) {
      *building = g->site.building;
      return SR_ACCESS_PORT;
    }
    *building = g->site.building; /* inside without a port: wireless at close range */
    return SR_ACCESS_WIRELESS;
  }
  if (g->ctx.kind == SR_CTX_SHOP && g->ctx.building >= 0 &&
      g->city->buildings[g->ctx.building].poi == SR_POI_MATRIX_BOOTH)
    return SR_ACCESS_BOOTH;
  /* street: the nearest networked building in deck range */
  SrPersona ps = sr_game_persona(g);
  float best = 1e9f;
  int blk = sr_city_block_at(g->city, (int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100));
  if (blk >= 0) {
    int bx = blk % g->city->grid, by = blk / g->city->grid;
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        int b2 = sr_city_block_index(g->city, bx + dx, by + dy);
        if (b2 < 0) continue;
        for (int i = 0; i < g->city->blocks[b2].building_count; i++) {
          int bi = g->city->blocks[b2].first_building + i;
          if (!sr_building_networked(g->city, bi)) continue;
          const SrBuilding *bd = &g->city->buildings[bi];
          float cx = (float)(bd->x0 + bd->x1) / 200.0f, cz = (float)(bd->y0 + bd->y1) / 200.0f;
          float dd =
              sqrtf((cx - g->player.pos.x) * (cx - g->player.pos.x) + (cz - g->player.pos.z) * (cz - g->player.pos.z));
          if (dd < best) best = dd, *building = bi;
        }
      }
  }
  if (*building >= 0 && best > ps.signal) *building = -2; /* out of range */
  return SR_ACCESS_WIRELESS;
}

void sr_game_net_reset(SrGame *g) {
  memset(&g->net, 0, sizeof g->net);
  memset(&g->dive, 0, sizeof g->dive);
  g->net.blackout_district = -1;
}

/* Programs come with the deck: fill free memory in this order. */
static void fill_loadout(SrGame *g) {
  static const SrProgram order[] = {SR_PRG_SPIKE, SR_PRG_VEIL,   SR_PRG_WORM,  SR_PRG_MEND,
                                    SR_PRG_SNIFF, SR_PRG_SHIELD, SR_PRG_GHOST, SR_PRG_CRASH};
  int grade = sr_game_deck_grade(g);
  for (int i = 0; i < (int)SR_ARRAY_LEN(order); i++)
    if (sr_loadout_find(&g->net.programs, order[i]) < 0) sr_loadout_add(&g->net.programs, grade, order[i]);
}

bool sr_game_blackout_at(const SrGame *g, int district) {
  return district >= 0 && g->net.blackout_district == district && sr_game_time(g) < g->net.blackout_until;
}

void sr_game_node_menu(SrGame *g, int node) {
  SrMenu *m = &g->menu;
  memset(m, 0, sizeof *m);
  SrNodeAction acts[8];
  int n = sr_game_node_actions(g, node, acts);
  if (n == 0) return;
  m->node_menu = true;
  m->node = (int16_t)node;
  m->building = g->dive.building;
  for (int i = 0; i < n && i < SR_MENU_MAX; i++) {
    m->action[i] = (uint8_t)acts[i];
    m->jailbreak[i] = -1;
  }
  m->count = n;
  g->screen = SR_SCREEN_SHOP;
}

SrResult sr_game_jack_in_via(SrGame *g, SrAccess access, int building) {
  if (g->dive.active) return SR_ERR_INVALID;
  if (sr_game_deck_grade(g) <= 0) {
    sr_game_toast(g, "No cyberdeck, no dive.");
    return SR_ERR_DENIED;
  }
  SrDive *d = &g->dive;
  memset(d, 0, sizeof *d);
  d->persona = sr_game_persona(g);
  d->speed = 1.0f;
  d->access = (uint8_t)access;
  d->feed_device = -1;
  d->breach_node = -1;
  d->download_node = -1;
  d->started = now(g);
  d->building = -1;
  if (access == SR_ACCESS_WIRELESS) {
    if (building == -2) {
      sr_game_toast(g, "Out of signal range. Get closer.");
      return SR_ERR_DENIED;
    }
    if (building < 0 || !sr_building_networked(g->city, building)) return SR_ERR_NOT_FOUND;
    d->speed = SR_WIRELESS_SPEED;
  }
  d->active = true;
  fill_loadout(g);
  if (access == SR_ACCESS_RIG || access == SR_ACCESS_BOOTH || building < 0) {
    /* the global grid, hovering over the player's own position */
    d->pos = sr_grid_pos((int32_t)(g->player.pos.x * 100), (int32_t)(g->player.pos.z * 100), 3.0f);
    d->yaw = g->player.yaw;
  } else {
    enter_host(g, building, access == SR_ACCESS_PORT || access == SR_ACCESS_SPIKE);
  }
  sr_progress_use_skill(&g->player.prog, SR_SK_NETRUNNING, SR_USE_SUCCESS, 2);
  sr_game_toast(g, d->in_host ? "Jacked in. The host hums around you." : "Jacked in. The grid opens up.");
  return SR_OK;
}

SrResult sr_game_jack_in(SrGame *g) {
  int building;
  SrAccess a = pick_access(g, &building);
  return sr_game_jack_in_via(g, a, building);
}

void sr_game_jack_out(SrGame *g, bool emergency) {
  SrDive *d = &g->dive;
  if (!d->active) return;
  d->active = false;
  g->chess.story.matrix_depth += d->nodes_hacked;
  d->nodes_hacked = 0;
  if (d->in_host && d->trace > 0.5f && !d->traced) raise_host_alert(g, d->building, SR_MAX(d->alert, 1));
  if (emergency) {
    g->player.health -= SR_DUMPSHOCK_DAMAGE;
    g->glitch_time = SR_MAX(g->glitch_time, SR_DUMPSHOCK_STUN);
    sr_game_toast(g, "Dumpshock. Your head rings.");
  } else {
    sr_game_toast(g, "Jacked out.");
  }
  if (g->player.health <= 0) sr_game_flatline(g);
}

/* ---- nodes ------------------------------------------------------------------------------------ */

static SrVec3 node_pos(const SrDive *d, int i) {
  const SrNode *n = &d->host.nodes[i];
  return sr_v3(n->x, n->y + 1.5f, n->z);
}

/* A node needs a breach when it is the gateway, a junction guarded by a barrier, or a core-layer
 * node (monitor, records, core) — unless something already opened it. */
static bool node_locked(const SrDive *d, int i) {
  if (d->open[i]) return false;
  const SrNode *n = &d->host.nodes[i];
  if (n->type == SR_NODE_GATEWAY) return true;
  for (int k = 0; k < d->ice_count; k++)
    if (d->ice[k].active && d->ice[k].type == SR_ICE_BARRIER && d->ice[k].node == i) return true;
  if (n->type == SR_NODE_MONITOR || n->type == SR_NODE_RECORDS || n->type == SR_NODE_CORE) return true;
  return false;
}

bool sr_game_node_locked(const SrGame *g, int node) { return node_locked(&g->dive, node); }

int sr_game_node_near(const SrGame *g) {
  const SrDive *d = &g->dive;
  if (!d->active || !d->in_host) return -1;
  int best = -1;
  float bd = NODE_REACH;
  for (int i = 0; i < d->host.node_count; i++) {
    float dist = sr_v3_len(sr_v3_sub(node_pos(d, i), d->pos));
    if (dist < bd) bd = dist, best = i;
  }
  return best;
}

int sr_game_host_near(const SrGame *g) {
  const SrDive *d = &g->dive;
  if (!d->active || d->in_host) return -1;
  int blk =
      sr_city_block_at(g->city, (int32_t)(d->pos.x * SR_GRID_SCALE * 100), (int32_t)(d->pos.z * SR_GRID_SCALE * 100));
  if (blk < 0) return -1;
  int best = -1;
  float bd = HOST_REACH;
  int bx = blk % g->city->grid, by = blk / g->city->grid;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int b2 = sr_city_block_index(g->city, bx + dx, by + dy);
      if (b2 < 0) continue;
      for (int i = 0; i < g->city->blocks[b2].building_count; i++) {
        int bi = g->city->blocks[b2].first_building + i;
        if (!sr_building_networked(g->city, bi)) continue;
        SrVec3 p = sr_grid_building_pos(g->city, bi);
        float dist = sqrtf((p.x - d->pos.x) * (p.x - d->pos.x) + (p.z - d->pos.z) * (p.z - d->pos.z));
        if (dist < bd) bd = dist, best = bi;
      }
    }
  return best;
}

int sr_game_node_actions(const SrGame *g, int node, SrNodeAction *out) {
  const SrDive *d = &g->dive;
  int n = 0;
  if (!d->active || !d->in_host || node < 0 || node >= d->host.node_count) return 0;
  const SrNode *nd = &d->host.nodes[node];
  bool worm = sr_loadout_find(&g->net.programs, SR_PRG_WORM) >= 0;
  switch ((SrNodeType)nd->type) {
    case SR_NODE_GATEWAY:
      out[n++] = SR_NA_JACK_OUT;
      if (worm) out[n++] = SR_NA_WORM;
      break;
    case SR_NODE_DEVICE: {
      const SrDevice *dv = &d->in.devices[nd->device];
      int v = sr_deltas_get(&g->deltas, dv->id);
      v = v < 0 ? 0 : v;
      switch (dv->type) {
        case SR_DEV_DOOR: out[n++] = (v & SR_DV_UNLOCKED) || !dv->locked ? SR_NA_LOCK : SR_NA_UNLOCK; break;
        case SR_DEV_CAMERA:
          out[n++] = SR_NA_LOOP;
          out[n++] = SR_NA_DISABLE;
          out[n++] = SR_NA_FEED;
          if (v & (SR_DV_LOOPED | SR_DV_DISABLED)) out[n++] = SR_NA_RESTORE;
          break;
        case SR_DEV_TURRET:
          out[n++] = SR_NA_DISABLE;
          out[n++] = SR_NA_FOF;
          if (v) out[n++] = SR_NA_RESTORE;
          break;
        case SR_DEV_ALARM:
          out[n++] = SR_NA_CANCEL_ALARM;
          out[n++] = SR_NA_FALSE_ALARM;
          break;
        case SR_DEV_LIGHTS: out[n++] = (v & SR_DV_DISABLED) ? SR_NA_LIGHTS_ON : SR_NA_LIGHTS_OFF; break;
        case SR_DEV_ELEVATOR: out[n++] = SR_NA_ELEVATOR; break;
        default: break; /* GCOVR_EXCL_LINE reason: every device type is listed */
      }
      break;
    }
    case SR_NODE_DATA: out[n++] = SR_NA_DOWNLOAD; break;
    case SR_NODE_MONITOR:
      out[n++] = SR_NA_CANCEL_ALARM;
      out[n++] = SR_NA_FALSE_ALARM;
      out[n++] = SR_NA_ERASE_LOGS;
      break;
    case SR_NODE_RECORDS:
      out[n++] = SR_NA_ERASE_WARRANT;
      out[n++] = SR_NA_FORGE_CREDS;
      out[n++] = SR_NA_ALTER_ACCOUNT;
      n += sr_game_chess_node_actions(g, d->building, out + n);
      break;
    case SR_NODE_POWER:
      out[n++] = SR_NA_LIGHTS_OFF;
      if (d->host.district_grid) out[n++] = SR_NA_BLACKOUT;
      break;
    case SR_NODE_CORE:
      out[n++] = SR_NA_OWN_ALL;
      out[n++] = SR_NA_ERASE_LOGS;
      if (worm) out[n++] = SR_NA_WORM;
      break;
    default: break; /* junctions have no actions */
  }
  return n;
}

static int alarm_device(const SrDive *d) { return device_of_type(&d->in, SR_DEV_ALARM); }

static void download_done(SrGame *g) {
  SrDive *d = &g->dive;
  const SrNode *nd = &d->host.nodes[d->download_node];
  SrItem it = sr_item_simple(SR_IK_PAYDATA, nd->data_kind, 1);
  it.seed = (uint32_t)nd->data_value;
  it.job = -1;
  /* a client waiting for data from this host: the objective is done */
  for (int j = 0; j < g->jobs.count; j++) {
    SrJob *jb = &g->jobs.jobs[j];
    if (jb->state == SR_JS_ACCEPTED && !jb->executed && jb->target_building == d->building &&
        jb->type == SR_JT_STEAL_DATA) {
      SrMissionResult r;
      memset(&r, 0, sizeof r);
      r.success = true;
      r.alarm = d->alert >= 2;
      r.seconds = now(g) - d->started;
      sr_job_apply_result(jb, &g->player, &r); /* adds the job's paydata item */
      sr_game_toast(g, "Data secured for the client. Report back.");
    }
    /* stolen security data: guard and camera details become known for jobs at this site */
    if (nd->data_kind == SR_PD_SECURITY && jb->target_building == d->building)
      jb->intel |= SR_INTEL_GUARDS | SR_INTEL_CAMERAS | SR_INTEL_ENTRANCE | SR_INTEL_SECURITY;
  }
  if (!sr_inv_add(&g->player.inv, it)) sr_game_toast(g, "Inventory full: the file is lost.");
  sr_game_chess_paydata(g, d->building);
  d->download_node = -1;
  d->nodes_hacked++;
}

SrResult sr_game_node_act(SrGame *g, int node, SrNodeAction a) {
  SrDive *d = &g->dive;
  if (!d->active || !d->in_host || node < 0 || node >= d->host.node_count) return SR_ERR_INVALID;
  if (node_locked(d, node)) return SR_ERR_DENIED;
  SrNodeAction allowed[8];
  int na = sr_game_node_actions(g, node, allowed);
  bool ok = false;
  for (int i = 0; i < na; i++) ok = ok || allowed[i] == a;
  if (!ok) return SR_ERR_INVALID;
  const SrNode *nd = &d->host.nodes[node];
  int sec = d->host.security;
  SrResult r = SR_OK;
  switch (a) {
    case SR_NA_JACK_OUT: sr_game_jack_out(g, false); return SR_OK;
    case SR_NA_WORM: {
      int slot = sr_loadout_find(&g->net.programs, SR_PRG_WORM);
      if (!sr_loadout_use(&g->net.programs, slot)) return SR_ERR_DENIED;
      remove_backdoors(g, d->building);
      if (g->net.backdoor_count < SR_NET_MAX_BACKDOORS)
        g->net.backdoors[g->net.backdoor_count++] = (SrBackdoor){d->building, now(g) + (int64_t)sr_discovery_time(sec)};
      sr_game_toast(g, "Worm planted. You can come back past the gateway.");
      break;
    }
    case SR_NA_DOWNLOAD:
      d->download_node = node;
      d->download_total = d->download_left = sr_download_time(nd->data_size, &d->persona, d->speed);
      break;
    case SR_NA_ERASE_LOGS:
      d->logs_erased = true;
      for (int i = 0; i < g->net.tamper_count; i++)
        if (g->net.tamper[i].building == d->building) g->net.tamper[i].logs_erased = true;
      sr_game_toast(g, "Logs erased. Nobody will know it was you.");
      break;
    case SR_NA_ERASE_WARRANT: {
      int pol = sr_game_police(g);
      g->heat.level[pol] = (int8_t)SR_MAX(0, g->heat.level[pol] - 2);
      int k = 0; /* drop the evidence the police hold */
      for (int i = 0; i < g->heat.evidence_count; i++)
        if (g->heat.evidence[i].jurisdiction != pol) g->heat.evidence[k++] = g->heat.evidence[i];
      g->heat.evidence_count = k;
      sr_game_toast(g, "Warrant erased. Evidence files gone.");
      break;
    }
    case SR_NA_FORGE_CREDS: {
      const SrJob *j = sr_jobs_get(&g->jobs, g->active_job);
      int target = j ? j->target_building : d->building;
      if (g->net.cred_count >= SR_NET_MAX_CREDS)
        memmove(g->net.creds, g->net.creds + 1, sizeof(SrCred) * --g->net.cred_count);
      g->net.creds[g->net.cred_count++] = (SrCred){target, now(g) + 2 * SR_DAY_S};
      sr_game_toast(g, "Credentials forged. The front desk will wave you through.");
      break;
    }
    case SR_NA_ALTER_ACCOUNT: {
      int32_t amount = 120 * sec;
      sr_player_tx(&g->player, now(g), amount, "creative accounting");
      d->alert = SR_MIN(3, d->alert + 1); /* money moves get noticed */
      break;
    }
    case SR_NA_BLACKOUT: {
      int district = g->city->blocks[g->city->buildings[d->building].block].district;
      g->net.blackout_district = (int8_t)district;
      g->net.blackout_until = now(g) + SR_BLACKOUT_S;
      sr_crowd_blackout(&g->crowd, g->city, district, &g->events, now(g));
      sr_game_toast(g, "The district goes dark.");
      break;
    }
    case SR_NA_OWN_ALL:
      for (int i = 0; i < d->in.device_count; i++) grant_token(g, d->in.devices[i].id, d->building, sec);
      sr_game_toast(g, "Every device in the building answers to you.");
      break;
    case SR_NA_LIGHTS_OFF:
    case SR_NA_LIGHTS_ON:
      if (nd->type == SR_NODE_POWER) {
        int lights = device_of_type(&d->in, SR_DEV_LIGHTS);
        r = device_action(g, d->building, lights, a, sec);
        break;
      }
      r = device_action(g, d->building, nd->device, a, sec);
      break;
    case SR_NA_CANCEL_ALARM:
    case SR_NA_FALSE_ALARM:
      r = device_action(g, d->building, nd->type == SR_NODE_DEVICE ? nd->device : alarm_device(d), a, sec);
      break;
    case SR_NA_PLANT_STORY:
    case SR_NA_KILL_STORY:
    case SR_NA_TAMPER_VOTE:
    case SR_NA_LEAK_BRIBES: r = sr_game_chess_node_act(g, d->building, a); break;
    default: r = device_action(g, d->building, nd->device, a, sec); break;
  }
  if (r == SR_OK) {
    d->nodes_hacked++;
    sr_progress_use_skill(&g->player.prog, SR_SK_NETRUNNING, SR_USE_SUCCESS, 3);
  }
  return r;
}

/* ---- breaching -------------------------------------------------------------------------------- */

SrResult sr_game_breach_start(SrGame *g, int node) {
  SrDive *d = &g->dive;
  if (!d->active || !d->in_host || !node_locked(d, node)) return SR_ERR_INVALID;
  int sec = d->host.security + (d->host.nodes[node].type == SR_NODE_GATEWAY ? 0 : 1);
  sr_breach_generate(&d->breach, sec, sr_hash_combine(g->city->seed, (uint64_t)d->building * 97u + (uint64_t)node));
  d->breach_node = node;
  return SR_OK;
}

static void breach_finished(SrGame *g) {
  SrDive *d = &g->dive;
  if (d->breach.done) {
    d->open[d->breach_node] = 1;
    for (int k = 0; k < d->ice_count; k++) /* a breached barrier collapses */
      if (d->ice[k].type == SR_ICE_BARRIER && d->ice[k].node == d->breach_node) d->ice[k].active = false;
    sr_progress_use_skill(&g->player.prog, SR_SK_NETRUNNING, SR_USE_SUCCESS, 6);
    sr_game_toast(g, "Breached.");
  } else {
    d->alert = SR_MIN(3, d->alert + 1);
    d->ice_count += sr_ice_reinforce(&d->host, d->alert, g->city->seed ^ (uint64_t)d->alert, d->ice + d->ice_count,
                                     SR_MX_MAX_ICE - d->ice_count);
    sr_game_toast(g, "Breach failed. The host is awake.");
  }
  d->breach_node = -1;
}

/* ---- programs --------------------------------------------------------------------------------- */

bool sr_game_program(SrGame *g, int slot) {
  SrDive *d = &g->dive;
  SrLoadout *l = &g->net.programs;
  if (!d->active || slot < 0 || slot >= l->count) return false;
  SrProgram p = (SrProgram)l->prog[slot];
  const SrProgramDef *def = sr_program_def(p);
  if (p == SR_PRG_WORM) return false; /* used on a gateway or core node */
  if (!sr_loadout_use(l, slot)) return false;
  switch (p) {
    case SR_PRG_SPIKE:
    case SR_PRG_CRASH: { /* nearest hostile ICE in front */
      SrVec3 f = sr_dir_from_yaw_pitch(d->yaw, d->pitch);
      int best = -1;
      float bd = 14.0f;
      for (int k = 0; k < d->ice_count; k++) {
        SrIce *ice = &d->ice[k];
        if (!ice->active) continue;
        SrVec3 to = sr_v3_sub(ice->e.pos, d->pos);
        float dist = sr_v3_len(to);
        if (dist < bd && (dist < 2.0f || sr_v3_dot(sr_v3_scale(to, 1.0f / dist), f) > 0.8f)) bd = dist, best = k;
      }
      if (best >= 0) sr_ice_damage(&d->ice[best], def->power);
      break;
    }
    case SR_PRG_SHIELD: d->shield = def->power; break;
    case SR_PRG_MEND: d->persona.integrity = SR_MIN(d->persona.max_integrity, d->persona.integrity + def->power); break;
    case SR_PRG_VEIL: d->veil = def->power; break;
    case SR_PRG_GHOST: d->trace = SR_MAX(0.0f, d->trace - def->power); break;
    default: break; /* SNIFF: reveals ICE (drawn by the renderer while on cooldown) */
  }
  return true;
}

/* ---- per-step ------------------------------------------------------------------------------- */

static void dumpshock_eject(SrGame *g, const char *msg) {
  sr_game_jack_out(g, true);
  sr_game_toast(g, msg);
}

static void trace_completed(SrGame *g) {
  SrDive *d = &g->dive;
  d->traced = true;
  SrCrime cr;
  memset(&cr, 0, sizeof cr);
  cr.jurisdiction = (int8_t)sr_game_jurisdiction(g, d->building);
  cr.severity = 3;
  cr.camera = true;
  cr.look = sr_appearance_key(&g->player.look);
  cr.time = now(g);
  sr_heat_crime(&g->heat, &cr);
  int owner = g->city->buildings[d->building].owner;
  if (owner >= 0) {
    cr.jurisdiction = (int8_t)owner;
    sr_heat_crime(&g->heat, &cr);
  }
  g->net.dispatch = true;
  g->net.dispatch_pos = g->player.pos;
  g->net.dispatch_at = now(g) + SR_DISPATCH_ETA;
  sr_game_toast(g, "TRACE COMPLETE. They know where your body is.");
}

void sr_game_dive_step(SrGame *g, const SrActionState *a, float dt) {
  SrDive *d = &g->dive;
  if (!d->active) return;
  sr_loadout_tick(&g->net.programs, dt);
  d->veil = SR_MAX(0.0f, d->veil - dt);
  d->shield = SR_MAX(0.0f, d->shield - dt);
  /* breach puzzle input: move_y selects a ring, look_x (mouse, right stick, touch drag) turns it */
  if (d->breach_node >= 0) {
    if (a) {
      int dir = a->move_y > 0.5f ? -1 : a->move_y < -0.5f ? 1 : 0;
      if (dir != 0 && dir != g->menu_axis) sr_breach_select(&d->breach, dir);
      g->menu_axis = dir;
      if (a->move_x != 0) sr_breach_rotate(&d->breach, a->move_x * SR_BREACH_TURN_RATE * dt * 1.3f);
      if (a->look_x != 0) sr_breach_rotate(&d->breach, -a->look_x * 3.0f);
      if (a->pressed[SR_ACT_INTERACT]) sr_breach_commit(&d->breach);
      if (a->pressed[SR_ACT_JACKIN]) d->breach.failed = true; /* abort */
    }
    sr_breach_tick(&d->breach, dt * d->speed);
    if (d->breach.done || d->breach.failed) breach_finished(g);
  } else if (a && d->feed_device >= 0) { /* watching a camera: any input returns to the persona */
    if (fabsf(a->move_x) + fabsf(a->move_y) > 0.2f || a->pressed[SR_ACT_INTERACT] || a->pressed[SR_ACT_JACKIN] ||
        a->pressed[SR_ACT_PAUSE])
      d->feed_device = -1;
  } else if (a) {
    /* flight: yaw/pitch from look, planar move, jump up and crouch down */
    d->yaw = sr_wrap_angle(d->yaw - a->look_x);
    d->pitch = sr_clampf(d->pitch - a->look_y, -1.3f, 1.3f);
    float speed = (d->in_host ? FLY_SPEED : GRID_SPEED) * (a->down[SR_ACT_SPRINT] ? 1.8f : 1.0f);
    float slow = 0;
    for (int k = 0; k < d->ice_count; k++)
      if (d->ice[k].active && d->ice[k].type == SR_ICE_TARPIT && sr_v3_len(sr_v3_sub(d->ice[k].e.pos, d->pos)) < 7.0f)
        slow = 0.6f;
    speed *= 1.0f - slow;
    float s = sinf(d->yaw), c = cosf(d->yaw);
    float vy = (a->down[SR_ACT_JUMP] ? 1.0f : 0.0f) - (a->down[SR_ACT_CROUCH] ? 1.0f : 0.0f);
    d->pos.x += (-s * a->move_y + c * a->move_x) * speed * dt;
    d->pos.z += (-c * a->move_y - s * a->move_x) * speed * dt;
    d->pos.y = sr_clampf(d->pos.y + vy * speed * dt, d->in_host ? -2.0f : 0.5f, d->in_host ? 40.0f : 60.0f);
    if (a->pressed[SR_ACT_INTERACT]) sr_game_interact(g);
    for (int q = 0; q < 4; q++)
      if (a->pressed[SR_ACT_QUICK1 + q]) sr_game_program(g, q);
    if (a->pressed[SR_ACT_JACKIN]) {
      int near = sr_game_node_near(g);
      bool clean = !d->in_host || (near >= 0 && d->host.nodes[near].type == SR_NODE_GATEWAY);
      sr_game_jack_out(g, !clean);
      return;
    }
  }
  if (!d->in_host) return;
  /* downloads */
  if (d->download_node >= 0) {
    d->download_left -= dt;
    if (d->download_left <= 0) download_done(g);
  }
  /* ICE */
  SrIceHit hit;
  memset(&hit, 0, sizeof hit);
  for (int k = 0; k < d->ice_count; k++)
    sr_ice_update(&d->ice[k], &d->host, d->pos, &d->persona, d->veil > 0, d->host.security, dt, &hit);
  if (hit.detected && d->alert < 3) {
    d->alert++;
    raise_host_alert(g, d->building, d->alert);
    d->ice_count += sr_ice_reinforce(&d->host, d->alert, g->city->seed ^ (uint64_t)(d->alert * 31),
                                     d->ice + d->ice_count, SR_MX_MAX_ICE - d->ice_count);
    sr_game_toast(g, "A Watcher saw you. The host alert rises.");
  }
  float shield = d->shield > 0 ? 0.5f : 1.0f;
  d->persona.integrity -= hit.integrity * shield;
  if (hit.body > 0) g->player.health -= hit.body * shield;
  if (!d->traced) {
    d->trace = SR_MIN(1.0f, d->trace + hit.trace);
    if (d->trace >= 1.0f) trace_completed(g);
  }
  if (d->persona.integrity <= 0) {
    dumpshock_eject(g, "Killer ICE flatlined your persona. Dumpshock.");
    return;
  }
  if (g->player.health <= 0) {
    sr_game_jack_out(g, true);
    return;
  }
  /* the body left behind */
  bool alert = false;
  if (g->site.active)
    for (int i = 0; i < g->site.guard_count; i++) {
      const SrGuard *gd = &g->site.guards[i];
      if (gd->e.dead || gd->e.ko || gd->e.alert == SR_ALERT_UNAWARE) continue;
      if (sr_v3_len(sr_v3_sub(gd->e.pos, g->player.pos)) < SR_BODY_ALERT_M) alert = true;
    }
  if (g->net.dispatch && now(g) >= g->net.dispatch_at &&
      sr_v3_len(sr_v3_sub(g->net.dispatch_pos, g->player.pos)) < SR_BODY_ALERT_M)
    alert = true;
  if (alert && !d->body_alert) sr_game_toast(g, "BODY ALERT: someone is next to you in meatspace.");
  d->body_alert = alert;
}

/* Upkeep of the network state, every step whether diving or not. */
void sr_game_net_tick(SrGame *g) {
  SrNet *n = &g->net;
  int64_t t = now(g);
  /* host countermeasures find tampering, reset devices and raise the alert */
  for (int i = n->tamper_count - 1; i >= 0; i--) {
    SrTamper *tp = &n->tamper[i];
    if (t < tp->detect_at) continue;
    if (tp->before == 255)
      sr_deltas_set(&g->deltas, tp->device, 0);
    else
      sr_deltas_set(&g->deltas, tp->device, tp->before);
    if (g->site.active && g->site.building == tp->building) {
      for (int dv = 0; dv < g->site.in.device_count; dv++)
        if (g->site.in.devices[dv].id == tp->device) {
          g->site.in.devices[dv].locked =
              original_locked(g, tp->building, dv) && !(tp->before != 255 && (tp->before & SR_DV_UNLOCKED));
          if (g->site.in.devices[dv].type == SR_DEV_LIGHTS)
            g->site.lights_off = tp->before != 255 && (tp->before & SR_DV_DISABLED);
        }
      g->site.hostile = true;
    }
    raise_host_alert(g, tp->building, 1);
    revoke_tokens(g, tp->building);
    remove_backdoors(g, tp->building);
    if (!tp->logs_erased) { /* the intrusion is attributed */
      SrCrime cr;
      memset(&cr, 0, sizeof cr);
      cr.jurisdiction = (int8_t)sr_game_jurisdiction(g, tp->building);
      cr.severity = 2;
      cr.camera = true;
      cr.evidence_left = true;
      cr.look = sr_appearance_key(&g->player.look);
      cr.time = t;
      sr_heat_crime(&g->heat, &cr);
    }
    n->detections++;
    memmove(&n->tamper[i], &n->tamper[i + 1], sizeof(SrTamper) * (size_t)(--n->tamper_count - i));
  }
  /* backdoors are found in time */
  for (int i = n->backdoor_count - 1; i >= 0; i--)
    if (t >= n->backdoors[i].discover_at) {
      raise_host_alert(g, n->backdoors[i].building, 1);
      memmove(&n->backdoors[i], &n->backdoors[i + 1], sizeof(SrBackdoor) * (size_t)(--n->backdoor_count - i));
    }
  /* expired tokens and credentials */
  for (int i = n->token_count - 1; i >= 0; i--)
    if (t >= n->tokens[i].expires)
      memmove(&n->tokens[i], &n->tokens[i + 1], sizeof(SrToken) * (size_t)(--n->token_count - i));
  if (n->blackout_district >= 0 && t >= n->blackout_until) {
    n->blackout_district = -1;
    sr_game_toast(g, "Power is back.");
  }
  if (n->dispatch && t >= n->dispatch_at + 30 * 60) n->dispatch = false; /* they gave up */
}

/* ---- Netsight quick actions ----------------------------------------------------------------- */

SrVec3 sr_game_device_pos(const SrGame *g, int device) {
  const SrSite *s = &g->site;
  const SrDevice *dv = &s->in.devices[device];
  if (dv->type == SR_DEV_CAMERA || dv->type == SR_DEV_TURRET) return sr_site_camera_pos(s, device);
  SrVec3 c = sr_site_room_center(s, dv->room);
  if (dv->type == SR_DEV_DOOR) {
    c.x += dv->door_dir == 0 ? 3.0f : 0;
    c.z += dv->door_dir == 1 ? 3.0f : 0;
  }
  if (dv->type == SR_DEV_LIGHTS) c.y = SR_SITE_FLOOR_Y + 2.9f;
  return c;
}

/* Device the player looks at within quick range (any device; ownership is checked by the caller). */
int sr_game_device_ahead(const SrGame *g) {
  if (!g->site.active) return -1;
  SrVec3 f = sr_dir_from_yaw_pitch(g->player.yaw, g->player.pitch);
  int best = -1;
  float bd = 0.94f;
  for (int i = 0; i < g->site.in.device_count; i++) {
    SrVec3 to = sr_v3_sub(sr_game_device_pos(g, i), g->player.pos);
    float dist = sr_v3_len(to);
    if (dist > QUICK_RANGE || dist < 0.01f) continue;
    float dot = sr_v3_dot(sr_v3_scale(to, 1.0f / dist), f);
    if (dot > bd) bd = dot, best = i;
  }
  return best;
}

static SrNodeAction quick_action_for(const SrGame *g, int device) {
  const SrDevice *dv = &g->site.in.devices[device];
  int v = sr_deltas_get(&g->deltas, dv->id);
  v = v < 0 ? 0 : v;
  switch (dv->type) {
    case SR_DEV_DOOR: return dv->locked ? SR_NA_UNLOCK : SR_NA_NONE;
    case SR_DEV_CAMERA: return (v & SR_DV_LOOPED) ? SR_NA_NONE : SR_NA_LOOP;
    case SR_DEV_TURRET: return SR_NA_FOF;
    case SR_DEV_LIGHTS: return (v & SR_DV_DISABLED) ? SR_NA_LIGHTS_ON : SR_NA_LIGHTS_OFF;
    case SR_DEV_ALARM: return SR_NA_CANCEL_ALARM;
    case SR_DEV_ELEVATOR: return SR_NA_ELEVATOR;
    default: return SR_NA_NONE; /* GCOVR_EXCL_LINE reason: every device type is listed */
  }
}

SrNodeAction sr_game_quick_action(const SrGame *g, int device) {
  if (!g->net.netsight || device < 0 || !g->site.active || sr_game_deck_grade(g) <= 0) return SR_NA_NONE;
  if (!sr_game_owns_device(g, g->site.in.devices[device].id)) return SR_NA_NONE;
  return quick_action_for(g, device);
}

SrResult sr_game_quick(SrGame *g, int device) {
  SrNodeAction a = sr_game_quick_action(g, device);
  if (a == SR_NA_NONE) return SR_ERR_DENIED;
  SrInterior tmp;
  sr_host_generate(g->city, g->site.building, interior_of(g, g->site.building, &tmp), &g->dive.host);
  return device_action(g, g->site.building, device, a, g->dive.host.security);
}

void sr_game_netsight(SrGame *g) { g->net.netsight = !g->net.netsight; }
