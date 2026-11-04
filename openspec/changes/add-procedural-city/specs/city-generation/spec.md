# Spec Delta: city-generation

## Purpose

Generates the playable city — geography, districts, transport, buildings, places of interest, interiors and names —
deterministically from a seed, streams its details around the player and guarantees that every city is valid.

## ADDED Requirements

### Requirement: Deterministic generation
The game SHALL generate the same city for the same seed and generator version on every target, and different cities
for different seeds.

#### Scenario: Same seed twice
- **WHEN** a city is generated twice from the same seed and generator version
- **THEN** both cities have identical world digests

#### Scenario: Identical across targets
- **WHEN** the reference seeds are generated on Windows, Linux and Web
- **THEN** the world digests are identical on all three targets

#### Scenario: Different seeds
- **WHEN** cities are generated from 1,000 different seeds
- **THEN** no two cities share a world digest

### Requirement: Seed codes
The game SHALL accept a random or player-entered seed, display it as a shareable code that includes a checksum, and
reject codes whose checksum does not match.

#### Scenario: Seed code round trip
- **WHEN** a seed is converted to its code and the code is entered in the new-game screen
- **THEN** the same city is generated

#### Scenario: Mistyped seed code
- **WHEN** the player enters a code with one wrong character
- **THEN** the code is rejected with a message and no city is generated

### Requirement: District composition
Each city SHALL consist of 8 to 14 districts, with exactly one Corporate Spire and one Civic Center and at least one
Residential, Commercial, Industrial, Entertainment and Undercity district. Docks SHALL only be placed on a coast.

#### Scenario: Required districts present
- **WHEN** cities are generated from 1,000 seeds
- **THEN** every city has between 8 and 14 districts and contains all required district types in the required numbers

#### Scenario: Docks on the coast
- **WHEN** a city contains a Docks district
- **THEN** that district borders open water

### Requirement: Connectivity
Every public walkable location SHALL be reachable on foot from the player's home, every district SHALL have at least
one metro station, and the metro network SHALL be connected.

#### Scenario: Everything is reachable
- **WHEN** a city is generated
- **THEN** a path search from the player's home reaches every public walkable location

#### Scenario: Metro serves every district
- **WHEN** a city is generated
- **THEN** every district has at least one metro station and every station can be reached from every other station by metro

### Requirement: Vertical layers
The city SHALL contain an Undercity layer below elevated structures, the street layer, skyways in rich districts and
accessible rooftops, and every skyway and rooftop area SHALL be reachable from the street by stairs, elevator,
ladder or grapple point.

#### Scenario: Skyway access
- **WHEN** a city is generated
- **THEN** every skyway segment has at least one access route from street level

### Requirement: Starter neighbourhood
The player's home SHALL be a studio apartment in a Residential district within 150 m walking distance of a food
vendor, a fixer and a clinic, and within 300 m of a metro station.

#### Scenario: Starter services nearby
- **WHEN** a city is generated
- **THEN** the walking distances from the player's home to the nearest food vendor, fixer, clinic and metro station are within the limits

### Requirement: Places of interest
The city SHALL contain shops, food vendors, bars, clinics, ripperdocs, gun shops, fences, capsule hotels, public
Matrix booths, fixer haunts and police precincts, distributed according to per-district weightings.

#### Scenario: Service availability
- **WHEN** a city is generated
- **THEN** every district contains at least one food vendor and every place-of-interest type exists somewhere in the city

### Requirement: Landmarks
Each megacorp SHALL have a landmark headquarters tower in the Corporate Spire with a silhouette unique among all
headquarters, and the city SHALL have one City Hall in the Civic Center.

#### Scenario: Unique headquarters silhouettes
- **WHEN** a city with N megacorps is generated
- **THEN** there are N headquarters towers in the Spire and no two share the same silhouette parameters

### Requirement: Enterable interiors
Every place of interest and every mission-capable building SHALL be enterable. Mission-capable interiors SHALL offer
at least two distinct entry routes and a complete security chain linking each camera and lock to a control point.

#### Scenario: Two routes into mission sites
- **WHEN** the interior of a mission-capable building is generated
- **THEN** it has at least two entry routes that share no doorway, vent or window

#### Scenario: Stable interiors
- **WHEN** the player leaves an interior and enters it again later
- **THEN** the layout is identical, plus any changes made earlier

### Requirement: Generated names
The game SHALL generate names for the city, districts, streets, businesses and factions, keep district names unique
within a city, and never use a name on the real-world blocklist.

#### Scenario: Blocklisted name never appears
- **WHEN** names are generated for 2,000 seeds
- **THEN** none of them matches an entry of the blocklist after normalising case, spacing and punctuation

#### Scenario: Unique district names
- **WHEN** a city is generated
- **THEN** no two districts share a name

### Requirement: Chunk streaming
The game SHALL generate world detail around the player on demand within a per-frame time budget, and revisiting an
area SHALL produce identical content plus all changes made there.

#### Scenario: Returning to an area
- **WHEN** the player leaves an area until it is unloaded and later returns
- **THEN** its detail digest is unchanged and earlier changes, such as an opened door, are still present

#### Scenario: Generation budget respected
- **WHEN** the player runs the benchmark route on the reference desktop
- **THEN** world detail generation never takes more than 4 ms of a single frame

### Requirement: Self-validation
The generator SHALL check every city against its invariants and resolve failures by deterministically re-generating
the affected stage, so that an invalid city is never presented.

#### Scenario: Invariant failure is re-rolled
- **WHEN** a generation stage violates an invariant
- **THEN** that stage is re-generated from a derived seed and the final city passes all invariants

### Requirement: Generation time
The initial city generation SHALL complete within 3 seconds on the reference desktop and 6 seconds on the reference
phone, with visible progress.

#### Scenario: Macro generation time
- **WHEN** a new city is generated on the reference desktop
- **THEN** generation finishes within 3 seconds and a progress indicator is shown meanwhile
