# Proposal: Living city (M2)

## Why

A generated city without people is a level, not a city. Pillar 1 ("the city doesn't need you") and pillar 3
("rent before glory") both start here: citizens with routines who react to the world, and a player who must eat,
sleep and pay rent. Saving and loading become necessary as soon as a week of play matters. Roadmap milestone:
**M2 Living City**.

## What Changes

- Citizen appearance generator with role and faction markers.
- Notable citizens with homes, workplaces, schedules and memory; ambient crowds seeded per block and hour;
  promotion from ambient to notable.
- Three-tier level-of-detail simulation with invisible transitions.
- Routines and shifts, reactions to danger and weather, services with opening hours, traffic, metro timetables,
  rumours.
- Character creation with five backgrounds and the new-game start in the rented studio.
- Needs (hunger, fatigue), sleep, money and trading with district prices, weekly rent, eviction, capsule hotels,
  housing tiers, apartment amenities, metro and cab travel.
- Save system: complete and compact saves, autosaves, manual slots, web persistence, atomic writes, integrity checks,
  versioning.

## Capabilities

### New Capabilities

- `population-simulation`: the citizens of the city — appearance, notable and ambient citizens, level-of-detail
  simulation, routines, reactions, services, traffic and rumours.
- `player-life`: life as a city dweller — character creation, the starting situation, needs, sleep, money, rent,
  eviction, housing and getting around.
- `game-persistence`: saving and loading the game reliably on every target.

### Modified Capabilities

None. (Weather effects from `world-environment` are consumed, not changed.)

## Out of scope

Jobs, combat, Heat and crime consequences (M3); faction-driven changes to the city (M5). Citizens emit crime-report
events in this change, but nothing consumes them until M3.

## Impact

- New modules in `sim/` (entities, population, schedules, needs, economy) and `game/` (character creation, save/load).
- Render: instanced crowds and rigid-part characters.
- Save format version 1; fuzz target for the save loader added to the nightly job.
- First autoplayer strategy ("live a week") added to game tests.
