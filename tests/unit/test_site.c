/* Interiors, guards, cameras, gadgets, cyberware, rooftops, shops, stash and wardrobe. */
#include "core/sr_rng.h"
#include "game/sr_save.h"
#include "game/sr_script.h"
#include "render/sr_render.h"
#include "sr_test.h"
#include "ui/sr_ui.h"

#include <math.h>
#include <stdlib.h>

static SrGame *game(uint64_t seed) {
  SrGame *g = sr_game_create();
  sr_game_new(g, seed, NULL, SR_BG_STREET_KID, "Tester");
  g->crowd_enabled = false;
  return g;
}

static int mission_building(const SrGame *g, int skip) {
  for (int b = 0; b < g->city->building_count; b++)
    if ((g->city->buildings[b].flags & SR_BF_MISSION) && skip-- <= 0) return b;
  return -1;
}

static void face(SrGame *g, SrVec3 at, float dist) {
  SrVec3 c = sr_site_room_center(&g->site, sr_site_room_at(&g->site, at.x, at.z));
  SrVec3 dir = sr_v3_norm(sr_v3(c.x - at.x, 0, c.z - at.z));
  if (sr_v3_len(sr_v3(c.x - at.x, 0, c.z - at.z)) < 0.01f) dir = sr_v3(0, 0, 1);
  g->player.pos = sr_v3(at.x + dir.x * dist, SR_SITE_FLOOR_Y + 1.7f, at.z + dir.z * dist);
  g->player.yaw = atan2f(dir.x, dir.z);
}

static void face_prop(SrGame *g, SrPropType t) {
  int pr = sr_interior_find_prop(&g->site.in, t);
  face(g, sr_site_prop_pos(&g->site, pr), 1.0f);
}

static void steps(SrGame *g, int n) {
  SrActionState a;
  memset(&a, 0, sizeof a);
  for (int i = 0; i < n; i++) sr_game_step(g, &a);
}

static SrJob *add_site_job(SrGame *g, int building) {
  SrJob j;
  sr_job_generate(g->city, SR_JT_STEAL_DATA, 7, sr_game_time(g), &j);
  j.state = SR_JS_ACCEPTED;
  j.approaches = 0xF;
  j.target_building = building;
  j.id = g->jobs.next_id++;
  g->jobs.jobs[g->jobs.count++] = j;
  g->active_job = j.id;
  return &g->jobs.jobs[g->jobs.count - 1];
}

/* ---- world: props and apartments -------------------------------------------------------- */

static void test_apartment_template(void) {
  SR_SPEC("player-life", "Moving to a better apartment");
  SrGame *g = game(21);
  int home = g->player.home;
  uint64_t first = 0;
  for (int tier = 0; tier < SR_HOUSE_COUNT; tier++) {
    SrInterior a, b;
    sr_interior_apartment(g->city, home, tier, &a);
    sr_interior_apartment(g->city, home, tier, &b);
    SR_CHECK(a.apartment);
    SR_CHECK_EQ(a.tier, tier);
    SR_CHECK_EQ(sr_interior_digest(&a), sr_interior_digest(&b)); /* deterministic */
    SR_CHECK(sr_interior_find_prop(&a, SR_PROP_BED) >= 0);
    SR_CHECK(sr_interior_find_prop(&a, SR_PROP_STASH) >= 0);
    SR_CHECK(sr_interior_find_prop(&a, SR_PROP_TERMINAL) >= 0);
    SR_CHECK(sr_interior_find_prop(&a, SR_PROP_WARDROBE) >= 0);
    SR_CHECK(sr_interior_find_prop(&a, SR_PROP_EXIT) >= 0);
    SR_CHECK_EQ(a.target_room, -1);
    if (tier == 0) first = sr_interior_digest(&a);
    for (int r = 0; r < a.w * a.h; r++) SR_CHECK(sr_interior_reachable(&a, 0, r, false, false));
  }
  SrInterior big;
  sr_interior_apartment(g->city, home, SR_HOUSE_PENTHOUSE, &big);
  SR_CHECK(big.w * big.h > 1);
  SR_CHECK(sr_interior_digest(&big) != first);
  /* decoration differs between homes */
  int differ = 0;
  for (int b = 1; b < 20; b++) {
    SrInterior x, y;
    sr_interior_apartment(g->city, b, SR_HOUSE_STUDIO, &x);
    sr_interior_apartment(g->city, b + 20, SR_HOUSE_STUDIO, &y);
    differ += memcmp(x.props, y.props, sizeof x.props) != 0;
  }
  SR_CHECK(differ > 15);
  SR_CHECK_EQ(sr_interior_find_prop(&big, SR_PROP_CONSOLE), -1);
  SR_CHECK_EQ(sr_interior_room_at(&big, -1, 0), -1);
  SR_CHECK_EQ(sr_interior_room_at(&big, 0, 100000), -1);
  SR_CHECK_EQ(sr_interior_room_at(&big, 10, 10), 0);
  sr_interior_apartment(g->city, home, 99, &big); /* clamped */
  SR_CHECK_EQ(big.tier, SR_HOUSE_PENTHOUSE);
  sr_game_destroy(g);
}

static void test_mission_interior_props(void) {
  SrGame *g = game(22);
  for (int k = 0; k < 10; k++) {
    int b = mission_building(g, k);
    SrInterior in;
    sr_interior_generate(g->city, b, &in);
    int obj = sr_interior_find_prop(&in, SR_PROP_OBJECTIVE);
    SR_CHECK(obj >= 0 && in.props[obj].room == in.target_room);
    int con = sr_interior_find_prop(&in, SR_PROP_CONSOLE);
    SR_CHECK(con >= 0 && in.props[con].room == in.security_room);
    int exits = 0;
    for (int i = 0; i < in.prop_count; i++) exits += in.props[i].type == SR_PROP_EXIT;
    SR_CHECK_EQ(exits, in.entry_count);
  }
  sr_game_destroy(g);
}

/* ---- the apartment in play ------------------------------------------------------------- */

static void test_start_inside_apartment(void) {
  SR_SPEC("city-generation", "Stable interiors");
  SrGame *g = game(23);
  SR_CHECK(sr_game_inside(g));
  SR_CHECK(g->site.in.apartment);
  SR_CHECK_EQ(g->site.building, g->player.home);
  SR_CHECK(g->player.pos.y < 0);
  /* walking into a wall is blocked; walking into the room is not */
  SR_CHECK(sr_game_blocked_here(g, g->site.ox - 1, g->site.oz + 3));
  SrVec3 c = sr_site_room_center(&g->site, 0);
  SR_CHECK(!sr_game_blocked_here(g, c.x, c.z));
  /* walk to the door and leave */
  face_prop(g, SR_PROP_EXIT);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_EXIT);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(!sr_game_inside(g));
  SR_CHECK(g->player.pos.y > 0);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_HOME);
  /* the apartment was generated on approach */
  steps(g, 31);
  SR_CHECK_EQ(g->approach_building, g->player.home);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(sr_game_inside(g));
  /* evicted: locked */
  sr_game_leave(g);
  g->player.evicted = true;
  g->player.rent_debt = 1000000;
  SR_CHECK_EQ(sr_game_enter(g, g->player.home, SR_ENTRY_FRONT), SR_ERR_DENIED);
  g->player.evicted = false;
  /* not enterable: an ordinary building */
  int plain = -1;
  for (int b = 0; b < g->city->building_count && plain < 0; b++)
    if (!sr_game_enterable(g, b)) plain = b;
  SR_CHECK_EQ(sr_game_enter(g, plain, SR_ENTRY_FRONT), SR_ERR_DENIED);
  SR_CHECK(!sr_game_enterable(g, -1));
  /* an apartment has no roof entry */
  SR_CHECK_EQ(sr_game_enter(g, g->player.home, SR_ENTRY_ROOF), SR_ERR_NOT_FOUND);
  sr_game_destroy(g);
}

