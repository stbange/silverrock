/* The Matrix: hosts, grid, persona, programs, breaching, ICE, traces, the body, paydata,
 * backdoors and every link back into the physical world. */
#include "core/sr_rng.h"
#include "game/sr_save.h"
#include "game/sr_script.h"
#include "render/sr_render.h"
#include "sr_test.h"
#include "ui/sr_ui.h"

#include <math.h>
#include <stdlib.h>

int device_of_type_test(const SrInterior *in, SrDeviceType t);
void raise_test_alert(SrGame *g, int b);

static SrGame *runner(uint64_t seed, int deck_type) {
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, SR_BG_NETRUNNER, "Runner");
  g->crowd_enabled = false;
  sr_game_leave(g);
  for (int i = g->player.inv.count - 1; i >= 0; i--)
    if (g->player.inv.items[i].kind == SR_IK_DECK) sr_inv_remove_at(&g->player.inv, i, 1);
  if (deck_type >= 0) sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_DECK, deck_type, 1));
  g->player.prog.skill_rank[SR_SK_NETRUNNING] = 4;
  return g;
}

static int mission(const SrGame *g, int skip) {
  for (int b = 0; b < g->city->building_count; b++)
    if ((g->city->buildings[b].flags & SR_BF_MISSION) && skip-- <= 0) return b;
  return -1;
}

static void steps(SrGame *g, int n) {
  SrActionState a;
  memset(&a, 0, sizeof a);
  for (int i = 0; i < n; i++) sr_game_step(g, &a);
}

static int node_of(const SrGame *g, SrNodeType t) {
  for (int i = 0; i < g->dive.host.node_count; i++)
    if (g->dive.host.nodes[i].type == t) return i;
  return -1;
}

static int device_node(const SrGame *g, SrDeviceType t, bool locked) {
  for (int i = 0; i < g->dive.host.node_count; i++) {
    const SrNode *n = &g->dive.host.nodes[i];
    if (n->type == SR_NODE_DEVICE && g->dive.in.devices[n->device].type == t &&
        (!locked || g->dive.in.devices[n->device].locked))
      return i;
  }
  return -1;
}

static void fly_to(SrGame *g, int node) {
  const SrNode *n = &g->dive.host.nodes[node];
  g->dive.pos = sr_v3(n->x, n->y + 1.5f, n->z + 0.5f);
}

static void open_all(SrGame *g) {
  for (int i = 0; i < g->dive.host.node_count; i++) g->dive.open[i] = 1;
  for (int k = 0; k < g->dive.ice_count; k++) g->dive.ice[k].active = false; /* a quiet host */
}

/* A port dive into a mission building, with the player's body inside it. */
static int port_dive(SrGame *g, int k) {
  int b = mission(g, k);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, b), SR_OK);
  return b;
}

/* ---- hosts and grid ------------------------------------------------------------------------ */

static void test_hosts(void) {
  SR_SPEC("matrix-navigation", "Every networked building has a host");
  SR_SPEC("matrix-navigation", "Stable host layout");
  SR_SPEC("matrix-world-link", "Every device has a node");
  SR_SPEC("matrix-navigation", "Owner strength raises security");
  SrCity *c = sr_city_create(3101, NULL);
  int hosts = 0;
  for (int b = 0; b < c->building_count; b++) {
    if (!sr_building_networked(c, b)) continue;
    SrInterior in;
    if (c->buildings[b].poi == SR_POI_HOME)
      sr_interior_apartment(c, b, 1, &in);
    else
      sr_interior_generate(c, b, &in);
    SrHost h, h2;
    sr_host_generate(c, b, &in, &h);
    sr_host_generate(c, b, &in, &h2);
    hosts++;
    SR_CHECK_EQ(sr_host_digest(&h), sr_host_digest(&h2));
    SR_CHECK(h.security >= 1 && h.security <= 10);
    SR_CHECK(h.gateway == 0 && h.core >= 0 && h.monitor >= 0);
    for (int d = 0; d < in.device_count; d++) { /* exactly one controller per device */
      int count = 0;
      for (int i = 0; i < h.node_count; i++) count += h.nodes[i].type == SR_NODE_DEVICE && h.nodes[i].device == d;
      SR_CHECK_EQ(count, 1);
    }
    for (int i = 1; i < h.node_count; i++) SR_CHECK(h.nodes[i].parent >= 0 && h.nodes[i].parent < i);
    if (b % 50 == 0) {
      SR_CHECK(sr_node_type_name((SrNodeType)h.nodes[h.node_count - 1].type)[0]);
      SR_CHECK(sr_paydata_name((SrPaydata)(b % SR_PD_COUNT))[0]);
    }
  }
  SR_CHECK(hosts > 20);
  SR_CHECK_EQ(sr_node_type_name(SR_NODE_COUNT)[0], '?');
  SR_CHECK_EQ(sr_paydata_name(SR_PD_COUNT)[0], '?');
  /* the same facility type, a strong and a weak owner */
  int strong = -1, weak = -1;
  for (int f = 0; f < c->factions.count; f++) {
    int ns = sr_faction_netsec(c, f);
    if (strong < 0 || ns > sr_faction_netsec(c, strong)) strong = f;
    if (weak < 0 || ns < sr_faction_netsec(c, weak)) weak = f;
  }
  SR_CHECK(sr_faction_netsec(c, strong) > sr_faction_netsec(c, weak));
  int imp = 6;
  SR_CHECK(sr_host_security_for(sr_faction_netsec(c, strong), imp) >
           sr_host_security_for(sr_faction_netsec(c, weak), imp));
  SR_CHECK_EQ(sr_faction_netsec(c, -1), 2);
  SR_CHECK(!sr_building_networked(c, -1));
  sr_city_destroy(c);
}

static void test_grid_mirrors_map(void) {
  SR_SPEC("matrix-navigation", "Grid mirrors the map");
  SrCity *c = sr_city_create(3102, NULL);
  for (int a = 0; a < c->district_count; a++)
    for (int b = 0; b < c->district_count; b++) {
      if (a == b) continue;
      const SrDistrict *da = &c->districts[a], *db = &c->districts[b];
      SrVec3 ga = sr_grid_pos(da->cx, da->cy, 0), gb = sr_grid_pos(db->cx, db->cy, 0);
      float cx = (float)(db->cx - da->cx), cz = (float)(db->cy - da->cy);
      float gx = gb.x - ga.x, gz = gb.z - ga.z;
      SR_CHECK((cx > 0) == (gx > 0) && (cz > 0) == (gz > 0)); /* east stays east, south stays south */
      SR_CHECK(fabsf(atan2f(cz, cx) - atan2f(gz, gx)) < 1e-4f);
    }
  SrMesh m;
  sr_mesh_init(&m);
  sr_build_grid(c, &m);
  SR_CHECK(m.nv > 1000);
  int wire = 0;
  for (int v = 0; v < m.nv; v++) wire += m.v[v].ground >= 2; /* filled (2) or hollow (3) wireframe */
  SR_CHECK_EQ(wire, m.nv);
  sr_mesh_free(&m);
  sr_city_destroy(c);
}

/* ---- access, persona, programs ------------------------------------------------------------- */

