# Silverrock

> *The sun hasn't risen in forty years. Rent is due on Friday.*

A first-person cyberpunk RPG in a procedurally generated, permanently night-time city. You are a mercenary on the
bottom rung, paying rent with small jobs — until the black-ops division of one of the city's 5–10 megacorps puts you on
retainer, and your jobs become moves in the corporate chess game. Retro low-poly 3D, a living city, a separate 3D
Matrix that changes the real world, and an emergent story about corporations, politicians and gangs, each with its own
agenda.

**Status:** concept phase. No game code yet — this repository holds the concept and the OpenSpec specifications the
implementation will follow.

## Concept

| Document | Contents |
|---|---|
| [Game design](docs/concept/01-game-design.md) | Vision, pillars, setting, player, core loops, procedural city, living city, factions and the corporate game, story, jobs, progression, economy, combat, weapons and gadgets, the Matrix, art direction, controls |
| [Technical design](docs/concept/02-technical-design.md) | C17 / SDL3 / OpenGL architecture, determinism, world generation, simulation, retro renderer, input and touch, persistence, platforms, performance budgets, testing strategy (≥ 95 % coverage, automated game tests), CI |
| [Roadmap](docs/concept/03-roadmap.md) | Milestones M0–M7, first playable slice, risks, open questions |

## Technology

- **C17**, **SDL3**, **OpenGL 3.3 Core** (desktop) and **OpenGL ES 3.0 / WebGL 2** (web)
- Targets: **Windows**, **Linux**, **Web** via Emscripten — mobile browsers switch to touch controls automatically
- Unit tests with ≥ 95 % line coverage, headless scenario tests, autoplayer bots, input replays, visual regression,
  browser end-to-end tests
- Spec-driven development with [OpenSpec](https://github.com/Fission-AI/OpenSpec)

## Specifications (OpenSpec)

Each roadmap milestone is an OpenSpec change in [`openspec/changes/`](openspec/changes) with a proposal, design, tasks
and spec deltas (22 capabilities, 206 requirements, 310 scenarios). Implemented changes get archived into
[`openspec/specs/`](openspec/specs).

| Milestone | Change | Capabilities |
|---|---|---|
| M0 Foundation | [`add-engine-foundation`](openspec/changes/add-engine-foundation) | platform-runtime, input-controls, quality-gates |
| M1 Procedural City | [`add-procedural-city`](openspec/changes/add-procedural-city) | city-generation, faction-roster, world-environment, retro-rendering |
| M2 Living City | [`add-living-city`](openspec/changes/add-living-city) | population-simulation, player-life, game-persistence |
| M3 Mercenary Work | [`add-mercenary-work`](openspec/changes/add-mercenary-work) | job-system, combat, equipment, character-progression, law-and-heat |
| M4 The Matrix | [`add-matrix`](openspec/changes/add-matrix) | matrix-navigation, matrix-world-link |
| M5 Corporate Chess | [`add-corporate-chess`](openspec/changes/add-corporate-chess) | faction-simulation, city-politics, corporate-market, news-media, story-campaign |

Validate the specs:

```bash
npx @fission-ai/openspec@1.16.0 validate --all --strict
```

List the changes and their task progress:

```bash
npx @fission-ai/openspec@1.16.0 list
```
