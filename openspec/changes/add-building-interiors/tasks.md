# Tasks: Every building has an inside

## 1. Floors

- [x] 1.1 Interior kinds and `sr_interior_kind` from the place of interest, the district and a hash (spec: "Floors fit the building")
- [x] 1.2 Open floor between cells: walls, collision, sight, doors and reachability
- [x] 1.3 Layouts for shops, diners, bars, clinics, capsules, fixers, booths, homes, offices, warehouses and civic lobbies
- [x] 1.4 New props (counter, table, seat, flat door, mailbox, lift, pod, booth) and solid extents
- [x] 1.5 Occupants: places, poses, roles

## 2. Game

- [x] 2.1 Every building enterable; mission and job sites keep their layout; the approach cache knows which (spec: "Every building has an inside")
- [x] 2.2 The door prompt: go inside, closed after hours
- [x] 2.3 Counter contexts by kind; talk and look lines; booths as public decks (spec: "Service at the counter")
- [x] 2.4 People are solid; shots inside make them take cover (spec: "People inside take cover")

## 3. Look

- [x] 3.1 Furniture meshes; wall, floor and light colours by kind; a street window
- [x] 3.2 The people drawn with the animation clips, staff looking at the player (spec: "Civilian floors are furnished and lived in")

## 4. Verification

- [x] 4.1 Unit tests: every floor in a city, every counter, looks, booths, panic, meshes, closed doors
- [x] 4.2 Platform test with screenshots of every kind; docs
- [ ] 4.3 A manual look at the floors on a phone (scale of the furniture, readability in the dark ones)
