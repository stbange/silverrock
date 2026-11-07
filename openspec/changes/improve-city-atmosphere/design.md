# Design: A more atmospheric city

## Context

The findings are in `docs/reviews/visuals-2026-11-07.md` (V1–V13).

The city is drawn from `SrCity`:
- Blocks of 96 m pitch, including a 16 m street.
- One to four buildings per block, each an axis-aligned footprint with a height, a `style` (0–7), a
  `poi` and a `silhouette[4]`.
- The renderer builds one mesh per chunk of 4 × 4 blocks (`build_block` in `sr_scene.c`).
- Lighting is per vertex, with up to 8 lights per draw.
- Textures are 128 px layers of one texture array, generated at start-up.

This design keeps that pipeline and adds detail around it.

## Goals / Non-Goals

**Goals:**
- From any street, the view shows a readable street wall:
  - a ground floor with shops and doors;
  - buildings that differ from their neighbours;
  - props on the pavement;
  - signs and adverts;
  - a sky with something in it;
  - a horizon that is not a black slot.
- A screenshot tells the district from its shapes and props, not only its colour.
- Rain, fog and smog each look different.
- Everything stays procedural and deterministic from the seed, with no art assets.
- Seeds, saves and the city digest do not change.
- Phones keep 30 FPS (95th percentile) on the benchmark route, and desktops keep 60 FPS.

**Non-Goals:**
- New district types, new interiors or new missions (M6).
- Realistic rendering. The look stays late-90s: low resolution, vertex lighting by default,
  dithering, nearest-neighbour textures.
- Screen-space reflections, shadow maps or deferred lighting. They are too expensive for phones and
  out of style.
- Changing the city layout. The street grid, lots and building footprints stay. Alleys come from
  the existing gaps between lots.

## Decisions

### D1 — Texture layers and atlases (V13)

- `SR_TEX_LAYERS` grows from 32 to 64. That is 4 MB of RGBA8; GLES 3.0 guarantees 256 layers.
- The 21 icons move into two atlas layers (4 × 4 cells of 32 px each), which frees 19 layers.
- `SR_LAYER_ICON_FIRST` goes away. Icons are addressed by atlas cell.
- The layer map:

| Layers | Use |
|---|---|
| 0–11 | Existing base materials (white, concrete, windows, brick, metal, asphalt, pavement, sign, font, water) |
| 12–19 | Window modules (D3): office strip, residential with balconies, capsule hive, glass curtain with mullions, brick with sills, shuttered, boarded-up, industrial clerestory |
| 20–25 | Ground-floor modules (D4): shopfront glass, roller shutter, lobby, loading dock, stall front, blank with posters |
| 26–31 | Roof and trim: tiles, corrugated metal, parapet, cornice, gravel roof, helipad |
| 32–33 | Icon atlas |
| 34–37 | Sprite atlases (16 cells of 32 px each): steam, smoke, splash, drip, litter, sparks, drones, birds, glow, light pool, lens flare, searchlight cone |
| 38–41 | Prop atlases for distant props and decals: manhole, drain, crosswalk, arrows, cracks, stains, puddle, graffiti tags |
| 42–45 | Billboard and poster content (D7), regenerated when the board changes |
| 46 | Neon script glyphs (D7) |
| 47 | Pall noise (D8) |
| 48–63 | Reserved for M6 |

- A unit test checks that no index in the layer map passes `SR_TEX_LAYERS`. Without that guard, the
  home icon read past the end of the array unnoticed.

### D2 — Sprites and the glow pass

- New calls build sprites:
  - `sr_mesh_sprite(m, pos, size, cell, SR_SPRITE_UPRIGHT | SR_SPRITE_FACING | SR_SPRITE_DECAL, frame)`
    builds one sprite.
  - Upright sprites turn around the vertical axis to face the camera (steam, people far away,
    trees).
  - Facing sprites face the camera fully (glows, sparks, splashes).
  - Decals lie flat on a surface (manholes, puddles, light pools).
- Static decals go into the chunk mesh. Near animated sprites (within 60 m on high, 30 m on low) go
  into one mesh rebuilt each frame from the game time. They are deterministic.
- Animation frames are picked on the CPU: `frame = (t * fps + hash) % frames`.
- A new pass, `SR_PASS_GLOW`, draws with additive blending and no depth write, after the world
  pass. It draws light pools, searchlight cones, steam, holo-ads and reflections (D9).
- Glows fade with fog, as emissive surfaces do.

