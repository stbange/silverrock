# Tasks: Help, guidance, navigation and a city map

## 1. Routes and guidance (game)

- [x] 1.1 `sr_guide.c/.h`:
  - Destinations (building, point, station).
  - Route planning on the pedestrian graph, corners only, straight inside sites.
  - Distance and turn hints.
- [x] 1.2 Following the active job, declined jobs, arrival and replanning.
- [x] 1.3 Tips: triggers, queue, fading text, seen bits in `settings.tips_seen`, reset.
- [x] 1.4 The current goal.
- [x] 1.5 Settings `guidance`, `minimap` and `nav_path`, plus `sr_guide_toggle`. Fix the text
  setting overflow (per-field size).
- [x] 1.6 Hooks: new game and load clear the route; the game step runs guidance.

## 2. City map and help (game, UI)

- [x] 2.1 Map screen:
  - View and transforms, zoom limits, filters, picks.
  - Pause while open; input by stick, buttons, keys.
- [x] 2.2 Map drawing:
  - Blocks, districts, water, highways, buildings, metro.
  - Icons, or dots far out.
  - Home, job, route, destination, player, cursor; touch buttons.
- [x] 2.3 Help: eight pages for desktop and touch, wrapped text, a settings page with states,
  opened from pause (H, tap, gamepad) or F1.

## 3. HUD and world

- [x] 3.1 Compass diamond with the distance and the next instruction.
- [x] 3.2 Heading-up minimap; the goal under the compass; the tip panel.
- [x] 3.3 Chevrons on the walking line, with a pulse, within 60 m. A beacon over the destination
  in the halo pass.

## 4. Platform

- [x] 4.1 Map:
  - Mouse click picks, right click clears, the wheel zooms.
  - Touch drag pans, a tap picks or presses a button.
- [x] 4.2 Help:
  - Keys G, N, P, R, up and down; taps on topics, settings and the title.
  - Gamepad north, west, shoulders, d-pad and east.
- [x] 4.3 The settings file is saved when a guidance setting changes.

## 5. Verification

- [x] 5.1 Unit tests (`tests/unit/test_guide.c`): routes, hints, job following, tips, goal, map,
  help, settings, UI, route in the world.
- [x] 5.2 Platform test `test_navigation_screens`:
  - Map, help and pause input by keys, mouse, touch and gamepad.
  - Screenshots via `SR_NAV_SHOTS`.
- [x] 5.3 Docs and screenshots (`docs/screenshots/nav/`).
- [ ] 5.4 A manual look on a phone: the map's buttons and drag, and the tip panel's size.