static void test_amenities(void) {
  SR_SPEC("player-life", "Stash keeps items");
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SrGame *g = game(24);
  SrPlayer *p = &g->player;
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_COMPONENT, 0, 4));
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_MEDKIT, 1));
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_SMOKE, 1));
  int before = p->inv.count;
  face_prop(g, SR_PROP_STASH);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_STASH);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(p->stash.count >= 2);
  SR_CHECK(p->inv.count < before);
  SR_CHECK(sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_MEDKIT) >= 0); /* quick-slot gadget stays */
  /* several days later, saved and loaded: still in the stash */
  int stashed = p->stash.count;
  face_prop(g, SR_PROP_BED);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_BED);
  for (int d = 0; d < 3; d++) SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(sr_save_to(g, &st, "stash.srsv", 0), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "stash.srsv"), SR_OK);
  SR_CHECK_EQ(l->player.stash.count, stashed);
  SR_CHECK(sr_game_inside(l)); /* saved at home: loaded at home */
  SR_CHECK(fabsf(l->player.pos.x - g->player.pos.x) < 1e-3f);
  /* take everything back */
  face_prop(l, SR_PROP_STASH);
  SR_CHECK_EQ(sr_game_interact(l), SR_OK);
  SR_CHECK_EQ(sr_game_stash(l), SR_OK); /* stores again */
  SR_CHECK_EQ(sr_game_stash(l), SR_OK); /* takes back */
  /* nothing at all to stash */
  l->player.inv.count = 0;
  l->player.stash.count = 0;
  SR_CHECK_EQ(sr_game_stash(l), SR_ERR_NOT_FOUND);
  /* wardrobe: a new look changes how witnesses remember the player */
  face_prop(g, SR_PROP_WARDROBE);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_WARDROBE);
  uint64_t key = sr_appearance_key(&p->look);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(sr_appearance_key(&p->look) != key);
  /* terminal */
  face_prop(g, SR_PROP_TERMINAL);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_TERMINAL);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(g->screen, SR_SCREEN_TERMINAL);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

static void test_moving_home(void) {
  SR_SPEC("player-life", "Moving to a better apartment");
  SrGame *g = game(40);
  SrPlayer *p = &g->player;
  p->cred = 50000;
  sr_inv_add(&p->stash, sr_item_simple(SR_IK_COMPONENT, 0, 3));
  int old = p->home, stash = p->stash.count;
  int64_t due = p->rent_due;
  face_prop(g, SR_PROP_TERMINAL);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(g->screen, SR_SCREEN_TERMINAL);
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_INTERACT] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_SHOP);
  SR_CHECK(g->menu.housing);
  int row = -1;
  for (int i = 0; i < g->menu.count; i++)
    if (g->menu.items[i].type == SR_HOUSE_MID) row = i;
  SrUi ui;
  sr_ui_init(&ui);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, NULL, false);
  sr_ui_free(&ui);
  SR_CHECK_EQ(sr_game_shop_buy(g, row), SR_OK);
  SR_CHECK(p->home != old);
  SR_CHECK_EQ(p->tier, SR_HOUSE_MID);
  SR_CHECK_EQ(p->rent, sr_housing_def(SR_HOUSE_MID)->rent);
  SR_CHECK_EQ(p->rent_due, due); /* the new rent applies from the next due date */
  SR_CHECK_EQ(p->stash.count, stash);
  SR_CHECK(sr_game_inside(g) && g->site.building == p->home && g->site.in.tier == SR_HOUSE_MID);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  /* no deposit, no keys; nonsense tiers */
  p->cred = 0;
  SR_CHECK_EQ(sr_game_move_home(g, SR_HOUSE_LOFT), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_move_home(g, 77), SR_ERR_NOT_FOUND);
  p->cred = 50000;
  sr_game_leave(g);
  SR_CHECK_EQ(sr_game_move_home(g, SR_HOUSE_STUDIO), SR_OK); /* from the street */
  sr_game_destroy(g);
}

/* ---- site geometry --------------------------------------------------------------------- */

static void test_site_geometry(void) {
  SrGame *g = game(25);
  sr_game_leave(g);
  int b = mission_building(g, 0);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  SrSite *s = &g->site;
  SR_CHECK(!s->in.apartment);
  SR_CHECK(s->guard_count > 0);
  /* outside the interior */
  SR_CHECK(sr_site_blocked(s, s->ox - 1, s->oz + 1, 0.35f, false));
  SR_CHECK_EQ(sr_site_room_at(s, s->ox - 1, s->oz), -1);
  /* find a door connection and a locked door */
  int open_room = -1, open_dir = -1, locked = -1;
  for (int r = 0; r < s->in.w * s->in.h; r++)
    for (int d = 0; d < 2; d++)
      if (s->in.doors[r] & (1 << d)) {
        if (sr_site_door_open(s, r, d) && open_room < 0) open_room = r, open_dir = d;
      }
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_DOOR && s->in.devices[i].locked) locked = i;
  SR_CHECK(open_room >= 0 && locked >= 0);
  SrVec3 c = sr_site_room_center(s, open_room);
  float dx = open_dir == 0 ? 3.0f : 0, dz = open_dir == 1 ? 3.0f : 0;
  SR_CHECK(!sr_site_blocked(s, c.x + dx, c.z + dz, 0.35f, false)); /* in the doorway */
  SR_CHECK(sr_site_blocked(s, c.x + dx + (open_dir ? 1.5f : 0), c.z + dz + (open_dir ? 0 : 1.5f), 0.35f, false));
  SrVec3 far = sr_site_room_center(s, open_dir == 0 ? open_room + 1 : open_room + s->in.w);
  SR_CHECK(sr_site_los(s, c, far)); /* through the open door */
  const SrDevice *ld = &s->in.devices[locked];
  SrVec3 lc = sr_site_room_center(s, ld->room);
  float ldx = ld->door_dir == 0 ? 3.0f : 0, ldz = ld->door_dir == 1 ? 3.0f : 0;
  SR_CHECK(sr_site_blocked(s, lc.x + ldx, lc.z + ldz, 0.35f, false)); /* locked door */
  SrVec3 behind = sr_site_room_center(s, ld->door_dir == 0 ? ld->room + 1 : ld->room + s->in.w);
  SR_CHECK(!sr_site_los(s, lc, behind));
  sr_site_unlock(s, &g->deltas, locked);
  SR_CHECK(!sr_site_blocked(s, lc.x + ldx, lc.z + ldz, 0.35f, false));
  /* vents: only crawling fits */
  int vr = -1, vd = -1;
  for (int r = 0; r < s->in.w * s->in.h && vr < 0; r++)
    for (int d = 0; d < 2 && vr < 0; d++)
      if ((s->in.vents[r] & (1 << d)) && !(s->in.doors[r] & (1 << d))) vr = r, vd = d;
  if (vr >= 0) {
    SrVec3 vc = sr_site_room_center(s, vr);
    float vx = vc.x + (vd == 0 ? 3.0f : SR_SITE_VENT_OFFSET), vz = vc.z + (vd == 1 ? 3.0f : SR_SITE_VENT_OFFSET);
    SR_CHECK(sr_site_blocked(s, vx, vz, 0.2f, false));
    SR_CHECK(!sr_site_blocked(s, vx, vz, 0.2f, true));
  }
  /* smoke blocks sight */
  SR_CHECK(sr_site_los(s, c, sr_v3(c.x + 2, c.y, c.z)));
  sr_site_add_effect(s, SR_FX_SMOKE, sr_v3(c.x + 1, 0, c.z), 0.6f, 5);
  SR_CHECK(!sr_site_los(s, c, sr_v3(c.x + 2, c.y, c.z)));
  /* lights and restricted rooms */
  float lit = sr_site_light(s, 0);
  s->lights_off = true;
  SR_CHECK(sr_site_light(s, 0) < lit);
  s->lights_off = false;
  int lobby = s->in.entries[0].room;
  SR_CHECK(!sr_site_restricted(s, lobby, false));
  SR_CHECK(sr_site_restricted(s, s->in.target_room, true));
  SR_CHECK(!sr_site_restricted(s, -1, false));
  int office = -1;
  for (int r = 0; r < s->in.w * s->in.h; r++)
    if (s->in.rooms[r] == SR_ROOM_OFFICE || s->in.rooms[r] == SR_ROOM_HALL) office = r;
  SR_CHECK(sr_site_restricted(s, office, false));
  SR_CHECK(!sr_site_restricted(s, office, true)); /* forged ID */
  s->alarm = true;
  SR_CHECK(sr_site_restricted(s, lobby, false));
  s->alarm = false;
  SR_CHECK(sr_site_entry_of_type(s, SR_ENTRY_ROOF) >= 0);
  SR_CHECK_EQ(sr_site_entry_of_type(s, (SrEntryType)99), -1);
  /* effect slots fill up and recycle */
  for (int i = 0; i < SR_SITE_MAX_EFFECTS + 2; i++) sr_site_add_effect(s, SR_FX_JAMMER, c, 1, 1);
  SR_CHECK_EQ(sr_site_add_effect(s, SR_FX_SMOKE, c, 1, 1), -1);
  s->effects[3].active = false;
  SR_CHECK_EQ(sr_site_add_effect(s, SR_FX_SMOKE, c, 1, 1), 3);
  sr_game_destroy(g);
}

