# Tasks

Phases are ordered so that each one improves the city on its own. Every phase ends green on CI, with
the reference views (D15) regenerated and looked at. In "(spec: …)", a name alone refers to a
scenario in this change's deltas, and "D1"…"D15" refer to design decisions.

## 1. Foundations (about 3–4 days)

- [ ] 1.1 Texture array of 64 layers with the layer map from D1; icons move into two atlas layers; fix the home icon reading past the array; layer map guard test (spec: "Every layer index is in range")
- [ ] 1.2 Sprite primitive (upright, facing, decal) with atlas cells and animation frames from game time; near-sprite mesh rebuilt each frame (spec: "Sprites face the camera", "Animation follows game time")
- [ ] 1.3 Additive glow pass (`SR_PASS_GLOW`): blending on, no depth write, faded by fog (spec: "Glows fade with fog")
- [ ] 1.4 Contrast: ambient −35 %, remove the window highlight boost, bloom keyed on emissive (spec: "No white walls of windows", "Neon is the brightest")
- [ ] 1.5 `u_time` and `u_hour` shader uniforms; per-vertex seed in the `pad` byte (D3, D11)
- [ ] 1.6 Reference views: 12 views × 3 seeds in the platform tests, `SR_ATMOS_SHOTS=1` writes `docs/screenshots/atmosphere/`; before sheet (spec: D15)
- [ ] 1.7 Image metrics in the platform tests: clipped pixels, emissive saturation, district distinctness, horizon (spec: "No white walls of windows", "Neon is the brightest", "Districts apart by shape and colour")

## 2. Facades (about 1 week)

- [ ] 2.1 `src/world/sr_dressing.c` with `sr_facade_for`: base, shaft, crown, usage type, side walls; pinned digest test (spec: "Derived detail keeps the digest")
- [ ] 2.2 Eight window modules (office strip, balconies, capsule hive, glass curtain, brick with sills, shuttered, boarded-up, clerestory) with glass marked in alpha
- [ ] 2.3 Per-building UV offset and tint; the variety rule for same-block and across-street neighbours (spec: "Neighbours differ")
- [ ] 2.4 Lit windows in the shader from usage type, seed and hour; four window colours; dark glass tinted by fog and sky (spec: "Offices go dark at night", "Homes light up in the evening")
- [ ] 2.5 Blank side walls facing close neighbours, with fire escapes, AC units and pipes

## 3. Silhouettes and roofs (about 1 week)

- [ ] 3.1 Shape grammar: box, podium + tower, setbacks, chamfer, L, slab, row houses; ground footprint unchanged (spec: "Shapes stay inside the footprint")
- [ ] 3.2 Trim: cornices, ledges at setbacks, parapets
- [ ] 3.3 Roof kits: tanks, AC and ducts, antenna clusters with beacons, helipads, neon crowns, tiled pitched, sawtooth, corrugated, dishes (spec: "Roofs vary")
- [ ] 3.4 Landmark crowns from `silhouette[2]` (spire, ring, blade, dome); City Hall columns and steps

## 4. Street level (about 1–1.5 weeks)

- [ ] 4.1 Ground-floor bands: shopfront, lobby, shutter, loading dock, stall front, blank with posters; POI door under the sign; closed shops shuttered (spec: "Shops at street level", "Closed shops are shuttered")
- [ ] 4.2 Kerb faces and gutters; crosswalks, stop lines, lane arrows, manholes, drains, stains and cracks as decals (spec: "The pavement has a kerb")
- [ ] 4.3 `sr_dressing_block`: prop table with per-district weights, kerb and building bands, clear lane; lamps every 24 m (spec: "Street furniture by district", "Lamps along every street", "The walking lane stays clear")
- [ ] 4.4 Solid props in the game's collision query; connectivity validator run with props (spec: "Solid props block movement")
- [ ] 4.5 Alleys in gaps of 4 m or more: dumpsters, fire escapes, a weak light, puddles, steam vent (spec: "Alleys")
- [ ] 4.6 Overhead cables, lantern strings and laundry lines across narrow streets

## 5. Signs and adverts (about 1 week)

- [ ] 5.1 Shop names in the bitmap font on a dark plate under the icon (spec: "Sign legibility", "Shop name readable")
- [ ] 5.2 Neon script of 64 procedural glyphs; blade signs on corners in Commercial, Entertainment and the Old Quarter
- [ ] 5.3 Billboard slots (roofs, side walls, highways) with product adverts in eight layouts drawn into the billboard atlas (spec: "Adverts for the city's corporations")
- [ ] 5.4 `chess.ads_version`; scandal vandalises, dominance takes empty slots, a lost takeover loses slots (spec: "Scandal vandalises adverts", "Winners multiply")
- [ ] 5.5 Holo-ads in the glow pass over plazas
- [ ] 5.6 LED tickers with the latest headline on media buildings and metro entrances (spec: "Ticker shows the news")
- [ ] 5.7 Broken neon (8 %, more in poor districts); posters; wanted posters at Heat 3 or more (spec: "Wanted posters")

