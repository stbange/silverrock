# Spec Delta: population-simulation

## Purpose

Fills the city with citizens who look distinct, follow routines, persist, react to danger and weather, staff
services and move through traffic, so that the city lives whether or not the player is watching.

## ADDED Requirements

### Requirement: Crowd density by district and time
The number of citizens around the player SHALL follow per-district density tables by time of day.

#### Scenario: Strip at peak and off-peak
- **WHEN** the player stands at the same Entertainment district location at 01:00 and at 09:00
- **THEN** the number of citizens within 50 m at 01:00 is at least twice the number at 09:00

### Requirement: Distinguishable citizens
Citizens SHALL be generated from body type, head, face details, hairstyle, hair colour, clothing by role and class,
faction markers, visible cyberware and accessories, and no two citizens in the same crowd SHALL look identical.

#### Scenario: Unique appearances in a crowd
- **WHEN** 20 citizens are generated for the same block and time
- **THEN** no two of them share the same full appearance combination

#### Scenario: Faction members show their marker
- **WHEN** a citizen belongs to a gang or wears a corporate security uniform
- **THEN** they show that faction's silhouette marker and colour

### Requirement: Notable citizens
The game SHALL keep persistent notable citizens with a name, home, workplace, weekly schedule, faction, traits and
memory of the player, and they SHALL follow their schedules whether or not the player is nearby.

#### Scenario: Vendor keeps her hours
- **WHEN** the player visits a food stall during its vendor's working hours on two different days
- **THEN** the same notable vendor is present both times

#### Scenario: Notable remembers the player
- **WHEN** the player helps or harms a notable citizen and meets them again later
- **THEN** the citizen's reaction reflects the remembered interaction

### Requirement: Stable ambient crowds
Ambient citizens SHALL be generated deterministically from the location and time, so that the same place at the same
time has the same kind of crowd.

#### Scenario: Same corner, same crowd
- **WHEN** two games with the same seed are at the same block at the same in-game time
- **THEN** the generated ambient citizens are identical

### Requirement: Promotion to notable
An ambient citizen SHALL become a persistent notable citizen when the player interacts with them meaningfully by
talking, trading, hiring, robbing, harming or helping them.

#### Scenario: Trading promotes
- **WHEN** the player trades with an ambient citizen, leaves the area and returns the next day
- **THEN** that citizen still exists with the same appearance and follows a schedule

### Requirement: Level-of-detail simulation
Citizens SHALL be simulated in full near the player, simplified at mid range and analytically far away, and changes
between tiers SHALL not be visible to the player.

#### Scenario: Consistent after leaving view
- **WHEN** a notable citizen leaves the simulated range and the player finds them again 2 in-game hours later
- **THEN** they are at the place or on the route their schedule specifies for that time

#### Scenario: No pop-in nearby
- **WHEN** the camera moves through a crowded street
- **THEN** no citizen appears or disappears within 30 m of the camera while inside the view

### Requirement: Routines and shifts
Citizens SHALL commute, work, eat, shop, go out and sleep according to their schedules, producing rush hours at shift
changes.

#### Scenario: Shift-change rush
- **WHEN** a metro station near a workplace district is observed at a shift change and two hours later
- **THEN** the number of citizens at the station is higher at the shift change

### Requirement: Reactions to danger
Citizens SHALL perceive gunfire, explosions, fights and bodies and respond by fleeing, taking cover, gawking or
reporting, depending on their traits and the district.

#### Scenario: Gunfire panic
- **WHEN** a gunshot is fired in a crowded street
- **THEN** every citizen within hearing range starts a flee, cover or gawk response within 1 second

#### Scenario: Nobody reports in the Undercity
- **WHEN** a crime is witnessed in an Undercity district
- **THEN** no crime report event is emitted by ambient witnesses

#### Scenario: Witness reports in the Spire
- **WHEN** a crime is witnessed in the Corporate Spire
- **THEN** a crime report event with location and description of the perpetrator is emitted

### Requirement: Weather reactions
Citizens SHALL react to rain by using umbrellas, sheltering under cover or going indoors.

#### Scenario: Downpour empties the street
- **WHEN** the weather changes from clear to downpour
- **THEN** the share of outdoor citizens without cover or umbrella drops by at least half within 10 in-game minutes

### Requirement: Services and opening hours
Shops, vendors, bars, clinics and other services SHALL be staffed by notable citizens and open only during their
opening hours.

#### Scenario: Closed shop
- **WHEN** the player arrives at a shop outside its opening hours
- **THEN** the shop is locked and its staff is elsewhere according to their schedules

### Requirement: Traffic and transit
The city SHALL have ground vehicles on the road network, aerodynes in sky lanes and metro trains on timetables, and
ground vehicles SHALL stop for pedestrians at crossings.

#### Scenario: Metro timetable
- **WHEN** a metro station is observed during service hours
- **THEN** a train in each direction arrives at least every 5 in-game minutes

#### Scenario: Vehicles stop at crossings
- **WHEN** a pedestrian is on a crossing in front of an approaching vehicle
- **THEN** the vehicle stops before the crossing

### Requirement: Rumours
Citizens SHALL mention recent significant world events in their barks and conversations.

#### Scenario: Rumour about a recent event
- **WHEN** a significant event happened in a district within the last 24 in-game hours
- **THEN** barks of citizens in that district reference the event

### Requirement: Population scale
The game SHALL display at least 150 citizens in view at peak on the reference desktop and at least 60 on the
reference phone within the frame-rate budget.

#### Scenario: Peak crowd on desktop
- **WHEN** the busiest street of the reference seed is viewed at peak time on the reference desktop
- **THEN** at least 150 citizens are visible and the frame-rate budget is met
