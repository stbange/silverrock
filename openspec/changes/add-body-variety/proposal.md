# Proposal: People of every shape

## Why

After `improve-animation`, people move well, but they are all roughly the same body: four widths
and three heights on one masculine frame, with the only chrome being both arms at high
augmentation. A crowd reads as clones in different colours. The concept (§16.3) asks for body
types, clothing by role and class, and visible cyberware such as chrome arms.

## What Changes

- **Builds:** average, slender, athletic, heavy, thin, muscular, curvy and stocky, weighted by role.
  Police and guards run athletic and muscular, drifters thin, executives slender.
- **Frames and heights:** masculine and feminine frames, half and half, and four height classes.
  Notables' frames match their given names.
- **Shapes:**
  - A two-part torso (chest and waist) with shoulder, waist and hip widths and chest depth.
  - Limb thickness by build, a belly on heavy builds, a modest bust on feminine frames.
  - Slightly smaller heads on feminine frames.
- **Clothes and hair:** short sleeves and tank tops, skirts and dress hems, long hair, ponytails
  and bobs.
- **Chrome limbs:** a left arm, a right arm, both arms, legs, or all four, in four finishes, with
  glowing seams. The player's own chrome or bare arms show in first person.
- **Movement by body:** hips that sway, arms held out on muscular and heavy builds, feet apart on
  heavy ones, and shorter strides for heavy builds.
- **Character creation:** choose the build, frame, height and chrome, with a figure that matches.
- **Distant crowd:** the instanced crowd takes the body's height and width.

## Capabilities

### Modified Capabilities

- `population-simulation`: bodies of every kind, builds by role, bodies that match names.
- `retro-rendering`: chrome limbs.
- `game-persistence`: older saves keep their people.
- `player-life`: the body is chosen at character creation.

## Impact

- **Saves:** the appearance keeps its size. `body` packs the build, frame and height; `cyber`
  packs the level, limbs and finish.
  - Older values (body 0..3, cyber 0..5) read as the same people, and the look keys used for
    recognition do not change.
  - The save version stays 5.
- **Determinism:** new choices are drawn after the existing fields, so heads, hair and clothes stay
  as they were for the same seed.
- **Budget:** at most 380 triangles per near character (the worst case carries a coat, a skirt or
  chrome, a bust or belly, an umbrella and a bag).
