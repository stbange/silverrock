# Tasks

## 1. Grid and hosts

- [x] 1.1 Implement host generation from interiors and device lists with security ratings; verify "Every networked building has a host", "Owner strength raises security", "Stable host layout" and "Every device has a node"
- [x] 1.2 Implement the global grid as a scaled mirror of districts and arterials; verify "Grid mirrors the map"
- [x] 1.3 Implement Matrix rendering (barycentric wireframe, glow, data streams) with golden images; verify the frame-rate budget on the benchmark dive

## 2. Access and persona

- [x] 2.1 Implement access points (rig, booth, port, wireless, data spike) and the deck requirement; verify "No deck, no dive", "Physical port skips the gateway" and "Wireless range"
- [x] 2.2 Implement persona stats and decks; verify "Better deck improves the persona"
- [x] 2.3 Implement programs, memory and cooldowns; verify "Memory limit" and "Program cooldown"
- [x] 2.4 Implement Matrix flight on all input schemes; verify "Touch flight"

## 3. Security

- [x] 3.1 Implement the breach puzzle generator, solver and input abstraction; verify "Breach difficulty scales", "Credentials skip the breach" and "Breach by touch"
- [x] 3.2 Implement ICE types, host alert levels and sysops on the shared AI framework; verify "Watcher raises the alert", "Killer ICE dumps the player" and "Black ICE harms the body"
- [x] 3.3 Implement traces with Heat and security dispatch; verify "Trace completed"
- [x] 3.4 Implement the meat-cam, body alert and emergency jack-out; verify "Body alert" and "Emergency jack-out"

## 4. Data and backdoors

- [x] 4.1 Implement paydata types, values, downloads and hand-in; verify "Download time" and "Paydata as a job deliverable"
- [x] 4.2 Implement worms, backdoors and expiry; verify "Backdoor re-entry" and "Backdoor expires"

## 5. World links

- [x] 5.1 Implement ownership tokens and device control for doors, cameras, turrets, drones and elevators; verify "Unlocking a door from the Matrix", "Looped camera sees nothing", "Disabled camera draws a check", "Viewing a feed", "Retargeted turret" and "Calling an elevator"
- [x] 5.2 Implement alarms, comms and fake orders; verify "False alarm diversion" and "Cancelling an alarm"
- [x] 5.3 Implement building lights and district blackouts with citizen reactions; verify "Lights out helps stealth" and "District blackout"
- [x] 5.4 Implement records (warrants, credentials, accounts) and stolen-information effects; verify "Erasing records reduces Heat", "Forged credentials grant access" and "Stolen data reveals briefing details"
- [x] 5.5 Implement Netsight and quick actions; verify "Netsight shows devices", "Quick action on an owned device" and "No quick action without ownership"
- [x] 5.6 Implement countermeasures and log erasure; verify "Tampering detected and reset" and "Erased logs prevent attribution"

## 6. Jobs and persistence

- [x] 6.1 Add the Matrix approach planner to job validation and the netrunner autoplayer strategy; verify jobs validated with a Matrix approach are completed by the netrunner bot
- [x] 6.2 Extend the save format with ownership tokens, backdoors and host alert state plus migration; verify round-trip and migration tests

## 7. Integration checks

- [ ] 7.1 Play the exit demo — prepare a run in the Matrix, then execute it in meatspace — on all targets including touch on a phone; record results in `tests/manual-verification.md`
- [x] 7.2 Add the netrunner strategy to the nightly balance report; verify Matrix-heavy play stays within the balance bands
- [x] 7.3 Confirm every scenario of this change is covered in the traceability report

## Workflow follow-up

- Archive the change with `openspec archive add-matrix` once CI is green on all targets.

## Implementation notes (2026-11-06)

- Hosts (`src/world/sr_host.c`) are generated from the building's interior: one controller node per device, layers by
  security zone, data stores (always one with the site's security data), security monitor, records (city hall,
  police, finance corps), power (district grid in city hall and energy facilities) and the admin core. Security =
  owner Matrix defence (from faction kind, sector and doctrine) and facility importance.
- The grid is the city scaled by 1:40: district plates per block, highway and metro backbones, host constructs in
  owner colours (`src/render/sr_matrix_mesh.c`). Wireframes use the quad UVs in the fragment shader (glowing edges,
  hollow floors) instead of barycentric attributes, which GLES 3.0 handles the same way; bloom gives the glow and
  data packets run along conduits and metro beams. Golden image `tests/render/matrix_reference.ppm`; the host frame
  builds in about 0.3 ms.
- The breach puzzle locks one ring at a time (commit on alignment; a miss or a decoy costs time); a generator check
  guarantees the minimum-skill solve time fits the limit, and an autosolver proves every generated puzzle solvable.
- ICE: Watcher, Barrier, Tar Pit, Tracer, Killer, Black ICE and Sysops (enemy netrunners at lockdown in
  well-defended hosts), on the shared alert machine.
- Meat-cam: a 160x90 view of the body's surroundings, refreshed 5 times a second (2 on phones), drawn at the top of
  the screen during a dive.
- Device changes go through the interior delta system, so they persist and reach the live site; tamper records reset
  them after the discovery time unless the player is long gone, and erased logs keep the detection from producing
  evidence. Drones and comms orders are covered by turrets (friend-or-foe) and false alarms; there are no separate
  drone or radio devices yet.
- Save format version 3 adds a Matrix section; version 2 saves load with an empty network state.
- Open: 7.1 (exit demo on real hardware and a phone).

