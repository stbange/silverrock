#include "game/sr_save.h"

#include "core/sr_log.h"
#include "core/sr_str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t k_magic[4] = {'S', 'R', 'S', 'V'};

/* ---- field writers ------------------------------------------------------------------- */

static void put_item(SrWriter *w, const SrItem *it) {
  sr_put_u8(w, it->kind);
  sr_put_u8(w, it->type);
  sr_put_u8(w, (uint8_t)it->maker);
  sr_put_u8(w, it->rarity);
  sr_put_u8(w, it->grade);
  sr_put_u8(w, it->ammo);
  sr_put_u8(w, it->flags);
  sr_put_bytes(w, it->mods, SR_SLOT_COUNT);
  sr_put_u16(w, it->count);
  sr_put_u16(w, (uint16_t)it->job);
  sr_put_u32(w, it->seed);
}

static void get_item(SrReader *r, SrItem *it) {
  memset(it, 0, sizeof *it);
  it->kind = sr_get_u8(r);
  it->type = sr_get_u8(r);
  it->maker = (int8_t)sr_get_u8(r);
  it->rarity = sr_get_u8(r);
  it->grade = sr_get_u8(r);
  it->ammo = sr_get_u8(r);
  it->flags = sr_get_u8(r);
  sr_get_bytes(r, it->mods, SR_SLOT_COUNT);
  it->count = sr_get_u16(r);
  it->job = (int16_t)sr_get_u16(r);
  it->seed = sr_get_u32(r);
}

static void put_inv(SrWriter *w, const SrInventory *inv) {
  sr_put_u16(w, (uint16_t)inv->count);
  for (int i = 0; i < inv->count; i++) put_item(w, &inv->items[i]);
}

static bool get_inv(SrReader *r, SrInventory *inv) {
  sr_inv_init(inv);
  int n = sr_get_u16(r);
  if (n > SR_INV_MAX) return false;
  for (int i = 0; i < n; i++) get_item(r, &inv->items[i]);
  inv->count = n;
  return !r->failed;
}

static void put_player(SrWriter *w, const SrPlayer *p, uint32_t version) {
  sr_put_str(w, p->name);
  sr_put_str(w, p->pronouns);
  sr_put_bytes(w, &p->look, sizeof p->look); /* all uint8 fields */
  sr_put_u8(w, p->background);
  sr_put_i64(w, p->cred);
  sr_put_f32(w, p->health);
  sr_put_f32(w, p->max_health);
  sr_put_f32(w, p->satiety);
  sr_put_f32(w, p->fatigue);
  sr_put_f32(w, p->starving_hours);
  sr_put_i32(w, p->home);
  sr_put_u8(w, p->tier);
  sr_put_i32(w, p->rent);
  sr_put_i64(w, p->rent_due);
  sr_put_i64(w, p->rent_debt);
  sr_put_u8(w, p->missed);
  sr_put_u8(w, p->evicted);
  sr_put_u8(w, p->reminders);
  sr_put_f32(w, p->pos.x);
  sr_put_f32(w, p->pos.y);
  sr_put_f32(w, p->pos.z);
  sr_put_f32(w, p->yaw);
  sr_put_f32(w, p->pitch);
  put_inv(w, &p->stash);
  put_inv(w, &p->inv);
  sr_put_u8(w, (uint8_t)p->msg_count);
  for (int i = 0; i < p->msg_count; i++) {
    sr_put_str(w, p->msgs[i].from);
    sr_put_str(w, p->msgs[i].text);
    sr_put_i64(w, p->msgs[i].time);
    sr_put_u8(w, p->msgs[i].read);
  }
  if (version < 2) return; /* version 1 (M2) ends here */
  sr_put_f32(w, p->armor);
  sr_put_u8(w, p->medevac);
  sr_put_bytes(w, p->prog.attr, SR_ATTR_COUNT);
  sr_put_bytes(w, p->base_attr, SR_ATTR_COUNT);
  sr_put_bytes(w, p->prog.skill_rank, SR_SK_COUNT);
  for (int s = 0; s < SR_SK_COUNT; s++) sr_put_i32(w, p->prog.skill_xp[s]);
  sr_put_i32(w, p->prog.level);
  sr_put_i32(w, p->prog.xp);
  sr_put_i32(w, p->prog.perk_points);
  sr_put_i32(w, p->prog.attr_points);
  sr_put_bytes(w, p->prog.perks, SR_SK_COUNT);
  sr_put_i32(w, p->prog.street_cred);
  sr_put_bytes(w, p->prog.standing, SR_MAX_FACTIONS);
  sr_put_u8(w, (uint8_t)p->cyber_count);
  for (int i = 0; i < p->cyber_count; i++) put_item(w, &p->cyber[i]);
  sr_put_u32(w, p->blueprints);
  sr_put_u8(w, (uint8_t)p->equipped);
  sr_put_bytes(w, p->quick, 4);
  sr_put_u8(w, p->outfit);
  sr_put_i32(w, p->kills);
  sr_put_i32(w, p->knockouts);
}

