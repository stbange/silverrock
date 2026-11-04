# Spec Delta: retro-rendering

## Purpose

Defines how the city looks: a low-resolution, retro 3D rendering style that stays readable — distinct districts,
visible landmarks, legible signage — within frame-rate budgets on desktop and mobile.

## ADDED Requirements

### Requirement: Low-resolution rendering
The 3D scene SHALL be rendered at a selectable internal resolution of 320×180, 480×270 (default), 640×360 or native,
and upscaled to the screen with nearest-neighbour filtering, optionally restricted to integer scale factors.

#### Scenario: Default internal resolution
- **WHEN** a new installation starts the game
- **THEN** the 3D scene is rendered at 480×270 and upscaled without filtering

#### Scenario: Integer scaling
- **WHEN** integer scaling is enabled on a 1920×1080 screen with a 480×270 internal resolution
- **THEN** the image is scaled exactly 4 times

### Requirement: Retro effect toggles
Dithering, colour quantisation, vertex snapping, affine texture mapping and the CRT effect SHALL each be toggleable
independently.

#### Scenario: Independent toggles
- **WHEN** one retro effect is switched off and all others stay on
- **THEN** only that effect disappears from the reference image

### Requirement: Night lighting
The renderer SHALL light scenes with many coloured point lights from neon, windows, vehicles and muzzle flashes,
using up to 8 most influential lights per object, and emissive surfaces SHALL remain visible through fog.

#### Scenario: Neon through fog
- **WHEN** an emissive sign is at the fog end distance
- **THEN** its colour still contrasts with the fog by at least the minimum contrast

### Requirement: District visual identity
Each district type SHALL have its own colour palette and fog tint, so that different district types can be told
apart from a single screenshot.

#### Scenario: Palettes differ
- **WHEN** the palettes of any two different district types are compared
- **THEN** their colour distance is above the minimum distinctness threshold

### Requirement: Landmark beacons
The holo-logos of megacorp headquarters SHALL be rendered beyond the normal fog distance and be visible from any
outdoor position with an unobstructed line of sight.

#### Scenario: Logo beyond fog
- **WHEN** a headquarters logo is twice the fog distance away with an unobstructed line of sight
- **THEN** the logo is visible in the rendered image

### Requirement: Skyline horizon
The horizon SHALL always show a city skyline silhouette, so that the world never ends in an empty background.

#### Scenario: No void at the horizon
- **WHEN** reference views from city edges and high rooftops are rendered
- **THEN** every pixel column below the horizon line contains skyline or scene geometry

### Requirement: Rain and wet surfaces
Rain SHALL be rendered as particles that do not fall under cover, and wet streets SHALL reflect emissive lights.

#### Scenario: No rain under cover
- **WHEN** the camera is under a roof or awning during a downpour
- **THEN** no rain particles are rendered in the covered volume

### Requirement: Readable signage
Shop signs SHALL show an icon for the business type and the generated name in a bitmap font, legible at the default
internal resolution from 10 m.

#### Scenario: Sign legibility
- **WHEN** a shop sign is viewed head-on from 10 m at the default internal resolution
- **THEN** its icon covers at least 12×12 pixels

### Requirement: Readable UI
The user interface SHALL be rendered at an integer-scaled UI resolution independent of the 3D internal resolution,
with a configurable UI scale.

#### Scenario: Text on a phone
- **WHEN** the game runs on a 390 CSS-pixel-tall landscape phone screen at the default UI scale
- **THEN** body text is at least 12 CSS pixels tall

### Requirement: Frame-rate budget
The game SHALL hold 60 frames per second on the reference desktop and at least 30 frames per second (95th
percentile) on the reference phone along the benchmark route, lowering the internal resolution when needed.

#### Scenario: Desktop benchmark route
- **WHEN** the benchmark route is played on the reference desktop at 1080p
- **THEN** at least 99 % of frames take at most 16.7 ms

#### Scenario: Dynamic resolution engages
- **WHEN** frame times exceed the budget for 2 seconds with dynamic resolution enabled
- **THEN** the internal resolution is lowered one step and raised again once the budget is met for 10 seconds