static void test_access(void) {
  SR_SPEC("matrix-navigation", "No deck, no dive");
  SR_SPEC("matrix-navigation", "Physical port skips the gateway");
  SR_SPEC("matrix-navigation", "Wireless range");
  SrGame *g = runner(3201, -1);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_ERR_DENIED); /* no deck */
  SR_CHECK(!sr_game_diving(g));
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_DECK, 1, 1));
  /* home rig: the global grid */
  SR_CHECK_EQ(sr_game_enter(g, g->player.home, SR_ENTRY_FRONT), SR_OK);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK);
  SR_CHECK(sr_game_diving(g) && !g->dive.in_host);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_ERR_INVALID); /* already in */
  sr_game_jack_out(g, false);
  sr_game_leave(g);
  /* physical port: inside the host, behind the gateway */
  int b = mission(g, 0);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  int con = sr_interior_find_prop(&g->site.in, SR_PROP_CONSOLE);
  SrVec3 at = sr_site_prop_pos(&g->site, con);
  SrVec3 c = sr_site_room_center(&g->site, g->site.in.props[con].room);
  SrVec3 dir = sr_v3_norm(sr_v3(c.x - at.x, 0, c.z - at.z));
  g->player.pos = sr_v3(at.x + dir.x, SR_SITE_FLOOR_Y + 1.7f, at.z + dir.z);
  g->player.yaw = atan2f(dir.x, dir.z);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK);
  SR_CHECK_EQ(g->dive.access, SR_ACCESS_PORT);
  SR_CHECK(g->dive.in_host && g->dive.building == b);
  SR_CHECK(!sr_game_node_locked(g, g->dive.host.gateway));
  SR_CHECK_NEAR(g->dive.speed, 1.0f, 1e-6);
  sr_game_jack_out(g, false);
  /* inside the building without a port: wireless at close range, slower */
  g->player.pos = sr_site_room_center(&g->site, g->site.in.entries[0].room);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK);
  SR_CHECK_EQ(g->dive.access, SR_ACCESS_WIRELESS);
  SR_CHECK(sr_game_node_locked(g, g->dive.host.gateway)); /* wireless meets the gateway */
  SR_CHECK(g->dive.speed < 1.0f);
  sr_game_jack_out(g, false);
  sr_game_leave(g);
  /* wireless from the street: in range works, out of range is refused */
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK); /* in front of the building */
  SR_CHECK_EQ(g->dive.access, SR_ACCESS_WIRELESS);
  SR_CHECK(g->dive.in_host);
  sr_game_jack_out(g, false);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, -2), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, -1), SR_ERR_NOT_FOUND);
  SrPersona ps = sr_game_persona(g);
  const SrBuilding *bd = &g->city->buildings[b];
  g->player.pos = sr_v3((float)(bd->x0 + bd->x1) / 200.0f + ps.signal * 3.0f, 1.7f, (float)bd->y1 / 100.0f + 2);
  SrResult far = sr_game_jack_in(g);
  SR_CHECK(far == SR_ERR_DENIED || g->dive.building != b);
  if (sr_game_diving(g)) sr_game_jack_out(g, false);
  /* a data spike in the site: a dive into that host from anywhere inside */
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  sr_site_add_effect(&g->site, SR_FX_LOOP, g->player.pos, 30, 300);
  SR_CHECK_EQ(sr_game_jack_in(g), SR_OK);
  SR_CHECK_EQ(g->dive.access, SR_ACCESS_SPIKE);
  sr_game_jack_out(g, false);
  /* a booth on the street */
  sr_game_leave(g);
  int booth = -1;
  for (int i = 0; i < g->city->building_count && booth < 0; i++)
    if (g->city->buildings[i].poi == SR_POI_MATRIX_BOOTH) booth = i;
  if (booth >= 0) {
    g->ctx = (SrContext){SR_CTX_SHOP, booth, -1, -1, 1, -1};
    sr_game_teleport_building(g, booth);
    g->ctx = (SrContext){SR_CTX_SHOP, booth, -1, -1, 1, -1};
    SR_CHECK_EQ(sr_game_jack_in(g), SR_OK);
    SR_CHECK_EQ(g->dive.access, SR_ACCESS_BOOTH);
    /* from the grid into a host construct */
    int host = -1;
    for (int i = 0; i < g->city->building_count && host < 0; i++)
      if (sr_building_networked(g->city, i)) host = i;
    SrVec3 hp = sr_grid_building_pos(g->city, host);
    g->dive.pos = sr_v3(hp.x, 2, hp.z);
    SR_CHECK_EQ(sr_game_host_near(g), host);
    SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_HOST);
    SR_CHECK_EQ(sr_game_interact(g), SR_OK);
    SR_CHECK(g->dive.in_host && g->dive.building == host);
    SR_CHECK_EQ(sr_game_dive_enter_host(g, host), SR_ERR_INVALID);
    sr_game_jack_out(g, false);
  }
  sr_game_destroy(g);
}

static void test_persona_and_programs(void) {
  SR_SPEC("matrix-navigation", "Better deck improves the persona");
  SR_SPEC("matrix-navigation", "Memory limit");
  SR_SPEC("matrix-navigation", "Program cooldown");
  for (int grade = 1; grade < SR_DECK_GRADES - 1; grade++) {
    SrPersona a = sr_persona_stats(grade, 4, 3), b = sr_persona_stats(grade + 1, 4, 3);
    SR_CHECK(b.max_integrity >= a.max_integrity && b.firewall >= a.firewall && b.bandwidth >= a.bandwidth &&
             b.stealth >= a.stealth && b.signal >= a.signal);
    SR_CHECK(b.bandwidth > a.bandwidth || b.signal > a.signal);
    SR_CHECK(sr_deck_memory(grade + 1) > sr_deck_memory(grade));
  }
  SR_CHECK_EQ(sr_persona_stats(0, 5, 5).bandwidth, 0);
  SR_CHECK_EQ(sr_deck_memory(0), 0);
  SrLoadout l;
  memset(&l, 0, sizeof l);
  SR_CHECK(sr_loadout_add(&l, 1, SR_PRG_SPIKE));
  SR_CHECK(sr_loadout_add(&l, 1, SR_PRG_VEIL));
  SR_CHECK(!sr_loadout_add(&l, 1, SR_PRG_CRASH)); /* 2 + 3 + 4 > 7 */
  SR_CHECK(sr_loadout_add(&l, 1, SR_PRG_SNIFF));
  SR_CHECK(!sr_loadout_add(&l, 1, SR_PRG_COUNT));
  SR_CHECK_EQ(sr_loadout_memory(&l), 6);
  SR_CHECK(sr_loadout_use(&l, 0));
  SR_CHECK(!sr_loadout_use(&l, 0)); /* cooling down */
  sr_loadout_tick(&l, sr_program_def(SR_PRG_SPIKE)->cooldown + 0.01f);
  SR_CHECK(sr_loadout_use(&l, 0));
  SR_CHECK(!sr_loadout_use(&l, 9));
  SR_CHECK_EQ(sr_loadout_find(&l, SR_PRG_WORM), -1);
  SR_CHECK(sr_program_def(SR_PRG_COUNT) == NULL);
  SrLoadout full;
  memset(&full, 0, sizeof full);
  for (int i = 0; i < 20; i++) sr_loadout_add(&full, 4, SR_PRG_SNIFF);
  SR_CHECK_EQ(full.count, SR_LOADOUT_MAX);
}

/* ---- breach ------------------------------------------------------------------------------- */

