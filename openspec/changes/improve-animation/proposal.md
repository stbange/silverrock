# Proposal: Animation that brings the city to life

## Why

After `improve-city-atmosphere`, the city looks right in a still frame and wrong in motion
(`docs/reviews/animation-2026-11-08.md`):

- **People are mannequins on rails.** Six rigid boxes, with one stick per limb swinging on a sine.
  There are no knees, elbows, hip bob or head movement (N1). Feet slide (N2). Turns and stops snap
  (N3).
- **Standing people are frozen.** Sheltering, gawking, idling and cover all share one rigid
  standing pose. With rain most of the time, the people waiting under awnings are what the player
  sees most (N4, N5).
- **Rain, fear and accessories don't show.** Umbrellas float above the head. No one hunches,
  hurries or splashes. A panicked crowd just walks faster. The briefcases, bags, coats and
  cigarettes the concept promises are missing (N5–N7).
- **Vehicles slide.** Cars have no wheels, never brake and never turn. Aerodynes neither bank nor
  land (N8, N9).
- **Combat gives no feedback.** Guards carry no weapon and don't aim, flinch or fall; they snap to
  a flat box. Enemy shots show no flash. The player has no hands, no gun, no recoil, no muzzle
  light, no head bob and no damage direction (N10, N11).
- **The world snaps.** Doors, shutters and train doors change state in one frame (N12).

The city now moves better than its people. This change closes that gap before M6 adds content,
so M6's new roles, set pieces and enemies can use a working animation system instead of the
single sine.

Roadmap position: after `improve-city-atmosphere`, before **M6**.

## What Changes

- **A rigid skeleton and a clip system** (render layer, presentation only):
  - 11 rigid parts: pelvis, chest, head, and upper and lower parts of each arm and leg.
  - Poses come from small keyframe tables in code. They are blended over 0.15–0.3 s and placed
    procedurally: the stride matches the ground speed, the body leans into turns, and the head
    looks at targets.
  - Animation state lives in a render-side cache keyed by agent, so saves and the city digest stay
    untouched.
- **Walking well.**
  - Walk, hurry, run, crouch-walk and limp cycles with knees, elbows, hip bob and shoulder counter-
    swing, varied by body type, role and a per-person gait seed.
  - Feet planted to within 10 % of the ground speed.
  - Smooth turns and accelerated starts and stops.
  - The far crowd shader learns knees and the run.
- **Life on the street.**
  - Idle breathing and weight shifts.
  - Activities chosen from the district and nearby street props: phone, smoking, talking in pairs
    and groups, sitting on benches, eating at stalls, buying from vending machines, waiting at
    crossings, leaning on walls, dancing outside clubs, begging in the Barrens, and watching
    tickers and holo-ads.
- **Rain and accessories.**
  - Umbrellas held in hand and tilted into the wind.
  - Without an umbrella: hunched shoulders, hoods up, a jacket held over the head, hurrying.
  - Shaking off water under cover, and splashes at each footfall.
  - Briefcases, shopping bags, takeaway boxes, cigarettes with a glowing tip, phones that light
    the face, and coat tails that swing.
- **Reactions.**
  - Flight with a real run, arms up and looks back.
  - Cowering and crouching in cover.
  - Gawkers who point or hold up a phone to film.
  - Police who walk a beat, wave people on and draw weapons.
- **Vehicles and aircraft.**
  - Cars get wheels that turn, suspension, braking and acceleration, turns at junctions with
    indicators, brake lights, headlight beams, and spray on wet roads.
  - Aerodynes bank into turns, hover and bob, pulse their thrusters, and land on roof pads.
- **The player's body.**
  - A first-person view model with hands and a weapon for each weapon category. It sways with the
    look, bobs while walking, lowers while sprinting, kicks on each shot, and reloads, swings,
    throws and reaches for doors and terminals.
  - Muzzle flashes light the surroundings for one frame.
  - The camera bobs, dips on landing and eases into a crouch. Taking damage flinches the view and
    shows the hit's direction.
  - Settings for head bob and camera shake, as §19 of the concept promises.
- **Combat animation.**
  - Guards hold weapons. Their stance shows their alert state: patrolling, looking around,
    searching with the weapon raised, aiming in combat, crouching and peeking from cover.
  - Hits cause a directional flinch, and heavy hits a stagger.
  - Death topples the body away from the shot. A knockout slumps it, knees first.
  - Surrender raises the hands, a restrained guard kneels, and a flashbang makes guards shield
    their eyes.
  - Drones tilt and bob.
  - Enemy shots show muzzle flashes and tracers.
- **World motion.**
  - Doors swing open and shut. Shutters roll at opening hours.
  - Metro trains stop at stations with their doors opening.
  - ICE in the Matrix spins and pulses.
- **Verification.**
  - Unit tests on poses: foot sliding, joint limits, continuity of loops and blends, and pinned
    pose digests.
  - Filmstrips and short animated GIFs of every clip, rendered in the platform tests for review.
  - Budgets for skinned-part counts and CPU time per frame, and a benchmark on desktop and the
    phone profile.

## Capabilities

### New Capabilities

None. All changes extend existing capabilities.

### Modified Capabilities

- `retro-rendering` — the skeleton and clips, character and vehicle animation, the view model,
  muzzle light, filmstrips and budgets.
- `population-simulation` — activities, pairs and groups, starts and stops, vehicle turns and
  braking, and aerodyne landing.
- `combat` — guard stances by alert state, hit reactions, death and knockout falls, surrender,
  restraint and stun poses, and enemy muzzle flashes and tracers.
- `player-life` — the first-person camera (bob, landing, crouch, damage flinch) and the head-bob
  and camera-shake settings.

## Impact

- **Code.**
  - New `src/render/sr_anim.c/.h`: skeleton, clips, blending and the animation cache.
  - New `src/render/sr_viewmodel.c`.
  - Changes to `sr_scene.c` (character and vehicle builders), the crowd shader in `sr_gl.c`,
    `sr_crowd.c` (activities, acceleration, vehicle turns), `sr_air.c` (landing), `sr_site.c`
    (hit, fall and stance timers for presentation), `sr_game.c` (camera motion) and `sr_settings`
    (two new settings).
- **Saves.** Unchanged. Animation state is presentation only. Crowd and traffic are not saved.
  Guards save their down state as before, and the fall is replayed from it.
- **Determinism.** Every choice comes from agent keys, the seed and game time. The city digest,
  balance bands and playtests stay as they are.
- **Performance.** The near-character mesh grows from 6 to 11 parts plus accessories, about 160
  triangles per person. Budgets are set in design D14.
- **Download.** A few kilobytes of keyframe tables. No asset files.
- **Docs.** The review, an after-review with filmstrips, the concept (§16.3 on how characters move
  and §19 for the new settings), the README and the roadmap.
