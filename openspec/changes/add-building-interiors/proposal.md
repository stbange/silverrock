# Proposal: Every building has an inside

## Why

Only the player's apartment and mission sites could be entered. Shops, diners, bars, clinics and fixers were a menu at
the door, and the towers that make up most of the city were solid blocks. A city you can only look at from the
pavement feels like a stage set.

## What Changes

- **Every door opens.** Walking up to any building and pressing interact goes inside. Shops still keep their hours:
  after closing the door stays shut.
- **Floors that fit the building:**
  - **Shops** (gunshop, electronics, clothing, ripperdoc, fence, grocery): a sales floor, shelves along the walls and
    behind the counter, a keeper behind the till, customers browsing.
  - **Diners and noodle bars:** a counter with stools, tables, people eating.
  - **Bars:** a long neon bar with bottles behind it, stools, tables and a bartender in pink light.
  - **Clinics:** a white waiting room with chairs, a reception desk, wards with beds and nurses.
  - **Capsule hotels:** a reception desk and rows of stacked, glowing pods.
  - **Fixers:** a dim front room with people waiting, and the fixer at a table in the back with muscle beside them.
  - **Matrix and UV booths:** booths with screens along the wall, an attendant.
  - **Residential towers:** a lobby with mailboxes and a lift, a corridor of numbered flat doors.
  - **Offices:** a reception desk and seats, open-plan desks with people working late.
  - **Warehouses:** an open hall of crates and racks.
  - **Police, city hall and media:** a lobby with a front desk, staff-only offices behind a locked door.
- **Service at the counter:** the counter does what the door used to do: eat, shop, the clinic, rent a capsule, the
  fixer's jobs. Desks with nothing to sell answer with a line. Mailboxes, the lift and flat doors can be looked at. A
  booth in a Matrix booth is a public deck (J).
- **People inside:** keepers, customers, residents, workers and officers standing, sitting, leaning, eating, talking
  or on the phone. Staff look at the player when close. When shots are fired everyone ducks or covers their head.
  People and furniture are solid.

## Capabilities

### Modified Capabilities

- `player-life`: every building can be entered; service at the counter.
- `city-generation`: each building's ground floor fits its use.
- `retro-rendering`: civilian floors are furnished, lit and lived in.
- `population-simulation`: the people inside take cover from gunfire.

## Impact

- **Saves:** unchanged (version 5). Floors are generated from the seed; only door and device deltas are saved, as
  before.
- **Simulation:** none. Bots and balance runs eat and shop through the same game calls. Interiors are presentation
  plus where the player walks.
- **Code:**
  - World: `src/world/sr_interior_civ.c` (floor kinds and layouts); `sr_interior.h` (kinds, new props, occupants,
    open floor between cells).
  - Game: `sr_game_site.c` (enter anything, counter contexts, info lines); `sr_game.c` (the door prompt);
    `sr_site.c` (open floor, solid furniture and people, panic).
  - Render: `sr_interior_mesh.c` (new furniture, colours and light by kind, the people).
- **Tests:**
  - `test_civilian_floors` and `test_counters_and_people` (unit).
  - `test_civilian_interiors` (platform, screenshots via `SR_INTERIOR_SHOTS`).