/* ---- stable interiors ------------------------------------------------------------------- */

static void test_stable_interiors(void) {
  SR_SPEC("city-generation", "Stable interiors");
  SrMemStore mem;
  SrStorage st;
  sr_memstore_init(&mem, &st);
  SrGame *g = game(26);
  sr_game_leave(g);
  int b = mission_building(g, 1);
  SrInterior fresh;
  sr_interior_generate(g->city, b, &fresh);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_FRONT), SR_OK);
  SrSite *s = &g->site;
  /* changes: a door unlocked, a camera disabled, a guard down */
  int locked = -1, cam = -1;
  for (int i = 0; i < s->in.device_count; i++) {
    if (s->in.devices[i].type == SR_DEV_DOOR && s->in.devices[i].locked) locked = i;
    if (s->in.devices[i].type == SR_DEV_CAMERA) cam = i;
  }
  sr_site_unlock(s, &g->deltas, locked);
  sr_site_console(s, &g->deltas);
  SR_CHECK(sr_site_device_disabled(s, &g->deltas, cam));
  SrGuard *gd = &s->guards[0];
  gd->e.yaw = 0;
  SrVec3 eye = sr_v3(gd->e.pos.x, gd->e.pos.y, gd->e.pos.z + 1.2f); /* behind (+z), facing -z */
  SR_CHECK(sr_site_takedown(s, &g->deltas, eye, 0, false));
  SR_CHECK_EQ(s->knockouts, 1);
  uint32_t gid = gd->id;
  sr_game_leave(g);
  /* later: the same layout plus the changes, also after a save and load */
  sr_game_skip(g, 2 * SR_DAY_S, true);
  SR_CHECK_EQ(sr_save_to(g, &st, "stable.srsv", 0), SR_OK);
  SrGame *l = sr_game_create();
  SR_CHECK_EQ(sr_load_from(l, &st, "stable.srsv"), SR_OK);
  SR_CHECK_EQ(sr_game_enter(l, b, SR_ENTRY_FRONT), SR_OK);
  SrSite *t = &l->site;
  SR_CHECK(memcmp(t->in.rooms, fresh.rooms, sizeof fresh.rooms) == 0);
  SR_CHECK(memcmp(t->in.doors, fresh.doors, sizeof fresh.doors) == 0);
  SR_CHECK(memcmp(t->in.props, fresh.props, sizeof fresh.props) == 0);
  SR_CHECK(!t->in.devices[locked].locked);
  SR_CHECK(sr_site_device_disabled(t, &l->deltas, cam));
  SR_CHECK(t->guards[0].e.ko);
  SR_CHECK_EQ(t->guards[0].id, gid);
  SR_CHECK(t->guards[0].reported);
  sr_game_destroy(l);
  sr_game_destroy(g);
}

/* ---- guards, cameras, alarms ------------------------------------------------------------- */

static int open_site(SrGame *g, int k) {
  sr_game_leave(g);
  int b = mission_building(g, k);
  sr_game_enter(g, b, SR_ENTRY_FRONT);
  return b;
}

/* Puts the player in front of guard i, in plain view, in a restricted room. */
static void stand_before(SrGame *g, int i, float dist) {
  SrGuard *gd = &g->site.guards[i];
  SrVec3 f = sr_dir_from_yaw_pitch(gd->e.yaw, 0);
  SrVec3 at = sr_v3(gd->e.pos.x + f.x * dist, SR_SITE_FLOOR_Y + 1.7f, gd->e.pos.z + f.z * dist);
  g->player.pos = at;
  g->player.yaw = gd->e.yaw + SR_PI;
}

static void cameras_off(SrGame *g) {
  for (int i = 0; i < g->site.in.device_count; i++)
    if (g->site.in.devices[i].type == SR_DEV_CAMERA || g->site.in.devices[i].type == SR_DEV_TURRET)
      sr_deltas_set(&g->deltas, g->site.in.devices[i].id, SR_DV_DISABLED);
}

static int guard_in_room_center(SrGame *g) {
  /* puts guard 0 in a restricted room 1.5 m west of its centre, facing east; others are out */
  SrSite *s = &g->site;
  int room = s->in.target_room;
  SrVec3 c = sr_site_room_center(s, room);
  s->guards[0].e.pos = sr_v3(c.x - 1.5f, c.y, c.z);
  s->guards[0].e.yaw = -SR_PI / 2;       /* facing +x */
  s->guards[0].route[0] = (uint8_t)room; /* patrols the centre of this room */
  s->guards[0].route_n = 1;
  s->guards[0].route_i = 0;
  for (int i = 1; i < s->guard_count; i++) { /* the others are elsewhere, already dealt with */
    s->guards[i].e.dead = true;
    s->guards[i].reported = true;
  }
  cameras_off(g);
  return room;
}

static void test_guard_detection_and_alarm(void) {
  SrGame *g = game(27);
  open_site(g, 2);
  SrSite *s = &g->site;
  guard_in_room_center(g);
  stand_before(g, 0, 3.0f);
  float hp = g->player.health;
  for (int i = 0; i < 600 && !s->alarm; i++) steps(g, 1);
  SR_CHECK_EQ(s->guards[0].e.alert, SR_ALERT_COMBAT);
  SR_CHECK(s->alarm);
  SR_CHECK(s->witnessed);
  SR_CHECK(g->combat_count >= 1);
  steps(g, 400);
  SR_CHECK(s->shots_fired > 0);
  SR_CHECK(g->player.health < hp);
  /* leaving after an alarm adds Heat */
  int heat = 0;
  sr_game_leave(g);
  for (int k = 0; k < SR_MAX_FACTIONS; k++) heat += g->heat.level[k];
  SR_CHECK(heat > 0);
  /* with a jammer running, the alarm cannot go out */
  open_site(g, 2);
  s = &g->site;
  guard_in_room_center(g);
  sr_site_add_effect(s, SR_FX_JAMMER, g->player.pos, 40, 1e6f);
  stand_before(g, 0, 3.0f);
  steps(g, 300);
  SR_CHECK_EQ(s->guards[0].e.alert, SR_ALERT_COMBAT);
  SR_CHECK(!s->alarm);
  SR_CHECK(s->hostile);
  sr_game_destroy(g);
}

