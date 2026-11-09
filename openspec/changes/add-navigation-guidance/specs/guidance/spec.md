# Spec Delta: guidance

## ADDED Requirements

### Requirement: Tips
The game SHALL show one-time tips triggered by what happens to the player, one at a time for nine seconds with a fade,
with desktop and touch wording. Tips seen SHALL be remembered in the settings, not the save, and SHALL be resettable.

#### Scenario: Tips explain what happens
- **WHEN** the player first stands on the street, gets hungry, tired, hurt, wanted or fights
- **THEN** the matching tips queue and show once each, the seen ones are kept in the settings, and a reset shows them
  again

### Requirement: Current goal
With guidance on, the HUD SHALL show the current goal: the active job and where, reporting back, rent due, eating,
sleeping, or finding work, with the distance when the route follows it.

#### Scenario: Current goal
- **WHEN** the player is hungry, then tired, then fine, then short of rent, then on a job
- **THEN** the goal reads "EAT SOMETHING", "GET SOME SLEEP", "FIND WORK ...", "RENT DUE IN ...", "JOB: ..."

### Requirement: Guidance can be turned off
The tips and goal, the minimap and the route in the world SHALL each be switchable, all on by default. With guidance
off no tip or goal SHALL show, and the active job SHALL still be shown plainly.

#### Scenario: Guidance can be turned off
- **WHEN** the player switches guidance, the minimap and the route off on the help's settings page
- **THEN** tips, the goal, the minimap and the route in the world disappear, and they return when switched on

#### Scenario: Guidance settings persist
- **WHEN** the settings are written and read back, including a value longer than its field
- **THEN** the guidance switches and tips seen survive, and the long value is cut to fit

### Requirement: Help
The game SHALL offer help pages on controls, getting around, money, survival, Heat, stealth and combat, the Matrix and
the guidance settings, open from the pause screen and from play, with keyboard, pointer, touch and gamepad control,
returning to where it was opened.

#### Scenario: Help can be opened at any time
- **WHEN** the player opens help from the pause screen or from play, turns the pages and closes it
- **THEN** every page has a title and lines, the pages wrap around, and closing returns to the pause screen or to play
