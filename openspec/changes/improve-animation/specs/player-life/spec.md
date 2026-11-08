# Spec Delta: player-life

## ADDED Requirements

### Requirement: First-person camera motion
The first-person camera SHALL bob with the player's steps, dip on landing, ease into a crouch, flinch toward the
source of damage and shake with explosions, and the player SHALL be able to turn head bob and camera shake off.

#### Scenario: Head bob
- **WHEN** the player walks with head bob on
- **THEN** the eye rises and falls by about 3 cm per step and sways sideways by about 1.5 cm, twice as much while
  sprinting and half as much in a crouch

#### Scenario: Landing dips the view
- **WHEN** the player lands from a jump or a drop
- **THEN** the eye dips by an amount proportional to the fall speed, at most 8 cm, and recovers

#### Scenario: Damage shows its direction
- **WHEN** the player takes damage from an enemy
- **THEN** the view flinches away from the source and a red edge shows on the side the hit came from

#### Scenario: Head bob can be turned off
- **WHEN** head bob is turned off in the settings
- **THEN** the eye height while walking and sprinting is constant, and aiming and movement are unchanged

#### Scenario: Shake can be turned off
- **WHEN** camera shake is set to off
- **THEN** explosions and heavy landings do not move the camera