static void test_cameras(void) {
  SrGame *g = game(28);
  open_site(g, 3);
  SrSite *s = &g->site;
  for (int i = 0; i < s->guard_count; i++) s->guards[i].e.dead = true;
  int cam = -1;
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_CAMERA && s->in.devices[i].room == s->in.target_room) cam = i;
  SR_CHECK(cam >= 0);
  SrVec3 c = sr_site_room_center(s, s->in.target_room);
  g->player.pos = sr_v3(c.x, SR_SITE_FLOOR_Y + 1.7f, c.z);
  for (int i = 0; i < 600 && !s->camera_seen; i++) steps(g, 1);
  SR_CHECK(s->camera_seen);
  SR_CHECK(s->alarm);
  /* turrets join once the alarm is up */
  int turret = -1;
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_TURRET) turret = i;
  if (turret >= 0) {
    int shots = s->shots_fired;
    steps(g, 120);
    SR_CHECK(s->shots_fired > shots);
  }
  /* the console shuts everything down */
  sr_site_console(s, &g->deltas);
  SR_CHECK(!s->alarm);
  s->camera_seen = false;
  steps(g, 300);
  SR_CHECK(!s->camera_seen);
  /* a data spike loops the cameras, an EMP fries them */
  open_site(g, 4);
  s = &g->site;
  for (int i = 0; i < s->guard_count; i++) s->guards[i].e.dead = true;
  c = sr_site_room_center(s, s->in.target_room);
  g->player.pos = sr_v3(c.x, SR_SITE_FLOOR_Y + 1.7f, c.z);
  sr_site_add_effect(s, SR_FX_LOOP, c, 30, 1e6f);
  steps(g, 300);
  SR_CHECK(!s->camera_seen);
  sr_game_destroy(g);
}

static void test_holo_decoy(void) {
  SR_SPEC("equipment", "Holo-decoy distracts");
  SrGame *g = game(29);
  open_site(g, 5);
  SrSite *s = &g->site;
  guard_in_room_center(g);
  SrGuard *gd = &s->guards[0];
  /* the player hides in another room; the decoy appears in the guard's view */
  g->player.pos = sr_site_room_center(s, s->in.entries[0].room);
  SrVec3 at = sr_v3(gd->e.pos.x + 3.5f, gd->e.pos.y, gd->e.pos.z);
  int fx = sr_site_add_effect(s, SR_FX_DECOY, at, 0.5f, 20);
  float hp = g->player.health;
  steps(g, 2);
  SR_CHECK_EQ(gd->decoy, fx);
  SR_CHECK_EQ(gd->e.alert, SR_ALERT_COMBAT); /* reacts as if it were the player */
  SR_CHECK(fabsf(gd->e.investigate.x - at.x) < 0.01f);
  float d0 = sqrtf((gd->e.pos.x - at.x) * (gd->e.pos.x - at.x) + (gd->e.pos.z - at.z) * (gd->e.pos.z - at.z));
  steps(g, 20);
  float d1 = sqrtf((gd->e.pos.x - at.x) * (gd->e.pos.x - at.x) + (gd->e.pos.z - at.z) * (gd->e.pos.z - at.z));
  SR_CHECK(d1 < d0);            /* closes in on the decoy */
  SR_CHECK(s->shots_fired > 0); /* and shoots at it */
  steps(g, 240);
  SR_CHECK(!s->effects[fx].active); /* revealed up close */
  SR_CHECK_EQ(gd->decoy, -1);
  SR_CHECK_EQ(gd->e.alert, SR_ALERT_SEARCHING);
  SR_CHECK_NEAR(g->player.health, hp, 1e-3);
  SR_CHECK(!s->alarm);
  /* deployed through the gadget */
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_GADGET, SR_GD_DECOY, 1));
  SR_CHECK(sr_game_use_gadget(g, SR_GD_DECOY));
  SR_CHECK_EQ(sr_inv_find(&g->player.inv, SR_IK_GADGET, SR_GD_DECOY), -1);
  /* decoys also expire */
  int fx2 = sr_site_add_effect(s, SR_FX_DECOY, at, 0.5f, 0.05f);
  steps(g, 10);
  SR_CHECK(!s->effects[fx2].active);
  sr_game_destroy(g);
}

static void test_takedown_and_shooting(void) {
  SrGame *g = game(30);
  open_site(g, 6);
  SrSite *s = &g->site;
  guard_in_room_center(g);
  SrGuard *gd = &s->guards[0];
  /* behind the guard: takedown */
  g->player.pos = sr_v3(gd->e.pos.x - 1.2f, SR_SITE_FLOOR_Y + 1.7f, gd->e.pos.z);
  g->player.yaw = -SR_PI / 2; /* facing +x, at the guard's back */
  SrContext c = sr_game_context(g);
  SR_CHECK_EQ(c.kind, SR_CTX_TAKEDOWN);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(gd->e.ko);
  SR_CHECK_EQ(sr_deltas_get(&g->deltas, gd->id), SR_GV_KO);
  SR_CHECK_EQ(g->player.knockouts, 1);
  SR_CHECK(sr_site_guards_up(s) < s->guard_count);
  /* nobody left to take down */
  SR_CHECK(!sr_site_takedown(s, &g->deltas, g->player.pos, g->player.yaw, true));
  /* another guard finds the body */
  if (s->guard_count > 1) {
    SrGuard *o = &s->guards[1];
    o->e.dead = false;
    o->e.ko = false;
    o->e.pos = sr_v3(gd->e.pos.x + 4.0f, gd->e.pos.y, gd->e.pos.z);
    o->e.yaw = SR_PI / 2; /* facing -x, towards the body */
    gd->reported = false;
    g->player.pos = sr_site_room_center(s, s->in.entries[0].room);
    steps(g, 2);
    SR_CHECK(gd->reported);
    SR_CHECK(o->e.heightened);
  }
  /* shooting: a lethal pistol drops a guard */
  open_site(g, 7);
  s = &g->site;
  guard_in_room_center(g);
  gd = &s->guards[0];
  gd->e.health = 1;
  SrPlayer *p = &g->player;
  sr_inv_add(&p->inv, sr_item_weapon(0, SR_STREET_MAKER_FIRST, 0, 1));
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_AMMO, SR_AMMO_STANDARD, 10));
  p->equipped = (int8_t)sr_inv_find(&p->inv, SR_IK_WEAPON, 0);
  p->pos = sr_v3(gd->e.pos.x - 1.4f, SR_SITE_FLOOR_Y + 1.7f, gd->e.pos.z);
  p->yaw = -SR_PI / 2;
  p->pitch = -0.3f;
  sr_game_fire(g);
  SR_CHECK(gd->e.dead || gd->e.ko);
  SR_CHECK(s->kills + s->knockouts == 1);
  /* a miss into an empty room only makes noise */
  p->yaw = SR_PI / 2;
  sr_game_fire(g);
  /* a hit that does not drop alerts the guard */
  if (s->guard_count > 1) {
    SrGuard *o = &s->guards[1];
    o->e.dead = false;
    o->e.health = 1000;
    o->e.pos = sr_v3(p->pos.x + 3.0f, o->e.pos.y, p->pos.z);
    SrWeaponStats w = sr_weapon_stats(g->seed, &p->inv.items[p->equipped]);
    SR_CHECK(sr_site_shoot(s, &g->deltas, p->pos, -SR_PI / 2, atan2f(-0.5f, 3.0f), &w));
    SR_CHECK(s->hostile);
  }
  sr_game_destroy(g);
}