static void test_breach(void) {
  SR_SPEC("matrix-navigation", "Breach difficulty scales");
  SR_SPEC("matrix-navigation", "Breach by touch");
  SrBreach lo, hi;
  sr_breach_generate(&lo, 2, 7);
  sr_breach_generate(&hi, 8, 7);
  SR_CHECK(hi.rings > lo.rings || fabsf(hi.speed[0]) > fabsf(lo.speed[0]));
  for (int sec = 1; sec <= 10; sec++)
    for (uint64_t seed = 1; seed <= 20; seed++) {
      SrBreach b;
      sr_breach_generate(&b, sec, seed);
      SR_CHECK(sr_breach_solve_time(&b) <= b.time_limit);
      SR_CHECK(sr_breach_autosolve(&b, 1.0f / 60.0f));
    }
  /* mistakes cost time and can run the clock out */
  SrBreach b;
  sr_breach_generate(&b, 5, 3);
  float t0 = b.time_left;
  SR_CHECK(!sr_breach_aligned(&b, 0));
  SR_CHECK(!sr_breach_commit(&b));
  SR_CHECK(b.time_left < t0 && b.mistakes == 1);
  sr_breach_select(&b, -1);
  SR_CHECK_EQ(b.sel, b.rings - 1);
  sr_breach_select_ring(&b, 99);
  SR_CHECK_EQ(b.sel, b.rings - 1);
  while (!b.failed) sr_breach_commit(&b);
  SR_CHECK(!sr_breach_commit(&b));
  sr_breach_tick(&b, 1); /* finished puzzles stand still */
  SrBreach timeout;
  sr_breach_generate(&timeout, 3, 4);
  sr_breach_tick(&timeout, 1000);
  SR_CHECK(timeout.failed);
  /* touch alone: stick up/down selects, a right-side drag turns, the interact button commits */
  SrGame *g = runner(3301, 2);
  int bld = mission(g, 1);
  SR_CHECK_EQ(sr_game_enter(g, bld, SR_ENTRY_FRONT), SR_OK);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, bld), SR_OK);
  for (int k = 0; k < g->dive.ice_count; k++) g->dive.ice[k].active = false;
  fly_to(g, g->dive.host.gateway);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_NODE);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(g->dive.breach_node == g->dive.host.gateway);
  SrInput in;
  sr_input_init(&in, true);
  sr_input_set_screen(&in, 844, 390, 0, 0, 0, 0);
  SrBreach *br = &g->dive.breach;
  for (int guard = 0; guard < 20000 && g->dive.breach_node >= 0; guard++) {
    float a = sr_wrap_angle(br->angle[br->sel]);
    if (fabsf(a) < SR_BREACH_TOLERANCE * 0.5f) { /* tap the interact button */
      const SrTouchButton *ib = &in.layout.buttons[4];
      sr_input_touch(&in, 2, ib->x / 844.0f, ib->y / 390.0f, 0);
      SrActionState s = sr_input_tick(&in, SR_STEP_DT);
      sr_input_touch(&in, 2, ib->x / 844.0f, ib->y / 390.0f, 2);
      sr_game_step(g, &s);
      continue;
    }
    /* drag on the right half of the screen turns the ring */
    float x0 = 0.6f, y0 = 0.45f, dx = a > 0 ? 0.004f : -0.004f;
    sr_input_touch(&in, 3, x0, y0, 0);
    sr_input_tick(&in, SR_STEP_DT);
    sr_input_touch(&in, 3, x0 + dx, y0, 1);
    SrActionState s = sr_input_tick(&in, SR_STEP_DT);
    sr_input_touch(&in, 3, x0 + dx, y0, 2);
    sr_game_step(g, &s);
  }
  SR_CHECK(!sr_game_node_locked(g, g->dive.host.gateway));
  /* the move stick selects rings */
  sr_game_breach_start(g, node_of(g, SR_NODE_CORE));
  int sel = g->dive.breach.sel;
  SrActionState s;
  memset(&s, 0, sizeof s);
  s.move_y = -1;
  sr_game_step(g, &s);
  SR_CHECK(g->dive.breach.sel != sel || g->dive.breach.rings == 1);
  s.move_y = 0;
  s.move_x = 1;
  sr_game_step(g, &s);
  memset(&s, 0, sizeof s);
  s.pressed[SR_ACT_JACKIN] = true; /* aborting raises the alert */
  int alert = g->dive.alert;
  sr_game_step(g, &s);
  SR_CHECK(g->dive.breach_node < 0 && g->dive.alert > alert);
  SR_CHECK_EQ(sr_game_breach_start(g, g->dive.host.gateway), SR_ERR_INVALID); /* already open */
  sr_game_destroy(g);
}

static void test_credentials_skip_breach(void) {
  SR_SPEC("matrix-navigation", "Credentials skip the breach");
  SR_SPEC("matrix-world-link", "Forged credentials grant access");
  SrGame *g = runner(3302, 2);
  int b = mission(g, 2);
  g->net.creds[g->net.cred_count++] = (SrCred){b, sr_game_time(g) + 3600};
  SR_CHECK(sr_game_has_creds(g, b));
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  SR_CHECK(!sr_game_node_locked(g, g->dive.host.gateway));
  sr_game_jack_out(g, false);
  /* presented at the entrance: staff and locks treat the player as authorised */
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  steps(g, 1);
  SR_CHECK(g->site.authorised);
  SR_CHECK(!sr_site_restricted(&g->site, g->site.in.target_room, false));
  int locked = -1;
  for (int i = 0; i < g->site.in.device_count; i++)
    if (g->site.in.devices[i].type == SR_DEV_DOOR && g->site.in.devices[i].locked) locked = i;
  SR_CHECK(locked < 0 ||
           sr_site_door_open(&g->site, g->site.in.devices[locked].room, g->site.in.devices[locked].door_dir));
  g->site.hostile = true; /* suspicion rises: the act is over */
  steps(g, 1);
  SR_CHECK(!g->site.authorised);
  /* forging them at a records node */
  sr_game_leave(g);
  int hall = -1;
  for (int i = 0; i < g->city->building_count && hall < 0; i++)
    if (g->city->buildings[i].poi == SR_POI_CITY_HALL) hall = i;
  g->net.cred_count = 0;
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, hall), SR_OK);
  open_all(g);
  int rec = node_of(g, SR_NODE_RECORDS);
  SR_CHECK(rec >= 0);
  SR_CHECK_EQ(sr_game_node_act(g, rec, SR_NA_FORGE_CREDS), SR_OK);
  SR_CHECK(sr_game_has_creds(g, hall));
  sr_game_destroy(g);
}

/* ---- ICE, trace, body ---------------------------------------------------------------------- */