static bool get_player(SrReader *r, SrPlayer *p, uint32_t version, const SrCity *c) {
  /* migration defaults for fields that version 1 did not have */
  SrPlayer fresh;
  sr_player_create(&fresh, c, "", SR_BG_STREET_KID, 0);
  *p = fresh;
  p->msg_count = 0;
  sr_get_str(r, p->name, sizeof p->name);
  sr_get_str(r, p->pronouns, sizeof p->pronouns);
  sr_get_bytes(r, &p->look, sizeof p->look);
  p->background = sr_get_u8(r);
  p->cred = sr_get_i64(r);
  p->health = sr_get_f32(r);
  p->max_health = sr_get_f32(r);
  p->satiety = sr_get_f32(r);
  p->fatigue = sr_get_f32(r);
  p->starving_hours = sr_get_f32(r);
  p->home = sr_get_i32(r);
  p->tier = sr_get_u8(r);
  p->rent = sr_get_i32(r);
  p->rent_due = sr_get_i64(r);
  p->rent_debt = sr_get_i64(r);
  p->missed = sr_get_u8(r);
  p->evicted = sr_get_u8(r) != 0;
  p->reminders = sr_get_u8(r);
  p->pos.x = sr_get_f32(r);
  p->pos.y = sr_get_f32(r);
  p->pos.z = sr_get_f32(r);
  p->yaw = sr_get_f32(r);
  p->pitch = sr_get_f32(r);
  if (!get_inv(r, &p->stash) || !get_inv(r, &p->inv)) return false;
  int nm = sr_get_u8(r);
  if (nm > SR_MAX_MESSAGES) return false;
  for (int i = 0; i < nm; i++) {
    sr_get_str(r, p->msgs[i].from, sizeof p->msgs[i].from);
    sr_get_str(r, p->msgs[i].text, sizeof p->msgs[i].text);
    p->msgs[i].time = sr_get_i64(r);
    p->msgs[i].read = sr_get_u8(r) != 0;
  }
  p->msg_count = nm;
  if (p->home < 0 || p->home >= c->building_count) return false;
  if (version < 2) {
    /* migrate: background starting attributes and a fresh progression */
    const SrBackgroundDef *d = sr_background_def(p->background);
    if (!d) return false;
    memcpy(p->prog.attr, d->attr, SR_ATTR_COUNT);
    memcpy(p->base_attr, d->attr, SR_ATTR_COUNT);
    p->equipped = (int8_t)sr_inv_find(&p->inv, SR_IK_WEAPON, -1);
    return !r->failed;
  }
  p->armor = sr_get_f32(r);
  p->medevac = sr_get_u8(r) != 0;
  sr_get_bytes(r, p->prog.attr, SR_ATTR_COUNT);
  sr_get_bytes(r, p->base_attr, SR_ATTR_COUNT);
  sr_get_bytes(r, p->prog.skill_rank, SR_SK_COUNT);
  for (int s = 0; s < SR_SK_COUNT; s++) p->prog.skill_xp[s] = sr_get_i32(r);
  p->prog.level = sr_get_i32(r);
  p->prog.xp = sr_get_i32(r);
  p->prog.perk_points = sr_get_i32(r);
  p->prog.attr_points = sr_get_i32(r);
  sr_get_bytes(r, p->prog.perks, SR_SK_COUNT);
  p->prog.street_cred = sr_get_i32(r);
  sr_get_bytes(r, p->prog.standing, SR_MAX_FACTIONS);
  p->cyber_count = sr_get_u8(r);
  if (p->cyber_count > SR_MAX_CYBER) return false;
  for (int i = 0; i < p->cyber_count; i++) get_item(r, &p->cyber[i]);
  p->blueprints = sr_get_u32(r);
  p->equipped = (int8_t)sr_get_u8(r);
  sr_get_bytes(r, p->quick, 4);
  p->outfit = sr_get_u8(r);
  p->kills = sr_get_i32(r);
  p->knockouts = sr_get_i32(r);
  return !r->failed;
}