static void test_site_gadgets(void) {
  SR_SPEC("equipment", "Holo-decoy distracts");
  SrGame *g = game(31);
  open_site(g, 8);
  SrSite *s = &g->site;
  SrSiteCtx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.deltas = &g->deltas;
  guard_in_room_center(g);
  SrGuard *gd = &s->guards[0];
  SrVec3 eye = sr_v3(gd->e.pos.x - 1.3f, gd->e.pos.y, gd->e.pos.z);
  float yaw = -SR_PI / 2;
  /* flashbang stuns, sleep gas knocks out */
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_FLASH, eye, yaw));
  SR_CHECK(gd->stun > 0);
  SrVec3 hit = sr_site_throw_point(s, eye, yaw, 12.0f);
  SR_CHECK(hit.y < 0);
  gd->e.pos = sr_v3(hit.x, gd->e.pos.y, hit.z);
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_SLEEPGAS, eye, yaw));
  SR_CHECK(gd->e.ko);
  gd->e.ko = false;
  /* frag grenade kills at the blast point */
  gd->e.health = 50;
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_FRAG, eye, yaw));
  SR_CHECK(gd->e.dead);
  SR_CHECK(s->hostile);
  /* close to the blast hurts the player too */
  ctx.player_damage = 0;
  SrVec3 wall_eye = sr_site_room_center(s, s->in.target_room);
  sr_site_gadget(s, &ctx, SR_GD_FRAG, wall_eye, 0);
  sr_site_gadget(s, &ctx, SR_GD_SLEEPGAS, wall_eye, 0);
  /* EMP disables cameras and lights in range */
  SrVec3 cc = sr_site_room_center(s, s->in.target_room);
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_EMP, sr_v3(cc.x, cc.y, cc.z + 2.5f), 0));
  int cams_off = 0;
  for (int i = 0; i < s->in.device_count; i++)
    cams_off += s->in.devices[i].type == SR_DEV_CAMERA && sr_site_device_disabled(s, &g->deltas, i);
  SR_CHECK(cams_off > 0);
  /* smoke, lure, jammer, tripmine, turret, data spike */
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_SMOKE, eye, yaw));
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_LURE, eye, yaw));
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_JAMMER, eye, yaw));
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_DATASPIKE, eye, yaw));
  if (s->guard_count > 1) {
    SrGuard *o = &s->guards[1];
    o->e.dead = false;
    SrVec3 mine = sr_site_throw_point(s, eye, yaw, 1.5f);
    SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_TRIPMINE, eye, yaw));
    o->e.pos = sr_v3(mine.x + 0.5f, o->e.pos.y, mine.z);
    sr_site_update(s, &ctx, 1.0f / 60.0f);
    SR_CHECK(o->e.dead);
    o->e.dead = false;
    o->e.health = 10;
    o->e.alert = SR_ALERT_COMBAT;
    o->e.awareness = 2;
    s->effect_count = 0;
    SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_TURRET, eye, yaw));
    o->e.pos = sr_v3(eye.x + 3.0f, o->e.pos.y, eye.z);
    ctx.player = sr_v3(1e5f, 0, 1e5f);
    for (int i = 0; i < 30; i++) sr_site_update(s, &ctx, 1.0f / 60.0f);
    SR_CHECK(o->e.dead);
  }
  /* recon gadgets tag guards */
  s->guards[0].tagged = false;
  SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_DRONE, s->guards[0].e.pos, 0));
  SR_CHECK(s->guards[0].tagged);
  SR_CHECK(!sr_site_gadget(s, &ctx, SR_GD_STICKYCAM, sr_v3(1e4f, 0, 1e4f), 0));
  /* door tools */
  int locked = -1;
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_DOOR && s->in.devices[i].locked) locked = i;
  SR_CHECK(!sr_site_gadget(s, &ctx, SR_GD_MULTITOOL, sr_v3(1e4f, 0, 1e4f), yaw)); /* no door ahead */
  if (locked >= 0) {
    const SrDevice *d = &s->in.devices[locked];
    SrVec3 rc = sr_site_room_center(s, d->room);
    SrVec3 at = sr_v3(rc.x + (d->door_dir == 0 ? 2.0f : 0), rc.y, rc.z + (d->door_dir == 1 ? 2.0f : 0));
    float y = d->door_dir == 0 ? -SR_PI / 2 : SR_PI;
    SR_CHECK_EQ(sr_site_locked_door_ahead(s, at, y, 2.5f), locked);
    SR_CHECK(sr_site_gadget(s, &ctx, SR_GD_BREACH, at, y));
    SR_CHECK(!s->in.devices[locked].locked);
  }
  SR_CHECK(!sr_site_gadget(s, &ctx, SR_GD_MEDKIT, eye, yaw)); /* not a site gadget */
  sr_game_destroy(g);
}

/* ---- missions played in 3D ------------------------------------------------------------- */

static void test_site_mission(void) {
  SrGame *g = game(32);
  sr_game_leave(g);
  int b = mission_building(g, 9);
  SrJob *j = add_site_job(g, b);
  int32_t id = j->id;
  sr_game_teleport_building(g, b);
  SrContext c = sr_game_context(g);
  SR_CHECK_EQ(c.kind, SR_CTX_JOB_TARGET);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(sr_game_inside(g));
  SR_CHECK_EQ(g->site.job, id);
  for (int i = 0; i < g->site.guard_count; i++) g->site.guards[i].e.ko = true; /* a quiet night */
  face_prop(g, SR_PROP_OBJECTIVE);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_OBJECTIVE);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  j = sr_jobs_get(&g->jobs, id);
  SR_CHECK(j->executed);
  SR_CHECK(g->site.objective_done);
  SR_CHECK(sr_inv_find(&g->player.inv, SR_IK_PAYDATA, -1) >= 0);
  SR_CHECK(sr_game_context(g).kind != SR_CTX_OBJECTIVE); /* done */
  SrMissionResult r = sr_site_result(&g->site, sr_game_time(g));
  SR_CHECK(r.success && !r.alarm);
  /* security console needs skill or a data jack */
  face_prop(g, SR_PROP_CONSOLE);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_CONSOLE);
  g->player.prog.skill_rank[SR_SK_ELECTRONICS] = 0;
  SR_CHECK_EQ(sr_game_interact(g), SR_ERR_DENIED);
  g->player.prog.skill_rank[SR_SK_ELECTRONICS] = 3;
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  /* out through the front, report back */
  face_prop(g, SR_PROP_EXIT);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(!sr_game_inside(g));
  SR_CHECK(sr_game_report(g, id) > 0);
  /* the objective stays done on a later visit */
  SrJob *j2 = add_site_job(g, b);
  sr_deltas_set(&g->deltas, sr_site_objective_id(j2->id), 1);
  SR_CHECK_EQ(sr_game_enter(g, b, SR_ENTRY_SERVICE), SR_OK);
  SR_CHECK(g->site.objective_done);
  /* locked doors: multitool, lockpick fingers */
  SrSite *s = &g->site;
  int locked = -1;
  for (int i = 0; i < s->in.device_count; i++)
    if (s->in.devices[i].type == SR_DEV_DOOR && s->in.devices[i].locked) locked = i;
  if (locked >= 0) {
    const SrDevice *d = &s->in.devices[locked];
    SrVec3 rc = sr_site_room_center(s, d->room);
    g->player.pos = sr_v3(rc.x + (d->door_dir == 0 ? 2.0f : 0), rc.y + 0.1f, rc.z + (d->door_dir == 1 ? 2.0f : 0));
    g->player.yaw = d->door_dir == 0 ? -SR_PI / 2 : SR_PI;
    for (int i = 0; i < s->guard_count; i++) s->guards[i].e.ko = true;
    c = sr_game_context(g);
    SR_CHECK_EQ(c.kind, SR_CTX_LOCKED);
    SR_CHECK_EQ(sr_game_interact(g), SR_ERR_DENIED);
    sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_GADGET, SR_GD_MULTITOOL, 1));
    SR_CHECK_EQ(sr_game_interact(g), SR_OK);
    SR_CHECK_EQ(sr_inv_find(&g->player.inv, SR_IK_GADGET, SR_GD_MULTITOOL), -1);
  }
  sr_game_destroy(g);
}