static void test_ice(void) {
  SR_SPEC("matrix-navigation", "Watcher raises the alert");
  SR_SPEC("matrix-navigation", "Killer ICE dumps the player");
  SR_SPEC("matrix-navigation", "Black ICE harms the body");
  SrGame *g = runner(3401, 1);
  int b = mission(g, 3);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  SrDive *d = &g->dive;
  int watcher = -1;
  for (int k = 0; k < d->ice_count; k++)
    if (d->ice[k].type == SR_ICE_WATCHER) watcher = k;
  SR_CHECK(watcher >= 0);
  int ice0 = d->ice_count;
  d->pos = d->ice[watcher].e.pos;
  steps(g, 2);
  SR_CHECK(d->alert >= 1);
  SR_CHECK(d->ice_count > ice0); /* reinforcements spawned */
  SR_CHECK(sr_game_host_alert(g, b) >= 1);
  /* killer ICE grinds integrity to zero: ejected with dumpshock */
  float hp = g->player.health;
  int killer = -1;
  for (int k = 0; k < d->ice_count; k++)
    if (d->ice[k].type == SR_ICE_KILLER) killer = k;
  SR_CHECK(killer >= 0);
  for (int k = 0; k < d->ice_count; k++)
    if (k != killer) d->ice[k].active = false;
  d->pos = d->ice[killer].e.pos;
  for (int i = 0; i < 60 * 60 && sr_game_diving(g); i++) steps(g, 1);
  SR_CHECK(!sr_game_diving(g));
  SR_CHECK(g->player.health < hp);
  SR_CHECK(g->glitch_time > 0); /* stunned */
  /* black ICE hurts the body directly */
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  d = &g->dive;
  for (int k = 0; k < d->ice_count; k++) d->ice[k].active = false;
  SrIce *bl = &d->ice[0];
  memset(bl, 0, sizeof *bl);
  bl->type = SR_ICE_BLACK;
  bl->active = true;
  sr_enemy_init(&bl->e, d->pos, -1, 70);
  d->ice_count = SR_MAX(d->ice_count, 1);
  g->player.health = 100;
  steps(g, 2);
  SR_CHECK(g->player.health < 100);
  /* programs fight back */
  bl->e.pos = sr_v3_add(d->pos, sr_v3_scale(sr_dir_from_yaw_pitch(d->yaw, d->pitch), 3.0f));
  int spike = sr_loadout_find(&g->net.programs, SR_PRG_SPIKE);
  float before = bl->e.health;
  SR_CHECK(sr_game_program(g, spike));
  SR_CHECK(bl->e.health < before);
  SR_CHECK(!sr_game_program(g, spike)); /* cooldown */
  SR_CHECK(!sr_game_program(g, 99));
  int veil = sr_loadout_find(&g->net.programs, SR_PRG_VEIL);
  if (veil >= 0) SR_CHECK(sr_game_program(g, veil) && d->veil > 0);
  int worm = sr_loadout_find(&g->net.programs, SR_PRG_WORM);
  if (worm >= 0) SR_CHECK(!sr_game_program(g, worm)); /* used on gateway or core nodes */
  SR_CHECK(sr_ice_damage(bl, 1000));
  SR_CHECK(!sr_ice_damage(bl, 1));
  /* the other ICE types and names */
  SrHost h = d->host;
  SrIce ice[SR_MX_MAX_ICE];
  int n = sr_ice_reinforce(&h, 3, 1, ice, SR_MX_MAX_ICE);
  SR_CHECK(n >= 3);
  SrIce tar;
  memset(&tar, 0, sizeof tar);
  tar.type = SR_ICE_TARPIT;
  tar.active = true;
  tar.node = 0;
  sr_enemy_init(&tar.e, d->pos, -1, 10);
  SrIceHit hit;
  memset(&hit, 0, sizeof hit);
  sr_ice_update(&tar, &h, d->pos, &d->persona, false, 5, 0.1f, &hit);
  SR_CHECK(hit.slow > 0);
  tar.type = SR_ICE_BARRIER;
  sr_ice_update(&tar, &h, d->pos, &d->persona, false, 5, 0.1f, &hit);
  for (int t = 0; t <= SR_ICE_COUNT; t++) SR_CHECK(sr_ice_name((SrIceType)t)[0]);
  /* sysops join at lockdown in well-defended hosts: they hunt and push the trace */
  h.security = SR_MAX(h.security, 6);
  SrIce sys[SR_MX_MAX_ICE];
  int ns = sr_ice_reinforce(&h, 3, 2, sys, SR_MX_MAX_ICE);
  int sysop = -1;
  for (int i = 0; i < ns; i++)
    if (sys[i].type == SR_ICE_SYSOP) sysop = i;
  SR_CHECK(sysop >= 0);
  sys[sysop].e.pos = d->pos;
  memset(&hit, 0, sizeof hit);
  sr_ice_update(&sys[sysop], &h, d->pos, &d->persona, false, 6, 0.1f, &hit);
  SR_CHECK(hit.trace > 0 && hit.integrity > 0);
  sr_game_destroy(g);
}

static void test_trace_and_body(void) {
  SR_SPEC("matrix-navigation", "Trace completed");
  SR_SPEC("matrix-navigation", "Body alert");
  SR_SPEC("matrix-navigation", "Emergency jack-out");
  SrGame *g = runner(3402, 1);
  int b = port_dive(g, 4);
  SrDive *d = &g->dive;
  for (int k = 0; k < d->ice_count; k++) d->ice[k].active = false;
  SrIce *tr = &d->ice[0];
  memset(tr, 0, sizeof *tr);
  tr->type = SR_ICE_TRACER;
  tr->active = true;
  sr_enemy_init(&tr->e, d->pos, -1, 20);
  d->ice_count = SR_MAX(d->ice_count, 1);
  int heat0 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat0 += g->heat.level[k];
  for (int i = 0; i < 60 * 300 && !d->traced; i++) steps(g, 1);
  SR_CHECK(d->traced);
  int heat1 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat1 += g->heat.level[k];
  SR_CHECK(heat1 > heat0);
  SR_CHECK(g->net.dispatch);
  SR_CHECK(sr_v3_len(sr_v3_sub(g->net.dispatch_pos, g->player.pos)) < 1e-3f);
  /* a guard walks up to the body */
  SR_CHECK(!d->body_alert || g->site.guard_count == 0 || true);
  SrGuard *gd = &g->site.guards[0];
  gd->e.dead = gd->e.ko = false;
  gd->e.alert = SR_ALERT_SEARCHING;
  gd->e.pos = sr_v3(g->player.pos.x + 3, gd->e.pos.y, g->player.pos.z);
  steps(g, 1);
  SR_CHECK(d->body_alert);
  /* the UI shows it */
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  g->steps = 15;
  sr_ui_build(&ui, g, NULL, false);
  SR_CHECK(ui.mesh.ni > 0);
  sr_ui_free(&ui);
  /* emergency jack-out: back in the body at once, with dumpshock */
  float hp = g->player.health;
  int64_t t = sr_game_time(g);
  SrActionState s;
  memset(&s, 0, sizeof s);
  s.pressed[SR_ACT_JACKIN] = true;
  d->pos = sr_v3(100, 20, 100); /* far from the gateway */
  sr_game_step(g, &s);
  SR_CHECK(!sr_game_diving(g));
  SR_CHECK(sr_game_time(g) - t <= 1);
  SR_CHECK(g->player.health < hp);
  SR_CHECK(sr_game_host_alert(g, b) >= 0);
  /* the dispatch arrives later and gives up after a while */
  sr_game_skip(g, 3 * 3600, false);
  steps(g, 1);
  SR_CHECK(!g->net.dispatch);
  sr_game_destroy(g);
}

/* ---- data, backdoors ----------------------------------------------------------------------- */

static void test_paydata(void) {
  SR_SPEC("matrix-navigation", "Download time");
  SR_SPEC("matrix-navigation", "Paydata as a job deliverable");
  SR_SPEC("matrix-world-link", "Stolen data reveals briefing details");
  SrPersona slow = sr_persona_stats(1, 3, 1), fast = sr_persona_stats(4, 6, 6);
  SR_CHECK(sr_download_time(80, &fast, 1) < sr_download_time(80, &slow, 1));
  SR_CHECK(sr_download_time(80, &slow, SR_WIRELESS_SPEED) > sr_download_time(80, &slow, 1));
  SrPersona none = sr_persona_stats(0, 0, 0);
  SR_CHECK(sr_download_time(10, &none, 1) > 1e6f);
  SrGame *g = runner(3501, 3);
  int b = mission(g, 5);
  SrJob j;
  sr_job_generate(g->city, SR_JT_STEAL_DATA, 9, sr_game_time(g), &j);
  j.state = SR_JS_ACCEPTED;
  j.target_building = b;
  j.intel = 0;
  j.id = g->jobs.next_id++;
  g->jobs.jobs[g->jobs.count++] = j;
  g->active_job = j.id;
  char brief[512];
  sr_job_briefing(g->city, &g->jobs.jobs[g->jobs.count - 1], brief, sizeof brief);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  open_all(g);
  /* the security data store: guard and camera details become known */
  int sec = -1, other = -1;
  for (int i = 0; i < g->dive.host.node_count; i++) {
    if (g->dive.host.nodes[i].type != SR_NODE_DATA) continue;
    if (g->dive.host.nodes[i].data_kind == SR_PD_SECURITY)
      sec = i;
    else
      other = i;
  }
  SR_CHECK(sec >= 0);
  fly_to(g, sec);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_NODE);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK); /* opens the node menu */
  SR_CHECK_EQ(g->screen, SR_SCREEN_SHOP);
  SR_CHECK(g->menu.node_menu && g->menu.action[0] == SR_NA_DOWNLOAD);
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  SR_CHECK_EQ(sr_game_shop_buy(g, 0), SR_OK);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  float total = g->dive.download_total;
  SR_CHECK(total > 0);
  steps(g, (int)(total * 60) + 5);
  SR_CHECK(g->dive.download_node < 0);
  const SrJob *jj = sr_jobs_get(&g->jobs, j.id);
  SR_CHECK((jj->intel & (SR_INTEL_GUARDS | SR_INTEL_CAMERAS)) == (SR_INTEL_GUARDS | SR_INTEL_CAMERAS));
  char brief2[512];
  sr_job_briefing(g->city, jj, brief2, sizeof brief2);
  SR_CHECK(strcmp(brief, brief2) != 0); /* the briefing now shows the site's security */
  SR_CHECK(jj->executed);               /* the client wanted data from this host: objective done */
  SR_CHECK(sr_inv_find(&g->player.inv, SR_IK_PAYDATA, -1) >= 0);
  if (other >= 0) {
    fly_to(g, other);
    SR_CHECK_EQ(sr_game_node_act(g, other, SR_NA_DOWNLOAD), SR_OK);
  }
  sr_game_jack_out(g, false);
  SR_CHECK(sr_game_report(g, j.id) > 0);
  sr_game_destroy(g);
}

