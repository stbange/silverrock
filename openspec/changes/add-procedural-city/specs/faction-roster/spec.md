# Spec Delta: faction-roster

## Purpose

Generates the factions of a city — megacorps with their black-ops divisions, gangs, government, police and minor
groups — together with their identities, assets, territory and initial relationships.

## ADDED Requirements

### Requirement: Megacorp count
Each city SHALL have between 5 and 10 megacorps, chosen randomly from the seed by default or fixed by the player in
the new-game options.

#### Scenario: Random count range
- **WHEN** cities are generated from 1,000 seeds with the default option
- **THEN** every city has between 5 and 10 megacorps and every count in that range occurs

#### Scenario: Fixed count
- **WHEN** the player sets the megacorp count to 6 in the new-game options
- **THEN** the generated city has exactly 6 megacorps

### Requirement: Megacorp identity
Each megacorp SHALL have a unique name, a primary and secondary sector, a doctrine, a colour pair, a procedural logo
and a black-ops division with its own codename and handler.

#### Scenario: Distinct identities
- **WHEN** a city is generated
- **THEN** megacorp names, logos and black-ops codenames are unique and primary colours differ by at least the minimum perceptual distance

### Requirement: Sector coverage
Each city SHALL contain at least one megacorp with Arms & Security and at least one with Cyberware as its primary or
secondary sector.

#### Scenario: Required sectors present
- **WHEN** cities are generated from 1,000 seeds
- **THEN** every city has a megacorp in each required sector

### Requirement: Corporate assets
Each megacorp SHALL own its headquarters in the Corporate Spire and at least three facilities spread over at least
two districts.

#### Scenario: Assets placed
- **WHEN** a city is generated
- **THEN** every megacorp owns a headquarters in the Spire and at least three facilities in two or more districts

### Requirement: Gangs
Each city SHALL have between 3 and 6 gangs, each with a name, theme, colours, a signature visual marker, a hideout
and turf of at least one district, never including the Corporate Spire or a Research Campus.

#### Scenario: Gang turf assigned
- **WHEN** a city is generated
- **THEN** every gang has a hideout inside its turf and no turf includes the Spire or a Campus

### Requirement: Government and police
Each city SHALL have a mayor, an odd number of councillors between 5 and 9, each with a political lean and patrons,
and a police department whose precincts cover every district.

#### Scenario: Council composition
- **WHEN** a city is generated
- **THEN** the council has 5, 7 or 9 members and every district is assigned to a police precinct

### Requirement: Minor groups
Each city SHALL have at least one fixer per district, at least two media outlets of which at least one is owned by a
megacorp, a hacker collective, a workers' union and the Church of the Deep Signal.

#### Scenario: Fixer coverage
- **WHEN** a city is generated
- **THEN** every district has at least one fixer with a haunt in that district

### Requirement: Initial relationships
Every pair of factions SHALL start with a relationship value, and every megacorp SHALL have at least one rival
megacorp.

#### Scenario: Every megacorp has a rival
- **WHEN** a city is generated
- **THEN** each megacorp's relationships include at least one rival megacorp

### Requirement: Recognisable factions
Every faction SHALL have a colour, a silhouette marker for its members and an icon, and no two factions in a city
SHALL share the same icon or marker.

#### Scenario: Distinct markers and icons
- **WHEN** a city is generated
- **THEN** all faction icons and member silhouette markers are pairwise distinct
