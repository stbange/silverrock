# Spec Delta: retro-rendering

## ADDED Requirements

### Requirement: Rigid-part skeleton
Characters SHALL be drawn from rigid low-poly parts on a skeleton with a pelvis, chest, head, and two-part arms and
legs, posed by keyframed clips that are sampled, blended and placed procedurally, without skinned meshes or animation
asset files.

#### Scenario: Clips loop without a jump
- **WHEN** any looping clip is sampled just before and just after its loop point
- **THEN** no joint angle differs by more than the largest change between two neighbouring samples inside the clip

#### Scenario: Blends are continuous
- **WHEN** a character changes from one clip to another
- **THEN** its pose changes over at least 0.15 s, with no joint jumping by more than 0.2 rad between frames at 60 Hz

#### Scenario: Joints stay in their limits
- **WHEN** every clip is sampled at 64 phases for every body type
- **THEN** knees never bend forward, elbows never bend backward, and every joint stays within its limits

#### Scenario: Poses are pinned
- **WHEN** the sampled poses of all clips are hashed
- **THEN** the digest matches the pinned value on every platform

### Requirement: Natural locomotion
Walking, hurrying, running and crouch-walking SHALL bend the knees and elbows, move the hips and shoulders, match the
stride to the ground speed, and turn the body smoothly toward a new direction.

#### Scenario: People walk with knees and elbows
- **WHEN** a walking citizen is sampled over one stride
- **THEN** the knees and elbows bend, and the pelvis rises and falls twice

#### Scenario: Feet do not slide
- **WHEN** a character of any body type walks, hurries or runs at the speed of its gait
- **THEN** the foot on the ground moves backward relative to the body at the ground speed within 10 %

#### Scenario: Turns are smooth
- **WHEN** a walking character's direction changes by 180° in one simulation step
- **THEN** the drawn heading turns at no more than 360° per second and the body leans into the turn

#### Scenario: Far crowd keeps the gait
- **WHEN** citizens beyond the near distance walk, run, sit or crouch
- **THEN** the instanced bodies show the same gait or pose with bending knees

### Requirement: Living street animation
People who stand SHALL breathe, shift their weight, look around and carry out their activity. Rain SHALL show in how
they hold umbrellas, walk and splash, and carried accessories SHALL show in their hands.

#### Scenario: Nobody stands frozen
- **WHEN** the street filmstrip under an awning in rain is rendered over 4 seconds
- **THEN** every person within 20 m of the camera moves in at least half of the frame pairs

#### Scenario: Umbrellas are held
- **WHEN** a citizen with an umbrella walks in rain
- **THEN** the umbrella rod is attached to a raised hand and the canopy tilts toward the wind

#### Scenario: Footsteps splash
- **WHEN** a person within 15 m plants a foot on wet ground
- **THEN** a splash appears at the foot, within the per-frame cap

#### Scenario: Accessories show
- **WHEN** a citizen whose appearance has a briefcase, bag, takeaway box, cigarette or phone is drawn within 12 m
- **THEN** the item is held in the hand and moves with the arm, and a cigarette tip or phone screen glows

### Requirement: Vehicle animation
Cars SHALL show turning wheels, suspension, fading brake lights, indicators before turns and headlight beams, and
aerodynes SHALL bank into turns and hover.

#### Scenario: Headlights cut through the rain
- **WHEN** a car within 60 m drives at night
- **THEN** two fogged headlight beams are drawn in front of it, within the headlight budget

#### Scenario: Wheels turn
- **WHEN** a car moves 1 m
- **THEN** its wheels turn by 1 m divided by the wheel radius

### Requirement: First-person view model
The first-person view SHALL show the player's hands and the equipped weapon, which sway, bob, kick on each shot,
reload and reach for what the player interacts with, and every shot SHALL light the surroundings briefly.

#### Scenario: Hands and weapon in view
- **WHEN** the player equips a weapon of any category
- **THEN** a model for that category is drawn in the hands, never cut by nearby walls

#### Scenario: The weapon moves with the body
- **WHEN** the player walks, sprints or crouches
- **THEN** the weapon bobs with the steps, lowers while sprinting and lowers in a crouch

#### Scenario: Shots kick and light the room
- **WHEN** the player fires a gun
- **THEN** the weapon kicks back and recovers, a muzzle flash is drawn, and a light at the muzzle brightens nearby
  walls for about two frames

### Requirement: World motion
Doors, shutters and train doors SHALL move between their states instead of switching in one frame.

#### Scenario: Doors swing
- **WHEN** an interior door opens or closes
- **THEN** it swings about its hinge over about 0.3 s

#### Scenario: Shutters roll at opening time
- **WHEN** a shop opens or closes near the camera
- **THEN** its shutter rolls up or down over about 3 s

### Requirement: Animation budget
Animation SHALL stay within the budgets for near characters, triangles, pose evaluation time and lights at every detail
level.

#### Scenario: Animation within budget
- **WHEN** the busy crossing and the firefight are rendered at every street-detail level
- **THEN** near characters, triangles per character, headlight beams and muzzle lights stay within the budgets of
  their detail level

#### Scenario: Saves unchanged by animation
- **WHEN** a game is saved and loaded with this change
- **THEN** the save format, the city digest, the balance bands and the playtest results are unchanged