### D3 — The facade grammar (V1, V3)

- `sr_facade_for(const SrCity *c, int building, SrFacade *out)` lives in the world layer
  (`sr_dressing.c`). It is a pure function of the seed, the building id, the district type, the POI,
  the height and the neighbours.
- It chooses:
  - **Base** (0–4.5 m): shopfront, lobby, shutter, loading dock, stall front or blank wall
    (from D4).
  - **Shaft:** the window module, the floor height (3.2–4.2 m) and the material tint.
  - **Crown:** a parapet, cornice, neon crown or roof kit (D5).
  - **Usage type:** office, residential, industrial, vacant or mixed, for the window lighting below.
  - **Side walls:** blank walls where the building faces a neighbour closer than 6 m, with fire
    escapes, AC units and pipes.
- **Variety rule:** two buildings on the same block never share both the window module and the base
  type. Neighbours across a street differ in at least one of window module, tint or crown.
- **Texture offset:** each building gets a UV offset of `hash % 64` cells, so the patterns of
  neighbours no longer line up.
- **Window lighting moves to the shader.**
  - The window textures mark glass texels with alpha 200. Wall stays at 255; cut-outs stay below
    128.
  - The vertex `pad` byte carries the building's usage type (2 bits) and a 6-bit seed.
  - For glass, the shader hashes the window cell with the seed and compares it with a lit share
    from `u_hour` and the usage type:
    - offices: 70 % lit at 18:00, 10 % at 02:00;
    - homes: 20 % lit at 03:00, 75 % at 21:00;
    - industrial: 40 % flat;
    - vacant: 2 %.
  - Lit windows take one of four colours: warm, cool, TV blue with flicker (D11) and neon-tinted.
  - Unlit glass shows a faint tint of the fog and the sky.
- The lit colour peaks at 0.8 instead of clipping to white, and the highlight boost in the shader
  is removed (D12).

### D4 — Ground floors (V3)

- Each building facing a street gets a 4.5 m base band, chosen by the facade grammar from the
  district and the POI.
- Places of interest always get a shopfront or lobby, with the door under the sign. The door is
  where the interior trigger already is.
- Commercial, Entertainment and the Old Quarter favour shopfronts: lit glass, a kick plate, a
  canopy or awning.
- Residential favours lobbies and shutters. Industrial and the Docks favour loading docks.
- Closed shops (night, or a delta that switched them off) show shutters with graffiti.
- Shopfronts are emissive at 60 % so they light the pavement through light pools (D12).

### D5 — Silhouettes and roofs (V2)

**Shape grammar.** The grammar picks a shape from the district and the height, and stays inside the
ground footprint:
- `box`
- `podium + tower`: the tower is 50–80 % of the footprint, above a podium of 2–5 floors.
- `setbacks`: 1–3 steps every 25–40 % of the height.
- `chamfer`: corner boxes cut at 45°, built from extra quads.
- `L`: two boxes.
- `slab`: thin and wide, Residential.
- `row houses`: the Old Quarter, 2–4 per lot, each with its own height and pitched roof.

**Trim.**
- A cornice band every 6–12 floors on brick and concrete.
- Ledges at setbacks.
- A parapet on every flat roof.

**Roof kits.**
- Water tanks on legs, AC units and ducts, antenna clusters with blinking beacons.
- Helipads (Spire and Campus), neon crowns on towers over 120 m, rooftop billboards (D7).
- Tiled pitched roofs (Old Quarter), sawtooth roofs (Industrial), corrugated sheds (Docks and
  Barrens), satellite dishes (Residential).

**Landmarks.**
- HQ crowns use `silhouette[2]`: spire, ring, blade or dome.
- City Hall gets a columned base and steps.

**Collision.** The ground footprint is unchanged, and everything above 3 m does not collide. Player
collision, the crowd and navigation therefore need no change.

### D6 — Street dressing (V4)

- `sr_dressing_block(const SrCity *c, int block, SrProp *out, int max)` lives in the world layer.
  It is deterministic and cached per block by the renderer and the game.
- A prop has a kind, a position, a yaw and a variant.
- The kinds come from a table with size, solid flag, light (colour, radius), emissive, sprite or
  mesh, and a weight per district type.
