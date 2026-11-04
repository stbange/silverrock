# Design: Procedural city

## Context

Builds on the foundation (core math, fixed-point, RNG streams, renderer bootstrap, test infrastructure). See
proposal.md for motivation and docs/concept/02-technical-design.md §5–§8 for the engine-wide approach.

## Goals / Non-Goals

**Goals:**
- Same seed and generator version → bit-identical city on Windows, Linux and Web.
- Macro generation within 3 s (desktop) / 6 s (mobile); chunk detail within a per-frame budget.
- Readable, distinct districts and landmarks from the first build.

**Non-Goals:**
- Simulated inhabitants and traffic (M2).
- Interior gameplay content such as guards and loot (M3).

## Decisions

### D1 — Two-phase generation
The **macro phase** produces compact arrays: geography, district polygons, road and metro graphs, lots, building
footprints and heights, places of interest, faction roster and assets. The **detail phase** turns one 64 × 64 m chunk
into meshes, props, signage and navigation data, and is a pure function of `(seed, generator version, chunk id)`
plus deltas. Interiors are a detail product generated when the player approaches an entrance.
*Alternative:* generating the whole city up front (too slow and too much memory on phones).

### D2 — Integer and fixed-point math only
All generation code uses integers and Q16.16 fixed-point, with in-house routines for square roots and trigonometry.
Floating-point conversion happens only when meshes are handed to the renderer. A digest (64-bit hash over all macro
arrays, and per-chunk digests over detail output) is compared across targets in CI.
*Alternative:* floats with strict compiler flags (still differs across libm implementations and Emscripten).

### D3 — District layout
District seeds are placed by weighted Poisson sampling inside the city boundary, relaxed with a few Lloyd iterations,
and turned into polygons by a fixed-point Voronoi. Types are assigned by a constraint solver with priorities
(Spire near the centre, Docks touching the coast, Industrial adjacent to Docks, Campus at the edge, Undercity under
highway corridors). Unsatisfiable constraints re-roll with a derived sub-seed.

### D4 — Roads, metro and layers
Arterials connect district centres along a relative-neighbourhood graph; each district fills its interior with a
style-specific street pattern (grid, organic, chaotic). Elevated highways follow selected arterials and define the
Undercity footprint. The metro is a spanning network over district hubs plus loop lines, with at least one station
per district. Skyways connect tall buildings in rich districts and always get a street-level access point.

### D5 — Buildings by grammar, facades by modules
A shape grammar per district style and wealth level produces mass models (footprint, floors, setbacks, rooftop props).
Facades are assembled from curated low-poly modules with procedural textures. Landmark buildings (HQ towers, City
Hall, Pall Control Spire) use dedicated grammars with per-corp parameters so silhouettes differ.

### D6 — Interiors from room graphs
Interiors are generated from typed room graphs (office, lab, warehouse, club, clinic, apartment block, data centre).
The generator checks two disjoint entry routes (graph connectivity with an edge-disjoint path search) and a complete
security chain before accepting a layout.

### D7 — Names and blocklist
Names come from morpheme tables with several sound palettes plus templates for businesses and corps. Every generated
name is normalised (case, punctuation, spacing) and checked against a blocklist of real companies, brands and public
figures stored in the data tables; hits are re-rolled.

### D8 — Streaming
Chunk generation runs as resumable tasks within a per-frame budget on the main thread (web) or on workers (desktop).
Chunks are kept in an LRU cache; deltas keyed by stable IDs are applied after generation.

### D9 — Rendering
Static chunk geometry is merged per chunk and material, using texture arrays. A CPU light grid selects up to 8 lights
per draw. Fog colour and palette come from the district at the camera position, blended at borders. The skyline
impostor is a low-poly ring generated in the macro phase. Landmark logos are drawn in a separate pass that ignores fog.

## Risks / Trade-offs

- [Generated cities look alike] → Strong district styles, curated module libraries, landmark rules, screenshot
  reviews of 50 random seeds per milestone.
- [Fixed-point precision artefacts in large coordinates] → City coordinates in centimetres in 32-bit integers
  (±21 km range); local fixed-point within chunks.
- [Constraint re-rolls make generation slow on some seeds] → Cap re-rolls per stage, measure the worst case in the
  nightly seed sweep, fail if the macro phase exceeds the budget on any seed.
- [Phones run out of memory] → Smaller LRU cache and fog distance in the mobile quality preset.

## Open Questions

- Exact curated facade module count for the first slice (art-driven; does not change specs).
