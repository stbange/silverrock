# Animation review after improve-animation (2026-11-08)

This review checks the findings of `animation-2026-11-08.md` against the game after all ten
phases of `improve-animation`. The strips are in `docs/screenshots/anim/`:

- `clips_sheet.png`: every clip on three body types, four frames each.
- `clip_*.gif`: a selection of clips.
- `street_*.gif`: four street scenes over four seconds.
- `view_model.png`: the player's hands and weapons.
- `firefight_flash.png`: a guard's shot in a dark room.
- `faces.png`: the eight expressions in four styles on four skin tones.

## Before and after

| Finding | Before | After |
|---|---|---|
| N1 No joints | Six boxes; each leg and arm one stick on a sine | An 11-part skeleton with knees, elbows, hips, chest and neck, plus feet and hands; 52 clips written as keyframe tables; posture by role (guards upright, drifters stooped, suits stiff, gangers loose); a gait seed per person |
| N2 Feet slide | Swing not matched to leg length; skating when fleeing | Feet follow a planned path and a two-bone solve finds hip and knee; the foot on the ground stays put at every speed and body type (tested within 10 %) |
| N3 Turns and stops snap | Heading and speed jump in one frame | Heading turns at up to 360°/s (540°/s running) with a lean into the turn; walkers accelerate over about 0.4 s and brake over about 0.3 s |
| N4 Standing people frozen | One rigid pose | Breathing and weight shifts; activities chosen from the props within 3 m and the district: benches, stalls, vending machines, crossings, club queues, barrel fires, booths, holo-trees; phones, smoking, calls, leaning, window shopping, dancing, begging; groups of two or three who take turns to talk |
| N5 States don't show | Fear, curiosity and cover look alike | Flight is a run with an arm over the head and looks back, after a startled recoil; cover is a cower; onlookers film, point or call; officers walk a beat, wave people on, ready their weapon at Heat 3 and aim at Heat 4 |
| N6 Rain changes nothing | Umbrellas float | Umbrellas held up in the hand, the canopy tilted into the wind (at most 20°) and carried closed when it stops raining; without one, people hurry, hunch, raise hoods or hold a jacket over the head, and shake off the water under cover; footfalls splash |
| N7 Accessories missing | Umbrellas only | Briefcases, shopping bags, takeaway boxes, cigarettes with a glowing tip, phones that glow, backpacks and swinging coat tails |
| N8 Cars slide | Boxes on straight lanes | Body, cabin and turning wheels; suspension that pitches and rolls; acceleration and braking within the braking distance; turns at junctions on a quarter circle with indicators; brake lights; headlight beams in the glow pass; wipers; spray behind fast wheels |
| N9 Aerodynes don't fly | Boxes sliding along lanes | A weave across the lane with banking, a hover bob, pulsing thrusters, and landings on helipads within 200 m: descend, stand with the beacon on, lift off |
| N10 No combat feedback | Guards unarmed, no reactions, flat-box deaths | Guards hold rifles or pistols; patrols pause and look around at waypoints; suspicion turns the head; searches raise the weapon; combat aims, strafes, and crouches and peeks from cover; hits flinch away from the shot, heavy hits stagger, headshots snap the head; deaths topple away from the shot and knockouts slump, knees first; surrender, restraint and flashbang poses; muzzle flashes, tracers every third round, and a flash that lights the room |
| N11 The player has no body | A floating camera | Hands and a model for each weapon category; sway, bob, sprint and crouch offsets; recoil of the weapon and the view; muzzle flash, light and shells; reloads, swings, throws and reaches; head bob, landing dip, eased crouch, flinch with a red edge on the side of the hit, shake; `head_bob` and `camera_shake` settings |
| Faces (added later) | Blank skin-coloured head cubes | A small face on every near head: low level brows, heavy lids, a nose and a thin mouth, in four styles (plain, stubble, scar, tattoo). Expressions follow what happens: fear in flight, pain when hit, closed eyes when down or asleep, a moving mouth when talking, surprise, a stern look for guards, police, gangers and anyone aiming. People blink. Heads are narrower, with a nose. The rest face is calm, never a grin. |
| N12 The world snaps | Doors, shutters and trains switch in one frame | Doors swing over 0.3 s; shutters roll at opening and closing; trains brake into stations and stand with their doors open; ICE ripples, throbs and turns its eye toward the avatar |

## How it is built

- **Rigid parts, moved with care.** The skeleton draws rigid boxes, as the art direction asks. No
  skinning and no asset files: the clips are tables in `src/render/sr_anim.c`.
- **Presentation only.** The animation cache, door angles, view sway and camera motion live in the
  renderer. The simulation changes are deterministic and none is saved: speed ramps, activities,
  groups, junction turns, braking, aerodyne landings and guard pauses. Guard hit and fall times
  are presentation timers. A body found after a load lies in its final pose.
- **The far crowd** uses ten parts in the vertex shader, with clip codes for standing, walking or
  running, sitting, crouching and lying, so people 30 to 90 m away keep their gait.

## Checks

- **Unit tests:** 194 pass, including 15 in `tests/unit/test_anim.c` that cover every scenario of
  the change.
  - The pose digest of the keyframe clips is pinned. Gaits are left out because they use libm
    functions whose last bit can differ between compilers; their properties are tested instead.
- **Platform tests:** 27 pass under Xvfb.
  - `test_anim_street` renders the street strips. Every standing person within 20 m of the camera
    in the downpour changes pose in at least half of the frame pairs.
  - In the firefight, a guard's flash brightens the dark room by about 29 levels.
- **Budgets:**
  - At most 306 triangles per near character with faces, against a budget of 320 (raised to 380 by `add-body-variety`, whose bodies reach 366).
  - Near skeletons are capped at 24, 36 and 48 by detail level.
  - Headlight beams are capped at 8, 12 and 16.
  - Muzzle lights are capped at 4, 6 and 8 per frame.
  - The view model stays under 400 triangles.
  - Posing 48 characters takes about 0.08 ms in the debug build.
- **Gameplay:** the balance bands (17 lines) and the 16 playtests (12 band lines) pass with the new
  crowd, traffic and guard behaviour. The save format stays at version 5.
- **Golden image:** updated once. The reference view now shows the starting pistol in the hands.

## Still open

1. **Real hardware.** As with the atmosphere work, frame times on the reference desktop and on
   phones have not been measured. Software GL in CI says nothing about frame times. A manual look
   is listed in `tests/manual-verification.md`.
2. **No before strips.** The old six-part walk was replaced in place, so the "before" side is the
   written review, not a strip.
3. **Reloads are cosmetic.** The game counts ammunition per round and has no magazines, so the
   reload is shown after a magazine's worth of shots or on the reload key, and never blocks firing.
4. **Knocked-out guards never wake.** The game never calls the wake-up timer, so the get-up clip
   exists and is tested but is never seen in play.
5. **Faces are small.** At 480 × 270 a head 5 m away is about ten pixels tall, so a face reads as
   brows, eyes and mouth up close and fades to a skin tone further off. Expressions swap whole faces;
   there is no lip sync.
