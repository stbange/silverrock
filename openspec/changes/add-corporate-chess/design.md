# Design: Corporate chess

## Context

Builds on the faction roster (M1), world events and jobs (M3), and Matrix-driven information effects (M4). See
proposal.md for motivation and docs/concept/01-game-design.md §8–§9 for the player-facing design.

## Goals / Non-Goals

**Goals:**
- An emergent corporate game that stays coherent and interesting for a whole campaign.
- Every job the player sees is a step in some faction's operation.
- A campaign that always reaches an ending, on any seed.

**Non-Goals:**
- A full economic model of the city (prices use indices and modifiers).
- Branching dialogue trees for every NPC (only the authored cast has dialogue templates).

## Decisions

### D1 — Goals, operations, steps
Each faction holds 1–3 goals chosen by doctrine (for example *Shark*: "become dominant in my sector"). Every strategic
tick, it scores operation templates against its goals, resources and relationships, and launches the best affordable
one. An operation is a small graph of steps; each step names the work needed (a job template plus bindings) and its
success effects. Everything uses the faction's RNG stream, so the same seed and the same player actions give the same
history.

### D2 — Contracts and NPC crews
A step becomes a contract offered through a channel matching the client (patron handler, fixer, gang contact, job
board). If no one takes it before its deadline, it is resolved off-screen: success probability from the client's
resources versus the target's defences, modified by competing crews. Off-screen outcomes produce the same world events
as player outcomes, but with different attribution evidence.

### D3 — Bounded resources and protected pieces
Resources are clamped, income is mean-reverting, and a megacorp cannot drop below a floor before the endgame. Gangs can
be destroyed; a replacement gang is generated after a cooldown. A 52-week soak without the player checks these bounds
nightly.

### D4 — Effects as modifiers on existing systems
Outcomes change data other systems already read: asset ownership (Matrix host security, guards' uniforms),
facility security level, patrol density, district price modifiers, gang turf (graffiti, members, shop protection), NPC
status, billboard ad weights. No system needs special-case code for the faction game.

### D5 — Arcs on top of operations
Arc templates are sequences of operation templates with shared bindings (the same two corps, the same scientist) and
branch conditions on outcomes. The story director keeps 1–3 arcs active, prefers arcs involving the patron and the
player's past targets, and pushes towards the endgame when Act II has run long enough.

### D6 — Endgame director
When a megacorp's share of total resources passes the dominance threshold, or Act II exceeds its duration, the director
picks the checkmate scenario that best fits the board (merger if two allied corps lead, coup if a corp owns a council
majority, Pall Protocol if the energy corp is strong, Ghost if the player explored the Deep) and starts the Act III
sequence.

### D7 — Daylight only for Sunrise
The renderer gets a daylight lighting mode (sky gradient, sun direction, neutral ambient). It is reachable only from
the Sunrise ending sequence and its aftermath sandbox.

## Risks / Trade-offs

- [Emergent nonsense, e.g. corps at war with everyone] → Doctrine weights, relationship inertia, operation cooldowns,
  narrative review of generated histories from the soak.
- [The player cannot see the game] → The Board, news, rumours and visible consequences; briefings name the
  operation once intel reveals it.
- [Story bots cannot judge fun] → Bots prove completability and pacing bounds; fun is judged in playtests.
- [Long simulation costs CPU on phones] → Strategic ticks are cheap (tens of factions) and spread over frames.