static void put_job(SrWriter *w, const SrJob *j) {
  sr_put_i32(w, j->id);
  uint8_t b[12] = {j->type,
                   j->tier,
                   j->state,
                   j->source,
                   (uint8_t)j->client_faction,
                   (uint8_t)j->target_faction,
                   j->constraints,
                   j->constraint_fails,
                   j->approaches,
                   j->intel,
                   (uint8_t)(j->negotiated | (j->executed << 1) | (j->has_bonus << 2) | (j->bonus_done << 3)),
                   0};
  sr_put_bytes(w, b, sizeof b);
  sr_put_i32(w, j->client_building);
  sr_put_i32(w, j->target_building);
  sr_put_i32(w, j->dest_building);
  sr_put_i64(w, j->offered);
  sr_put_i64(w, j->deadline);
  sr_put_i32(w, j->pay);
  sr_put_i32(w, j->upfront);
  sr_put_i32(w, j->bonus);
  sr_put_i32(w, j->paid);
  sr_put_i32(w, j->kills);
  sr_put_i32(w, j->alarms);
  sr_put_str(w, j->title);
}

static void get_job(SrReader *r, SrJob *j) {
  memset(j, 0, sizeof *j);
  j->id = sr_get_i32(r);
  uint8_t b[12];
  sr_get_bytes(r, b, sizeof b);
  j->type = b[0];
  j->tier = b[1];
  j->state = b[2];
  j->source = b[3];
  j->client_faction = (int8_t)b[4];
  j->target_faction = (int8_t)b[5];
  j->constraints = b[6];
  j->constraint_fails = b[7] != 0;
  j->approaches = b[8];
  j->intel = b[9];
  j->negotiated = (b[10] & 1) != 0;
  j->executed = (b[10] & 2) != 0;
  j->has_bonus = (b[10] & 4) != 0;
  j->bonus_done = (b[10] & 8) != 0;
  j->client_building = sr_get_i32(r);
  j->target_building = sr_get_i32(r);
  j->dest_building = sr_get_i32(r);
  j->offered = sr_get_i64(r);
  j->deadline = sr_get_i64(r);
  j->pay = sr_get_i32(r);
  j->upfront = sr_get_i32(r);
  j->bonus = sr_get_i32(r);
  j->paid = sr_get_i32(r);
  j->kills = sr_get_i32(r);
  j->alarms = sr_get_i32(r);
  sr_get_str(r, j->title, sizeof j->title);
}

/* ---- sections -------------------------------------------------------------------------- */

typedef struct SecCtx {
  SrWriter *w;
  size_t start;
} SecCtx;