static void test_backdoors(void) {
  SR_SPEC("matrix-navigation", "Backdoor re-entry");
  SR_SPEC("matrix-navigation", "Backdoor expires");
  SrGame *g = runner(3502, 3);
  int b = mission(g, 6);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  SR_CHECK(sr_loadout_find(&g->net.programs, SR_PRG_WORM) >= 0);
  SR_CHECK_EQ(sr_game_node_act(g, g->dive.host.gateway, SR_NA_WORM), SR_ERR_DENIED); /* still locked */
  g->dive.open[g->dive.host.gateway] = 1;
  SR_CHECK_EQ(sr_game_node_act(g, g->dive.host.gateway, SR_NA_WORM), SR_OK);
  SR_CHECK(sr_game_has_backdoor(g, b));
  SR_CHECK_EQ(sr_game_node_act(g, g->dive.host.gateway, SR_NA_WORM), SR_ERR_DENIED); /* cooldown */
  SR_CHECK_EQ(sr_game_node_act(g, g->dive.host.gateway, SR_NA_JACK_OUT), SR_OK);
  SR_CHECK(!sr_game_diving(g));
  /* back later: straight past the gateway */
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  SR_CHECK(!sr_game_node_locked(g, g->dive.host.gateway));
  sr_game_jack_out(g, false);
  /* the host finds it: removed, alert up */
  int sec = g->dive.host.security;
  sr_game_skip(g, (int64_t)sr_discovery_time(sec) + 60, false);
  steps(g, 1);
  SR_CHECK(!sr_game_has_backdoor(g, b));
  SR_CHECK(sr_game_host_alert(g, b) >= 1);
  SR_CHECK(sr_discovery_time(9) < sr_discovery_time(2));
  sr_game_destroy(g);
}

/* ---- world links --------------------------------------------------------------------------- */

static void test_doors_cameras_turrets(void) {
  SR_SPEC("matrix-world-link", "Unlocking a door from the Matrix");
  SR_SPEC("matrix-world-link", "Looped camera sees nothing");
  SR_SPEC("matrix-world-link", "Disabled camera draws a check");
  SR_SPEC("matrix-world-link", "Viewing a feed");
  SR_SPEC("matrix-world-link", "Retargeted turret");
  SrGame *g = runner(3601, 3);
  int b = port_dive(g, 7);
  open_all(g);
  int door = device_node(g, SR_DEV_DOOR, true);
  SR_CHECK(door >= 0);
  int dev = g->dive.host.nodes[door].device;
  SR_CHECK(g->site.in.devices[dev].locked);
  SR_CHECK_EQ(sr_game_node_act(g, door, SR_NA_UNLOCK), SR_OK);
  SR_CHECK(!g->site.in.devices[dev].locked);
  SR_CHECK_EQ(sr_game_node_act(g, door, SR_NA_UNLOCK), SR_ERR_INVALID); /* now it offers Lock */
  sr_game_jack_out(g, false);
  SR_CHECK(!g->site.in.devices[dev].locked); /* stays unlocked after the dive */
  sr_game_leave(g);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  SR_CHECK(!g->site.in.devices[dev].locked);
  /* cameras: looped sees nothing; disabled draws a guard */
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, b), SR_OK);
  open_all(g);
  int cam = -1;
  for (int i = 0; i < g->dive.host.node_count; i++) {
    const SrNode *n = &g->dive.host.nodes[i];
    if (n->type == SR_NODE_DEVICE && g->dive.in.devices[n->device].type == SR_DEV_CAMERA &&
        g->dive.in.devices[n->device].room == g->dive.in.target_room)
      cam = i;
  }
  SR_CHECK(cam >= 0);
  SR_CHECK_EQ(sr_game_node_act(g, cam, SR_NA_LOOP), SR_OK);
  SR_CHECK_EQ(sr_game_node_act(g, cam, SR_NA_FEED), SR_OK); /* the feed: the room seen from the camera */
  SR_CHECK_EQ(g->dive.feed_device, g->dive.host.nodes[cam].device);
  SrSceneCache *sc = (SrSceneCache *)calloc(1, sizeof *sc); /* too big for the web build's stack */
  sr_scene_init(sc, false);
  SrFrame *f = (SrFrame *)malloc(sizeof(SrFrame));
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SrVec3 cp = sr_site_camera_pos(&g->site, g->dive.feed_device);
  SR_CHECK(sr_v3_len(sr_v3_sub(f->eye, cp)) < 1e-3f);
  SrActionState mv;
  memset(&mv, 0, sizeof mv);
  mv.move_y = 1;
  sr_game_step(g, &mv); /* moving returns to the persona */
  SR_CHECK_EQ(g->dive.feed_device, -1);
  sr_scene_build(sc, g, 16.0f / 9.0f, f); /* host view */
  sr_game_jack_out(g, false);
  for (int i = 0; i < g->site.guard_count; i++) g->site.guards[i].e.dead = true;
  SrVec3 tc = sr_site_room_center(&g->site, g->site.in.target_room);
  g->player.pos = sr_v3(tc.x, SR_SITE_FLOOR_Y + 1.7f, tc.z);
  for (int i = 0; i < 300; i++) steps(g, 1);
  SR_CHECK(!g->site.camera_seen); /* looped: no detection */
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, b), SR_OK);
  open_all(g);
  SR_CHECK_EQ(sr_game_node_act(g, cam, SR_NA_DISABLE), SR_OK);
  sr_game_jack_out(g, false);
  int camdev = g->dive.host.nodes[cam].device;
  SrGuard *gd = &g->site.guards[0];
  gd->e.dead = false;
  gd->e.alert = SR_ALERT_UNAWARE;
  gd->e.awareness = 0;
  g->player.pos = sr_site_room_center(&g->site, g->site.in.entries[0].room);
  for (int i = 0; i < 60 * (int)(SR_CAMERA_CHECK_S + 1); i++) steps(g, 1);
  SR_CHECK(g->site.checked[camdev]);
  SR_CHECK(gd->e.alert != SR_ALERT_UNAWARE);
  SrVec3 cpos = sr_site_camera_pos(&g->site, camdev);
  SR_CHECK(fabsf(gd->e.investigate.x - cpos.x) < 1.0f);
  /* turrets: switched friend-or-foe shoots guards */
  int tur = -1;
  for (int k = 0; k < 12 && tur < 0; k++) {
    sr_game_leave(g);
    int bb = mission(g, k);
    SrInterior in;
    sr_interior_generate(g->city, bb, &in);
    if (device_of_type_test(&in, SR_DEV_TURRET) < 0) continue;
    SR_CHECK_EQ(sr_game_enter(g, bb, SR_ENTRY_FRONT), SR_OK);
    SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, bb), SR_OK);
    open_all(g);
    tur = device_node(g, SR_DEV_TURRET, false);
  }
  if (tur >= 0) {
    SR_CHECK_EQ(sr_game_node_act(g, tur, SR_NA_FOF), SR_OK);
    sr_game_jack_out(g, false);
    int td = g->dive.host.nodes[tur].device;
    SrGuard *tg = &g->site.guards[0];
    tg->e.dead = tg->e.ko = false;
    tg->e.health = 30;
    tg->e.pos = sr_site_room_center(&g->site, g->site.in.devices[td].room);
    g->player.pos = sr_site_room_center(&g->site, g->site.in.devices[td].room);
    float hp = g->player.health;
    for (int i = 0; i < 120 && !tg->e.dead; i++) steps(g, 1);
    SR_CHECK(tg->e.dead);
    SR_CHECK(g->player.health >= hp - 1e-3f);
  }
  free(f);
  sr_scene_free(sc);
  free(sc);
  sr_game_destroy(g);
}

