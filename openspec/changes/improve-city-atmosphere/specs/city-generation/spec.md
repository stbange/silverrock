# Spec Delta: city-generation

## ADDED Requirements

### Requirement: Derived building detail
Facades, building shapes and roof kits SHALL be derived on demand from the seed and the existing building data with
hashes, without consuming the generator's random streams and without changing the city data, and every shape SHALL
stay inside the building's ground footprint.

#### Scenario: Derived detail keeps the digest
- **WHEN** a city is generated before and after the derived detail is computed for every building
- **THEN** the city digest is unchanged, and the digest of the derived facades for a fixed seed matches its pinned
  value

#### Scenario: Shapes stay inside the footprint
- **WHEN** the shapes of all buildings on five seeds are built
- **THEN** no part below 3 m lies outside the building's footprint

### Requirement: Street dressing
Every block SHALL get street props placed deterministically from the seed in a kerb band and a building band along each
pavement, with kinds and weights chosen per district type, lamps at most 24 m apart along every street, a clear
walking lane between the bands, and solid props that block movement.

#### Scenario: Street furniture by district
- **WHEN** the dressing of every block on five seeds is counted
- **THEN** every district type except the Barrens has at least 6 props per 100 m of kerb, and the props of each
  district type follow its kit

#### Scenario: Lamps along every street
- **WHEN** the lamps along any street segment are listed
- **THEN** no two neighbouring lamps on the same side are more than 24 m apart

#### Scenario: The walking lane stays clear
- **WHEN** the solid props of every block are checked
- **THEN** none lies in the clear walking lane or in front of a door of an enterable building

#### Scenario: Solid props block movement
- **WHEN** the player walks into a dumpster or vending machine
- **THEN** the player is stopped at its edge, and every place of interest is still reachable on foot

### Requirement: Alleys
Gaps of at least 4 m between neighbouring buildings SHALL be dressed as alleys with clutter, a weak light and a steam
vent, more densely in the Undercity and the Old Quarter.

#### Scenario: Alleys
- **WHEN** a block has two buildings at least 4 m apart
- **THEN** the gap between them contains at least one light and two alley props

### Requirement: Vertical structures
Elevated highways SHALL have decks on pillars with barriers, lamps and traffic lights, the land beneath them SHALL be
darker and dressed as the Undercity, Spire towers close to each other SHALL be linked by skyways, and metro lines SHALL
run on elevated tracks with trains passing on the metro timetable.

#### Scenario: Life under the highway
- **WHEN** the camera stands under a highway deck
- **THEN** pillars and hanging lights are in view, and the ambient light is lower than in the open street next to it

#### Scenario: Skyways
- **WHEN** two Spire towers taller than 100 m stand between 40 m and 90 m apart
- **THEN** at least one pair of such towers in the district is linked by a lit skyway

#### Scenario: Trains pass overhead
- **WHEN** the metro timetable has a train between two stations
- **THEN** a lit train is drawn on the elevated track at the position the timetable gives