- **Placement uses three bands on each pavement**, so the crowd's walking lanes stay clear:
  - **Kerb band** (outer 1.2 m): lamps every 24 m on both sides, traffic lights and crosswalk
    poles at intersections, bollards, hydrants, bins, parking meters, trees and planters, news boxes
    and parked cars in the road's parking lane.
  - **Building band** (inner 1 m): vending machines, benches, AC units, stalls with tarps,
    dumpsters, crates, trash bags, Matrix booths and stoops.
  - **Clear lane** (the middle): left empty.
- **Road:** crosswalks at every intersection, lane arrows and stop lines, manholes every 30–50 m
  (some steaming, D11), drains along the kerb, and stains and cracks. More of these in poor
  districts.
- **Kerb:** the pavement gets a 12 cm kerb face and a gutter strip, so it no longer floats.
- **Alleys:** gaps between buildings of 4 m or more become alleys, with dumpsters, fire escapes,
  hanging cables, a weak light, puddles and a steam vent. Undercity and Old Quarter alleys get extra
  clutter.
- **Overhead:** cables between facing buildings across streets narrower than the block in Old
  Quarter, Undercity and Residential; lantern strings in the Old Quarter; laundry lines in
  Residential.
- **Solid props** (dumpsters, vending machines, stalls, parked cars, booths) are added to the
  collision query in the game layer from the same function. The crowd's lanes avoid the building
  band already.
- **Density** follows the detail setting (D13), drawn by the same deterministic list. Low keeps
  every prop with a light, plus every third other prop.

### D7 — Signs, adverts and tickers (V5)

- **Shop names.** The generated name is drawn in the bitmap font under the icon, on a dark plate,
  sized to pass the 10 m legibility check.
- **Blade signs.** Vertical neon signs on building corners: 2–6 per block in Commercial,
  Entertainment and the Old Quarter. They use a made-up neon script of 64 procedural glyphs. It
  looks like writing without copying any real language, which follows the naming rules in §6.5.
- **Billboards.**
  - Slots sit on roofs, on blank side walls (D3) and over highways.
  - Each slot shows an advert for a megacorp product: the corp's colour and logo, a product name
    from the name tables and one of eight layouts. The images are drawn into the billboard atlas
    layers.
  - Each corp's adverts follow the board:
    - a scandal in the last 7 days makes 50 % of its adverts vandalised (sprayed tags, torn panels,
      flicker);
    - a corp above 25 % of megacorp resources takes over empty slots;
    - a corp that loses a takeover loses its slots in that district.
  - The renderer rebuilds the adverts when `chess.ads_version` changes, as it already does for gang
    turf.
- **Holo-ads.** Floating animated adverts in the glow pass, over plazas in the Spire, Commercial and
  Entertainment districts.
- **Tickers.** LED bands on media buildings and over metro entrances scroll the latest headline from
  the news feed. Only tickers within 80 m are rebuilt each frame.
- **Broken neon.** 8 % of signs flicker or have dead letters, more in poor districts.
- **Posters.** Gig posters, gang tags as sprites from each gang's marker, and wanted posters when
  the player has a bounty (Heat 3 or more). Wanted posters show the player's outfit colour.

### D8 — Sky, Pall and horizon (V6)

- **The Pall.** A tessellated ceiling at 380 m above the city, made of 32 × 32 vertices:
  - It uses the scrolling noise layer.
  - Each vertex is lit by the emissive density of the district below it, so it glows magenta over
    the Strip, cyan over the Spire and dull orange over Industrial.
  - It drifts with the weather's wind. It is hidden during the Sunrise ending.
- **Searchlights.** 2–4 per HQ and 6 in the Spire: additive cones in the glow pass, sweeping on
  hashed phases. They brighten when the owner's alert is raised.
- **Blimps.** 2–3 per city, on slow loops 150–250 m up, with emissive screens showing adverts (D7)
  and blinking navigation lights.
- **Distant city.** A static mesh for the whole city with one box per building, at a coarse height,
  using the lit-window shader, the crown colour and beacons. It is drawn in the no-fog pass beyond
  the streamed chunks and faded with distance. The streamed chunks overdraw it, and fog hides the
  seam. This removes the dark slot at the end of each street.
- **Skyline.** The skyline ring gets lit windows (the D3 shader at lower contrast), blinking red
  beacons on its tallest segments, and the Pall glow above it.

### D9 — Wet streets and weather looks (V7)

- **Reflections.** For each emissive sign, shopfront, lamp head and vehicle light within 40 m, the
  glow pass draws a mirrored, vertically stretched, dimmed quad on the ground beneath it.
  - The shader adds ripple distortion and lowers the strength with distance and with the angle to
    the camera.
  - The strength is `wetness × 0.6`.
  - On low detail only the 16 brightest reflections are drawn.
  - The "Reflections" setting turns them off.
