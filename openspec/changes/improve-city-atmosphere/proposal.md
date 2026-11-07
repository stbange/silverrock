# Proposal: A more atmospheric city

## Why

The visual review (`docs/reviews/visuals-2026-11-07.md`) found that the city works as a space but
does not feel like a place:

- **Facades read as noise.** Three window textures, tiled the same way on every building, with 38 %
  of windows lit at all hours. Every lit window clips to white.
- **Every building is a box.** No setbacks, ledges, roofs or crowns, and no rhythm along the street.
- **The street is the least detailed part of the city.** There are no shopfronts and no street
  furniture, one lamp per block, and a pavement that floats with no kerb.
- **Signage is missing.** Only places of interest have an icon. There are no shop names, billboards,
  holo-ads or tickers, though the art direction promises adverts that follow the corporate game.
- **The sky and the horizon are empty.** The Pall is not drawn, and each street ends in a dark slot.
- **Rain does not make the street wet.** There are no reflections, puddles, splashes or steam.
- **Districts differ only in colour, and the vertical layers are missing.** Highways are flat slabs,
  the Undercity is under nothing, and there are no skyways or elevated metro.
- **Nothing moves but people and cars**, lamp light never pools, and contrast is upside down: walls
  are hazy grey, windows are white and neon loses.
- **The texture array is full.** The home sign shows the wrong icon, and no room is left for new
  materials.

This change brings the city up to the art direction in `docs/concept/01-game-design.md` §6.5, §6.6
and §16 before M6 adds content. M6's content (new districts and set pieces) then builds on the kits
this change introduces. Roadmap position: after the playability pass, before **M6**.

## What Changes

- **Render foundations.**
  - The texture array grows to 64 layers, with atlases for icons and sprites.
  - A sprite primitive (upright, facing, decal) with animation frames.
  - An additive glow pass.
  - Lower ambient light, so neon wins.
  - The home icon bug is fixed.
- **Facades.**
  - A deterministic facade grammar for each building: a base, a shaft and a crown.
  - Eight window modules instead of three, and a texture offset for each building.
  - The shader decides which windows are lit, from the building's type and the hour, so offices go
    dark at night and homes light up in the evening.
- **Silhouettes.** Podiums and towers, setbacks, chamfers, L-shapes, cornices and parapets. Roof
  kits: water tanks, air-conditioning units, antenna clusters, helipads, neon crowns, pitched and
  tiled roofs, sawtooth roofs.
- **Street level.**
  - Shopfronts, lobbies, shutters and doors on ground floors.
  - Kerbs, crosswalks, lane arrows, manholes and drains.
  - Street furniture chosen per district: lamps every 24 m, traffic lights, benches, vending
    machines, bins, hydrants, parked cars, food stalls, planters and barriers.
  - Alleys between buildings, with fire escapes, dumpsters and steam vents.
  - Overhead cables and lantern strings.
- **Signs and adverts.**
  - Shop names in the bitmap font, and vertical blade signs in a made-up neon script.
  - Billboards and holo-ads for the megacorps' products. They get vandalised after a scandal and
    multiply when the corp wins.
  - LED tickers that show the latest headline, flickering broken neon, and wanted posters at high
    Heat.
- **Sky and horizon.**
  - The Pall as a cloud ceiling lit from below by the districts.
  - Searchlights sweeping from the corporate towers, and advertising blimps.
  - A low-detail "distant city" that fills the space between the streamed chunks and the skyline,
    and a skyline with lit windows and blinking beacons.
- **Weather.**
  - Neon reflections on wet ground, and puddles.
  - Rain splashes, drips under awnings, and denser rain tinted by nearby lights.
  - Ground mist, a smog tint, a green tint during acid-rain advisories, and steam that thickens in
    the rain.
- **District kits and vertical layers.**
  - Architecture and props for each of the 11 district types: cranes and containers at the Docks,
    stacks and flares in Industrial, lantern streets in the Old Quarter, skybridges in the Spire,
    shanties under the highways.
  - Highway decks with pillars, barriers and traffic lights.
  - An elevated metro with passing trains.
- **Motion.** Neon flicker, chasing marquee lights, scrolling tickers, windows that switch on and off
  through the evening, TV flicker in windows, blinking beacons, steam, smoke, drones, drifting litter
  and police light bars.
- **Lighting and post effects.**
  - Light pools under lamps and signs, drawn as decals.
  - More lights per draw near the camera, and optional per-pixel lighting for the ground on
    desktop.
  - Height fog and colour grading for each district.
- **Detail settings and budgets.**
  - A "Street detail" setting (low, medium, high) and a "Reflections" toggle.
  - Mesh, sprite and draw budgets that keep phones at 30 FPS.
- **Verification.**
  - Reference views across seeds and districts.
  - Image metrics: clipped pixels, neon saturation, district distinctness and the horizon.
  - Unit tests for facade variety and street dressing density.
  - A refreshed screenshot set.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `retro-rendering`: facade variety, silhouettes, storefronts, sprites, light pools, distant city,
  motion, detail settings; modified rain, signage, skyline and night lighting.
- `city-generation`: building shape grammar and street dressing, derived from the seed without
  changing the city digest.
- `world-environment`: the Pall overhead and the look of each weather type.
- `news-media`: billboards that follow the board, and tickers in the street.

## Impact

- **Code:**
  - `src/render/sr_scene.c` is split: `sr_facade_mesh.c`, `sr_street_mesh.c`, `sr_sky_mesh.c`,
    `sr_sprite.c`.
  - `src/render/sr_mesh.c` gets new texture layers and atlases.
  - `src/render/gl/sr_gl.c` gets the glow pass, shader time, the window logic, height fog and
    grading.
  - A new file `src/world/sr_dressing.c` holds the facade grammar and prop placement. The game
    layer uses the same data for collision.
- **Saves and seeds:** unchanged. Everything is derived from the existing city data with hashes, so
  `SR_GEN_VERSION`, the city digest and the save format stay as they are.
- **Tests:**
  - New unit tests for the facade grammar, dressing, sprites and the layer map.
  - Platform reference views with image metrics; golden images are updated once per phase.
  - A benchmark with the new detail.
- **Performance:** detail is limited by distance and the setting, and checked against the existing
  frame-rate budget on the benchmark route.
- **Download size:** all textures stay procedural. The web size check must still pass.
