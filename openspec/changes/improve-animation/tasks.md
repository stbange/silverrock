# Tasks

The phases are ordered so that each one improves the game on its own. The skeleton comes first
because every later phase poses it. The street (phases 2–4) comes before the player's body and
combat because the player sees it most. Every phase ends green on CI, with the filmstrips (D16)
regenerated and looked at. In "(spec: …)", a name alone refers to a scenario in this change's
deltas, and "D1"…"D16" refer to design decisions.

## 1. Foundations (about 3–4 days)

- [x] 1.1 `src/render/sr_anim.c/.h`: the 11-part skeleton with pivots scaled by the appearance; `SrPose`; building the character mesh from a pose; hair, markers and optics attached to the head (D1)
- [x] 1.2 Clips as keyframe tables with Catmull-Rom sampling; distance, time and one-shot drivers; gait seed and role posture (D2) (spec: "Clips loop without a jump")
- [x] 1.3 `SrAnimCache` keyed by agent and guard: clip blend, heading, one-shot layers by part group, expiry (D4) (spec: "Blends are continuous")
- [x] 1.4 Joint limits and a pinned pose digest test (spec: "Joints stay in their limits", "Poses are pinned")
- [x] 1.5 Filmstrips: `SR_ANIM_SHOTS=dir` turntable of every clip in the platform tests; `tools/anim_sheet.py` makes contact sheets and GIFs in `docs/screenshots/anim/`; the old walk is described in the review; no before strips were recorded, since the old six-part walk was replaced in place (D16)

## 2. Walking well (about 1 week)

- [x] 2.1 Walk, hurry, run, crouch-walk and limp clips with knees, elbows, hip bob and sway, shoulder counter-swing (D2, D3) (spec: "People walk with knees and elbows")
- [x] 2.2 Gait chosen and blended by speed; stride from leg length (spec: "Feet do not slide")
- [x] 2.3 Rendered heading follows at the turn rate with lean (spec: "Turns are smooth")
- [x] 2.4 Sim acceleration and braking for agents; crowd tests updated (D3) (spec: "Walkers start and stop")
- [x] 2.5 Far crowd: 10-part instance body, clip id and phase in the instance data, the shader evaluates stand, walk, hurry, run, sit and crouch (D13) (spec: "Far crowd keeps the gait")
- [x] 2.6 Guards and police use the new walk with upright posture

## 3. Life on the street (about 1 week)

- [x] 3.1 Idle breathing and weight shifts; look-around; heads that look at nearby events and the player (spec: "Nobody stands frozen")
- [x] 3.2 Activity choice in the sim from the agent key, the district and street props within 3 m; activity id and timer (D5) (spec: "Activities fit the place")
- [x] 3.3 Clips: sit, sleep, eat, buy, lean, smoke, phone call, look in the window, wait at a crossing, queue, dance, watch, point, warm hands, beg
- [x] 3.4 Groups of two or three who face each other and take turns gesturing (spec: "People talk in groups")
- [x] 3.5 District weights; the activity-mix test (spec: "The street is busy with small things")

## 4. Rain and accessories (about 4–5 days)

- [x] 4.1 Umbrella held in hand, eight-segment canopy, tilted into the wind; closing at shelter (D6) (spec: "Umbrellas are held")
- [x] 4.2 Without an umbrella: hunch, hurry, hood, jacket or bag over the head; shake off at shelter (spec: "Rain changes the walk")
- [x] 4.3 Footfall splashes on wet ground, capped per frame (spec: "Footsteps splash")
- [x] 4.4 Accessories from `appearance.accessory`: briefcase, bags, takeaway box, cigarette with a glowing tip and smoke, phone that lights the face, backpack (spec: "Accessories show")
- [x] 4.5 Coat tails on long coats, following the thighs and the wind

## 5. Reactions (about 4–5 days)

- [x] 5.1 Flight: recoil one-shot, run with an arm over the head, looking back; flee speed 4.5 m/s (D7) (spec: "Panic reads as panic")
- [x] 5.2 Cower in cover near gunfire
- [x] 5.3 Gawkers point, film with a glowing phone or make a call
- [x] 5.4 Police: beat walk, wave-on and stop gestures, weapon low at Heat 3 or more, aim at an armed response (spec: "Police show their intent")

## 6. Vehicles and aircraft (about 1 week)