static SecCtx begin(SrWriter *w, SrSection type, uint32_t version) {
  sr_put_u32(w, (uint32_t)type);
  sr_put_u32(w, version);
  SecCtx s = {w, w->len};
  sr_put_u32(w, 0); /* length placeholder */
  return s;
}

static void end(SecCtx s) {
  size_t payload = s.w->len - s.start - 4;
  sr_patch_u32(s.w, s.start, (uint32_t)payload);
  uint32_t crc = s.w->failed ? 0 : sr_crc32(s.w->data + s.start + 4, payload);
  sr_put_u32(s.w, crc);
}

SrResult sr_save_write(const SrGame *g, SrWriter *w, uint32_t version, int64_t counter) {
  sr_put_bytes(w, k_magic, 4);
  sr_put_u32(w, version);
  sr_put_u32(w, g->city->gen_version);
  sr_put_u64(w, g->seed);
  sr_put_u8(w, (uint8_t)g->opt.size);
  sr_put_u8(w, (uint8_t)g->opt.corp_count);
  SecCtx s = begin(w, SR_SEC_META, 1);
  sr_put_i64(w, counter);
  sr_put_u8(w, (uint8_t)g->difficulty);
  sr_put_u8(w, g->hardcore);
  sr_put_u8(w, g->pall_removed);
  end(s);
  s = begin(w, SR_SEC_CLOCK, 1);
  sr_put_i64(w, g->clock.scaled);
  sr_put_i32(w, g->clock.rate);
  sr_put_i64(w, g->last_minute);
  sr_put_u64(w, g->steps);
  end(s);
  s = begin(w, SR_SEC_PLAYER, version >= 2 ? 2 : 1);
  put_player(w, &g->player, version);
  end(s);
  s = begin(w, SR_SEC_NPCS, 1);
  int base = 0;
  for (int i = 0; i < g->pop->count; i++) base += !g->pop->npcs[i].promoted;
  sr_put_u32(w, (uint32_t)base);
  int changed = 0;
  for (int i = 0; i < base; i++) changed += g->pop->npcs[i].status != SR_NPC_ALIVE || g->pop->npcs[i].attitude != 0;
  sr_put_u32(w, (uint32_t)changed);
  for (int i = 0; i < base; i++) {
    const SrNpc *n = &g->pop->npcs[i];
    if (n->status == SR_NPC_ALIVE && n->attitude == 0) continue;
    sr_put_u32(w, n->id);
    sr_put_u8(w, n->status);
    sr_put_u8(w, (uint8_t)n->attitude);
  }
  sr_put_u32(w, (uint32_t)(g->pop->count - base));
  for (int i = base; i < g->pop->count; i++) sr_put_bytes(w, &g->pop->npcs[i], sizeof(SrNpc));
  end(s);
  if (version >= 2) {
    s = begin(w, SR_SEC_JOBS, 1);
    sr_put_u16(w, (uint16_t)g->jobs.count);
    sr_put_i32(w, g->jobs.next_id);
    sr_put_i32(w, g->jobs.completed);
    sr_put_i32(w, g->jobs.failed);
    sr_put_i32(w, g->active_job);
    for (int i = 0; i < g->jobs.count; i++) put_job(w, &g->jobs.jobs[i]);
    end(s);
    s = begin(w, SR_SEC_HEAT, 1);
    for (int j = 0; j < SR_MAX_FACTIONS; j++) {
      sr_put_u8(w, (uint8_t)g->heat.level[j]);
      sr_put_i64(w, g->heat.last_seen[j]);
      sr_put_i64(w, g->heat.decay_from[j]);
      sr_put_u64(w, g->heat.known_look[j]);
    }
    sr_put_u8(w, (uint8_t)g->heat.evidence_count);
    for (int i = 0; i < g->heat.evidence_count; i++) {
      const SrEvidence *e = &g->heat.evidence[i];
      uint8_t b[4] = {e->type, (uint8_t)e->jurisdiction, (uint8_t)e->points_to, e->severity};
      sr_put_bytes(w, b, 4);
      sr_put_i64(w, e->time);
      sr_put_u64(w, e->look);
    }
    end(s);
  }
  s = begin(w, SR_SEC_EVENTS, 1);
  int ne = sr_events_size(&g->events);
  sr_put_u16(w, (uint16_t)ne);
  for (int i = ne - 1; i >= 0; i--) {
    const SrWorldEvent *e = sr_events_recent(&g->events, i);
    sr_put_u8(w, (uint8_t)e->type);
    sr_put_i64(w, e->time);
    sr_put_u8(w, (uint8_t)e->district);
    sr_put_i32(w, e->building);
    sr_put_u8(w, (uint8_t)e->victim_faction);
    sr_put_u8(w, (uint8_t)e->blamed_faction);
    sr_put_u8(w, e->significant);
    sr_put_str(w, e->text);
  }
  end(s);
  s = begin(w, SR_SEC_DELTAS, 1);
  sr_put_u16(w, (uint16_t)g->deltas.count);
  for (int i = 0; i < g->deltas.count; i++) {
    sr_put_u32(w, g->deltas.id[i]);
    sr_put_u8(w, g->deltas.value[i]);
  }
  end(s);
  sr_put_u32(w, SR_SEC_END);
  return w->failed ? SR_ERR_NOMEM : SR_OK;
}

