# Design: People of every shape

## Encoding (no save change)

`SrAppearance` keeps eleven bytes:

| Byte | Bits | Meaning |
|---|---|---|
| `body` | 0-2 | `SrBuild` |
| `body` | 3 | frame: 0 masculine, 1 feminine |
| `body` | 4-5 | `SrHeight`: average, short, tall, very tall |
| `cyber` | 0-2 | augmentation level 0..5 |
| `cyber` | 3-5 | chrome limbs: left arm, right arm, legs |
| `cyber` | 6-7 | finish: chrome, matte black, gunmetal, pale gold |

Older saves hold body 0..3 and cyber 0..5. They read as average height, masculine frame, the build
of the same index, and no limb bits. As before, a level-4 augmentation without limb bits means both
arms.

`sr_appearance_key` keeps its layout for those values and puts the new cyber bits on top (bit 58
up), so the heat system's remembered faces still match.

## Generation

The new choices are drawn after the existing fields, in this order:
1. Frame, half and half.
2. Build, from an eight-slot table per role.
3. Height, from a table weighted toward average.
4. Chrome limbs:
   - level 2 and up: one arm 35 % of the time;
   - level 3 and up: legs 30 % of the time;
   - level 4 and up: both arms always.

Notables then take the frame their given name suggests (`sr_name_frame`; names read either way keep
the roll).

## Shapes

Build factors, as multiples of an average masculine body:

| Build | Shoulder | Waist | Hip | Chest depth | Limbs | Belly |
|---|---|---|---|---|---|---|
| Average | 1.00 | 0.88 | 0.92 | 1.00 | 1.00 | 0 |
| Slender | 0.92 | 0.76 | 0.86 | 0.88 | 0.85 | 0 |
| Athletic | 1.10 | 0.80 | 0.90 | 1.06 | 1.14 | 0 |
| Heavy | 1.12 | 1.18 | 1.16 | 1.24 | 1.26 | 0.10 m |
| Thin | 0.85 | 0.70 | 0.80 | 0.80 | 0.74 | 0 |
| Muscular | 1.28 | 0.90 | 0.95 | 1.18 | 1.42 | 0 |
| Curvy | 0.98 | 0.82 | 1.14 | 1.02 | 1.06 | 0 |
| Stocky | 1.18 | 1.06 | 1.04 | 1.16 | 1.22 | 0.04 m |

Feminine frames have:
- shoulders 0.86, waist 0.84 and hips 1.12 times the masculine values;
- slightly longer legs relative to height, a head 6 % smaller and smaller feet;
- a modest bust of 2.5 to 6.5 cm.

Heights:
- 1.74 m masculine and 1.63 m feminine;
- −9 cm short, +8 cm tall, +15 cm very tall;
- ±2.4 cm by head shape;
- stocky builds 4 cm shorter.

The torso is two boxes, a waist and a chest, plus a belly box and a bust box where the body has
them. The legs sit inside the hip width.

## Clothes and hair

- **Sleeves:** long for executives, police, guards and fixers. Short or none for some gangers,
  vendors, workers and drifters.
- **Skirts and dress hems:** feminine frames in some executive, fixer, worker and vendor outfits.
- **Hair:** style 9 is long, 10 tied back, 11 shaved. Feminine frames rarely shave and more often
  wear their hair long, and some short cuts become bobs.

## Chrome limbs

A chrome arm replaces the upper arm, forearm and hand. Chrome legs replace the thigh, shin and foot
and are slightly thicker. Each has a glowing seam (the shoulder joint or the knee) in the finish's
accent colour, and the legs get a calf piston. The view model reads the same arm colours, so the
player sees their own chrome or bare forearms.

## Movement

- Hip sway: a sideways pelvis shift with a chest counter-roll, strongest on feminine and curvy
  bodies. It fades when running.
- Arms are held out on muscular, heavy and stocky builds, and the stance is wider on heavy builds.
- Strides are 0.9 times as long on heavy builds and 1.04 times on thin and slender ones; the feet
  stay planted (tested for every build and frame).

## Character creation

Four choices cycle from the roll:

| Choice | Keyboard | Gamepad | Touch (right half) |
|---|---|---|---|
| Build | B | Y | rows below the top |
| Frame | F | LB | rows below the top |
| Height | H | D-pad up | rows below the top |
| Chrome | C | RB | rows below the top |

Re-rolling the look clears the choices. The creation screen draws a figure from the skeleton's
measures, with the chosen chrome.
