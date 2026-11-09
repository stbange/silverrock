# Design: Help, guidance, navigation and a city map

## Routes

- **Planning:** `sr_route_plan` uses the pedestrian graph (`sr_nav_path`). It starts at the
  nearest node to the player and ends at the building's door node, or the nearest node to a point
  or a metro entrance.
- **Simplification:** points on a straight line are dropped, so a route is its corners. It holds at
  most 96 points, which covers a walk across the city.
- **Inside a site:** the graph does not apply, so the route is a straight line to the target.
- **Turn hints:** walk the corners, skip bends straighter than about 30° and the short step from a
  door onto the walking line. Name the first turn, rounded to 10 m, or "CONTINUE" or
  "DESTINATION AHEAD".
- **Updates:** a replan every 60 steps keeps the route from the player without per-frame cost.
- **Arrival:** within 8 m.
- **Following the job:** when there is no destination, or the current one is automatic, the route
  follows the active job's building: the target, then the client once executed.
  - Clearing an automatic route remembers the job (`declined_job`).
  - Arriving remembers the building (`done_building`), so it does not route straight back.

## Tips

- **Triggers:** checked every 30 steps; a tip can only be queued once.
- **Queue:** eight entries. The first queued tip shows when the last one has gone (after 9 s).
- **Seen tips:** a bit per tip, stored as hex in `settings.tips_seen`, so they survive new games,
  loads and restarts but never touch the save.
- **Timing:** tip times use the presentation clock (`sr_game_fx_now`), not game time, so skipping
  time does not flash through them.
- **Guidance off:** no tips, no queue, no goal. The active job still shows plainly at the bottom
  left, so no information is lost.

## The map

- **Screen:** `SR_SCREEN_MAP`. Opening it pauses the clock and closing it resumes, like the
  terminal.
- **View:** a centre, the metres per screen height (150 m to the city's size × 1.1) and a filter.
  The platform sets the screen's aspect each frame, so picks match what is drawn.
- **Detail by zoom:**
  - Buildings show below 1400 m.
  - District names show above 1200 m.
  - Above 1600 m, with every place shown, places are coloured dots: amber food, cyan services,
    red work, grey other. Icons would cover each other there.
- **Picks:** the nearest place of the filter within 3.5 % of the span, else a metro entrance, else
  the point itself.
- **Touch:** five buttons (zoom in, zoom out, filter, clear, close). A drag pans, and a tap that
  did not drag picks.

## In the world

- **Chevrons:**
  - Flat ">" shapes 14 cm above the ground, every 2.5 m from 3 m to 60 m along the route, both
    windings.
  - Drawn into the additive glow mesh (`crowd_fx`), so they light the wet street and fade in the fog.
  - A pulse moves along them at 8 m/s, and they thin out toward 60 m.
- **Beacon:** a shaft of eight stacked quads, 176 m tall, turned to face the camera and a few
  pixels wide at any distance. It is drawn in the halo pass, which has no fog, so it shows above
  the roofs from across the district. The depth test still hides it behind towers.
- **Off:** with `nav_path` off, neither is built.

## Layers

- `game/sr_guide` uses world (nav, city) and sim (jobs).
- `ui/sr_ui_nav` uses game.
- `render/sr_nav_mesh` uses game for the route type.
- The platform maps keys, taps and buttons to game calls (`sr_guide_toggle`, `sr_map_zoom`,
  `sr_map_pick`, `sr_help_open`) and saves the settings when they change.
