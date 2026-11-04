# Design: Mercenary work

## Context

Builds on the living city (citizens, economy, needs, saves). See proposal.md for motivation and
docs/concept/01-game-design.md §10–§14 for the player-facing design.

## Goals / Non-Goals

**Goals:**
- Jobs that are always solvable and always offer a choice of approach.
- Combat and stealth that work with mouse, gamepad and touch.
- An economy and progression curve validated by bots across many seeds.

**Non-Goals:**
- Hand-authored missions (all jobs are template-based; authored story beats come in M5).
- Vehicles and vehicle combat.

## Decisions

### D1 — Job templates bound to the live world
A template declares roles (client, target, location, item, person), selection predicates over the world state
(for example "a facility owned by a rival of the client within reach of the player"), objectives, constraints, pay
formula and tier. Binding picks concrete entities with the job's own RNG stream, so jobs are deterministic.

### D2 — Solvability check by approach planners
After binding, the generator runs a cheap planner per approach over the site's room graph: Ghost (route avoiding
camera and guard coverage using vents, roofs and darkness), Face (disguise or credential available and a social route),
Hammer (any route; checks the expected enemy strength against the tier). Matrix planning is added in M4. A job is
accepted only if at least two approaches succeed; otherwise the site gets an extra route or the binding is re-rolled.

### D3 — Hitscan with ballistic exceptions
Most guns are hitscan with spread, recoil and hit zones. Slow projectiles (launchers, thrown gadgets, smart rounds) are
simulated entities. Damage = base × zone multiplier × type-vs-resistance × armour penetration factor.

### D4 — Perception from the light grid
Each perception check samples light intensity at the player's position from the same light grid used for rendering,
combined with distance, movement speed, stance, weather visibility and camo. The result fills a per-observer
awareness meter; thresholds move the observer between alert states. The HUD visibility meter shows the strongest
current sample.

### D5 — AI: alert FSM plus utility for tactics
Alert states (unaware, suspicious, searching, combat) are a finite-state machine. In combat, a utility selector picks
actions (take cover, flank, suppress, throw grenade, call reinforcements, retreat, surrender) from scored options using
morale, health, numbers and cover.

### D6 — Items from seeds
An item is `(archetype id, manufacturer id, rarity, item seed, installed mods)`. Stats are computed from the archetype
base, the manufacturer's tendency vector, the rarity range and the seed. Only the tuple is saved.

### D7 — Heat as per-jurisdiction meters with evidence
Crimes create evidence records (witness, camera, trace, body) attached to a jurisdiction. Heat rises only when evidence
reaches its owner (witness reports, camera footage reviewed, body found). Decay runs while no observer of that
jurisdiction sees the player; recognition uses the appearance key, so changing clothes or face breaks recognition.

### D8 — Balance by bots
Autoplayer strategies play jobs through the action layer. The nightly balance report tracks income, expenses, rent
misses, deaths and job success rates per tier and strategy across 200 seeds, with thresholds that fail the job if the
curves leave their bands.

## Risks / Trade-offs

- [Template jobs feel repetitive] → Large template and site variety, constraints and twists; M5 ties jobs to arcs.
- [Planners are too optimistic or too slow] → Planner results are cross-checked by autoplayer runs; planner time is
  budgeted and measured.
- [Light-based stealth is unreadable] → Always-visible visibility meter and clear audio cues for alert changes.
- [Touch combat is too hard] → Aim assistance, larger hit tolerance on touch, difficulty sliders.
