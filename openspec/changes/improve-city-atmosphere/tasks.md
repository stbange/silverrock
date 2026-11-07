# Tasks

Phases are ordered so that each one improves the city on its own. The gloom (phase 2) and the wet
streets (phase 3) come early because they set the look that every later phase builds on. Every phase
ends green on CI, with the reference views (D15) regenerated and looked at. In "(spec: …)", a name
alone refers to a scenario in this change's deltas, and "D1"…"D18" refer to design decisions.

## 1. Foundations (about 3–4 days)

- [x] 1.1 Texture array of 64 layers with the layer map from D1; icons move into two atlas layers; fix the home icon reading past the array; layer map guard test (spec: "Every layer index is in range")
- [x] 1.2 Sprite primitive (upright, facing, decal) with atlas cells and animation frames from game time; near-sprite mesh rebuilt each frame (spec: "Sprites face the camera", "Animation follows game time")
- [x] 1.3 Additive glow pass (`SR_PASS_GLOW`): blending on, no depth write, faded by fog (spec: "Glows fade with fog")
- [x] 1.4 Contrast: ambient −35 %, remove the window highlight boost, bloom keyed on emissive (spec: "No white walls of windows", "Neon is the brightest")
- [x] 1.5 `u_time` and `u_hour` shader uniforms; per-vertex seed in the `pad` byte (D3, D11)
- [x] 1.6 Reference views: 14 views × 3 seeds in the platform tests, `SR_ATMOS_SHOTS=1` writes `docs/screenshots/atmosphere/`; before sheet (spec: D15)
- [x] 1.7 Image metrics in the platform tests: clipped pixels, emissive saturation, district distinctness, horizon (spec: "No white walls of windows", "Neon is the brightest", "Districts apart by shape and colour")

## 2. The gloom: light cycle, climate and neon through fog (about 1 week)

- [x] 2.1 Light cycle in `sr_sky_at`: 19 h night, dawn 10:00–11:00, grey day 11:00–14:00, dusk 14:00–15:00; sky luminance at most 0.25; lamps and signs on at all hours (spec: "Night at midnight", "Noon stays dark", "Short days")
- [x] 2.2 Dawn and dusk bands on the sky dome and in the fog colour toward east and west; a soft smear, never a sun disc; weaker in heavy rain (spec: "Dusk glows red in the west")
- [x] 2.3 Climate: new weather weights and visibilities from D17; "clear" shown as "overcast"; wetness at least 0.6 (spec: "Rain most of the time", "Short sight lines")
- [x] 2.4 Emissive fog factor 0.85; fog halos for large emissives up to 3× visibility, brightest 48 first; halos around lamps and signs in wet air (spec: "Neon halos through fog", "Neon through fog")
- [x] 2.5 Fog silhouettes of tall buildings just beyond the fog end (spec: "No dark slot at the end of the street")
- [x] 2.6 Low cloud deck (the Pall at 220–300 m): two drifting noise layers, lit by neon at night, slate grey by day, warmed at dawn and dusk; hidden at Sunrise (spec: "The Pall glows over bright districts", "No Pall at sunrise")
- [x] 2.7 Sunrise ending: clouds break, rain stops, fog lifts, clear blue sky with the sun (spec: "Sunrise shows daylight")
- [x] 2.8 Re-run the balance bands and the 16 playtests with the new climate; check stealth, crowds and umbrellas (spec: D17)
- [x] 2.9 Update the concept document (§3 setting, §6.6 environment, §16.1, §9.4, tagline, the Look row) for the vision
- [x] 2.10 Image metrics for noon, dusk and fog views (spec: "Noon stays dark", "Dusk glows red in the west", "Neon halos through fog")

## 3. Wet streets (about 1 week)

- [x] 3.1 Neon reflections on wet ground in the glow pass, with ripple, fade and the brightest-N limit (spec: "Wet streets reflect neon")
- [x] 3.2 Puddle decals, twice as reflective, rippling in rain
- [x] 3.3 Denser rain tinted by nearby lights; splashes; drips from awnings; rain visible in lamp cones (spec: "No rain under cover", "Rain splashes")
- [x] 3.4 Height fog; smog tint; acid-rain tint; ground mist in the Undercity and Docks (spec: "Each weather looks different", "Mist hugs the ground")
- [x] 3.5 Steam from manholes, vents and stalls, doubled in rain; smoke and flares from Industrial stacks (spec: "Steam rises from the street")

## 4. Facades (about 1 week)

- [x] 4.1 `src/world/sr_dressing.c` with `sr_facade_for`: base, shaft, crown, usage type, side walls; pinned digest test (spec: "Derived detail keeps the digest")
- [x] 4.2 Eight window modules (office strip, balconies, capsule hive, glass curtain, brick with sills, shuttered, boarded-up, clerestory) with glass marked in alpha
- [x] 4.3 Per-building UV offset and tint; the variety rule for same-block and across-street neighbours (spec: "Neighbours differ")
- [x] 4.4 Lit windows in the shader from usage type, seed and hour; four window colours; dark glass tinted by fog and sky (spec: "Offices go dark at night", "Homes light up in the evening")
- [x] 4.5 Blank side walls facing close neighbours, with fire escapes, AC units and pipes

## 5. Silhouettes and roofs (about 1 week)

- [ ] 5.1 Shape grammar: box, podium + tower, setbacks, chamfer, L, slab, row houses; ground footprint unchanged (spec: "Shapes stay inside the footprint")
- [ ] 5.2 Trim: cornices, ledges at setbacks, parapets
- [ ] 5.3 Roof kits: tanks, AC and ducts, antenna clusters with beacons, helipads, neon crowns, tiled pitched, sawtooth, corrugated, dishes (spec: "Roofs vary")
- [ ] 5.4 Landmark crowns from `silhouette[2]` (spire, ring, blade, dome); City Hall columns and steps; tall towers fade into the cloud deck

