# Proposal: Procedural city (M1)

## Why

The city is the game's main character. Every later system — population, jobs, the Matrix, the faction game — needs a
generated city with districts, buildings, places of interest and factions to live in. It also carries the biggest
creative risk: procedural output that looks samey or unreadable. Building it early, with its look, lets us judge
that risk before investing in gameplay. Roadmap milestone: **M1 Procedural City**.

## What Changes

- Seed codes and fully deterministic, cross-platform city generation (macro phase plus on-demand chunk detail).
- Geography, 8–14 districts of defined types, roads, elevated highways, metro network, vertical layers.
- Buildings from shape grammars, landmarks, places of interest, the starter neighbourhood around the player's home.
- Enterable interiors generated on approach, with at least two routes into mission-capable sites.
- Generated names with a real-world blocklist.
- Faction roster: 5–10 megacorps with sectors, doctrines, identities and black-ops divisions; gangs; government and
  police; fixers, media and minor groups; initial relationships.
- World environment: world clock, calendar, time skipping, permanent night, deterministic weather.
- Retro renderer: low-resolution pipeline, retro effect toggles, night lighting, district palettes, landmark
  beacons, skyline horizon, rain and wet streets, readable signage, performance budgets.

## Capabilities

### New Capabilities

- `city-generation`: deterministic generation of the city's geography, districts, transport, buildings, places of
  interest, interiors and names, plus streaming and validation.
- `faction-roster`: generation of the factions that inhabit the city — megacorps, gangs, government, police and
  minor groups — with their identities, assets and initial relationships.
- `world-environment`: the world clock, calendar, time skipping, permanent night and weather.
- `retro-rendering`: how the city looks — the low-resolution retro pipeline, lighting, district identity,
  landmarks, horizon, rain, signage and frame-rate budgets.

### Modified Capabilities

None.

## Out of scope

Citizens and traffic (M2), player needs and money (M2), faction behaviour over time (M5), the Matrix (M4), audio
beyond placeholder ambience.

## Impact

- New modules: `world/` (generation, chunks, navigation, environment), the render front-end and GL backend
  extensions, data tables for district styles, facade modules, names and the blocklist.
- New golden data: city digests per reference seed for every target; reference images per district type.
- The nightly job gains the 2,000-seed invariant sweep.
