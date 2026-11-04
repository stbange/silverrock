# Spec Delta: world-environment

## Purpose

Provides the world clock and calendar that drive routines and rent cycles, time skipping, the permanent night under
the Pall, and deterministic weather.

## ADDED Requirements

### Requirement: World clock
The in-game clock SHALL advance 20 in-game minutes per real minute of unpaused play by default, with a configurable
rate, and SHALL stop while the game is paused or a full-screen menu is open.

#### Scenario: Default time scale
- **WHEN** 3 real minutes of unpaused gameplay pass at the default rate
- **THEN** the in-game clock has advanced by 60 minutes

#### Scenario: Clock stops in menus
- **WHEN** the pause menu is open for 5 real minutes
- **THEN** the in-game clock has not advanced

### Requirement: Calendar
The game SHALL track in-game days, weekdays and weeks, starting a new game on a Monday evening, and SHALL show the
current day and time to the player.

#### Scenario: Week rollover
- **WHEN** the clock passes Sunday 23:59
- **THEN** the weekday becomes Monday and the week counter increases by one

### Requirement: Time skipping
Sleeping, waiting and travelling SHALL advance the clock by the skipped duration, and the world SHALL be in the state
it would have reached had the time been played.

#### Scenario: Waiting one hour
- **WHEN** the player waits for one in-game hour
- **THEN** the clock advances by exactly one hour and time-dependent world state, such as the weather, matches the state at the new time

### Requirement: Permanent night
The sky and ambient light SHALL show night at every clock time, with no sun or daylight, except in the story ending
that removes the Pall.

#### Scenario: Night at noon
- **WHEN** the clock shows 12:00 outside the Pall-removal ending
- **THEN** sky colour and ambient light are within the night tolerance of their values at 00:00

### Requirement: Weather
The weather SHALL change between clear, drizzle, downpour, fog, smog alert and acid-rain advisory with gradual
transitions, determined by the seed and the clock.

#### Scenario: Deterministic weather
- **WHEN** two games with the same seed reach the same date and time
- **THEN** they have the same weather state

#### Scenario: Gradual transition
- **WHEN** the weather changes from one state to another
- **THEN** the transition takes at least 5 in-game minutes

### Requirement: Weather effects
Weather SHALL change visibility distance and surface wetness and SHALL be readable by other systems, so that
citizens and perception can react to it.

#### Scenario: Downpour reduces visibility
- **WHEN** the weather is downpour or fog
- **THEN** the visibility distance reported to perception and rendering is lower than in clear weather
