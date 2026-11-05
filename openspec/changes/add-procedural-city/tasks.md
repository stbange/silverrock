# Tasks

## 1. Seeds and digests

- [x] 1.1 Implement seed codes with checksum and the new-game seed field; verify "Seed code round trip" and "Mistyped seed code"
- [x] 1.2 Implement world and chunk digests plus cross-platform golden digest tests for 10 reference seeds; verify "Same seed twice" and "Identical across targets"
- [x] 1.3 Add the invariant sweep runner (50 seeds per PR, 2,000 nightly); verify "Different seeds"

## 2. Geography and districts

- [x] 2.1 Implement coastline, terrain terraces and the city boundary in fixed-point; verify unit tests and digests
- [x] 2.2 Implement district placement, Voronoi, relaxation and type assignment with constraints; verify "Required districts present" and "Docks on the coast"
- [x] 2.3 Implement stage re-rolls on invariant failure; verify "Invariant failure is re-rolled" with a forced-failure fixture

## 3. Transport and layers

- [x] 3.1 Implement arterials, district street styles and elevated highways; verify "Everything is reachable"
- [x] 3.2 Implement the metro network and stations; verify "Metro serves every district"
- [x] 3.3 Implement Undercity footprints, skyways and rooftop access points; verify "Skyway access"

## 4. Buildings, landmarks and places

- [x] 4.1 Implement blocks, lots and the building shape grammar per district style and wealth; verify unit tests for grammar rules and no overlapping footprints
- [x] 4.2 Implement landmark grammars for headquarters, City Hall and the Pall Control Spire; verify "Unique headquarters silhouettes"
- [x] 4.3 Implement place-of-interest distribution and the starter neighbourhood; verify "Service availability" and "Starter services nearby"
- [x] 4.4 Implement the player's studio apartment template with procedural decoration; verify a scenario script that starts a new game inside the apartment

## 5. Faction roster

- [x] 5.1 Implement megacorp generation (count, names, sectors, doctrine, colours, logos, black-ops divisions); verify "Random count range", "Fixed count", "Distinct identities" and "Required sectors present"
- [x] 5.2 Implement corporate asset placement; verify "Assets placed"
- [x] 5.3 Implement gangs, government, police precincts and minor groups; verify "Gang turf assigned", "Council composition" and "Fixer coverage"
- [x] 5.4 Implement initial relationships, faction icons and member markers; verify "Every megacorp has a rival" and "Distinct markers and icons"

## 6. Names

- [x] 6.1 Implement morpheme-based name generators, business and corp templates, and the normalised blocklist check; verify "Blocklisted name never appears" and "Unique district names"
- [x] 6.2 Add fuzz and property tests for the name generators; verify they run in the nightly job

## 7. Interiors

- [x] 7.1 Implement typed room-graph interiors (office, lab, warehouse, club, clinic, apartment block, data centre); verify unit tests per type
- [x] 7.2 Implement the two-route and security-chain checks; verify "Two routes into mission sites"
- [x] 7.3 Implement interior generation on approach and delta persistence; verify "Stable interiors"

## 8. Streaming and navigation

- [x] 8.1 Implement chunk detail tasks with per-frame budgets, the LRU cache and deltas keyed by stable IDs; verify "Returning to an area" and "Generation budget respected"
- [x] 8.2 Implement per-chunk navigation graphs and hierarchical path search; verify unit tests and the connectivity invariants on 50 seeds
- [x] 8.3 Measure macro generation time on reference hardware with a progress screen; verify "Macro generation time"

## 9. World environment

- [x] 9.1 Implement the world clock, rate setting and menu pause; verify "Default time scale" and "Clock stops in menus"
- [x] 9.2 Implement the calendar and time skipping; verify "Week rollover" and "Waiting one hour"
- [x] 9.3 Implement the permanent-night sky and ambient model; verify "Night at noon"
- [x] 9.4 Implement deterministic weather states, transitions and the visibility query; verify "Deterministic weather", "Gradual transition" and "Downpour reduces visibility"

## 10. Retro renderer

- [x] 10.1 Implement internal resolution presets, integer scaling and the UI resolution; verify "Default internal resolution", "Integer scaling" and "Text on a phone"
- [x] 10.2 Implement retro effect toggles with golden images per toggle; verify "Independent toggles"
- [x] 10.3 Implement the light grid, per-object light selection, emissive pass and bloom; verify "Neon through fog"
- [x] 10.4 Implement district palettes, fog tint and border blending; verify "Palettes differ"
- [x] 10.5 Implement landmark logo pass and skyline impostor; verify "Logo beyond fog" and "No void at the horizon"
- [x] 10.6 Implement rain particles with cover masks and wet-street reflections; verify "No rain under cover"
- [x] 10.7 Implement procedural textures and signage (icons, bitmap-font names); verify "Sign legibility" and texture golden images on all targets
- [x] 10.8 Implement chunk batching, texture arrays, instancing and dynamic resolution; verify "Desktop benchmark route" and "Dynamic resolution engages"

## 11. Integration checks

- [ ] 11.1 Run the exit demo — wake up in the apartment and walk across the city at night in the rain — on all targets including a phone; record results in `tests/manual-verification.md`
- [x] 11.2 Review screenshots from 50 random seeds for district readability and landmark visibility; log findings and fixes
- [x] 11.3 Confirm every scenario of this change is covered in the traceability report

## Implementation notes (2026-11-05)

- Chunks are 4 x 4 blocks (384 m) rather than 64 m; streaming budget is two chunk builds per frame with an LRU cache.
- Open: 11.1 (exit demo on real hardware and a phone). Done since the last status: walkable apartment and mission interiors with delta persistence, per-chunk navigation graphs with hierarchical search (50-seed connectivity in CI), light grid and bloom, name-generator fuzzing, 50-seed screenshot review in docs/reviews/.

## Workflow follow-up

- Archive the change with `openspec archive add-procedural-city` once CI is green on all targets.
