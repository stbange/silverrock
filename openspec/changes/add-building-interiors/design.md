# Design: Every building has an inside

## Which floor

- **Interior kind:** `sr_interior_kind` picks it from the building's place of interest, and for plain buildings from
  the district and a hash of the building:
  - Industrial districts and docks: mostly warehouses.
  - Spire, civic and campus: mostly offices.
  - Commercial and entertainment: offices and homes.
  - Everywhere else: mostly homes.
- **Layouts:**
  - Mission sites and buildings with an active site job keep the mission layout (security chain, routes, guards).
  - The player's home keeps the apartment.
  - The approach cache records which layout it holds, so a building that gains a site job is regenerated.
- **Grid:** floors are small grids of 6 m cells:
  - 2-3 × 2 for shops, diners, booths and fixers.
  - 3 × 2 for bars, clinics and civic buildings.
  - Up to 5 × 3 for homes, offices and warehouses.
  - The front entry is always the middle of the street row, so the player walks straight in.

## Open floor

- **Problem:** cells used to be rooms with walls and doors everywhere.
- **Bits:** a connection can now be open floor (doors bits 4 and 8):
  - No wall is built.
  - Collision and sight ignore the line.
  - No door device or leaf exists.
  - Reachability treats it as passable.
- **Use:**
  - A shop, bar or warehouse is one open space.
  - Wards, back rooms, the residential corridor and civic offices are behind real doors.
  - Civic offices are locked: staff only.

## Furniture and people

- **New props:**
  - Counter, with four looks: shop, bar, desk, table.
  - Table, seat, flat door, mailbox, lift, sleeping pod, booth.
  - Counters, tables, pods, the lift and mailboxes are solid, with extents along their facing.
  - Seats and booths are walk-in.
- **Kept clear:** layouts keep the line from the front door to the counter clear, and put doors to back rooms beside,
  not behind, the counter.
- **Occupants:** up to ten per floor, each with a position, facing, pose (stand, sit, lean, eat, talk, phone) and role
  (keeper, patron, resident, worker, officer).
  - Looks come from the population generator with a role that fits (vendors, the fixer, cops or gang muscle, a mix of
    customers).
  - They are posed with the existing clips, are solid (0.25 m), and staff turn their heads to a player within 4 m.
- **Panic:** when the player shoots inside (or an alarm sounds) the floor panics. Seated people cower, the rest cover
  their heads.

## Service

- **Door:** the door prompt is "GO INSIDE" for every building except the home, job targets, the metro, and shops
  outside their hours.
- **Counter context by kind:**
  - Shop, or grocery (eat).
  - Diner and bar (eat).
  - Clinic, capsule (sleep), fixer (jobs).
  - Anything else (talk).
- **Handlers:** the existing handlers serve with the building the player is inside.
- **Talk and look:** pick a line from small tables, varying by building and every ten minutes:
  - Desks have one table of lines for police and one for everyone else.
  - Booth attendants explain the booth.
  - Flat doors say what can be heard behind them.
- **Booths:** standing at a booth in a Matrix booth gives booth access for jacking in.

## Look

- **Walls and the ceiling light by kind:** white clinics and offices, pink-lit bars, an amber fixer's room, a blue booth
  hall, yellowish residential lobbies.
- **Room lights:** take the kind's colour, dimmer in bars and with the fixer.
- **Street window:** the street wall of a civilian floor has a dim window glowing from outside.
