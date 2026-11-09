# Spec Delta: world-environment

## MODIFIED Requirements

### Requirement: Permanent night
The sky and ambient light SHALL show night for at least 19 of every 24 hours, with a short overcast day of about three
hours around noon framed by a dawn and a dusk of about one hour each. The sun itself SHALL never be visible outside the
story ending that removes the Pall, and at every hour the sky SHALL stay darker than the city's neon.

#### Scenario: Night at midnight
- **WHEN** the clock shows any time between 15:00 and 10:00 outside the Pall-removal ending
- **THEN** sky colour and ambient light are within the night tolerance of their values at 00:00

#### Scenario: Noon stays dark
- **WHEN** the clock shows 12:30 outside the Pall-removal ending
- **THEN** the sky is overcast with a mean luminance of at most 0.25, ambient light is at most 1.6 times its night
  value, and no sun disc is drawn

#### Scenario: Short days
- **WHEN** the light phase is sampled every minute over one day
- **THEN** it is night for at least 19 hours, dawn lies between 10:00 and 11:00 and dusk between 14:00 and 15:00

#### Scenario: Dusk glows red in the west
- **WHEN** the clock shows 14:30 in weather other than downpour or fog
- **THEN** the sky and fog toward the west show a red to orange band low on the horizon, under the clouds

### Requirement: Weather
The weather SHALL change between overcast, drizzle, downpour, fog, smog alert and acid-rain advisory with gradual
transitions, determined by the seed and the clock, with rain falling most of the time and some fog always present.

#### Scenario: Deterministic weather
- **WHEN** two games with the same seed reach the same date and time
- **THEN** they have the same weather state

#### Scenario: Gradual transition
- **WHEN** the weather changes from one state to another
- **THEN** the transition takes at least 5 in-game minutes

#### Scenario: Rain most of the time
- **WHEN** the weather of every slot over one simulated year is counted
- **THEN** rain falls in at least 85 % of the slots, and the street wetness never drops below 0.6

### Requirement: Weather effects
Weather SHALL change visibility distance and surface wetness, with visibility never above 100 m at street level
outside the Pall-removal ending, and SHALL be readable by other systems, so that citizens and perception can react to it.

#### Scenario: Downpour reduces visibility
- **WHEN** the weather is downpour or fog
- **THEN** the visibility distance reported to perception and rendering is lower than in overcast weather

#### Scenario: Short sight lines
- **WHEN** the visibility is sampled over one simulated year outside the Pall-removal ending
- **THEN** it never exceeds 100 m, and it is at most 75 m in every weather with rain

## ADDED Requirements

### Requirement: The Pall overhead
The Pall SHALL be visible as a low, heavy cloud deck above the city that drifts with the weather's wind, is lit from
below by the districts beneath it at night, is dark slate grey by day, picks up the dawn and dusk glow, is swept by
searchlights from the corporate towers, and is gone in the ending that removes it.

#### Scenario: The Pall glows over bright districts
- **WHEN** the cloud deck is rendered at night above the Entertainment district and above the Barrens
- **THEN** the deck above the Entertainment district is brighter and takes the district's neon hue

#### Scenario: No Pall at sunrise
- **WHEN** the Pall has been removed by the ending
- **THEN** the cloud deck and the searchlights are not drawn

#### Scenario: Searchlights
- **WHEN** a megacorp headquarters is in view from the street
- **THEN** its searchlight cones sweep up into the clouds, and they are brighter while the corp's alert is raised

### Requirement: Weather looks
Each weather type SHALL have its own look: rain density and tint, fog colour and height, the cloud deck's colour, steam
and mist, so that overcast, drizzle, downpour, fog, smog alert and acid-rain advisory can be told apart from a
screenshot.

#### Scenario: Each weather looks different
- **WHEN** the same street view is rendered in each of the six weather types
- **THEN** every pair of images differs in mean colour or in visible rain by more than the weather distinctness
  threshold

#### Scenario: Mist hugs the ground
- **WHEN** it is foggy in the Undercity or at the Docks
- **THEN** fog is denser below 3 m than above it

#### Scenario: Steam rises from the street
- **WHEN** a steaming manhole or vent is within the animated sprite distance
- **THEN** steam sprites rise from it, and twice as many are drawn while it rains