/* ---- rooftops and the grapple ---------------------------------------------------------- */

static void test_grapple_line(void) {
  SR_SPEC("equipment", "Grapple line to a rooftop");
  SrGame *g = game(33);
  sr_game_leave(g);
  SrPlayer *p = &g->player;
  /* stand in front of a low building and aim at it */
  int target = -1;
  for (int b = 0; b < g->city->building_count && target < 0; b++) {
    const SrBuilding *d = &g->city->buildings[b];
    if (d->height_cm < 2500 && (d->flags & SR_BF_MISSION)) target = b;
  }
  SR_CHECK(target >= 0);
  sr_game_teleport_building(g, target);            /* facing the building */
  SR_CHECK(!sr_game_use_gadget(g, SR_GD_GRAPPLE)); /* none carried */
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_GRAPPLE, 1));
  SR_CHECK(sr_game_use_gadget(g, SR_GD_GRAPPLE));
  SR_CHECK_EQ(g->roof_building, target);
  const SrBuilding *d = &g->city->buildings[target];
  SR_CHECK_NEAR(p->pos.y, (float)d->height_cm / 100.0f + 1.7f, 1e-3);
  SR_CHECK(p->pos.x >= (float)d->x0 / 100.0f && p->pos.x <= (float)d->x1 / 100.0f);
  SR_CHECK(sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_GRAPPLE) >= 0); /* a reusable tool */
  /* the roof edge holds you in */
  SR_CHECK(sr_game_blocked_here(g, (float)d->x0 / 100.0f - 1.0f, p->pos.z));
  SR_CHECK(!sr_game_blocked_here(g, (float)(d->x0 + d->x1) / 200.0f, (float)(d->y0 + d->y1) / 200.0f));
  /* the roof hatch lets you drop in */
  p->pos.x = (float)(d->x0 + d->x1) / 200.0f;
  p->pos.z = (float)(d->y0 + d->y1) / 200.0f;
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_ROOF_HATCH);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK(sr_game_inside(g));
  SR_CHECK_EQ(g->site.in.entries[g->site.entry].type, SR_ENTRY_ROOF);
  SR_CHECK_EQ(sr_game_grapple(g), SR_ERR_DENIED); /* not indoors */
  sr_game_leave(g);                               /* back out on the roof */
  SR_CHECK_EQ(g->roof_building, target);
  /* away from the hatch: the fire escape */
  p->pos.x = (float)d->x0 / 100.0f + 0.6f;
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_CLIMB_DOWN);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(g->roof_building, -1);
  SR_CHECK(p->pos.y < 2.0f);
  SR_CHECK_EQ(sr_game_climb_down(g), SR_ERR_INVALID);
  /* nothing in range */
  p->yaw += SR_PI;
  p->pos = sr_v3(p->pos.x, 1.7f, p->pos.z + 0.0f);
  SrResult r = sr_game_grapple(g);
  SR_CHECK(r == SR_OK || r == SR_ERR_NOT_FOUND);
  p->pos = sr_v3(-500, 1.7f, -500); /* outside the city */
  SR_CHECK_EQ(sr_game_grapple(g), SR_ERR_NOT_FOUND);
  sr_game_destroy(g);
}

/* ---- cyberware in play ------------------------------------------------------------------ */

static int install(SrGame *g, int type) {
  sr_inv_add(&g->player.inv, sr_item_simple(SR_IK_CYBER, type, 1));
  return sr_game_install_cyber(g, sr_inv_find(&g->player.inv, SR_IK_CYBER, type));
}

static void test_cyberware_in_play(void) {
  SR_SPEC("equipment", "Cyberware ability works");
  SR_SPEC("equipment", "Over capacity causes glitches");
  SrGame *g = game(34);
  SrPlayer *p = &g->player;
  SR_CHECK(!sr_game_ability(g)); /* nothing installed */
  SR_CHECK(install(g, 2));       /* thermal optics */
  SR_CHECK(!install(g, 3));      /* same slot: optics taken */
  SR_CHECK(!sr_game_install_cyber(g, -1));
  SR_CHECK(sr_game_ability(g));
  SR_CHECK(g->thermal);
  /* thermal: warm bodies are drawn through smoke and walls */
  open_site(g, 10);
  sr_site_add_effect(&g->site, SR_FX_SMOKE, g->player.pos, 3, 100);
  SrSceneCache sc;
  sr_scene_init(&sc, false);
  SrFrame *f = (SrFrame *)malloc(sizeof(SrFrame));
  sr_scene_build(&sc, g, 16.0f / 9.0f, f);
  bool xray = false;
  for (int i = 0; i < f->ndraws; i++)
    if (f->draws[i].pass == SR_PASS_XRAY && f->draws[i].mesh->nv > 0) {
      xray = true;
      SR_CHECK_EQ(f->draws[i].mesh->v[0].emissive, 255);
    }
  SR_CHECK(xray);
  SR_CHECK(sr_game_ability(g)); /* toggles off */
  SR_CHECK(!g->thermal);
  sr_scene_build(&sc, g, 16.0f / 9.0f, f);
  for (int i = 0; i < f->ndraws; i++) SR_CHECK(f->draws[i].pass != SR_PASS_XRAY);
  /* on the street, citizens glow too */
  sr_game_leave(g);
  g->crowd_enabled = true;
  steps(g, 2);
  g->thermal = true;
  sr_scene_build(&sc, g, 16.0f / 9.0f, f);
  free(f);
  sr_scene_free(&sc);
  /* reflex booster and chameleon skin: timed abilities with a cooldown */
  p->prog.attr[SR_ATTR_BODY] = 10;
  p->prog.attr[SR_ATTR_TECH] = 10;
  SR_CHECK(install(g, 0));
  SR_CHECK(install(g, 11));
  g->thermal = false;
  SR_CHECK(sr_game_ability(g));
  SR_CHECK(g->buff_reflex > 0 && g->buff_camo > 0);
  SR_CHECK(g->ability_cd > 0);
  float cd = g->ability_cd;
  sr_game_ability(g);
  SR_CHECK_NEAR(g->ability_cd, cd, 1e-4); /* still cooling down */
  /* armour, second heart */
  p->cyber_count = 2; /* thermal optics, reflex booster */
  SR_CHECK(install(g, 10));
  SR_CHECK(p->armor > 0);
  SR_CHECK(install(g, 13));
  SR_CHECK(p->max_health > 100);
  p->health = 0;
  sr_game_flatline(g);
  SR_CHECK(p->health > 0); /* the second heart */
  SR_CHECK(sr_game_cyber_load(g) <= sr_game_neural_capacity(g));
  /* glitches at the configured rate while over capacity, none within it */
  int glitches = g->glitches;
  steps(g, 60 * 60 * 5);
  SR_CHECK_EQ(g->glitches, glitches);
  p->prog.attr[SR_ATTR_BODY] = 1;
  p->prog.attr[SR_ATTR_TECH] = 1;
  SR_CHECK(sr_game_cyber_load(g) > sr_game_neural_capacity(g));
  int minutes = 30;
  steps(g, 60 * 60 * minutes);
  int n = g->glitches - glitches;
  printf("  glitches over %d minutes: %d (rate %.1f/min)\n", minutes, n, (double)SR_GLITCH_PER_MINUTE);
  SR_CHECK(n >= (int)(minutes * SR_GLITCH_PER_MINUTE * 0.5f) && n <= (int)(minutes * SR_GLITCH_PER_MINUTE * 1.6f));
  /* a glitch freezes the body and blocks abilities */
  g->glitch_time = 1.0f;
  SR_CHECK(!sr_game_ability(g));
  SrVec3 before = p->pos;
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.move_y = 1;
  g->glitch_time = 1.0f;
  sr_game_step(g, &a);
  SR_CHECK_NEAR(p->pos.x, before.x, 1e-5);
  SR_CHECK_NEAR(p->pos.z, before.z, 1e-5);
  /* street-grade chrome malfunctions some days */
  SrGame *h = game(35);
  for (int t = 0; t < 8; t++) {
    h->player.cyber[t] = sr_item_simple(SR_IK_CYBER, t * 2, 1);
    h->player.cyber[t].seed = (uint32_t)(t * 7919);
  }
  h->player.cyber_count = 8;
  int offline_days = 0;
  for (int d = 0; d < 60; d++) {
    sr_game_skip(h, SR_DAY_S, false);
    steps(h, 1);
    offline_days += h->cyber_offline != 0;
  }
  SR_CHECK(offline_days > 0);
  sr_game_destroy(h);
  sr_game_destroy(g);
}

