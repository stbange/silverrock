# Design: Animation that brings the city to life

## Context

Characters are built by `sr_build_character_walk` in `src/render/sr_scene.c`. It uses six rigid
boxes: two legs, a torso, two arms and a head, plus hair, markers and an umbrella. Each leg and
arm turns around one pivot by `sr_walk_swing(walk)`. Beyond `SR_CROWD_NEAR_M` (28 m), the same
six parts are instanced. Each instance carries 16 floats (position, yaw, swing, width, height,
outfit and skin), and the vertex shader turns parts 0–4 by the part id held in the vertex `pad`
byte.

The crowd simulation (`src/sim/sr_crowd.c`) runs up to 400 agents with six states and a constant
cruise speed. Vehicles move on straight lanes. Guards (`SrGuard` in `src/game/sr_site.h`) carry
`walk`, the `SrEnemy` alert state and the dead, ko, surrendered and restrained flags. The player
is a camera in `move_player` (`src/game/sr_game.c`).

The art direction (§16.3 of the concept) asks for rigid low-poly parts in the style of late-90s
games. That rules out skinning and points to a small hierarchy of rigid parts, posed by hand-made
keyframes.

## Goals / Non-Goals

**Goals**

- People read as people at 5 m and at 30 m: gait, posture, idle life, rain, fear.
- The player feels their own body: hands, weapon, recoil, light, camera motion.
- Combat is readable: where fire comes from, what was hit, who is down.
- Cars and flyers move like vehicles.
- Everything stays deterministic and asset-free, with saves untouched.

**Non-Goals**

- Skinned meshes, inverse kinematics on uneven ground (the ground is flat), and ragdoll physics.
- Faces that animate (the faces are 8 × 8 texels).
- Sound. There is no audio system yet. The foot-plant and shot events this change produces are
  where sound will hook in later.
- Third-person play and cutscenes.

## Decisions

### D1 — An 11-part rigid skeleton (N1)

The parts are the pelvis (the root), the chest, the head, and for each side the thigh, shin,
upper arm and forearm. Each part is a box with a pivot in its parent's space:

```
pelvis ─┬─ chest ─┬─ head
        │         ├─ upper arm L ── forearm L (+ hand prop)
        │         └─ upper arm R ── forearm R (+ hand prop)
        ├─ thigh L ── shin L
        └─ thigh R ── shin R
```

Joint angles:

| Joint | Axes |
|---|---|
| Pelvis | Height offset, pitch, roll and yaw (the stride twist) |
| Chest | Pitch, yaw and roll |
| Head | Pitch and yaw |
| Hip and shoulder | Pitch and roll |
| Knee and elbow | Pitch only |

A pose is 26 floats (`SrPose`): 25 joint channels, including a root pitch, roll and height used for falls and lying poses, plus one channel for the glow of a held cigarette or phone. Proportions come from the appearance, as they do today:
`tall` and `bw` scale pivots and boxes, so body types differ in limb length as well as width.

Hair, markers, optics and hats attach to the head. Shoulder plates attach to the chest. Hand
props attach to the forearm. The umbrella and long coats are covered in D6.

Option rejected: keeping six parts and adding a bob. Without knees and elbows the walk cannot
plant its feet, and sitting, crouching and aiming cannot be posed.

### D2 — Clips: keyframe tables in code (N1, N4)

A clip is a short table of poses: 2–8 keys, each with a time in [0, 1) and the joint angles that
differ from the rest pose. Clips are sampled with a Catmull-Rom spline, and looping clips wrap.
A clip has a driver:

- **Distance clips** (walk, hurry, run, crouch-walk, limp): the phase is metres walked divided by
  the stride length. Stride length = leg length × a factor per clip. The feet then plant by
  construction (D3).
- **Time clips** (idle, activities, gestures, reactions): the phase is game time divided by the
  clip length, offset by the agent key so that neighbours are out of step.
- **One-shot clips** (flinch, stagger, fall, slump, reload, swing, throw, reach): the phase runs
  from 0 to 1 once, then holds the last key or returns to the base clip.