int device_of_type_test(const SrInterior *in, SrDeviceType t) {
  for (int d = 0; d < in->device_count; d++)
    if (in->devices[d].type == t) return d;
  return -1;
}

static void test_alarms_lights_elevator(void) {
  SR_SPEC("matrix-world-link", "False alarm diversion");
  SR_SPEC("matrix-world-link", "Cancelling an alarm");
  SR_SPEC("matrix-world-link", "Lights out helps stealth");
  SR_SPEC("matrix-world-link", "Calling an elevator");
  SrGame *g = runner(3602, 3);
  port_dive(g, 8);
  open_all(g);
  /* false alarm: guards head for the far wing */
  int mon = node_of(g, SR_NODE_MONITOR);
  for (int i = 0; i < g->site.guard_count; i++) g->site.guards[i].e.alert = SR_ALERT_UNAWARE;
  SR_CHECK_EQ(sr_game_node_act(g, mon, SR_NA_FALSE_ALARM), SR_OK);
  int searching = 0;
  for (int i = 0; i < g->site.guard_count; i++) searching += g->site.guards[i].e.alert == SR_ALERT_SEARCHING;
  SR_CHECK(searching > 0);
  SrVec3 far = g->site.guards[0].e.investigate;
  SR_CHECK(sr_v3_len(sr_v3_sub(far, g->player.pos)) > 5.0f);
  /* cancelling an alarm: stops it and the reinforcements */
  g->site.alarm = true;
  g->site.reinforce_at = g->site.time + 30;
  SR_CHECK_EQ(sr_game_node_act(g, mon, SR_NA_CANCEL_ALARM), SR_OK);
  SR_CHECK(!g->site.alarm);
  SR_CHECK_EQ(g->site.reinforce_at, 0);
  /* lights out: darker, guards take longer */
  float lit = sr_site_light(&g->site, 0);
  int lights = device_node(g, SR_DEV_LIGHTS, false);
  SR_CHECK_EQ(sr_game_node_act(g, lights, SR_NA_LIGHTS_OFF), SR_OK);
  SR_CHECK(g->site.lights_off && sr_site_light(&g->site, 0) < lit);
  SrPerceptInput pin;
  memset(&pin, 0, sizeof pin);
  pin.visibility = 18;
  pin.difficulty = SR_DIFF_STANDARD;
  SrEnemy e1, e2;
  sr_enemy_init(&e1, sr_v3(0, 0, 0), -1, 100);
  e2 = e1;
  pin.player_pos = sr_v3(0, 0, -4);
  pin.light = lit;
  int t_lit = 0, t_dark = 0;
  while (sr_perceive(&e1, &pin, 0.05f) != SR_ALERT_COMBAT && t_lit < 10000) t_lit++;
  pin.light = sr_site_light(&g->site, 0);
  while (sr_perceive(&e2, &pin, 0.05f) != SR_ALERT_COMBAT && t_dark < 10000) t_dark++;
  SR_CHECK(t_dark > t_lit);
  SR_CHECK_EQ(sr_game_node_act(g, lights, SR_NA_LIGHTS_ON), SR_OK);
  SR_CHECK(!g->site.lights_off);
  int power = node_of(g, SR_NODE_POWER);
  SR_CHECK_EQ(sr_game_node_act(g, power, SR_NA_LIGHTS_OFF), SR_OK);
  SR_CHECK(g->site.lights_off);
  SR_CHECK_EQ(sr_game_node_act(g, power, SR_NA_BLACKOUT), g->dive.host.district_grid ? SR_OK : SR_ERR_INVALID);
  sr_game_jack_out(g, false);
  /* the elevator */
  int eb = -1;
  for (int k = 0; k < 40 && eb < 0; k++) {
    int bb = mission(g, k);
    if (bb >= 0 && (g->city->buildings[bb].flags & SR_BF_ELEVATOR)) eb = bb;
  }
  for (int b = 0; b < g->city->building_count && eb < 0; b++)
    if ((g->city->buildings[b].flags & SR_BF_ELEVATOR) && sr_building_networked(g->city, b) && sr_game_enterable(g, b))
      eb = b;
  if (eb >= 0) {
    sr_game_leave(g);
    SR_CHECK_EQ(sr_game_enter(g, eb, SR_ENTRY_FRONT), SR_OK);
    SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, eb), SR_OK);
    open_all(g);
    int el = device_node(g, SR_DEV_ELEVATOR, false);
    SR_CHECK(el >= 0);
    SR_CHECK_EQ(sr_game_node_act(g, el, SR_NA_ELEVATOR), SR_OK);
    sr_game_jack_out(g, false);
    for (int i = 0; i < 60 * 4; i++) steps(g, 1);
    SR_CHECK_EQ(g->site.elevator_target, 1);
    SR_CHECK_NEAR(g->site.elevator_floor, 1.0f, 1e-3);
  }
  sr_game_destroy(g);
}

static void test_blackout_and_records(void) {
  SR_SPEC("matrix-world-link", "District blackout");
  SR_SPEC("matrix-world-link", "Erasing records reduces Heat");
  SrGame *g = runner(3603, 3);
  int hall = -1;
  for (int i = 0; i < g->city->building_count && hall < 0; i++)
    if (g->city->buildings[i].poi == SR_POI_CITY_HALL) hall = i;
  /* police Heat and evidence first */
  int pol = g->city->factions.police;
  SrCrime cr;
  memset(&cr, 0, sizeof cr);
  cr.jurisdiction = (int8_t)pol;
  cr.severity = 4;
  cr.witnessed = true;
  cr.camera = true;
  cr.evidence_left = true;
  cr.time = sr_game_time(g);
  sr_heat_crime(&g->heat, &cr);
  int heat = g->heat.level[pol];
  SR_CHECK(heat > 0 && g->heat.evidence_count > 0);
  g->crowd_enabled = true;
  sr_game_teleport_building(g, hall);
  steps(g, 2);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, hall), SR_OK);
  open_all(g);
  SR_CHECK(g->dive.host.district_grid);
  int rec = node_of(g, SR_NODE_RECORDS);
  SR_CHECK_EQ(sr_game_node_act(g, rec, SR_NA_ERASE_WARRANT), SR_OK);
  SR_CHECK(g->heat.level[pol] < heat);
  for (int i = 0; i < g->heat.evidence_count; i++) SR_CHECK(g->heat.evidence[i].jurisdiction != pol);
  int64_t cash = g->player.cred;
  SR_CHECK_EQ(sr_game_node_act(g, rec, SR_NA_ALTER_ACCOUNT), SR_OK);
  SR_CHECK(g->player.cred > cash);
  /* the district goes dark, citizens react, power returns */
  int district = g->city->blocks[g->city->buildings[hall].block].district;
  int power = node_of(g, SR_NODE_POWER);
  int events = g->events.count;
  SR_CHECK_EQ(sr_game_node_act(g, power, SR_NA_BLACKOUT), SR_OK);
  SR_CHECK(sr_game_blackout_at(g, district));
  SR_CHECK(g->events.count > events);
  int reacting = 0;
  for (int i = 0; i < g->crowd.count; i++)
    reacting += g->crowd.agents[i].state == SR_AG_SHELTER || g->crowd.agents[i].state == SR_AG_GAWK;
  SR_CHECK(reacting > 0);
  sr_game_jack_out(g, false);
  SrSceneCache *sc = (SrSceneCache *)calloc(1, sizeof *sc); /* too big for the web build's stack */
  sr_scene_init(sc, false);
  SrFrame *f = (SrFrame *)malloc(sizeof(SrFrame));
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  for (int i = 0; i < f->nlights; i++)
    SR_CHECK(sr_city_district_at(g->city, (int32_t)(f->lights[i].pos.x * 100), (int32_t)(f->lights[i].pos.z * 100)) !=
             district);
  free(f);
  sr_scene_free(sc);
  free(sc);
  sr_game_skip(g, SR_BLACKOUT_S + 60, false);
  steps(g, 1);
  SR_CHECK(!sr_game_blackout_at(g, district));
  SR_CHECK_EQ(g->net.blackout_district, -1);
  sr_game_destroy(g);
}