SrResult sr_save_peek(const void *data, size_t len, SrSaveInfo *info) {
  SrReader r;
  sr_reader_init(&r, data, len);
  uint8_t m[4];
  if (!sr_get_bytes(&r, m, 4) || memcmp(m, k_magic, 4) != 0) return SR_ERR_CORRUPT;
  memset(info, 0, sizeof *info);
  info->version = sr_get_u32(&r);
  info->gen_version = sr_get_u32(&r);
  info->seed = sr_get_u64(&r);
  sr_get_u8(&r);
  sr_get_u8(&r);
  if (r.failed) return SR_ERR_CORRUPT;
  if (info->version > SR_SAVE_VERSION || info->version == 0) return SR_ERR_VERSION;
  if (info->gen_version != SR_GEN_VERSION) return SR_ERR_VERSION;
  /* walk sections to validate checksums and find meta/clock */
  for (;;) {
    uint32_t type = sr_get_u32(&r);
    if (r.failed) return SR_ERR_CORRUPT;
    if (type == SR_SEC_END) break;
    sr_get_u32(&r); /* section version */
    uint32_t plen = sr_get_u32(&r);
    if (r.failed || plen > len - r.pos) return SR_ERR_CORRUPT;
    const uint8_t *payload = r.data + r.pos;
    sr_get_bytes(&r, NULL, plen);
    uint32_t crc = sr_get_u32(&r);
    if (r.failed || crc != sr_crc32(payload, plen)) return SR_ERR_CORRUPT;
    SrReader sub;
    sr_reader_init(&sub, payload, plen);
    if (type == SR_SEC_META) info->saved_at = sr_get_i64(&sub);
    if (type == SR_SEC_CLOCK) info->time = sr_get_i64(&sub) / 60;
  }
  return SR_OK;
}

