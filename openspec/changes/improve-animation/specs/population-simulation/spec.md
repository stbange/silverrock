# Spec Delta: population-simulation

## ADDED Requirements

### Requirement: Street activities
Citizens who stop SHALL take up an activity chosen deterministically from their key, the district and the street props
nearby, alone or in small groups, and walk on when it ends.

#### Scenario: Walkers start and stop
- **WHEN** a citizen starts walking from a stand or stops
- **THEN** its speed changes over at least 0.25 s instead of in one step

#### Scenario: Activities fit the place
- **WHEN** a citizen stops within 3 m of a bench, stall, vending machine, crossing or club entrance
- **THEN** its activity is one of those listed for that prop or place

#### Scenario: People talk in groups
- **WHEN** two or three citizens stop within 2 m of each other and form a group
- **THEN** they face each other and at most one of them gestures as the speaker at a time

#### Scenario: The street is busy with small things
- **WHEN** a busy street at night is simulated for 10 minutes
- **THEN** at least half of the standing citizens around the camera are doing something other than plain idling

#### Scenario: Rain changes the walk
- **WHEN** a citizen without an umbrella walks in rain at least as strong as drizzle
- **THEN** it walks faster than its cruise speed with its shoulders hunched, and covers its head by its key

### Requirement: Visible reactions
Fear, curiosity and police intent SHALL show in movement and posture.

#### Scenario: Panic reads as panic
- **WHEN** citizens flee from gunfire
- **THEN** they run at 4.5 m/s with the run clip, one arm over the head, and look back at the danger

#### Scenario: Police show their intent
- **WHEN** a police officer is near the player at Heat 3 or more
- **THEN** the officer holds a weapon low and ready, and aims it during an armed response

### Requirement: Vehicle motion
Cars SHALL accelerate, brake and turn at junctions, and aerodynes SHALL land on roof pads and take off again,
deterministically from the seed and the clock.

#### Scenario: Cars brake and pull away
- **WHEN** a car stops for a pedestrian on the crossing ahead and the crossing clears
- **THEN** it slows to a stop and then accelerates back to its cruise speed, instead of stopping and starting in one step

#### Scenario: Cars turn at junctions
- **WHEN** a car chooses to turn at a junction
- **THEN** it blinks its indicator, follows a quarter circle and ends on the cross lane's offset, still on the road

#### Scenario: Aerodynes land on roofs
- **WHEN** an aerodyne within 200 m of the player is scheduled to land
- **THEN** it descends onto a helipad, waits with its beacon on and lifts off again, at the same time in two games with
  the same seed

#### Scenario: Trains stop at stations
- **WHEN** a metro train reaches a station on its timetable
- **THEN** it brakes, stands for about 20 s with its doors open and pulls away