- [x] 6.1 Car body, cabin and four wheels that turn; suspension pitch, roll and bob (D8) (spec: "Wheels turn")
- [x] 6.2 Sim acceleration and braking; in view, cars pull away instead of appearing at speed (spec: "Cars brake and pull away")
- [x] 6.3 Turns at junctions on a quarter circle into the cross lane; indicators (spec: "Cars turn at junctions")
- [x] 6.4 Brake lights fade; headlight cones in the glow pass; wipers in rain; spray on wet roads (spec: "Headlights cut through the rain")
- [x] 6.5 Aerodynes bank, pitch, bob and pulse their thrusters (D9)
- [x] 6.6 Aerodyne landing on helipads within 200 m: descend, wait, lift off, deterministic from the seed and the clock (spec: "Aerodynes land on roofs")

## 7. The player's body (about 1–1.5 weeks)

- [x] 7.1 View model pass with its own depth range; hands and one model per weapon category, tinted by the weapon, outfit and cyberware (D10) (spec: "Hands and weapon in view")
- [x] 7.2 Sway, bob, sprint and crouch offsets (spec: "The weapon moves with the body")
- [x] 7.3 Recoil kick of the weapon and the camera, recovered; muzzle flash sprite, a two-frame light and shells (spec: "Shots kick and light the room")
- [x] 7.4 Reload, melee light and heavy swings, block, gadget throw, reach for doors, terminals, items and the climb
- [x] 7.5 Camera: head bob, landing dip, eased crouch, damage flinch with a directional red edge, shake (spec: "Head bob", "Landing dips the view", "Damage shows its direction")
- [x] 7.6 Settings `head_bob` and `camera_shake`; defaults; the settings file (spec: "Head bob can be turned off", "Shake can be turned off")

## 8. Combat animation (about 1 week)

- [x] 8.1 Guards hold their archetype's weapon; stance by alert state: patrol, look around, careful search, aim, strafe, crouch and peek from cover (D11) (spec: "Alert state shows in the stance")
- [x] 8.2 Hit flinch by direction, stagger on heavy hits, head snap on headshots; presentation timers on `SrGuard` (spec: "Hits show")
- [x] 8.3 Death topple away from the last hit and knockout slump to a lying pose; bodies after a load lie still; blood decal grows (spec: "The dead fall", "Bodies stay down after a load")
- [x] 8.4 Surrender, restrained, flashbang stun and get-up poses (spec: "Surrender shows")
- [x] 8.5 Drones tilt, bob and spin rotors
- [x] 8.6 Enemy muzzle flashes, lights and tracers (spec: "Enemy fire can be traced")

## 9. World motion (about 3–4 days)

- [x] 9.1 Interior doors swing over 0.3 s (D12) (spec: "Doors swing")
- [x] 9.2 Shopfront shutters roll at opening and closing (spec: "Shutters roll at opening time")
- [x] 9.3 Metro trains dwell at stations with their doors open (spec: "Trains stop at stations")
- [x] 9.4 ICE motion by type and data streams along conduits

## 10. Verification (about 3–4 days)

- [x] 10.1 Street filmstrips: busy crossing, awning in rain, fleeing crowd, junction, firefight; motion and muzzle-light metrics (D16) (spec: "Nobody stands frozen", "Enemy fire can be traced")
- [x] 10.2 Budgets from D14 in the budget test; benchmark route on high and low (spec: "Animation within budget")
- [x] 10.3 Re-run the balance bands and the 16 playtests; check that saves, digests and bands are unchanged (D15) (spec: "Saves unchanged by animation")
- [x] 10.4 After strips and `docs/reviews/animation-<date>-after.md` against N1–N12; concept §16.3 and §19; README and roadmap
- [ ] 10.5 Manual look on desktop and phone (a busy crossing, rain under an awning, a firefight, driving traffic, a landing aerodyne); record in `tests/manual-verification.md`
- [x] 10.6 Coverage at 95 % or above; traceability covers every scenario of this change

## 11. Faces (about 2 days, added after the first review)

- [x] 11.1 Face atlas in two texture layers: 4 styles (plain, stubble, scar, tattoo) × 8 expressions, drawn on white and multiplied by the skin colour; serious proportions: low level brows, heavy lids, small irises, a nose shadow, a thin mouth (D17)
- [x] 11.2 Narrower, taller heads with a nose; the face quad on the head's front; hair, hood, optics and masks fitted to the new head
- [x] 11.3 Expression choice from clips, hits, role and time: fear, pain, closed eyes, talking mouth, surprise, stern, content; blinking every three to six seconds (spec: "Faces show feeling", "Faces stay serious", "People blink")
- [x] 11.4 Close-up face strips in the filmstrip test; tests on the atlas and the choice; budget 320 triangles per near character