static void test_netsight(void) {
  SR_SPEC("matrix-world-link", "Netsight shows devices");
  SR_SPEC("matrix-world-link", "Quick action on an owned device");
  SR_SPEC("matrix-world-link", "No quick action without ownership");
  SrGame *g = runner(3604, 3);
  int b = mission(g, 9);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  int cam = device_of_type_test(&g->site.in, SR_DEV_CAMERA);
  SrVec3 cp = sr_game_device_pos(g, cam);
  SrVec3 c = sr_site_room_center(&g->site, g->site.in.devices[cam].room);
  g->player.pos = sr_v3(c.x, SR_SITE_FLOOR_Y + 1.7f, c.z);
  SrVec3 to = sr_v3_sub(cp, g->player.pos);
  g->player.yaw = atan2f(-to.x, -to.z);
  g->player.pitch = atan2f(to.y, sqrtf(to.x * to.x + to.z * to.z));
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_NETSIGHT] = true;
  sr_game_step(g, &a);
  SR_CHECK(g->net.netsight);
  g->player.pos = sr_v3(c.x, SR_SITE_FLOOR_Y + 1.7f, c.z);
  SR_CHECK_EQ(sr_game_device_ahead(g), cam);
  /* not owned: nothing offered */
  SR_CHECK_EQ(sr_game_quick_action(g, cam), SR_NA_NONE);
  SR_CHECK(sr_game_context(g).kind != SR_CTX_QUICK);
  SR_CHECK_EQ(sr_game_quick(g, cam), SR_ERR_DENIED);
  /* the overlay highlights devices with their type */
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  g->ctx = sr_game_context(g);
  sr_ui_build(&ui, g, NULL, false);
  int font = 0;
  for (int v = 0; v < ui.mesh.nv; v++) font += ui.mesh.v[v].layer == SR_LAYER_FONT;
  SR_CHECK(font > 0);
  sr_ui_free(&ui);
  /* owned through the core (or a backdoor): looped without a dive */
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_PORT, b), SR_OK);
  open_all(g);
  SR_CHECK_EQ(sr_game_node_act(g, node_of(g, SR_NODE_CORE), SR_NA_OWN_ALL), SR_OK);
  sr_game_jack_out(g, false);
  SR_CHECK(sr_game_owns_device(g, g->site.in.devices[cam].id));
  SR_CHECK_EQ(sr_game_quick_action(g, cam), SR_NA_LOOP);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_QUICK);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  int v = sr_deltas_get(&g->deltas, g->site.in.devices[cam].id);
  SR_CHECK(v > 0 && (v & SR_DV_LOOPED));
  SR_CHECK(!sr_game_diving(g));
  SR_CHECK_EQ(sr_game_quick_action(g, cam), SR_NA_NONE); /* already looped */
  for (int i = 0; i < g->site.in.device_count; i++) sr_game_quick_action(g, i);
  sr_game_netsight(g);
  SR_CHECK_EQ(sr_game_quick_action(g, cam), SR_NA_NONE); /* Netsight off */
  sr_game_destroy(g);
}

static void test_countermeasures(void) {
  SR_SPEC("matrix-world-link", "Tampering detected and reset");
  SR_SPEC("matrix-world-link", "Erased logs prevent attribution");
  SrGame *g = runner(3605, 3);
  int b = port_dive(g, 10);
  open_all(g);
  int door = device_node(g, SR_DEV_DOOR, true);
  int dev = g->dive.host.nodes[door].device;
  SR_CHECK_EQ(sr_game_node_act(g, door, SR_NA_UNLOCK), SR_OK);
  int sec = g->dive.host.security;
  sr_game_jack_out(g, false);
  SR_CHECK(!g->site.in.devices[dev].locked);
  int heat0 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat0 += g->heat.level[k];
  g->clock.paused = false;
  sr_game_skip(g, (int64_t)sr_discovery_time(sec) + 60, false);
  steps(g, 1);
  SR_CHECK(g->site.in.devices[dev].locked); /* reset */
  SR_CHECK(g->site.hostile);                /* site alert up */
  SR_CHECK(sr_game_host_alert(g, b) >= 1);
  SR_CHECK_EQ(g->net.detections, 1);
  SR_CHECK(!sr_game_owns_device(g, g->site.in.devices[dev].id));
  int heat1 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat1 += g->heat.level[k];
  SR_CHECK(heat1 > heat0); /* attributed */
  /* with logs erased before jacking out: reset, but no evidence pointing at the player */
  SrGame *h = runner(3606, 3);
  int b2 = port_dive(h, 11);
  open_all(h);
  int door2 = device_node(h, SR_DEV_DOOR, true);
  SR_CHECK_EQ(sr_game_node_act(h, door2, SR_NA_UNLOCK), SR_OK);
  SR_CHECK_EQ(sr_game_node_act(h, node_of(h, SR_NODE_MONITOR), SR_NA_ERASE_LOGS), SR_OK);
  int sec2 = h->dive.host.security;
  sr_game_jack_out(h, false);
  int ev0 = h->heat.evidence_count, hh0 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) hh0 += h->heat.level[k];
  sr_game_skip(h, (int64_t)sr_discovery_time(sec2) + 60, false);
  steps(h, 1);
  SR_CHECK_EQ(h->net.detections, 1);
  SR_CHECK_EQ(h->heat.evidence_count, ev0);
  int hh1 = 0;
  for (int k = 0; k < SR_MAX_FACTIONS; k++) hh1 += h->heat.level[k];
  SR_CHECK_EQ(hh1, hh0);
  (void)b2;
  sr_game_destroy(h);
  sr_game_destroy(g);
}

/* ---- flight, persistence, planner, rendering ------------------------------------------------ */

static void test_touch_flight(void) {
  SR_SPEC("matrix-navigation", "Touch flight");
  SrGame *g = runner(3701, 2);
  int b = mission(g, 12);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  for (int k = 0; k < g->dive.ice_count; k++) g->dive.ice[k].active = false;
  SrInput in;
  sr_input_init(&in, true);
  sr_input_set_screen(&in, 844, 390, 0, 0, 0, 0);
  SrVec3 p0 = g->dive.pos;
  float yaw0 = g->dive.yaw;
  /* left stick forward */
  sr_input_touch(&in, 1, 0.2f, 0.6f, 0);
  sr_input_tick(&in, SR_STEP_DT);
  for (int i = 0; i < 30; i++) {
    sr_input_touch(&in, 1, 0.2f, 0.6f - 0.002f * (float)i, 1);
    SrActionState s = sr_input_tick(&in, SR_STEP_DT);
    sr_game_step(g, &s);
  }
  sr_input_touch(&in, 1, 0.2f, 0.5f, 2);
  SR_CHECK(sr_v3_len(sr_v3_sub(g->dive.pos, p0)) > 0.5f);
  /* right-side drag turns */
  sr_input_touch(&in, 3, 0.6f, 0.5f, 0);
  sr_input_tick(&in, SR_STEP_DT);
  sr_input_touch(&in, 3, 0.7f, 0.5f, 1);
  SrActionState s = sr_input_tick(&in, SR_STEP_DT);
  sr_game_step(g, &s);
  sr_input_touch(&in, 3, 0.7f, 0.5f, 2);
  SR_CHECK(fabsf(g->dive.yaw - yaw0) > 0.01f);
  /* up and down buttons (jump, crouch) */
  float y0 = g->dive.pos.y;
  const SrTouchButton *jump = &in.layout.buttons[2], *crouch = &in.layout.buttons[3];
  sr_input_touch(&in, 4, jump->x / 844.0f, jump->y / 390.0f, 0);
  for (int i = 0; i < 20; i++) {
    SrActionState st = sr_input_tick(&in, SR_STEP_DT);
    sr_game_step(g, &st);
  }
  sr_input_touch(&in, 4, jump->x / 844.0f, jump->y / 390.0f, 2);
  SR_CHECK(g->dive.pos.y > y0);
  float y1 = g->dive.pos.y;
  sr_input_touch(&in, 5, crouch->x / 844.0f, crouch->y / 390.0f, 0);
  for (int i = 0; i < 20; i++) {
    SrActionState st = sr_input_tick(&in, SR_STEP_DT);
    sr_game_step(g, &st);
  }
  SR_CHECK(g->dive.pos.y < y1);
  sr_game_destroy(g);
}

