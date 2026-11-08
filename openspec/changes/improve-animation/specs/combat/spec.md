# Spec Delta: combat

## ADDED Requirements

### Requirement: Readable enemy animation
Enemies SHALL hold their weapons and show their alert state, every hit, falls to death or knockout, surrender,
restraint and stun in their pose, and their shots SHALL be visible.

#### Scenario: Alert state shows in the stance
- **WHEN** a guard is unaware, suspicious, searching or in combat
- **THEN** it walks with the weapon slung, turns toward the noise, walks with the weapon raised, or aims at the player,
  respectively

#### Scenario: Hits show
- **WHEN** a shot hits a guard who survives
- **THEN** the guard flinches away from the shot within one frame, and staggers if the hit dealt at least 35 % of its
  maximum health

#### Scenario: The dead fall
- **WHEN** a guard is killed or knocked out
- **THEN** it topples away from the last hit, or slumps knees first, over 0.6–0.9 s and ends lying on the floor

#### Scenario: Bodies stay down after a load
- **WHEN** a site with downed guards is loaded from a save
- **THEN** the guards lie in their final pose with no fall replayed

#### Scenario: Surrender shows
- **WHEN** a guard surrenders, is restrained or is stunned by a flashbang
- **THEN** it raises its hands and drops the weapon, kneels with its hands behind its back, or covers its eyes,
  respectively

#### Scenario: Enemy fire can be traced
- **WHEN** a guard fires in a dark room
- **THEN** a muzzle flash is drawn at its weapon, a short light brightens the wall behind it, and every third round
  draws a tracer toward the target