- **Puddles.** Decals at fixed hashed places, more in poor districts. They are twice as reflective,
  and they ripple in rain.
- **Rain.**
  - Up to 1,500 streaks in a downpour on high (600 on low), longer streaks in heavy rain, tinted by
    the nearest light.
  - Splashes on the ground near the camera, drips along awning edges, and a sheet of rain visible in
    lamp cones.
- **Fog types.**
  - Fog becomes thicker near the ground: the shader adds density below 3 m, scaled by district
    (Undercity, Docks) and weather (fog).
  - **Smog alert:** a brown-yellow tint, 60 % visibility, and an orange Pall.
  - **Acid-rain advisory:** green-tinted rain, a sickly Pall, and more steam.
- **Steam and smoke.** Animated sprites from manholes, vents, stalls and chimneys. Steam doubles in
  rain. Industrial stacks carry smoke columns and an occasional flare.

### D10 — District kits and vertical layers (V8, V9)

**District kits.** Each district type has a kit: allowed shapes, window modules, base types, roof
kits, prop weights and one or two signature set pieces.

| District | Signature |
|---|---|
| Spire | Glass curtains, podium towers, skybridges at 60–150 m, plazas with planters and holo-trees, clean pavement, security drones |
| Civic | Columned bases, flags, police barriers, wide steps, floodlit facades |
| Commercial | Dense shopfronts, stacked billboards, market stalls, vending rows |
| Entertainment | Marquees with chasing lights, tall blade signs, holo-ads, queues of people at club doors |
| Residential | Slab blocks with balconies, laundry, satellite dishes, AC units, corner kiosks |
| Old Quarter | Row houses with tiled roofs, lantern strings, narrow lanes, food stalls with steam, shrines |
| Industrial | Sawtooth roofs, stacks with smoke and flares, tanks, pipes over the street, chain-link fences |
| Docks | Cranes, container stacks on empty lots, corrugated sheds, bollards and floodlights at the water |
| Campus | Low glass labs, lawns, glowing trees, fences, gate houses |
| Undercity | Shanty stacks against highway pillars, tarps, cables, barrel fires, hanging bulbs |
| Barrens | Rubble, burned cars, fences, collapsed shells, fires |

**Highways.**
- The decks get pillars every 24 m, barriers with lamps, and lines.
- Traffic on top is shown as moving pairs of head and tail lights.
- Under a deck, the ambient light drops by 40 % and bulbs hang from it.
- The Undercity is the land under the decks.

**Skyways.** Glass tubes between Spire towers 40–90 m apart, lit inside. They do not collide.

**Elevated metro.** Track beams on pillars along each metro line between stations, with a lit train
passing on the timetable the metro already uses.

### D11 — Motion (V10)

A `u_time` uniform and per-vertex seeds drive the animated details:

| Detail | How |
|---|---|
| Neon flicker | 8 % of signs, per-sign seed |
| Chasing marquee lights | UV scroll |
| Tickers | Text mesh offset |
| Windows switching on and off | The lit share follows `u_hour`, so windows change through the evening |
| TV flicker | Blue windows modulated at 2–6 Hz |
| Beacons | Blink in sync per tower |
| Steam, smoke and sparks | Animated sprites |
| Fans | Rotating quads |
| Litter | Drifts with the weather's wind |
| Drones | Short loops over the Spire, Campus and Civic districts |
| Birds | Flocks over the Docks and Barrens |
| Police light bars | Cycle on patrol cars, with a light at Heat 3 or more |

All animation is a function of game time, so screenshots in tests are reproducible.

### D12 — Lighting, contrast and post effects (V11, V12)

- **Contrast.**
  - Ambient drops by 35 % (palettes keep their hue).
  - The shader's window highlight boost is removed.
  - Bloom keys on the emissive flag instead of luminance, so neon blooms and windows do not.
- **Light pools.** Lamps, shopfronts, signs, vending machines and fires add a decal on the ground
  (radial, coloured, additive), and lamps and signs add a smaller splash on the nearest wall. This
  gives pooled light without per-pixel lighting, at almost no cost, on every device.
- **More lights near the camera.**
  - Chunks within one ring of the camera are drawn per block, each block with its own 8 lights.
    Farther chunks stay as one draw.
  - Ground near the camera is tessellated into 4 m quads, so vertex lighting can show pools from
    dynamic lights (vehicles, muzzle flashes).