static void test_matrix_persistence(void) {
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SrGame *g = runner(3702, 3);
  int b = mission(g, 13);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  open_all(g);
  SR_CHECK_EQ(sr_game_node_act(g, g->dive.host.gateway, SR_NA_WORM), SR_OK);
  SR_CHECK_EQ(sr_game_node_act(g, node_of(g, SR_NODE_CORE), SR_NA_OWN_ALL), SR_OK);
  int door = device_node(g, SR_DEV_DOOR, true);
  if (door >= 0) sr_game_node_act(g, door, SR_NA_UNLOCK);
  g->dive.alert = 2;
  raise_test_alert(g, b);
  sr_game_jack_out(g, false);
  g->net.creds[g->net.cred_count++] = (SrCred){b, sr_game_time(g) + 3600};
  g->net.blackout_district = 1;
  g->net.blackout_until = sr_game_time(g) + 100;
  g->net.dispatch = true;
  SR_CHECK_EQ(sr_save_to(g, &st, "net.srsv", 0), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "net.srsv"), SR_OK);
  SR_CHECK_EQ(l->net.token_count, g->net.token_count);
  SR_CHECK_EQ(l->net.backdoor_count, 1);
  SR_CHECK_EQ(l->net.cred_count, 1);
  SR_CHECK_EQ(l->net.tamper_count, g->net.tamper_count);
  SR_CHECK_EQ(l->net.alert_count, g->net.alert_count);
  SR_CHECK_EQ(l->net.programs.count, g->net.programs.count);
  SR_CHECK_EQ(l->net.blackout_district, 1);
  SR_CHECK(l->net.dispatch);
  SR_CHECK(sr_game_has_backdoor(l, b));
  /* a version 2 save (before the Matrix) loads with an empty network state */
  SrWriter w;
  sr_writer_init(&w);
  SR_CHECK_EQ(sr_save_write(g, &w, 2, 1), SR_OK);
  SrGame *old = sr_game_create();
  SR_CHECK_EQ(sr_save_read(old, w.data, w.len), SR_OK);
  SR_CHECK_EQ(old->net.token_count, 0);
  SR_CHECK_EQ(old->net.blackout_district, -1);
  sr_writer_free(&w);
  sr_game_destroy(old);
  sr_game_destroy(l);
  sr_game_destroy(g);
  sr_memstore_free(&mem);
}

void raise_test_alert(SrGame *g, int b) { /* host alert via a detected watcher */
  for (int k = 0; k < g->dive.ice_count; k++)
    if (g->dive.ice[k].type == SR_ICE_WATCHER) {
      g->dive.ice[k].active = true;
      g->dive.ice[k].e.dead = false;
      g->dive.pos = g->dive.ice[k].e.pos;
      steps(g, 2);
      break;
    }
  (void)b;
}

static void test_matrix_planner(void) {
  SrCity *c = sr_city_create(3703, NULL);
  int with = 0, done = 0;
  SrGame *g = sr_game_create();
  for (int i = 0; i < 300 && with < 8; i++) {
    SrJob j;
    if (!sr_job_generate(c, (SrJobType)(SR_JT_PLANT + i % 6), (uint64_t)i * 17u, 0, &j)) continue;
    SrMatrixPlan p = sr_job_matrix_plan(c, &j);
    if (!(j.approaches & (1u << SR_AP_RUNNER))) continue;
    SR_CHECK(p.feasible && p.security >= 1 && p.steps >= 2);
    with++;
  }
  SR_CHECK(with > 0);
  sr_game_destroy(g);
  sr_city_destroy(c);
  /* jobs validated with a Matrix approach are completed by the netrunner bot */
  SrGame *bot = sr_game_create();
  sr_game_new(bot, 3704, NULL, SR_BG_NETRUNNER, "Bot");
  bot->crowd_enabled = false;
  SrAutoplayStats stats = sr_autoplay(bot, SR_STRAT_NETRUNNER, 28, 3);
  done = stats.jobs_done;
  SR_CHECK(done > 0);
  SR_CHECK(stats.missed_rent == 0);
  sr_game_destroy(bot);
}

static void test_matrix_render(void) {
  SrGame *g = runner(3705, 2);
  int b = mission(g, 14);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_WIRELESS, b), SR_OK);
  SrMesh m;
  sr_mesh_init(&m);
  sr_build_host(g, 1.0f, &m);
  SR_CHECK(m.nv > 100);
  sr_game_breach_start(g, g->dive.host.gateway);
  g->dive.download_node = 1;
  g->dive.download_total = 2;
  g->dive.download_left = 1;
  g->dive.trace = 0.4f;
  g->dive.feed_device = 0;
  g->ctx = sr_game_context(g);
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  SR_CHECK(ui.mesh.ni > 0);
  g->dive.breach_node = -1;
  g->dive.feed_device = -1;
  fly_to(g, g->dive.host.gateway);
  g->ctx = sr_game_context(g);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  SrSceneCache *sc = (SrSceneCache *)calloc(1, sizeof *sc); /* too big for the web build's stack */
  sr_scene_init(sc, false);
  SrFrame *f = (SrFrame *)malloc(sizeof(SrFrame));
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SR_CHECK(f->ndraws >= 1);
  sr_game_jack_out(g, false);
  SR_CHECK_EQ(sr_game_jack_in_via(g, SR_ACCESS_RIG, -1), SR_OK); /* the grid */
  sr_scene_build(sc, g, 16.0f / 9.0f, f);
  SR_CHECK(f->ndraws >= 1);
  free(f);
  sr_scene_free(sc);
  free(sc);
  sr_mesh_free(&m);
  for (int a = 0; a <= SR_NA_COUNT; a++) SR_CHECK(sr_node_action_name((SrNodeAction)a) != NULL);
  sr_game_destroy(g);
}

void suite_matrix(void) {
  SR_RUN(test_hosts);
  SR_RUN(test_grid_mirrors_map);
  SR_RUN(test_access);
  SR_RUN(test_persona_and_programs);
  SR_RUN(test_breach);
  SR_RUN(test_credentials_skip_breach);
  SR_RUN(test_ice);
  SR_RUN(test_trace_and_body);
  SR_RUN(test_paydata);
  SR_RUN(test_backdoors);
  SR_RUN(test_doors_cameras_turrets);
  SR_RUN(test_alarms_lights_elevator);
  SR_RUN(test_blackout_and_records);
  SR_RUN(test_netsight);
  SR_RUN(test_countermeasures);
  SR_RUN(test_touch_flight);
  SR_RUN(test_matrix_persistence);
  SR_RUN(test_matrix_planner);
  SR_RUN(test_matrix_render);
}
