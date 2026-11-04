# Tasks

## 1. Grid and hosts

- [ ] 1.1 Implement host generation from interiors and device lists with security ratings; verify "Every networked building has a host", "Owner strength raises security", "Stable host layout" and "Every device has a node"
- [ ] 1.2 Implement the global grid as a scaled mirror of districts and arterials; verify "Grid mirrors the map"
- [ ] 1.3 Implement Matrix rendering (barycentric wireframe, glow, data streams) with golden images; verify the frame-rate budget on the benchmark dive

## 2. Access and persona

- [ ] 2.1 Implement access points (rig, booth, port, wireless, data spike) and the deck requirement; verify "No deck, no dive", "Physical port skips the gateway" and "Wireless range"
- [ ] 2.2 Implement persona stats and decks; verify "Better deck improves the persona"
- [ ] 2.3 Implement programs, memory and cooldowns; verify "Memory limit" and "Program cooldown"
- [ ] 2.4 Implement Matrix flight on all input schemes; verify "Touch flight"

## 3. Security

- [ ] 3.1 Implement the breach puzzle generator, solver and input abstraction; verify "Breach difficulty scales", "Credentials skip the breach" and "Breach by touch"
- [ ] 3.2 Implement ICE types, host alert levels and sysops on the shared AI framework; verify "Watcher raises the alert", "Killer ICE dumps the player" and "Black ICE harms the body"
- [ ] 3.3 Implement traces with Heat and security dispatch; verify "Trace completed"
- [ ] 3.4 Implement the meat-cam, body alert and emergency jack-out; verify "Body alert" and "Emergency jack-out"

## 4. Data and backdoors

- [ ] 4.1 Implement paydata types, values, downloads and hand-in; verify "Download time" and "Paydata as a job deliverable"
- [ ] 4.2 Implement worms, backdoors and expiry; verify "Backdoor re-entry" and "Backdoor expires"

## 5. World links

- [ ] 5.1 Implement ownership tokens and device control for doors, cameras, turrets, drones and elevators; verify "Unlocking a door from the Matrix", "Looped camera sees nothing", "Disabled camera draws a check", "Viewing a feed", "Retargeted turret" and "Calling an elevator"
- [ ] 5.2 Implement alarms, comms and fake orders; verify "False alarm diversion" and "Cancelling an alarm"
- [ ] 5.3 Implement building lights and district blackouts with citizen reactions; verify "Lights out helps stealth" and "District blackout"
- [ ] 5.4 Implement records (warrants, credentials, accounts) and stolen-information effects; verify "Erasing records reduces Heat", "Forged credentials grant access" and "Stolen data reveals briefing details"
- [ ] 5.5 Implement Netsight and quick actions; verify "Netsight shows devices", "Quick action on an owned device" and "No quick action without ownership"
- [ ] 5.6 Implement countermeasures and log erasure; verify "Tampering detected and reset" and "Erased logs prevent attribution"

## 6. Jobs and persistence

- [ ] 6.1 Add the Matrix approach planner to job validation and the netrunner autoplayer strategy; verify jobs validated with a Matrix approach are completed by the netrunner bot
- [ ] 6.2 Extend the save format with ownership tokens, backdoors and host alert state plus migration; verify round-trip and migration tests

## 7. Integration checks

- [ ] 7.1 Play the exit demo — prepare a run in the Matrix, then execute it in meatspace — on all targets including touch on a phone; record results in `tests/manual-verification.md`
- [ ] 7.2 Add the netrunner strategy to the nightly balance report; verify Matrix-heavy play stays within the balance bands
- [ ] 7.3 Confirm every scenario of this change is covered in the traceability report

## Workflow follow-up

- Archive the change with `openspec archive add-matrix` once CI is green on all targets.