SrResult sr_save_read(SrGame *g, const void *data, size_t len) {
  SrSaveInfo info;
  SrResult res = sr_save_peek(data, len, &info);
  if (res != SR_OK) return res;
  SrReader r;
  sr_reader_init(&r, data, len);
  sr_get_bytes(&r, NULL, 4 + 4 + 4 + 8);
  SrCityOptions opt;
  opt.size = (SrCitySize)sr_get_u8(&r);
  opt.corp_count = sr_get_u8(&r);
  /* rebuild the world from the seed, then apply all changes */
  sr_game_build_world(g, info.seed, &opt);
  sr_population_generate(g->pop, g->city);
  sr_jobs_init(&g->jobs);
  sr_heat_init(&g->heat);
  sr_events_init(&g->events);
  memset(&g->deltas, 0, sizeof g->deltas);
  sr_crowd_init(&g->crowd);
  for (;;) {
    uint32_t type = sr_get_u32(&r);
    if (type == SR_SEC_END) break;
    uint32_t sver = sr_get_u32(&r);
    uint32_t plen = sr_get_u32(&r);
    SrReader s;
    sr_reader_init(&s, r.data + r.pos, plen);
    sr_get_bytes(&r, NULL, plen + 4);
    switch (type) {
      case SR_SEC_META:
        sr_get_i64(&s);
        g->difficulty = (SrDifficulty)sr_get_u8(&s);
        g->hardcore = sr_get_u8(&s) != 0;
        g->pall_removed = sr_get_u8(&s) != 0;
        break;
      case SR_SEC_CLOCK:
        g->clock.scaled = sr_get_i64(&s);
        g->clock.rate = sr_get_i32(&s);
        g->last_minute = sr_get_i64(&s);
        g->steps = sr_get_u64(&s);
        g->clock.paused = false;
        break;
      case SR_SEC_PLAYER:
        if (!get_player(&s, &g->player, sver, g->city)) return SR_ERR_CORRUPT;
        break;
      case SR_SEC_NPCS: {
        uint32_t base = sr_get_u32(&s);
        if ((int)base != g->pop->count) return SR_ERR_CORRUPT;
        uint32_t changed = sr_get_u32(&s);
        for (uint32_t i = 0; i < changed; i++) {
          uint32_t id = sr_get_u32(&s);
          uint8_t st = sr_get_u8(&s);
          int8_t att = (int8_t)sr_get_u8(&s);
          if (id >= base) return SR_ERR_CORRUPT;
          g->pop->npcs[id].status = st;
          g->pop->npcs[id].attitude = att;
        }
        uint32_t promoted = sr_get_u32(&s);
        for (uint32_t i = 0; i < promoted && g->pop->count < SR_MAX_NPCS; i++)
          sr_get_bytes(&s, &g->pop->npcs[g->pop->count++], sizeof(SrNpc));
        break;
      }
      case SR_SEC_JOBS: {
        int n = sr_get_u16(&s);
        if (n > SR_MAX_JOBS) return SR_ERR_CORRUPT;
        g->jobs.next_id = sr_get_i32(&s);
        g->jobs.completed = sr_get_i32(&s);
        g->jobs.failed = sr_get_i32(&s);
        g->active_job = sr_get_i32(&s);
        for (int i = 0; i < n; i++) get_job(&s, &g->jobs.jobs[i]);
        g->jobs.count = n;
        break;
      }
      case SR_SEC_HEAT:
        for (int j = 0; j < SR_MAX_FACTIONS; j++) {
          g->heat.level[j] = (int8_t)sr_get_u8(&s);
          g->heat.last_seen[j] = sr_get_i64(&s);
          g->heat.decay_from[j] = sr_get_i64(&s);
          g->heat.known_look[j] = sr_get_u64(&s);
        }
        g->heat.evidence_count = sr_get_u8(&s);
        if (g->heat.evidence_count > SR_MAX_EVIDENCE) return SR_ERR_CORRUPT;
        for (int i = 0; i < g->heat.evidence_count; i++) {
          SrEvidence *e = &g->heat.evidence[i];
          uint8_t b[4];
          sr_get_bytes(&s, b, 4);
          e->type = b[0];
          e->jurisdiction = (int8_t)b[1];
          e->points_to = (int8_t)b[2];
          e->severity = b[3];
          e->time = sr_get_i64(&s);
          e->look = sr_get_u64(&s);
        }
        break;
      case SR_SEC_EVENTS: {
        int n = sr_get_u16(&s);
        for (int i = 0; i < n; i++) {
          SrWorldEvent e;
          memset(&e, 0, sizeof e);
          e.type = (SrEventType)sr_get_u8(&s);
          e.time = sr_get_i64(&s);
          e.district = (int8_t)sr_get_u8(&s);
          e.building = sr_get_i32(&s);
          e.victim_faction = (int8_t)sr_get_u8(&s);
          e.blamed_faction = (int8_t)sr_get_u8(&s);
          e.significant = sr_get_u8(&s) != 0;
          sr_get_str(&s, e.text, sizeof e.text);
          sr_events_add(&g->events, &e);
        }
        break;
      }
      case SR_SEC_DELTAS: {
        int n = sr_get_u16(&s);
        for (int i = 0; i < n; i++) {
          uint32_t id = sr_get_u32(&s);
          sr_deltas_set(&g->deltas, id, sr_get_u8(&s));
        }
        break;
      }
      default: break; /* unknown sections from the future are skipped */
    }
    if (s.failed) return SR_ERR_CORRUPT;
  }
  g->started = true;
  g->screen = SR_SCREEN_PLAY;
  return SR_OK;
}