- **Optional per-pixel ground lighting** ("Lighting: vertex / pixel", desktop default pixel, phone
  default vertex). Only the ground and the lower 6 m of facades use it.
- **Height fog** as in D9.
- **Colour grading.** The post shader applies lift, gamma and gain for each district, from the
  palette, blended over 30 m when crossing a district border.
- **Vignette** on by default. The VHS option adds a chromatic offset.

### D13 — Detail settings and budgets

**Settings** (saved with the other settings):
- `street_detail`: low, medium or high. The default is high on desktop and medium on phones.
- `reflections`: on or off.
- `pixel_lighting`: on or off.

**Effect of `street_detail`:**

| | Low | Medium | High |
|---|---|---|---|
| Props kept | lights + every third | lights + two in three | all |
| Animated sprite distance | 30 m | 45 m | 60 m |
| Rain streaks (downpour) | 600 | 1,000 | 1,500 |
| Reflections | 16 | 32 | 64 |
| Distant city | boxes per block | boxes per building | boxes per building + beacons |

**Budgets:**
- At most 60,000 vertices per chunk mesh on high and 25,000 on low.
- At most 256 draws.
- At most 4,000 near sprites.
- At most 96 frame lights.

The existing benchmark route and the frame-rate requirement are the gate. Dynamic resolution still
applies.

### D14 — Determinism and seeds

- Everything new is derived from the seed with `sr_mix64(seed ^ id * salt)`, never from the
  generator's RNG streams.
- `SrCity` does not change size. The facade grammar and the dressing are computed on demand and
  cached, so `SR_GEN_VERSION`, the city digest and the save format stay as they are.
- A unit test pins the digest of `sr_facade_for` and `sr_dressing_block` for a fixed seed, so later
  edits change them deliberately.

### D15 — Verification

**Reference views.** The platform tests render 12 fixed views on 3 seeds:
- street level in six district types;
- an alley;
- under a highway;
- a rooftop;
- rain at night;
- fog;
- smog.

The images are written to `docs/screenshots/atmosphere/` with `SR_ATMOS_SHOTS=1`, and before/after
sheets go into the review.

**Image metrics,** checked in the platform tests on the reference views:
- at most 3 % of pixels clipped to white;
- the 1 % most saturated pixels come from emissive surfaces;
- the mean colours of the six district views are separated by at least the existing palette
  distance;
- the horizon rule (no empty column below the horizon) still holds.

**Unit tests:**
- facade variety (the variety rule in D3 over 5 seeds);
- dressing density: at least 6 props per 100 m of kerb in every district except the Barrens, and
  at least 2 lights per 100 m everywhere;
- clear walking lanes (no solid prop in the clear lane);
- the layer map guard (D1);
- the advert state follows the board (scandal → vandalised);
- the sprite and glow pass building.

**Golden images** are updated once per phase, after a manual look.

**Benchmark.** The benchmark route on high and low stays inside the frame-rate budget.

## Risks / Trade-offs

- **Performance on phones.** Many small props and sprites cost draw time and vertices.
  - *Mitigation:* props are baked into chunk meshes, near sprites are batched in one mesh,
    everything is limited by distance and the detail setting, and the benchmark gates each phase.
- **Visual noise.** Adding detail can make the image busier, not better.
  - *Mitigation:* the contrast work (D12) comes in phase 1. The image metrics guard the clipping and
    saturation balance, and each phase ends with a manual look at the reference views.
- **Legibility of places.** More signs could hide the icons that tell a shop's function.
  - *Mitigation:* POI signs keep the brightest emissive value and the largest size on their facade.
    The legibility scenario stays.
- **Collision changes.** Solid props could block paths or doors.
  - *Mitigation:* solid props stay out of the clear lane and the door zone, and the
    connectivity validator also runs with the props.
- **Golden image churn.** Every phase changes the reference images.
  - *Mitigation:* one update per phase, reviewed with the before/after sheet.

## Migration Plan

- No save migration. The new settings default as in D13 when they are missing from a settings file.
- Golden images and the reference views are regenerated per phase with `SR_UPDATE_GOLDENS=1`.

## Open Questions

- Should billboards also show the player's own exploits (a "Who is the runner?" campaign) once
  notoriety is high? This is suggested for M6.
- Should the Pall's glow react to blackouts the player causes, with a district going dark on the
  ceiling? It would be cheap, and it is listed as an optional task.
