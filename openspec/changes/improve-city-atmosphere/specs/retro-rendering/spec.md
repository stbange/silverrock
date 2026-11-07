# Spec Delta: retro-rendering

## MODIFIED Requirements

### Requirement: Night lighting
The renderer SHALL light scenes with many coloured point lights from neon, windows, vehicles and muzzle flashes,
using up to 8 most influential lights per draw, SHALL draw the chunks next to the camera per block so that each block
gets its own lights, and emissive surfaces SHALL remain visible through fog.

#### Scenario: Neon through fog
- **WHEN** an emissive sign is at the fog end distance
- **THEN** its colour still contrasts with the fog by at least the minimum contrast

#### Scenario: Night lighting near the camera
- **WHEN** the camera stands in a block with more than 8 lights in its chunk
- **THEN** the draws of that block use the 8 lights nearest to it, not the 8 nearest to the chunk centre

### Requirement: Skyline horizon
The view SHALL never end in an empty background: in fog, the end of the street SHALL show fog with neon halos and
silhouettes of tall buildings, and in rare long views from high places or in overcast weather a skyline with lit
windows and beacons and a low-detail distant city SHALL fill the horizon.

#### Scenario: No void at the horizon
- **WHEN** reference views from city edges and high rooftops are rendered in overcast weather
- **THEN** every pixel column below the horizon line contains skyline or scene geometry

#### Scenario: No dark slot at the end of the street
- **WHEN** the camera looks down a long straight street at street level
- **THEN** the pixels around the vanishing point show neon halos or building silhouettes in the fog, not a flat fog
  colour alone

### Requirement: Rain and wet surfaces
Rain SHALL be rendered as particles that do not fall under cover and that splash on the ground near the camera, and
wet streets SHALL reflect nearby emissive lights with a strength that follows the weather's wetness.

#### Scenario: No rain under cover
- **WHEN** the camera is under a roof or awning during a downpour
- **THEN** no rain particles are rendered in the covered volume

#### Scenario: Wet streets reflect neon
- **WHEN** an emissive sign is within 40 m of the camera, reflections are enabled and the wetness is above 0.5
- **THEN** a reflection of the sign is drawn on the ground below it, and none is drawn when the wetness is 0

#### Scenario: Rain splashes
- **WHEN** it rains with intensity above 0.3
- **THEN** splash sprites are drawn on uncovered ground within 20 m of the camera

### Requirement: Readable signage
Shop signs SHALL show an icon for the business type and the generated name in a bitmap font, legible at the default
internal resolution from 10 m, and signs of places of interest SHALL stay the brightest and largest sign on their
facade.

#### Scenario: Sign legibility
- **WHEN** a shop sign is viewed head-on from 10 m at the default internal resolution
- **THEN** its icon covers at least 12×12 pixels

#### Scenario: Shop name readable
- **WHEN** a shop sign is viewed head-on from 10 m at the default internal resolution
- **THEN** the shop's generated name is drawn under the icon with glyphs at least 5 pixels tall

### Requirement: Frame-rate budget
The game SHALL hold 60 frames per second on the reference desktop and at least 30 frames per second (95th
percentile) on the reference phone along the benchmark route, with all atmosphere detail at the device's default
setting, lowering the internal resolution when needed.

#### Scenario: Desktop benchmark route
- **WHEN** the benchmark route is played on the reference desktop at 1080p with street detail high
- **THEN** at least 99 % of frames take at most 16.7 ms

#### Scenario: Dynamic resolution engages
- **WHEN** frame times exceed the budget for 2 seconds with dynamic resolution enabled
- **THEN** the internal resolution is lowered one step and raised again once the budget is met for 10 seconds

## ADDED Requirements

### Requirement: Texture layers
All procedural textures SHALL live in one texture array of 64 layers with a fixed layer map, icons and sprites SHALL
be packed into atlas layers, and no draw SHALL reference a layer outside the array.

#### Scenario: Every layer index is in range
- **WHEN** the layer map, every icon cell and every sprite cell are checked
- **THEN** every layer index is below the number of layers in the array, including the home icon

### Requirement: Sprites and glows
The renderer SHALL draw sprites that turn to face the camera (upright or fully facing) or lie flat as decals, with
animation frames chosen from the game time, and SHALL draw glows, light cones, steam and reflections in an additive
pass that does not write depth and fades with fog.

#### Scenario: Sprites face the camera
- **WHEN** an upright sprite is viewed from two directions 90° apart
- **THEN** its quad faces the camera in both views and stays vertical

#### Scenario: Animation follows game time
- **WHEN** the same scene is built twice at the same game time
- **THEN** every animated sprite, sign and light uses the same frame and the meshes are identical

#### Scenario: Glows fade with fog
- **WHEN** a glow sprite is beyond the fog end distance
- **THEN** it adds no colour to the image

### Requirement: Neon through fog
Emissive surfaces SHALL keep most of their colour inside the fog, large emissives (headquarters logos, neon crowns,
billboards, blade signs, tower beacons and searchlights) SHALL show as soft coloured halos up to three times the
visibility distance, and lamps and signs SHALL carry a faint halo in wet air.

#### Scenario: Neon halos through fog
- **WHEN** a headquarters logo or a billboard is between one and three visibility distances away in fog
- **THEN** a halo in its colour is drawn at its position, and none is drawn beyond three visibility distances

