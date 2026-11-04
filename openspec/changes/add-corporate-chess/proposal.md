# Proposal: Corporate chess (M5)

## Why

So far the factions exist but do not act. Pillar 2 — "a pawn on the board" — needs the megacorps, politicians and gangs
to pursue their agendas, turn them into the jobs the player takes, and react to the results. The story campaign gives
this simulation a spine: the patron's black-ops division, its handler, rival offers, procedural arcs and an endgame
with several endings. Roadmap milestone: **M5 Corporate Chess**.

## What Changes

- Faction simulation: resources, a strategic tick every 6 in-game hours, goals and operations by doctrine, contracts
  offered through fitting channels, off-screen resolution by NPC crews, counterplay, attribution and framing, visible
  consequences, gang turf wars, long-run stability, and the intel Board.
- City politics: council and mayor, scheduled elections, policies with world effects, corruption and scandals.
- Corporate market: share prices driven by the simulation, trading, insider-trading suspicion.
- News media: news from world events, owner spin, distribution on terminals, billboards and rumours, planted stories,
  anonymous reporting of the player's deeds.
- Story campaign: acts, patron selection, the prologue test and retainer, handler trust, black-op gating, betrayal,
  counter-offers and defection, procedural arcs, the endgame trigger, five endings including Sunrise, the authored cast
  with succession, and campaign completability by bots.

## Capabilities

### New Capabilities

- `faction-simulation`: how factions pursue agendas, generate contracts, react to outcomes and change the city.
- `city-politics`: government, elections, policies and corruption.
- `corporate-market`: share prices and trading.
- `news-media`: how world events become news and rumours, and how the player can influence them.
- `story-campaign`: the campaign structure around the patron, arcs, the endgame and endings.

### Modified Capabilities

None. T3 and T4 jobs use the tiers already defined by `job-system`; the patron-trust gate for T4 is specified in
`story-campaign`.

## Out of scope

The Deep as explorable content (M6), additional arc templates beyond the initial eight (M6), localisation (M6).

## Impact

- New `sim/` modules for strategy, politics, market and news; story data (arc templates, operation templates, dialogue
  templates for the authored cast).
- The Sunrise ending adds a daylight lighting mode to the renderer, used nowhere else.
- New nightly jobs: 52-week faction soak without the player, story-progression bots on 20 seeds.
