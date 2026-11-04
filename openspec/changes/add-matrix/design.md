# Design: The Matrix

## Context

Builds on generated buildings and interiors (M1), devices and guards in mission sites (M3), and the Heat system (M3).
See proposal.md for motivation and docs/concept/01-game-design.md §15 for the player-facing design.

## Goals / Non-Goals

**Goals:**
- One consistent mapping between physical devices and Matrix nodes, so every Matrix action has a predictable physical
  effect.
- A Matrix that is fun to fly through on mouse, gamepad and touch.
- No extra download weight: the Matrix is generated and drawn with the same engine.

**Non-Goals:**
- Multiplayer or asynchronous netrunning.
- Free-form programming by the player.

## Decisions

### D1 — Hosts derived from interiors
A host is generated from the building's interior room graph and device list: every device becomes a device-controller
node; nodes are grouped into layers by security zone; gateways, junctions, data stores, a security monitor, records (for
government, police and finance buildings) and an admin core are added by building type. Security rating =
f(owner NetSec, facility importance). Generation is a pure function of `(seed, building id, owner state)`; owner
changes in M5 regenerate the security parameters, not the topology.

### D2 — Grid as a scaled mirror
The global grid uses the macro district polygons and arterial graph scaled by a fixed factor, extruded as plates and
backbone beams. Host constructs are placed at their buildings' scaled positions with shapes chosen by owner and size.
This keeps orientation intuitive: if the clinic is east of the metro station in the city, its host is east of the
station's backbone node in the grid.

### D3 — Ownership tokens
Controlling a device node, or planting a worm on its controller, gives the player an ownership token for that device
with an expiry. Netsight quick actions check tokens; countermeasures revoke them. Tokens are saved.

### D4 — ICE uses the shared AI framework
ICE agents use the same alert state machine as guards, with 3D steering along conduits. Tracers run a pursuit timer;
when it completes, a trace event with the body's position is sent to the owner's jurisdiction.

### D5 — Breach puzzle generator
Ring count, rotation speeds and decoy gaps derive from the node's security rating and the node's RNG stream. Inputs are
abstracted to "rotate ring", "select ring" and "commit", so mouse, gamepad and touch play the same puzzle. A solver
verifies every generated puzzle is solvable within the time limit at the minimum skill level.

### D6 — Time keeps running
Meatspace keeps simulating at normal rate during a dive. The meat-cam renders the body's surroundings into a small
low-resolution target at 5 Hz (2 Hz on mobile) to save GPU time. A perception check around the body raises the body
alert.

### D7 — Rendering
Wireframes are drawn with barycentric coordinates in the fragment shader (GLES 3.0 has no line polygon mode), plus
additive glow and instanced particles for data streams. The Matrix uses a slightly higher internal resolution than
meatspace for line clarity, within the same frame budget.

## Risks / Trade-offs

- [The Matrix feels like a menu] → Movement, stealth and combat in 3D; breaching is in-world, not a separate screen.
- [Switching worlds is disorienting] → The grid mirrors the city map; landmarks keep their corp colours and shapes.
- [Too powerful compared to other approaches] → Traces, Black ICE, body vulnerability, backdoor expiry and
  countermeasures keep risk high; balance report includes the netrunner strategy.
- [Meat-cam costs too much on phones] → Low update rate, tiny resolution, can be disabled in favour of audio alerts.