### Requirement: Contrast and emphasis
Lit windows SHALL stay below full white, ambient light SHALL leave walls darker than the neon on them, and bloom
SHALL apply to emissive surfaces only, so that neon is the brightest and most saturated part of the scene at every
hour, by day as well as by night.

#### Scenario: No white walls of windows
- **WHEN** the reference views at night are rendered
- **THEN** at most 3 % of their pixels are clipped to white

#### Scenario: Neon is the brightest
- **WHEN** the 1 % most saturated pixels of a night reference view are traced to their surfaces
- **THEN** at least 80 % of them come from emissive surfaces

### Requirement: Facade variety
Each building SHALL get a facade with a ground-floor band, a shaft of a window module and a crown, chosen
deterministically from the seed, the building and its district, with a texture offset per building, so that
neighbouring buildings look different.

#### Scenario: Neighbours differ
- **WHEN** the facades of all buildings on five seeds are compared
- **THEN** no two buildings on the same block share both the window module and the ground-floor type, and buildings
  facing each other across a street differ in window module, tint or crown

### Requirement: Window lighting by use and hour
Which windows are lit SHALL depend on the building's use and the hour, chosen per window from the building's seed, so
that offices empty at night and homes light up in the evening, and lit windows SHALL take one of several colours.

#### Scenario: Offices go dark at night
- **WHEN** an office building is rendered at 18:00 and at 02:00
- **THEN** the share of lit windows at 02:00 is at most a quarter of the share at 18:00

#### Scenario: Homes light up in the evening
- **WHEN** a residential building is rendered at 21:00 and at 03:00
- **THEN** the share of lit windows at 21:00 is at least twice the share at 03:00

### Requirement: Building silhouettes
Buildings SHALL take shapes from a grammar per district (box, podium and tower, setbacks, chamfered corners, L-shapes,
slabs, row houses) with cornices, parapets and roof kits, and landmarks SHALL get distinct crowns.

#### Scenario: Roofs vary
- **WHEN** the buildings of a district with at least 20 buildings are counted
- **THEN** at least three different roof kits occur

### Requirement: Street-level frontage
Every building that faces a street SHALL have a ground-floor band (shopfront, lobby, shutter, loading dock, stall
front or blank wall), places of interest SHALL have a lit shopfront or lobby with the door under the sign, closed
shops SHALL show shutters, and pavements SHALL meet the road with a kerb face, with crosswalks at intersections.

#### Scenario: The pavement has a kerb
- **WHEN** a street segment and an intersection are built
- **THEN** the pavement edge has a vertical kerb face down to the road, and every crossing of the intersection has a
  crosswalk decal

#### Scenario: Shops at street level
- **WHEN** a place of interest that is open is rendered
- **THEN** its ground floor shows an emissive shopfront or lobby band with the door at the interior entrance

#### Scenario: Closed shops are shuttered
- **WHEN** a place of interest is closed
- **THEN** its ground floor shows a roller shutter instead of a lit shopfront

### Requirement: Light pools
Lamps, shopfronts, signs, vending machines and fires SHALL cast a coloured pool of light on the ground and, for lamps
and signs, on the nearest wall, drawn as decals so that it works on every device.

#### Scenario: Lamp light pools on the pavement
- **WHEN** a street lamp is rendered
- **THEN** a light pool decal in the lamp's colour is drawn on the ground beneath it

### Requirement: District architecture
Each district type SHALL have its own kit of shapes, window modules, ground floors, roof kits, props and signature set
pieces, so that districts can be told apart by shape as well as colour.

#### Scenario: Districts apart by shape and colour
- **WHEN** the six street-level district reference views are compared
- **THEN** their mean colours are separated by at least the palette distinctness threshold, and each view contains
  at least one prop or set piece that occurs in no other district's kit

### Requirement: Ambient motion
Signs, windows, beacons, steam, smoke, drones, litter and police lights SHALL animate as functions of the game time,
with some neon flickering or broken, so that the city moves when the player stands still.

#### Scenario: Neon flickers
- **WHEN** the signs of a Commercial district are checked over 10 seconds of game time
- **THEN** between 4 % and 12 % of them change brightness at least once, and the others stay steady

### Requirement: Detail settings
The settings SHALL offer street detail (low, medium, high), reflections, per-pixel lighting and brightness, with defaults by device
class, and SHALL keep chunk meshes, near sprites, draws and frame lights within the budgets for the chosen level.

#### Scenario: Detail setting
- **WHEN** street detail is lowered from high to low
- **THEN** every prop that carries a light is still drawn, and the number of other props drops to about a third

#### Scenario: Brightness lifts the dark
- **WHEN** the brightness setting is raised from 0 to +2
- **THEN** the darkest tones of the image get brighter while the fog distance and the emissive colours stay the same

#### Scenario: Atmosphere within budget
- **WHEN** the benchmark route is built at each detail level
- **THEN** no chunk mesh exceeds the vertex budget of its level, and draws, near sprites and frame lights stay within
  their limits

### Requirement: Colour grading
The post-processing pass SHALL apply a colour grade per district, blended when the camera crosses a district border,
and SHALL add a vignette by default.

#### Scenario: Grading follows the district
- **WHEN** the camera walks from one district into another
- **THEN** the grade blends from the first district's values to the second's over about 30 m with no sudden jump