## 6. Sky and horizon (about 4–5 days)

- [ ] 6.1 The Pall ceiling: tessellated, noise layer, lit from below by district emissive density, drifting with the wind, hidden at Sunrise (spec: "The Pall glows over bright districts", "No Pall at sunrise")
- [ ] 6.2 Searchlights from HQs and the Spire in the glow pass, brighter on alert (spec: "Searchlights")
- [ ] 6.3 Advertising blimps on loops with screens and navigation lights
- [ ] 6.4 Distant city mesh beyond the streamed chunks; seam hidden by fog (spec: "No dark slot at the end of the street")
- [ ] 6.5 Skyline ring with lit windows, beacons and the Pall glow above it (spec: "No void at the horizon")

## 7. Weather (about 1 week)

- [ ] 7.1 Neon reflections on wet ground in the glow pass, with ripple, fade and the brightest-N limit (spec: "Wet streets reflect neon")
- [ ] 7.2 Puddle decals, twice as reflective, rippling in rain
- [ ] 7.3 Denser rain tinted by nearby lights; splashes; drips from awnings; rain visible in lamp cones (spec: "No rain under cover", "Rain splashes")
- [ ] 7.4 Height fog; smog tint; acid-rain tint; ground mist in the Undercity and Docks (spec: "Each weather looks different", "Mist hugs the ground")
- [ ] 7.5 Steam from manholes, vents and stalls, doubled in rain; smoke and flares from Industrial stacks (spec: "Steam rises from the street")

## 8. District kits and vertical layers (about 1.5 weeks)

- [ ] 8.1 Kit table for the 11 district types (shapes, modules, bases, roofs, prop weights, signature pieces) (spec: "Districts apart by shape and colour")
- [ ] 8.2 Signature set pieces: Docks cranes and containers; Industrial stacks, tanks and pipes; Old Quarter lantern lanes and shrines; Spire plazas and holo-trees; Campus lawns and glowing trees; Barrens rubble and burned cars; Civic flags and barriers
- [ ] 8.3 Highway decks with pillars, barriers, lamps and moving traffic lights; darker ambient and hanging bulbs underneath; Undercity shanties against pillars (spec: "Life under the highway")
- [ ] 8.4 Skyways between Spire towers (spec: "Skyways")
- [ ] 8.5 Elevated metro track with passing trains on the metro timetable (spec: "Trains pass overhead")

## 9. Motion (about 3–4 days)

- [ ] 9.1 Neon flicker, chasing marquee lights, beacon blink in sync per tower (spec: "Neon flickers")
- [ ] 9.2 Windows switching through the evening; TV flicker windows
- [ ] 9.3 Drones, bird flocks, drifting litter with the wind, rotating fans, sparks from broken signs
- [ ] 9.4 Police light bars; a light bar on nearby patrols at Heat 3 or more (spec: "Animation follows game time")
- [ ] 9.5 Optional: the Pall darkens over a blacked-out district

## 10. Lighting, settings and verification (about 1 week)

- [ ] 10.1 Light pool decals on the ground and wall splashes for lamps, shopfronts, signs, vending and fires (spec: "Lamp light pools on the pavement")
- [ ] 10.2 Per-block draws within one ring of the camera; tessellated ground near the camera (spec: "Night lighting near the camera")
- [ ] 10.3 Optional per-pixel lighting for the ground and lower facades on desktop
- [ ] 10.4 Colour grading per district, blended across borders; vignette; chromatic offset with VHS (spec: "Grading follows the district")
- [ ] 10.5 Settings: street detail, reflections, pixel lighting; defaults by device; budgets from D13 (spec: "Detail setting", "Atmosphere within budget")
- [ ] 10.6 Benchmark route on high and low inside the frame-rate budget on desktop and the phone profile (spec: "Desktop benchmark route")
- [ ] 10.7 Update goldens; after sheet; refresh `docs/screenshots/m5` as `docs/screenshots/atmosphere`; write `docs/reviews/visuals-<date>-after.md` against V1–V13
- [ ] 10.8 Manual look on desktop and phone (six districts, rain, fog, smog, under a highway, a rooftop); record in `tests/manual-verification.md`
- [ ] 10.9 Coverage at 95 % or above; traceability covers every scenario of this change