/* ---- gadgets on the street and quick slots ---------------------------------------------- */

static void test_street_gadgets(void) {
  SrGame *g = game(36);
  sr_game_leave(g);
  SrPlayer *p = &g->player;
  SR_CHECK(!sr_game_use_gadget(g, SR_GD_FRAG)); /* none carried */
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_FRAG, 2));
  g->crowd_enabled = true;
  steps(g, 2);
  SR_CHECK(sr_game_use_gadget(g, SR_GD_FRAG));
  SR_CHECK_EQ(sr_inv_count(&p->inv, SR_IK_GADGET, SR_GD_FRAG), 1);
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_DECOY, 1));
  SR_CHECK(!sr_game_use_gadget(g, SR_GD_DECOY)); /* nobody to fool out here */
  SR_CHECK_EQ(sr_inv_count(&p->inv, SR_IK_GADGET, SR_GD_DECOY), 1);
  static const int consumables[] = {SR_GD_TRAUMA,    SR_GD_PAINKILLER, SR_GD_REFLEXSTIM,
                                    SR_GD_FOCUSSTIM, SR_GD_CAMO,       SR_GD_HOLOMASK};
  for (int i = 0; i < 6; i++) {
    sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, consumables[i], 1));
    SR_CHECK(sr_game_use_gadget(g, consumables[i]));
  }
  SR_CHECK(g->buff_pain > 0 && g->buff_reflex > 0 && g->buff_focus > 0 && g->buff_camo > 0 && g->buff_mask > 0);
  SR_CHECK(sr_inv_find(&p->inv, SR_IK_GADGET, SR_GD_CAMO) >= 0); /* optical camo recharges */
  sr_inv_add(&p->inv, sr_item_simple(SR_IK_GADGET, SR_GD_FAKEID, 1));
  SR_CHECK(!sr_game_use_gadget(g, SR_GD_FAKEID));
  /* the gadget key triggers active cyberware */
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_GADGET] = true;
  sr_game_step(g, &a);
  /* buffs wear off */
  steps(g, 60 * 610);
  SR_CHECK(g->buff_pain <= 0 && g->buff_reflex <= 0 && g->buff_mask <= 0);
  sr_game_destroy(g);
}

/* ---- shops and ripperdocs ---------------------------------------------------------------- */

static int open_poi(SrGame *g, int poi) {
  int b = -1;
  for (int i = 0; i < g->city->building_count && b < 0; i++)
    if (g->city->buildings[i].poi == poi) b = i;
  int open, hours;
  sr_poi_hours(g->city, b, &open, &hours);
  sr_game_skip(g, ((open + 1 + 24) - sr_time_hour(sr_game_time(g))) % 24 * 3600, false);
  sr_game_teleport_building(g, b);
  return b;
}

static void test_shops(void) {
  SR_SPEC("equipment", "Jailbreak removes the backdoor");
  SrGame *g = game(37);
  SrPlayer *p = &g->player;
  p->cred = 100000;
  int b = open_poi(g, SR_POI_ELECTRONICS);
  SR_CHECK_EQ(sr_game_context(g).kind, SR_CTX_SHOP);
  SR_CHECK_EQ(sr_game_interact(g), SR_OK);
  SR_CHECK_EQ(g->screen, SR_SCREEN_SHOP);
  SR_CHECK_EQ(g->menu.building, b);
  SR_CHECK(g->menu.count > 0);
  /* navigate with the move axis, buy with interact */
  SrActionState a;
  memset(&a, 0, sizeof a);
  a.move_y = -1;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->menu.sel, 1 % g->menu.count);
  sr_game_step(g, &a); /* held: no repeat */
  SR_CHECK_EQ(g->menu.sel, 1 % g->menu.count);
  a.move_y = 1;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->menu.sel, 0);
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_INTERACT] = true;
  int64_t cash = p->cred;
  sr_game_step(g, &a);
  SR_CHECK(p->cred < cash);
  /* a bought gadget lands on a free quick slot */
  bool bound = false;
  for (int q = 0; q < 4; q++) bound = bound || p->quick[q] == g->menu.items[0].type;
  SR_CHECK(bound);
  SR_CHECK_EQ(sr_game_shop_buy(g, 99), SR_ERR_INVALID);
  p->cred = 0;
  SR_CHECK_EQ(sr_game_shop_buy(g, 0), SR_ERR_DENIED);
  p->cred = 100000;
  for (int i = 0; i < g->menu.count; i++) sr_game_shop_buy(g, i);
  memset(&a, 0, sizeof a);
  a.pressed[SR_ACT_PAUSE] = true;
  sr_game_step(g, &a);
  SR_CHECK_EQ(g->screen, SR_SCREEN_PLAY);
  /* gun shop: the first weapon is equipped */
  p->equipped = -1;
  p->inv.count = 0;
  sr_game_shop_open(g, open_poi(g, SR_POI_GUNSHOP));
  SR_CHECK_EQ(sr_game_shop_buy(g, 0), SR_OK);
  SR_CHECK(p->equipped >= 0);
  sr_game_shop_buy(g, g->menu.count - 1); /* a mod */
  sr_game_shop_close(g);
  /* ripperdoc: cyberware is installed; corporate backdoors can be jailbroken */
  int rd = open_poi(g, SR_POI_RIPPERDOC);
  p->cyber_count = 0;
  SrItem corp = sr_item_simple(SR_IK_CYBER, 4, 1);
  corp.flags |= SR_IF_BACKDOOR;
  p->cyber[p->cyber_count++] = corp;
  sr_game_shop_open(g, rd);
  SR_CHECK(g->menu.ripperdoc);
  int jb = -1, cy = -1;
  for (int i = 0; i < g->menu.count; i++) {
    if (g->menu.jailbreak[i] >= 0)
      jb = i;
    else if (g->menu.items[i].kind == SR_IK_CYBER && cy < 0)
      cy = i;
  }
  SR_CHECK(jb >= 0);
  p->cred = 10;
  SR_CHECK_EQ(sr_game_shop_buy(g, jb), SR_ERR_DENIED);
  p->cred = 100000;
  SR_CHECK_EQ(sr_game_shop_buy(g, jb), SR_OK);
  SR_CHECK(!(p->cyber[0].flags & SR_IF_BACKDOOR));
  for (int i = 0; i < g->menu.count; i++) SR_CHECK(g->menu.jailbreak[i] < 0);
  if (cy >= 0) {
    int before = p->cyber_count;
    sr_game_shop_buy(g, cy);
    sr_game_shop_buy(g, cy); /* second of the same slot stays in the bag */
    SR_CHECK(p->cyber_count >= before);
  }
  sr_game_shop_close(g);
  sr_game_shop_close(g);
  sr_game_destroy(g);
}

/* ---- rendering and UI ------------------------------------------------------------------- */