## 6. Street level (about 1–1.5 weeks)

- [ ] 6.1 Ground-floor bands: shopfront, lobby, shutter, loading dock, stall front, blank with posters; POI door under the sign; closed shops shuttered (spec: "Shops at street level", "Closed shops are shuttered")
- [ ] 6.2 Kerb faces and gutters; crosswalks, stop lines, lane arrows, manholes, drains, stains and cracks as decals (spec: "The pavement has a kerb")
- [ ] 6.3 `sr_dressing_block`: prop table with per-district weights, kerb and building bands, clear lane; lamps every 24 m (spec: "Street furniture by district", "Lamps along every street", "The walking lane stays clear")
- [ ] 6.4 Solid props in the game's collision query; connectivity validator run with props (spec: "Solid props block movement")
- [ ] 6.5 Alleys in gaps of 4 m or more: dumpsters, fire escapes, a weak light, puddles, steam vent (spec: "Alleys")
- [ ] 6.6 Overhead cables, lantern strings and laundry lines across narrow streets

## 7. Signs and adverts (about 1 week)

- [ ] 7.1 Shop names in the bitmap font on a dark plate under the icon (spec: "Sign legibility", "Shop name readable")
- [ ] 7.2 Neon script of 64 procedural glyphs; blade signs on corners in Commercial, Entertainment and the Old Quarter
- [ ] 7.3 Billboard slots (roofs, side walls, highways) with product adverts in eight layouts drawn into the billboard atlas (spec: "Adverts for the city's corporations")
- [ ] 7.4 `chess.ads_version`; scandal vandalises, dominance takes empty slots, a lost takeover loses slots (spec: "Scandal vandalises adverts", "Winners multiply")
- [ ] 7.5 Holo-ads in the glow pass over plazas
- [ ] 7.6 LED tickers with the latest headline on media buildings and metro entrances (spec: "Ticker shows the news")
- [ ] 7.7 Broken neon (8 %, more in poor districts); posters; wanted posters at Heat 3 or more (spec: "Wanted posters")

## 8. Sky details (about 3–4 days)

- [ ] 8.1 Searchlights from HQs and the Spire in the glow pass, cutting into the cloud deck, brighter on alert (spec: "Searchlights")
- [ ] 8.2 Advertising blimps on loops just under the clouds, with screens and navigation lights
- [ ] 8.3 Distant city mesh for rooftops and overcast weather above 150 m visibility (spec: "No void at the horizon")
- [ ] 8.4 Skyline ring with lit windows, beacons and the cloud glow above it

## 9. District kits and vertical layers (about 1.5 weeks)

- [ ] 9.1 Kit table for the 11 district types (shapes, modules, bases, roofs, prop weights, signature pieces) (spec: "Districts apart by shape and colour")
- [ ] 9.2 Signature set pieces: Docks cranes and containers; Industrial stacks, tanks and pipes; Old Quarter lantern lanes and shrines; Spire plazas and holo-trees; Campus lawns and glowing trees; Barrens rubble and burned cars; Civic flags and barriers
- [ ] 9.3 Highway decks with pillars, barriers, lamps and moving traffic lights; darker ambient and hanging bulbs underneath; Undercity shanties against pillars (spec: "Life under the highway")
- [ ] 9.4 Skyways between Spire towers (spec: "Skyways")
- [ ] 9.5 Elevated metro track with passing trains on the metro timetable (spec: "Trains pass overhead")

## 10. Motion (about 3–4 days)

- [ ] 10.1 Neon flicker, chasing marquee lights, beacon blink in sync per tower (spec: "Neon flickers")
- [ ] 10.2 Windows switching through the day; TV flicker windows
- [ ] 10.3 Drones, bird flocks, drifting litter with the wind, rotating fans, sparks from broken signs
- [ ] 10.4 Police light bars; a light bar on nearby patrols at Heat 3 or more (spec: "Animation follows game time")
- [ ] 10.5 Optional: the cloud deck darkens over a blacked-out district

## 11. Lighting, settings and verification (about 1 week)

- [ ] 11.1 Light pool decals on the ground and wall splashes for lamps, shopfronts, signs, vending and fires (spec: "Lamp light pools on the pavement")
- [ ] 11.2 Per-block draws within one ring of the camera; tessellated ground near the camera (spec: "Night lighting near the camera")
- [ ] 11.3 Optional per-pixel lighting for the ground and lower facades on desktop
- [ ] 11.4 Colour grading per district, blended across borders; vignette; chromatic offset with VHS (spec: "Grading follows the district")
- [ ] 11.5 Settings: street detail, reflections, pixel lighting, brightness; defaults by device; budgets from D13 (spec: "Detail setting", "Atmosphere within budget", "Brightness lifts the dark")
- [ ] 11.6 Benchmark route on high and low inside the frame-rate budget on desktop and the phone profile (spec: "Desktop benchmark route", "Dynamic resolution engages")
- [ ] 11.7 Update goldens; after sheet; refresh `docs/screenshots/m5` as `docs/screenshots/atmosphere`; write `docs/reviews/visuals-<date>-after.md` against V1–V13 and the vision
- [ ] 11.8 Manual look on desktop and phone (six districts, dawn, noon, dusk, rain, fog, smog, under a highway, a rooftop); record in `tests/manual-verification.md`
- [ ] 11.9 Coverage at 95 % or above; traceability covers every scenario of this change