Each clip has a base set and per-style variants. A gait seed from the agent key changes stride,
arm swing, bounce, lean and tempo by up to ±15 %. Role adds posture: guards and police stand
upright, Barrens residents stoop, suits walk stiffer, and kids bounce. Mood adds lean: rain hunches
the shoulders, fear hurries the step.

There are 52 clips, a few kilobytes of tables in all. Standing clips are put on the ground automatically: the pelvis drops until the lower ankle touches the floor, so crouching, sitting and kneeling need no hand-tuned heights.

### D3 — Locomotion, turns and starts (N2, N3)

- **Speed picks the gait.** Below 0.3 m/s the person stands. Then walk, hurry above 1.7 m/s, run
  above 2.6 m/s. Neighbouring gaits blend by speed, so a person who speeds up changes gait
  smoothly.
- **Feet plant.** Each foot follows a planned path (flat on the ground in stance, an arc in swing), and a two-bone solve in the side plane finds the hip and knee. A foot on the ground moves backward at the ground speed within 10 % (tested; in practice it stays put).
- **Hips move.** Walking rides a smooth curve that falls just after each footfall, kept low enough for both legs to reach; running rides as high as the legs allow, with a flight phase.
- **Turns are smooth.** The rendered heading follows the sim direction at up to 360°/s (540°/s
  when running), and the body leans into the turn by up to 8°.
- **Starts and stops take time.** The sim accelerates agents to cruise speed over about 0.4 s
  and brakes them over about 0.3 s. This is a sim change. It is deterministic, and crowds are not
  saved. The collision and pathing logic is unchanged.

### D4 — The animation cache and blending (all)

`SrAnimCache` is a render-side hash table of 512 slots, keyed by the agent or guard key. Each slot
holds:

- the current and previous clip, and the blend timer;
- the rendered heading;
- the start time of any one-shot clip;
- the look-at target;
- the last foot plant, for splashes.

Entries not touched for 2 s are dropped. Cross-fades last 0.15 s (gait changes) to 0.3 s
(activities), with the pose blended per joint. One-shot clips layer on top of the base clip, with
a weight per part group: a flinch moves the chest and head, a reload moves the arms.

Because the cache is presentation only, a missing entry only costs a blend. A fresh game, a load
or a teleport starts every person in their correct base pose, with no pop beyond what the sim
already avoids.

### D5 — Life on the street: activities (N4)

- **Standing gets life.** When an agent stands (idle, shelter, gawk, waiting at a crossing), the
  sim picks an activity from its key, the district and the street props within 3 m. It reuses the
  prop table from `sr_dressing_block`.

  | Prop or place | Activities |
  |---|---|
  | Bench | Sit, sit with a phone, sleep (Barrens and Undercity) |
  | Stall, food cart | Eat, wait to order, talk to the vendor |
  | Vending machine | Buy: reach, wait, take |
  | Wall, shopfront | Lean, smoke, phone call, look in the window |
  | Crossing | Wait, impatient weight shift, look both ways |
  | Club marquee | Queue, dance, smoke |
  | Ticker, holo-ad | Watch, point |
  | Barrel fire | Warm hands |
  | Anywhere | Idle, phone, smoke, stretch, look around |

- **Groups.** Two or three agents who stop within 2 m may form a group, from the key of the first.
  They face each other and take turns gesturing: one speaks while the others nod.
- **Activities last.** Each takes 6–40 s, then the agent walks on. The sim keeps the activity id
  and timer. The renderer picks the clip. The activity is chosen once the agent has come to rest, so
  the props within reach are the ones it actually stands by.
- **Districts weigh the choices.** The Spire has fewer smokers and more phones. The Barrens have
  sleepers and beggars. Entertainment has dancers. The Docks and Industrial districts have workers
  on a smoke break in overalls.
- **The weights are tunable and tested.** On a busy street at night, at least half of the standing
  people within 20 m do something other than the plain idle (tested).

### D6 — Rain and accessories (N6, N7)

- **Umbrellas are held.** The right upper arm raises, the forearm points up, and the umbrella rod
  attaches to the hand. The canopy becomes eight low-poly segments and tilts into the wind from
  `sr_wind` by up to 20°. A sheltering agent closes it: a short one-shot, after which it hangs by
  the side.
