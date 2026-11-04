# Spec Delta: matrix-navigation

## Purpose

Lets the player enter the Matrix — a separate, navigable 3D space mirroring the city — and move through it: access
points, the global grid, hosts, persona and programs, breaching, ICE, traces, the body left behind, data theft and
backdoors.

## ADDED Requirements

### Requirement: Access points
The player SHALL be able to dive from a home rig, a public jack-in booth, a physical network port in a building,
wirelessly within range of a building, or through a deployed data spike, and SHALL always need a cyberdeck.

#### Scenario: No deck, no dive
- **WHEN** the player tries to jack in without a cyberdeck
- **THEN** the dive is refused with a message

#### Scenario: Physical port skips the gateway
- **WHEN** the player jacks in at a network port inside a building
- **THEN** the persona appears inside that building's host behind its gateway

#### Scenario: Wireless range
- **WHEN** the player tries a wireless dive into a building's host from beyond the deck's signal range
- **THEN** the dive is refused, and within range it succeeds with reduced action speed

### Requirement: The global grid
Diving SHALL move the player into a separate 3D space whose global grid mirrors the city's layout: district plates and
backbone lines are arranged like the districts and arterial roads of the city.

#### Scenario: Grid mirrors the map
- **WHEN** the relative directions between any two districts are compared in the city and in the grid
- **THEN** they are the same

### Requirement: Hosts
Every networked building SHALL have a host with a deterministic layout and a security rating from 1 to 10 derived
from its owner's Matrix defence and the facility's importance.

#### Scenario: Every networked building has a host
- **WHEN** a city is generated
- **THEN** every building marked as networked has exactly one host

#### Scenario: Owner strength raises security
- **WHEN** the same facility type is owned by a faction with high Matrix defence and by one with low Matrix defence
- **THEN** the host of the stronger owner has the higher security rating

#### Scenario: Stable host layout
- **WHEN** the player enters the same host twice
- **THEN** its node layout is identical

### Requirement: Persona and deck
The persona SHALL have Integrity, Firewall, Bandwidth, Stealth and Signal, derived from the cyberdeck, MIND and
Netrunning.

#### Scenario: Better deck improves the persona
- **WHEN** the player replaces the deck with one of higher grade
- **THEN** none of the persona's stats is lower and at least one is higher

### Requirement: Programs
The player SHALL load attack, defence, stealth and utility programs up to the deck's memory limit, and each program
SHALL have a cooldown after use.

#### Scenario: Memory limit
- **WHEN** the player tries to load a program that exceeds the deck's free memory
- **THEN** loading is refused

#### Scenario: Program cooldown
- **WHEN** a program is used
- **THEN** it cannot be used again until its cooldown has expired

### Requirement: Breaching
Gateways and secured nodes SHALL be opened by a ring-alignment puzzle whose difficulty scales with security, or
bypassed with valid credentials or a backdoor, and the puzzle SHALL be playable with mouse, gamepad and touch.

#### Scenario: Breach difficulty scales
- **WHEN** breach puzzles are generated for security ratings 2 and 8
- **THEN** the rating-8 puzzle has more rings or faster rotation and the solver can solve both within their time limits

#### Scenario: Credentials skip the breach
- **WHEN** the player owns valid credentials for a host
- **THEN** its gateway opens without a puzzle

#### Scenario: Breach by touch
- **WHEN** a breach puzzle is played with the touch scheme
- **THEN** every ring can be selected, rotated and committed using touch input alone

### Requirement: ICE
Hosts SHALL be defended by Watcher, Barrier, Tar Pit, Tracer, Killer and Black ICE and by enemy sysops, and each host
SHALL have an alert level that rises when ICE detects the persona.

#### Scenario: Watcher raises the alert
- **WHEN** a Watcher detects the persona
- **THEN** the host alert level rises and additional ICE is spawned according to the host's security

#### Scenario: Killer ICE dumps the player
- **WHEN** Killer ICE reduces the persona's Integrity to zero
- **THEN** the player is ejected from the Matrix and suffers dumpshock (stun and health damage)

#### Scenario: Black ICE harms the body
- **WHEN** Black ICE hits the persona
- **THEN** the player's physical health is reduced

### Requirement: Traces
A Tracer that completes its trace SHALL reveal the player's physical location to the host's owner, raising Heat with
that owner and sending security to the body's location.

#### Scenario: Trace completed
- **WHEN** a Tracer completes its trace
- **THEN** the owner's Heat rises and a security team is dispatched to the body's location

### Requirement: The body left behind
During a dive the world SHALL continue in real time, the player SHALL see the body's surroundings in a small view and
be alerted when someone approaches, and SHALL be able to jack out instantly at the cost of dumpshock.

#### Scenario: Body alert
- **WHEN** a hostile approaches within 15 m of the diving player's body
- **THEN** a body alert is shown in the Matrix

#### Scenario: Emergency jack-out
- **WHEN** the player triggers an emergency jack-out
- **THEN** the player is back in the body within one second and suffers dumpshock

### Requirement: Paydata
Data stores SHALL contain typed paydata — research, finance, personnel, blackmail, security codes — with a value;
downloading SHALL take time depending on size and bandwidth, and paydata SHALL be sellable or deliverable to clients.

#### Scenario: Download time
- **WHEN** the same file is downloaded with two personas of different Bandwidth
- **THEN** the persona with higher Bandwidth finishes sooner

#### Scenario: Paydata as a job deliverable
- **WHEN** a job requires a data file and the player hands it over after downloading it
- **THEN** the objective is marked complete

### Requirement: Backdoors
A worm program SHALL leave a backdoor that lets the player re-enter the host past its gateway, until the host's
security discovers and removes it after a time that shrinks with the security rating.

#### Scenario: Backdoor re-entry
- **WHEN** the player returns to a host with an active backdoor
- **THEN** the persona enters past the gateway without a breach

#### Scenario: Backdoor expires
- **WHEN** the discovery time of a backdoor elapses
- **THEN** the backdoor is removed and the host's alert level rises

### Requirement: Matrix flight on every input scheme
The persona SHALL fly freely through the Matrix with keyboard and mouse, gamepad and touch, including vertical movement.

#### Scenario: Touch flight
- **WHEN** the player uses the touch scheme in the Matrix
- **THEN** the left stick moves and strafes, a right-side drag turns, and dedicated buttons move up and down
