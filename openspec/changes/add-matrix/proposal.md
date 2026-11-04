# Proposal: The Matrix (M4)

## Why

Pillar 4 — "two worlds, one job" — is what sets Silverrock apart from a plain shooter. The Matrix is a separate,
navigable 3D space that mirrors the city, and what the player does there changes the physical world: doors, cameras,
turrets, lights, records, money and information. Jobs from M3 gain their fourth approach. Roadmap milestone:
**M4 The Matrix**.

## What Changes

- Access points: home rig, public booths, physical ports, wireless with a deck in range, the data-spike gadget.
- The global grid as a compressed 3D mirror of the city; hosts for every networked building, generated
  deterministically from the building and its owner.
- Persona stats, programs, host nodes, the ring breach puzzle, ICE types and host alert levels, traces, Black ICE
  biofeedback, body vulnerability with the meat-cam and emergency jack-out, paydata, backdoors, flight controls on all
  input schemes.
- Real-world links: device controllers for doors, cameras, turrets, drones, alarms, comms, lights, power and elevators;
  records (warrants, credentials, accounts); intel from stolen data; Netsight quick actions in meatspace;
  countermeasures and log erasure.
- Matrix approach planner for job validation.
- Matrix rendering style (wireframe via barycentric coordinates, additive glow, data streams).

## Capabilities

### New Capabilities

- `matrix-navigation`: entering and moving through the Matrix — access, the grid, hosts, persona, programs, breaching,
  ICE, traces, the body left behind, data theft and backdoors.
- `matrix-world-link`: how Matrix actions change the physical world and how the player uses them from meatspace.

### Modified Capabilities

None. The job system already requires two approaches among stealth, social, Matrix and force; this change adds the
Matrix planner as an implementation detail.

## Out of scope

The Deep and the Ghost (M5/M6 content), the Board screen (M5), share-price manipulation through planted data (M5).

## Impact

- New `matrix/` module, Matrix render materials, breach puzzle UI for all input schemes.
- Host data per networked building generated with chunk detail; save format sections for owned devices, backdoors and
  Matrix alert state.
- New autoplayer strategy "netrunner" and Matrix scenario scripts.