- **People without umbrellas react to rain.** They hunch the chest, lower the head and hurry
  (speed ×1.25). By key, they raise a hood, hold a jacket or bag over the head, or shake off water
  at shelter. Rain weaker than drizzle doesn't trigger any of this.
- **Feet splash.** Each foot plant on wet ground within 15 m leaves a splash sprite. This reuses
  the splash sprite from atmosphere phase 3 and is capped per frame.
- **Accessories from `appearance.accessory`.** Briefcase, shopping bag or bags, takeaway box,
  cigarette (a glowing tip and a puff of smoke on exhale), phone (an emissive screen that lights
  the face and shows on the instanced far crowd as one pixel), and a backpack.
- **Long coats.** The coat outfit gets a coat-tail panel hung from the pelvis. It follows the
  thighs at half their angle and flutters with the wind when standing.

### D7 — Reactions and police (N5)

- **Fleeing.** A run clip with an arm over the head, looking back every 2–4 s. The flee speed
  rises from 4.0 to 4.5 m/s, and the start is a one-shot recoil.
- **Cower.** Crouched with the arms over the head, used for cover near gunfire.
- **Gawkers.** Turn their heads to the event. By key, they point, hold up a phone to film (the
  screen glows), or call someone.
- **Police.**
  - Patrol walk with the hands on the belt.
  - Wave-on and stop gestures at a stop or questioning.
  - At Heat 3 or more, the weapon is held low and ready.
  - At an armed response, the aim pose.

### D8 — Vehicles (N8)

- **Body.** The car becomes a body, a cabin and four wheels. The wheels turn by distance / radius.
  A two-tone hub shows the spin at the low resolution.
- **Suspension.** The body pitches back on acceleration and forward on braking, and rolls in
  turns. A small bob comes from a per-car seed.
- **Driving.** The sim gives cars acceleration (3 m/s²) and braking (6 m/s²). They spawn at speed
  only outside the view. In view, they pull away from a stop.
- **Turns.** At a junction, a car turns left or right with a probability seeded by its key and the
  junction. It follows a quarter circle of the lane's radius, and the indicators blink for 2 s
  before and during the turn. New lanes are not needed: a turn ends on the cross lane's existing
  offset.
- **Lights.** Brake lights fade over 0.1 s. Headlight beams are drawn as cones in the glow pass,
  fogged, two per car within 60 m, and capped like the lamp cones. Wipers sweep in rain, as one
  bar on the windscreen.
- **Spray.** Rear wheels throw spray sprites on wet roads above 6 m/s.
- **Patrol cars** keep their light bar. They swerve to the kerb when stopping a suspect.

### D9 — Aerodynes (N9)

- **Flight.** Bank into the curve of the sky lane: roll ∝ heading change × speed, up to 25°.
  Pitch on climbs and descents. A hover bob of 0.2 m at 0.5 Hz.
- **Thrusters.** Four glow sprites underneath, pulsing with the speed.
- **Landing.** One aerodyne per lane in 20 lands on a helipad from atmosphere phase 5 within
  200 m of the player. It descends along an S-curve over 8 s, waits 20–60 s with its beacon on,
  then lifts off. The sim adds the `landing` phase to the air traffic schedule. It stays
  deterministic from the seed and the clock.

### D10 — The first-person body (N11)

- **View model.**
  - Drawn after the world in its own pass over a cleared depth buffer, so it never clips into
    walls. Lit by the same lights at the camera position.
  - One model for each weapon category: pistol, SMG, shotgun, rifle, sniper, heavy, melee blade,
    melee blunt, taser and unarmed. Each is a few boxes in the weapon's tint, held by two
    low-poly hands. Gloves and chrome arms follow the player's outfit and cyberware.
- **View model motion.**
  - Sway follows the look speed, with a lag of 0.08 s.
  - Bob follows the walk phase. Sprint lowers and tilts the weapon. Crouch lowers it.
  - Firing kicks the weapon back and up, scaled by recoil. The kick recovers over 0.12–0.3 s.
  - Reload (lower, swap, raise), melee light and heavy swings, a block pose, a gadget throw.
  - Reaching for doors, terminals, items and the climb.
  - With no weapon equipped, the hands appear only for interactions.
