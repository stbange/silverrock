# Proposal: Help, guidance, navigation and a city map

## Why

Silverrock is a large city with no way to find anything except walking around and reading signs.
New players don't know where food, fixers or the metro are, what to do first, or what a key does.
The future setting makes a navigation system natural: a city map, a route that lights up on the
pavement, and an assistant that offers tips. Not everyone wants that, so every part can be turned off.

## What Changes

- **City map (M, or the map button on touch):**
  - A top-down view of the blocks, district colours, water, highways, buildings, metro lines and
    stations, places by icon, home, the job's target, the route and the player.
  - Pan with the stick, a drag or WASD. Zoom from a street corner to the whole city.
  - Filters: all, food, services, work and travel.
  - Pick a place (E, a click or a tap) to set it as the destination; Q or right click clears it.
  - Time stops while the map is open.
- **Navigation:**
  - Routes run along the walking lines of the pedestrian graph to a building, a metro entrance or
    a marked point.
  - Replanned once a second; arriving shows a message.
  - The active job is followed when no destination is set, and the client once the work is done.
    A job's route that the player clears is not followed again.
  - **HUD:** a diamond on the compass with the distance and the next instruction ("TURN LEFT IN
    40 M"), and a heading-up minimap with streets, places, the route and the destination.
  - **In the world:** glowing chevrons on the walking line along the next 60 m, with a pulse
    running toward the destination, and a shaft of light over the destination that shows above
    the roofs and through the fog.
- **Guidance:**
  - One-time tips triggered by what happens: the first steps outside, the map, a route, something
    to interact with, hunger, fatigue, rent due, no work, Heat, a site, combat, injury, the Matrix,
    long walks (the metro), the apartment terminal, and where help is.
  - Each tip shows for nine seconds, fading in and out, one at a time. The tips seen are kept in
    the settings.
  - The current goal under the compass: the job and where, reporting back, rent due, eat, sleep,
    or find work.
- **Help:** eight pages (controls, getting around, making money, staying alive, Heat and the law,
  stealth and combat, the Matrix, guidance settings), open from the pause screen (H, a tap or the
  gamepad's top button) or with F1 in play. Desktop and touch wording differ.
- **Settings:**
  - `guidance` (tips and goal), `minimap` and `nav_path` (the route in the world), all on by
    default, switched on the help's last page with G, N and P.
  - R shows every tip again.
  - Text settings are now copied with their real field size, which fixes an overflow for long values.

## Capabilities

### New Capabilities

- `navigation`: the city map, routes, turn hints, following the job, the minimap and the route in
  the world.
- `guidance`: tips, the current goal, help pages and their settings.

## Impact

- **Saves:** unchanged (version 5). The destination, route and tips are not saved. A loaded game
  starts with no destination.
- **Simulation:** none. Guidance reads the game and sets a route; it changes nothing that the
  replays and the balance runs see.
- **Code:**
  - Game: `src/game/sr_guide.c/.h`.
  - UI: `src/ui/sr_ui_nav.c` (map, help, HUD parts).
  - Render: `src/render/sr_nav_mesh.c` (chevrons and beacon).
  - Wiring: two screens (MAP, HELP), `sr_platform.c` input, settings fields.
- **Tests:**
  - `tests/unit/test_guide.c` (9 tests).
  - `test_navigation_screens` in the platform tests, with screenshots via `SR_NAV_SHOTS`.
