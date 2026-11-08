# Animation review (2026-11-08)

After `improve-city-atmosphere`, the city holds up as a still image. In motion it doesn't: people
glide, cars slide, guards drop flat, and the player is a floating camera. This review lists what
moves today, what is missing, and why it matters. The fixes are proposed in
`openspec/changes/improve-animation/`.

## What moves today

| Thing | How it moves |
|---|---|
| People near the camera (≤ 28 m) | Six rigid boxes. Each leg and arm swings as one stick around the hip or shoulder: `sin(metres × 2.4) × 0.55`. Nothing else moves. |
| People farther away | The same six parts, instanced. The shader swings legs and arms by the same angle. |
| Umbrellas | A flat slab 30 cm above the head, with no handle and no arm holding it. |
| Crowd states | Walk, flee, cover, gawk, shelter and idle. All except walk and flee use the same standing pose. Flee is the walk cycle played at 4 m/s. |
| Cars | One box with two lit boxes for the lights. They spawn moving at full speed in a straight line, stop in one frame for pedestrians, and never turn. |
| Aerodynes | A box and a cockpit sliding along sky lanes, with a blinking beacon. They don't bank or hover. |
| Guards | The street character walk. In combat a red marker appears over the head. When hit, nothing shows. When killed or knocked out, the body becomes a flat box on the floor in a single frame. |
| The player | A camera with no body. The eye height snaps between standing and crouching. Firing changes nothing on screen: no weapon, no recoil, no flash. |
| Doors, shutters | Open and close in one frame. |
| The city | Rich: flicker, beacons, chasing lights, fans, litter, drones, birds, sparks, trains, rain and steam (atmosphere phase 10). |

The city now moves better than its people, and that gap is what stands out most.

## Findings

**N1 — Bodies have no joints.** Limbs are single sticks with no knees or elbows. The hips never
rise and fall, and the torso, head and shoulders never move. Every walker has the same stride,
rhythm and posture whatever their body type, role, mood or speed. From 10 m the city reads as
mannequins on rails.

**N2 — Feet slide.** The swing follows distance walked, but the stride doesn't match the leg
length. At walk speed the feet slip a little; when fleeing at 4 m/s they skate.

**N3 — Turns and stops snap.** The heading jumps to the new direction in one frame. Walkers go from
standing to full speed, and back, in one step.

**N4 — Standing people are frozen.** Agents who gawk, shelter, idle or take cover stand at
attention, motionless, sometimes for minutes. No one breathes, shifts weight, looks around, talks,
smokes, checks a phone, sits, eats, waits at a crossing or dances outside a club. With rain most of
the time, the people sheltering under awnings are the most frozen of all, and they are what the
player sees most.

**N5 — States don't show.** Fear, curiosity and cover look the same. A fleeing crowd should be the
most dramatic moment on the street, but people just walk faster. Gawkers only face the event.
People in cover stand upright.

**N6 — Rain changes nothing in how people move.** Umbrellas float. People without one don't hunch,
hurry, raise a hood or hold a jacket over their head. Feet don't splash. Umbrellas never tilt into
the wind, even though litter and steam already follow it.

**N7 — Accessories are missing.** §16.3 of the concept promises briefcases, takeaway boxes,
shopping bags and cigarettes, as well as long coats. The appearance record has an `accessory`
field, but only umbrellas use it.

**N8 — Cars are sliding boxes.** They have no wheels and no suspension. They never brake, they
spawn at speed, and they never turn at a junction, so traffic runs only straight along the north
and west lanes. Brake lights switch in one frame, and there are no indicators or headlight beams.

**N9 — Aerodynes don't fly.** They don't bank, hover, pulse their thrusters, or land on the pads
the roofs now have.

**N10 — Combat has no feedback.** Guards show no alert behaviour: no looking around, no raised
weapon, no aiming, no crouching in cover. They carry no weapon at all. Hits cause no flinch.
Deaths and knockouts snap to a flat box. Surrender, restraint and flashbang stun all look like
standing. Enemy fire leaves no muzzle flash or tracer, so the player cannot tell where the shots
come from.

**N11 — The player has no body.** The view shows no hands and no weapon. Firing has no recoil, no
flash and no light, though §16.1 lists muzzle flashes among the coloured lights. There is no
reload, melee swing, throw or reach toward a door or terminal. The camera doesn't bob, dip on
landing or ease into a crouch, and taking damage has no direction on screen. §19 promises toggles
for head bob and camera shake, but nothing exists yet to toggle.

**N12 — The world snaps.** Doors open in one frame. Shop shutters are drawn shut or open, but
never roll. Metro trains pass by and never stop with their doors open.

## What good looks like

The rule for this game: **late-90s rigid parts, moved with care.** No skinned meshes and no
motion-capture files. Rigid boxes on a small skeleton with knees, elbows, a hip and a neck, posed
by short keyframe tables written in code, then blended and placed procedurally. This is how the
games the art direction cites animated their characters. It keeps the download small and the
result deterministic, and it fits the low internal resolution: at 480 × 270, a clear silhouette
and good timing matter more than detail.

Priorities, in the order the player notices them:

1. **The street at walking distance.** Gait, turns, idle life, rain behaviour (N1–N7). This is
   most of the screen, most of the time.
2. **The player's own hands and gun.** Every second of first-person play (N11).
3. **Combat readability.** Where shots come from, whether they hit, and who is down (N10).
4. **Traffic and flyers** (N8, N9), then **world snaps** (N12).