- **Muzzle flash.**
  - A flash sprite at the muzzle.
  - A warm point light for about two frames, 1.5 m ahead of the muzzle, so the room lights up and
    the hands do not blow out.
  - Shell sprites for weapons that eject.
- **Camera.**
  - Head bob: vertical 3 cm and lateral 1.5 cm at walk, double at sprint, half in a crouch. It
    can be turned off.
  - Landing dip proportional to the fall speed, up to 8 cm.
  - Crouch eases over 0.15 s.
  - Recoil pitch kick by weapon category, fully recovered within 0.4 s. The kick is in the view
    only: the aim the game shoots from does not move.
  - Damage flinch: 1.5° away from the source, and a red edge on the side of the hit.
  - Shake for explosions and heavy landings, scaled by the camera-shake setting (off, low, full).
- **Settings.** `head_bob` (default on) and `camera_shake` (default full), saved in the settings
  file. When off, neither the aim nor the gameplay changes.

### D11 — Combat animation (N10)

- **Guards hold their archetype's weapon** in a rifle or pistol grip.
- **Stance by alert state:**

  | Alert | Stance |
  |---|---|
  | Unaware | Patrol walk, weapon slung. At waypoints, look around for 2–4 s. |
  | Suspicious | Stop, turn the head and chest toward the noise, then a slow step. |
  | Searching | Weapon raised, a careful walk, sweeping the head. |
  | Combat | Aim pose aimed at the player, strafe walk, crouch near cover props, peek out to fire. |

- **Hits.**
  - A flinch one-shot whose direction comes from the shot: the chest turns away from it and the
    head snaps.
  - A heavy hit (damage ≥ 35 % of max health) gives a stagger step.
  - A headshot snaps the head back.
- **Falls.**
  - **Death:** a topple of 0.6–0.9 s about the foot pivot, away from the last hit. The limbs
    relax toward a lying pose chosen by key (on the back, on the front, on the side).
  - **Knockout:** the knees buckle, the body slumps forward and lies on its side. The lying pose
    is kept for the rest of the visit. A body found after a load lies in its final pose, with no
    replay.
  - The blood decal grows over 2 s.
- **Status poses.** Surrender: the hands go up and the weapon drops. Restrained: kneeling, hands
  behind the back. Flashbang stun: the forearms cover the eyes and the guard staggers. Waking from
  a knockout: a get-up one-shot.
- **Drones** tilt toward their direction of travel, bob, and spin up their rotors (a blurred disc
  sprite).
- **Enemy fire.** A muzzle flash sprite and a short light at the guard's weapon, plus a tracer
  sprite for every third round, so the player can see where fire comes from even in the dark.
- **Presentation timers.** `SrGuard` gains `hit_time`, `hit_dir` and `down_time`. These are
  presentation timers, set by `sr_site_shoot` and the takedown functions. They are not saved:
  a loaded guard who is down starts in its final pose.

### D12 — World motion (N12)

- **Doors.** Interior doors swing about the hinge over 0.3 s from `sr_site_door_open`. The
  renderer keeps the angle per door in the interior cache.