static void test_interior_render_and_ui(void) {
  SR_SPEC("input-controls", "Context icon on interact");
  SrGame *g = game(38);
  SrMesh m;
  sr_mesh_init(&m);
  for (int r = 0; r < g->site.in.w * g->site.in.h; r++) {
    sr_build_interior_room(&g->site, r, &m);
    SR_CHECK(m.ni > 0);
  }
  uint64_t key = sr_site_mesh_key(&g->site, &g->deltas);
  open_site(g, 11);
  SR_CHECK(sr_site_mesh_key(&g->site, &g->deltas) != key);
  key = sr_site_mesh_key(&g->site, &g->deltas);
  sr_site_console(&g->site, &g->deltas);
  SR_CHECK(sr_site_mesh_key(&g->site, &g->deltas) != key);
  for (int r = 0; r < g->site.in.w * g->site.in.h; r++) sr_build_interior_room(&g->site, r, &m);
  sr_site_add_effect(&g->site, SR_FX_SMOKE, g->player.pos, 3, 10);
  sr_site_add_effect(&g->site, SR_FX_DECOY, g->player.pos, 1, 10);
  sr_site_add_effect(&g->site, SR_FX_TURRET, g->player.pos, 1, 10);
  sr_build_site_dynamic(&g->site, &g->deltas, 1.0f, &m);
  SR_CHECK(m.ni > 0);
  g->site.guards[0].e.dead = true;
  g->site.guards[1 % g->site.guard_count].e.alert = SR_ALERT_COMBAT;
  sr_build_guards(&g->site, &m);
  SR_CHECK(m.ni > 0);
  SrLight lights[64];
  SR_CHECK(sr_site_collect_lights(&g->site, lights, 64) > 0);
  g->site.lights_off = true;
  sr_build_interior_room(&g->site, 0, &m);
  /* walk cycle: legs swing with distance walked */
  SR_CHECK(fabsf(sr_walk_swing(0.3f)) > 0.1f);
  SR_CHECK_NEAR(sr_walk_swing(0), 0, 1e-6);
  SrMesh a1, a2;
  sr_mesh_init(&a1);
  sr_mesh_init(&a2);
  SrAppearance look = sr_appearance_generate(5, SR_ROLE_WORKER, 0);
  sr_build_character_walk(&look, sr_v3(0, 0, 0), sr_v3(1, 0, 0), 0.0f, false, &a1);
  sr_build_character_walk(&look, sr_v3(0, 0, 0), sr_v3(1, 0, 0), 0.6f, false, &a2);
  SR_CHECK(sr_mesh_digest(&a1) != sr_mesh_digest(&a2));
  sr_mesh_free(&a1);
  sr_mesh_free(&a2);
  sr_mesh_free(&m);
  /* the interact button shows the icon of the available interaction */
  SrInput in;
  sr_input_init(&in, true);
  in.scheme = SR_SCHEME_TOUCH;
  sr_input_set_screen(&in, 1280, 720, 0, 0, 0, 0);
  SrUi ui;
  sr_ui_init(&ui);
  int food = -1;
  for (int b = 0; b < g->city->building_count && food < 0; b++)
    if (g->city->buildings[b].poi == SR_POI_FOOD) food = b;
  g->ctx = (SrContext){SR_CTX_FOOD, food, -1, -1, 1, -1};
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  int icons = 0;
  for (int v = 0; v < ui.mesh.nv; v++) icons += ui.mesh.v[v].layer == SR_LAYER_ICON_FIRST + SR_POI_FOOD;
  SR_CHECK(icons >= 8); /* the prompt and the interact button */
  SR_CHECK_EQ(sr_ui_context_icon(g, &g->ctx), SR_POI_FOOD);
  for (int k = SR_CTX_FOOD; k < SR_CTX_COUNT; k++) {
    SrContext c = {(SrContextKind)k, -1, -1, -1, 1, -1};
    SR_CHECK(sr_ui_context_icon(g, &c) >= 0);
    SR_CHECK(sr_ui_context_label((SrContextKind)k)[0] != '\0');
  }
  SrContext none = {SR_CTX_NONE, -1, -1, -1, 1, -1};
  SR_CHECK_EQ(sr_ui_context_icon(g, &none), -1);
  /* HUD states inside a site */
  g->thermal = true;
  g->buff_camo = 1;
  g->buff_reflex = 1;
  g->glitch_time = 1;
  g->site.alarm = true;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  /* shop screen and row hit-testing */
  g->player.cred = 100000;
  int shop = open_poi(g, SR_POI_CLINIC);
  sr_game_shop_open(g, shop);
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  SR_CHECK(ui.mesh.ni > 0);
  SR_CHECK_EQ(sr_ui_shop_row_at(&ui, 0), -1);
  SR_CHECK_EQ(sr_ui_shop_row_at(&ui, 10.0f * 2 + 6.0f * 2 + 11.0f * 2 * 1.5f + 1), 0);
  in.scheme = SR_SCHEME_KBM;
  sr_ui_begin(&ui, 1280, 720, 2);
  sr_ui_build(&ui, g, &in, false);
  sr_ui_free(&ui);
  sr_game_destroy(g);
}

/* ---- scripts ------------------------------------------------------------------------------ */

static void test_site_script_commands(void) {
  SrGame *g = game(39);
  char out[128];
  SR_CHECK(sr_game_query(g, "site.active", out, sizeof out) && strcmp(out, "true") == 0);
  SR_CHECK(sr_game_query(g, "site.apartment", out, sizeof out) && strcmp(out, "true") == 0);
  SR_CHECK_EQ(sr_game_command(g, "face bed", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "face nothing", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "face console", out, sizeof out), SR_ERR_NOT_FOUND);
  SR_CHECK_EQ(sr_game_command(g, "leave", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "leave", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "enter home", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "enter home sideways", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "give gadget smoke_grenade 2", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "give gadget nothing", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "give cyber thermal_optics", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "install reflex_booster", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "install nothing", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "ability", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "use nothing", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "give gadget medkit", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "use medkit", out, sizeof out), SR_OK);
  SR_CHECK_EQ(sr_game_command(g, "use decoy", out, sizeof out), SR_ERR_INVALID);
  SR_CHECK_EQ(sr_game_command(g, "use holo-decoy", out, sizeof out), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_command(g, "grapple", out, sizeof out), SR_ERR_DENIED);
  SR_CHECK_EQ(sr_game_command(g, "enter target", out, sizeof out), SR_ERR_DENIED);
  const char *qs[] = {"site.building",  "site.guards",   "site.guards_up",    "site.alarm",
                      "site.objective", "site.camera",   "site.digest",       "player.roof",
                      "player.outfit",  "player.stash",  "cyber.load",        "cyber.capacity",
                      "cyber.glitches", "cyber.thermal", "approach.building", "job.target"};
  for (int i = 0; i < (int)(sizeof qs / sizeof qs[0]); i++) SR_CHECK(sr_game_query(g, qs[i], out, sizeof out));
  g->screen = SR_SCREEN_SHOP;
  SR_CHECK(sr_game_query(g, "screen", out, sizeof out) && strcmp(out, "shop") == 0);
  sr_game_destroy(g);
}

void suite_site(void) {
  SR_RUN(test_apartment_template);
  SR_RUN(test_mission_interior_props);
  SR_RUN(test_start_inside_apartment);
  SR_RUN(test_amenities);
  SR_RUN(test_moving_home);
  SR_RUN(test_site_geometry);
  SR_RUN(test_stable_interiors);
  SR_RUN(test_guard_detection_and_alarm);
  SR_RUN(test_cameras);
  SR_RUN(test_holo_decoy);
  SR_RUN(test_takedown_and_shooting);
  SR_RUN(test_site_gadgets);
  SR_RUN(test_site_mission);
  SR_RUN(test_grapple_line);
  SR_RUN(test_cyberware_in_play);
  SR_RUN(test_street_gadgets);
  SR_RUN(test_shops);
  SR_RUN(test_interior_render_and_ui);
  SR_RUN(test_site_script_commands);
}
