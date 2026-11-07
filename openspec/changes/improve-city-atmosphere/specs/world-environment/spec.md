# Spec Delta: world-environment

## ADDED Requirements

### Requirement: The Pall overhead
The Pall SHALL be visible as a cloud ceiling above the city, lit from below by the districts beneath it, drifting with
the weather's wind, swept by searchlights from the corporate towers, and gone in the ending that removes it.

#### Scenario: The Pall glows over bright districts
- **WHEN** the Pall is rendered above the Entertainment district and above the Barrens
- **THEN** the ceiling above the Entertainment district is brighter and takes the district's neon hue

#### Scenario: No Pall at sunrise
- **WHEN** the Pall has been removed by the ending
- **THEN** the cloud ceiling and the searchlights are not drawn

#### Scenario: Searchlights
- **WHEN** a megacorp headquarters is in view from the street
- **THEN** its searchlight cones sweep the sky, and they are brighter while the corp's alert is raised

### Requirement: Weather looks
Each weather type SHALL have its own look: rain density and tint, fog colour and height, the Pall's colour, steam and
mist, so that clear, drizzle, downpour, fog, smog alert and acid-rain advisory can be told apart from a screenshot.

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