- **Shutters.** Shopfront shutters roll up at opening and down at closing over about three real
  seconds (three seconds times the game's time rate), at most two moving per frame near the camera.
- **Metro.** Trains brake into stations on the timetable, stop for 20 s with their doors slid
  open, and pull away. The train position function gains the dwell.
- **Matrix.** ICE spins and pulses by type: watchers turn their eye toward the avatar, barriers
  ripple, black ICE throbs. Data streams flow along the conduits.

### D13 — Far crowd and levels of detail

- **Far instances.** Each far instance gains a clip id (stand, walk, hurry, run, sit, crouch) and
  a phase in place of the plain swing. Its 16 floats keep their layout: the swing slot carries the
  phase, and the spare slot carries the clip id and a lean.
- **Far body.** The instance body grows to 10 parts, adding shins and forearms. The vertex shader
  evaluates the clips as a few sines with knee and elbow terms, so the run and the sit read
  correctly at 30–90 m.
- **Distance limits for details:**

  | Distance | What shows |
  |---|---|
  | ≤ 4 m | Heads turn to look at the player |
  | ≤ 15 m | Footfall splashes |
  | ≤ 28 m | Full near skeleton with accessories and hand props |
  | Beyond 28 m | Instanced |

- **Street detail scales the near distances.** At street detail 0, the near skeleton reaches only
  16 m and footfall splashes are off; the near skeletons are capped at 24, 36 and 48 by detail
  level. Sit, crouch and lying still show at every distance through the instanced clip codes.

### D14 — Budgets

| Item | Low (phone) | High (desktop) |
|---|---|---|
| Near skeleton characters | 24 | 48 |
| Triangles per near character | ≤ 300 | ≤ 300 |
| Pose evaluation per frame | ≤ 0.4 ms | ≤ 0.8 ms (reference desktop, measured by the benchmark route) |
| Headlight cones | 8 | 16 |
| Muzzle and tracer lights | 4 | 8 (6 at medium) |
| View model | ≤ 400 triangles | ≤ 400 triangles |

The automated budget test from `improve-city-atmosphere` (D13 there) gains these rows.

### D15 — Determinism and saves

- Sim changes (acceleration, activities, groups, vehicle turns, aerodyne landing) come from agent
  keys, the seed and the clock. Crowds and traffic are not saved, and air traffic remains a
  function of the seed and time.
- Render-side state (the animation cache, door angles, camera bob) never feeds back into the sim.
- Guard presentation timers are not saved.
- The city digest, the facade and dressing digests, the save format (v5), the balance bands and
  the 16 playtests stay unchanged. The playtests are re-run to prove it.

### D16 — Verification

- **Unit tests** (`tests/unit/test_anim.c`):
  - Every clip loops without a jump.
  - Every blend is continuous.
  - Joint angles stay in their limits (knees never bend forward, elbows never backward).
  - Foot sliding is within 10 % for every gait and body type.
  - Turns respect the turn rate.
  - A pinned digest of sampled poses for all keyframe clips catches unintended changes. Gaits are
    left out because they use libm functions whose last bit can differ between compilers; their
    properties are tested instead.
  - The activity mix meets D5.
  - Car turns end in the cross lane.
  - Recoil recovers.
  - Head bob is zero when turned off.
  - Guard falls end on the floor.
- **Filmstrips** (platform tests):
  - `SR_ANIM_SHOTS=dir` renders every clip on a turntable: 8 frames per clip, 3 body types, at
    the game's internal resolution.
  - It also renders five street scenes over 4 s, at 8 frames per second: a busy crossing, an
    awning in rain, a fleeing crowd, a junction with traffic, and a firefight in a site.
  - `tools/anim_sheet.py` assembles contact sheets and animated GIFs into `docs/screenshots/anim/`.
    The after review looks at them.
- **Image metrics:**
  - In the rain scene, every standing person within 20 m changes pose in at least half of the
    frame pairs, read from the animation cache while the strip renders.
  - In the firefight scene, a guard's muzzle flash lights the wall behind it.
- **Benchmark.** The atmosphere benchmark route gains the busy crossing and the firefight. It is
  run on high and low.

## Risks / Trade-offs

- **Pose evaluation cost on phones.** 48 characters × 11 parts × clip sampling. Mitigation: clips
  are sampled once per character per frame with no allocation. The phone profile caps at 24 near
  skeletons and the instanced shader covers the rest.
- **The uncanny middle.** Better gait makes stiff faces more visible. At 480 × 270 the head is a
  few pixels across, so head motion (look-at, nods) carries the expression. Faces stay as they
  are.
- **Sim changes ripple into gameplay.** Acceleration and turning cars change when pedestrians are
  hit and when crossings clear. Mitigation: the stop check is unchanged, and the balance and
  playtests are re-run. Crowd-based tests that count positions get the new timing.
- **The view model hides the screen on phones.** Mitigation: it is small (about 18 % of the screen
  height) and low on the screen, and touch layouts lower it further.
- **Motion sickness.** Head bob and shake can be turned off, and both are subtle by default.

## Migration Plan

No data migration. Saves (v5) load unchanged. Settings files without the two new keys fall back to
the defaults.

## Open Questions

- Should the player see their own legs and shadow when looking down? Proposed: no for now. Revisit
  with third-person cameras, if they ever come.
- Should vehicles also change lanes? Proposed: no. Turns at junctions bring enough life.
