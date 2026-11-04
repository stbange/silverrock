# Design: Living city

## Context

Builds on the city, faction roster, clock and weather from M1. See proposal.md for motivation and
docs/concept/01-game-design.md §7 and §12 for the player-facing design.

## Goals / Non-Goals

**Goals:**
- A city that feels busy (150+ visible citizens on desktop, 60+ on phones) within budget.
- Persistent people the player can rely on (the noodle vendor is there every evening).
- An economy loop that creates pressure without being punishing.

**Non-Goals:**
- Conversations with branching dialogue (M3 introduces job dialogue; M5 story dialogue).
- Faction-driven changes to prices and services (M5).

## Decisions

### D1 — Records far away, entities nearby
Notable citizens are compact records (~200 bytes: identity, appearance seed, home, workplace, schedule template,
traits, memory slots, status). Entering the mid tier spawns an entity; leaving it writes the entity back to its
record. Up to ~3,000 records per city.
*Alternative:* everyone as a full entity (too much memory and CPU, especially on the web).

### D2 — Analytic schedules for the far tier
A schedule is a list of `(time window, activity, place)` entries per weekday. For far-tier citizens the game computes
where they should be at time *t* (at a place, or on a route between two places at an interpolated position) instead
of simulating the movement. The same function places citizens when they enter the mid tier, which makes transitions
invisible.

### D3 — Ambient crowds seeded by block and time slot
Ambient citizens are generated from `hash(seed, block id, 30-minute slot)` using district and hour density tables.
Each ambient citizen has a stable ID derived from that hash; interacting with one promotes it to a notable record
with the same appearance.

### D4 — Appearance from parts
Body type, head, face texture layers, hairstyle, hair colour, clothing set (by role and class), faction marker,
visible cyberware and accessories are chosen from weighted tables. Uniqueness within a crowd is checked on the
combination key; collisions re-roll a detail part.

### D5 — Reactions as stimulus → response tables
Stimuli (gunshot, explosion, body, fight, weird behaviour, rain) carry a position and radius. Citizens choose a response
(flee, cover, gawk, film, report, join, shelter) from tables weighted by traits and district. Reports become events on
the event bus.

### D6 — Needs as rates, economy as indices
Hunger and fatigue are linear rates per in-game hour with modifiers (stims, sprinting). Prices are
`base × district index × category modifier`, with hooks for event modifiers later. All money is integer cred.

### D7 — Save format version 1
Little-endian, `SRSV` magic, format version, generator version, sections with type, version, length and CRC32.
Writes go to a temporary file followed by rename. On the web, the storage directory is an IndexedDB-backed mount that
is synchronised after each save and before listing or loading.

## Risks / Trade-offs

- [Crowds look cloned] → Uniqueness check per crowd; screenshot reviews; more clothing and hair variety in content.
- [Schedules make the city feel robotic] → Random variation per day from the citizen's stream; occasional errands.
- [Rent pressure frustrates players] → Difficulty presets change rent and hunger rates; the autoplayer balance
  report tracks how often a "gig worker" bot misses rent.
- [IndexedDB sync loses data when the tab is closed mid-save] → Atomic write plus autosave on page hide; the previous
  save stays valid.
