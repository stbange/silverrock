# Proposal: Mercenary work (M3)

## Why

The player can now live in the city but cannot earn a living. Jobs are the heart of the game loop: they pay the rent,
grow the character and — later — move pieces in the corporate game. Jobs need the tools to solve them (combat,
stealth, social options, weapons and gadgets), a progression system that turns experience into better jobs, and a law
system that makes risk real. Roadmap milestone: **M3 Mercenary Work**.

## What Changes

- Job system: sources (job board, fixers, contacts, events), generation from templates against the live world state,
  tiers T0–T2 in this change (T3–T4 arrive with M5), briefings and intel, negotiation, at least two approaches per
  job, constraints, completion, failure and consequences as world events.
- Combat and stealth: firearms with hit zones and armour, damage types, melee and takedowns, non-lethal play, enemy
  perception with light, alert states, tactics, the visibility meter, health and healing, flatlining, difficulty,
  bodies.
- Equipment: weapon archetypes, procedural variants by manufacturer and rarity, mods, ammo types, gadgets, quick slots,
  inventory, crafting, vendors and fences, cyberware.
- Character progression: attributes, use-based skills, levels, perks, Street Cred, faction standing, respec.
- Law and Heat: Heat per jurisdiction, detection, escalating responses, decay, reducing Heat, arrests, disguises,
  restricted areas.

## Capabilities

### New Capabilities

- `job-system`: finding, negotiating, executing and completing jobs, and their consequences.
- `combat`: fighting and sneaking — damage, melee, non-lethal options, enemy perception and tactics, health,
  flatlining and difficulty.
- `equipment`: weapons, mods, ammunition, gadgets, inventory, crafting, trading and cyberware.
- `character-progression`: attributes, skills, levels, perks, Street Cred, faction standing and respec.
- `law-and-heat`: crime detection, Heat per jurisdiction, responses, decay, disguises and restricted areas.

### Modified Capabilities

None. Autosave after a finished job (`game-persistence`) and crime-report events (`population-simulation`) were
already specified in M2 and are now exercised.

## Out of scope

Matrix-based approaches and Matrix traces (M4), patron jobs, T3–T4 tiers, faction reactions and story (M5).

## Impact

- New modules in `sim/` (jobs, combat, AI, items, progression, heat) and data tables (job templates, weapon
  archetypes, manufacturers, gadgets, cyberware, perks).
- New autoplayer strategies (gig worker, ghost, hammer, face) and the balance report in the nightly job.
- Larger save format (version 2 sections for inventory, progression, jobs and Heat).
