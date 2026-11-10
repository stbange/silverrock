/* Headline writing: templates for each kind of event and each outlet's voice, filled with the
 * names involved and fitted to the 63 characters a headline can hold without cutting a word. */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

/* First word of a name ("Zamora-Petrov" -> "Zamora", "Hollis Undercity" -> "Hollis"). */
static void short_name(const char *in, char *out, size_t cap) {
  size_t n = 0;
  while (in[n] && in[n] != ' ' && in[n] != '-' && n + 1 < cap) n++;
  memcpy(out, in, n);
  out[n] = '\0';
}

static size_t expand(char *out, size_t cap, const char *tmpl, const char *a, const char *b, const char *c3) {
  size_t n = 0;
  for (const char *p = tmpl; *p && n + 1 < cap; p++) {
    const char *ins = NULL;
    if (p[0] == '%' && p[1] >= '1' && p[1] <= '3') {
      ins = p[1] == '1' ? a : p[1] == '2' ? b : c3;
      p++;
    }
    if (ins) {
      for (const char *q = ins; *q && n + 1 < cap; q++) out[n++] = *q;
    } else {
      out[n++] = *p;
    }
  }
  out[n] = '\0';
  return n;
}

void sr_headline(char *out, size_t cap, const char *tmpl, const char *a, const char *b, const char *c3) {
  char big[256];
  a = a ? a : "";
  b = b ? b : "";
  c3 = c3 ? c3 : "";
  size_t limit = cap - 1 < 63 ? cap - 1 : 63;
  if (expand(big, sizeof big, tmpl, a, b, c3) > limit) {
    char sa[SR_NAME_LEN], sb[SR_NAME_LEN], sc[SR_NAME_LEN];
    short_name(a, sa, sizeof sa);
    short_name(b, sb, sizeof sb);
    short_name(c3, sc, sizeof sc);
    if (expand(big, sizeof big, tmpl, sa, sb, sc) > limit) { /* still too long: end at a whole word */
      big[limit] = '\0';
      char *sp = strrchr(big, ' ');
      if (sp && sp > big) *sp = '\0';
      size_t n = strlen(big);
      while (n > 0 && (big[n - 1] == ',' || big[n - 1] == ':' || big[n - 1] == '-' || big[n - 1] == ' '))
        big[--n] = '\0';
    }
  }
  snprintf(out, cap, "%s", big);
}

typedef enum Voice { VOICE_STATION, VOICE_OWNER, VOICE_PIRATE } Voice;

/* [voice][variant]; %1 subject, %2 place, %3 culprit. */
static const char *const k_sabotage[3][3] = {
    {"Sabotage cripples %1 site in %2", "%1 facility in %2 hit by saboteurs", "Blast halts %1 operations in %2"},
    {"Cowardly attack on %1 in %2", "%1 vows to rebuild after %2 attack", "Terror strikes %1 site in %2"},
    {"%1 bleeds in %2 - who is next?", "Somebody finally hit %1 in %2", "%1 burns in %2, investors sweat"}};
static const char *const k_theft[3][3] = {
    {"Data stolen from %1 in %2", "%1 confirms breach at %2 site", "Thieves raid %1 offices in %2"},
    {"Criminals target %1 data in %2", "%1: customer data safe after %2 break-in", "%1 hunts thieves after %2 raid"},
    {"%1 secrets walk out the door in %2", "%1 files loose on the street", "Who robbed %1 in %2? Nobody cries"}};
static const char *const k_break_in[3][3] = {
    {"Break-in at %1 site in %2", "Security scare at %1 in %2", "Intruders slip into %1 site in %2"},
    {"%1 tightens security in %2", "Attempted intrusion at %1 in %2 foiled", "%1 reviews guards after %2 incident"},
    {"%1 fortress in %2 not so tight", "Ghosts walk through %1 in %2", "%1 guards asleep in %2 again"}};
/* When nobody knows whose place it was: [event 0 sabotage, 1 theft, 2 break-in, 3 failed][variant]; %2 place. */
static const char *const k_anon[4][3] = {
    {"Sabotage cripples a site in %2", "Unmarked facility in %2 hit by saboteurs", "Blast halts work at a %2 site"},
    {"Data stolen in %2 break-in", "Breach confirmed at an unnamed %2 office", "Thieves raid offices in %2"},
    {"Break-in at an unmarked site in %2", "Security scare in %2", "Intruders slip into a %2 site"},
    {"Intruders repelled in %2", "Guards drive off a raid in %2", "Failed raid on a %2 site"}};
static const char *const k_failed[3] = {"Intruders repelled at %1 in %2", "Guards drive off raid on %1 in %2",
                                        "Failed raid on %1 site in %2"};