/* ---- storage helpers ----------------------------------------------------------------- */

void sr_autosave_name(int slot, char *out, size_t cap) { snprintf(out, cap, "autosave%d.srsv", slot); }

SrResult sr_save_to(const SrGame *g, SrStorage *s, const char *name, int64_t counter) {
  SrWriter w;
  sr_writer_init(&w);
  SrResult r = sr_save_write(g, &w, SR_SAVE_VERSION, counter);
  if (r == SR_OK) r = s->write(s->user, name, w.data, w.len);
  if (r != SR_OK) sr_log(SR_LOG_ERROR, "save", "writing %s failed: %s", name, sr_result_name(r));
  sr_writer_free(&w);
  return r;
}

SrResult sr_load_from(SrGame *g, SrStorage *s, const char *name) {
  void *data = NULL;
  size_t len = 0;
  SrResult r = s->read(s->user, name, &data, &len);
  if (r != SR_OK) return r;
  r = sr_save_read(g, data, len);
  free(data);
  if (r != SR_OK) sr_log(SR_LOG_ERROR, "save", "loading %s failed: %s", name, sr_result_name(r));
  return r;
}

static int64_t slot_counter(SrStorage *s, const char *name) {
  void *data = NULL;
  size_t len = 0;
  if (s->read(s->user, name, &data, &len) != SR_OK) return -1;
  SrSaveInfo info;
  int64_t c = sr_save_peek(data, len, &info) == SR_OK ? info.saved_at : -1;
  free(data);
  return c;
}

int sr_autosave(const SrGame *g, SrStorage *s) {
  int oldest = 0;
  int64_t oldest_c = INT64_MAX, newest_c = -1;
  for (int i = 0; i < SR_AUTOSAVE_SLOTS; i++) {
    char name[32];
    sr_autosave_name(i, name, sizeof name);
    int64_t c = slot_counter(s, name);
    if (c > newest_c) newest_c = c;
    if (c < oldest_c) {
      oldest_c = c;
      oldest = i;
    }
  }
  char name[32];
  sr_autosave_name(oldest, name, sizeof name);
  return sr_save_to(g, s, name, newest_c + 1) == SR_OK ? oldest : -1;
}

SrResult sr_manual_save(const SrGame *g, SrStorage *s, int slot) {
  if (g->combat_count > 0) return SR_ERR_DENIED;
  if (slot < 0 || slot >= SR_MANUAL_SLOTS) return SR_ERR_INVALID;
  char name[32];
  snprintf(name, sizeof name, "slot%d.srsv", slot);
  return sr_save_to(g, s, name, 0);
}

SrResult sr_save_delete_hardcore(SrStorage *s) { return s->remove(s->user, "hardcore.srsv"); }