static const char *const k_gang_war[3] = {"Gang war in %2: %3 push out %1", "%3 seize %2 from %1",
                                          "Turf war in %2 as %3 take over"};
static const char *const k_new_gang[3] = {"New gang %3 claims %2", "%3 colours appear across %2",
                                          "Residents of %2 meet the %3"};
static const char *const k_gang_end[3] = {"%1 wiped out", "End of the line for the %1", "The %1 are finished"};
static const char *const k_shots[3] = {"Shots fired in %2", "Gunfire panics %2 at night", "Shooting in %2 streets"};
static const char *const k_killing[3] = {"Killing in %2 shocks residents", "Body found in %2",
                                         "Violence in %2 claims a life"};
static const char *const k_blackout[3] = {"Lights out in %2", "Power cut plunges %2 into darkness",
                                          "Blackout hits %2, grid blames hackers"};

void sr_news_event_headline(char *out, size_t cap, const SrCity *c, const SrWorldEvent *e, int outlet, int perp) {
  int v = (int)(sr_mix64(sr_hash_combine((uint64_t)e->time, (uint64_t)(uint32_t)e->building + 31u)) % 3u);
  int subject = e->victim_faction;
  Voice voice = outlet < 0                                                 ? VOICE_PIRATE
                : (subject >= 0 && c->factions.f[outlet].owner == subject) ? VOICE_OWNER
                                                                           : VOICE_STATION;
  const char *sub = subject >= 0 && subject < c->factions.count ? c->factions.f[subject].name : "a firm";
  const char *place = e->district >= 0 && e->district < c->district_count ? c->districts[e->district].name : "the city";
  const char *who = perp >= 0 && perp < c->factions.count ? c->factions.f[perp].name : "";
  char base[96];
  bool anon = subject < 0 || subject >= c->factions.count;
  int anon_kind = e->type == SR_EV_SABOTAGE ? 0 : e->type == SR_EV_THEFT ? 1 : e->type == SR_EV_JOB_DONE ? 2 : 3;
  bool site_event =
      e->type == SR_EV_SABOTAGE || e->type == SR_EV_THEFT || e->type == SR_EV_JOB_DONE || e->type == SR_EV_JOB_FAILED;
  if (anon && site_event) { /* no owner to name: say what happened and where */
    sr_headline(base, sizeof base, k_anon[anon_kind][v], sub, place, who);
    sub = "a site";
  } else
    switch (e->type) {
      case SR_EV_SABOTAGE: sr_headline(base, sizeof base, k_sabotage[voice][v], sub, place, who); break;
      case SR_EV_THEFT: sr_headline(base, sizeof base, k_theft[voice][v], sub, place, who); break;
      case SR_EV_JOB_DONE: sr_headline(base, sizeof base, k_break_in[voice][v], sub, place, who); break;
      case SR_EV_JOB_FAILED: sr_headline(base, sizeof base, k_failed[v], sub, place, who); break;
      case SR_EV_GUNFIRE:
        if (subject < 0 && perp >= 0)
          sr_headline(base, sizeof base, k_new_gang[v], sub, place, who);
        else if (subject >= 0 && perp >= 0)
          sr_headline(base, sizeof base, k_gang_war[v], sub, place, who);
        else
          sr_headline(base, sizeof base, k_shots[v], sub, place, who);
        break;
      case SR_EV_KILLING:
        if (subject >= 0 && e->building < 0)
          sr_headline(base, sizeof base, k_gang_end[v], sub, place, who);
        else
          sr_headline(base, sizeof base, k_killing[v], sub, place, who);
        break;
      case SR_EV_BLACKOUT: sr_headline(base, sizeof base, k_blackout[v], sub, place, who); break;
      default: sr_headline(base, sizeof base, "%1", e->text, place, who); break;
    }
  /* the culprit, as far as the evidence goes (gang news names the gangs already) */
  bool gang_news = e->type == SR_EV_GUNFIRE || e->type == SR_EV_KILLING;
  char tail[128];
  if (perp == -2 && !gang_news) { /* the culprit added only when it fits; else lead with it */
    snprintf(tail, sizeof tail, voice == VOICE_PIRATE ? "%s - a runner did it" : "%s; runner blamed", base);
    if (strlen(tail) <= 63)
      snprintf(out, cap, "%s", tail);
    else
      sr_headline(out, cap, "Runner hits %1 in %2", sub, place, NULL);
  } else if (perp >= 0 && perp != subject && !gang_news) {
    snprintf(tail, sizeof tail, voice == VOICE_PIRATE ? "%s; %s behind it?" : "%s; %s suspected", base, who);
    if (strlen(tail) <= 63)
      snprintf(out, cap, "%s", tail);
    else
      sr_headline(out, cap, "%2 suspected in %1 attack", sub, who, NULL);
  } else {
    snprintf(out, cap, "%s", base);
  }
}
