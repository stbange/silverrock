# Spec Delta: character-progression

## MODIFIED Requirements

### Requirement: Street Cred
Street Cred SHALL range from 0 to 100 in the tiers Nobody, Pawn, Runner, Operator, Knight and Legend, grow with notable
deeds rather than job volume (at most 6 points per in-game week), decay by one point per idle week above Runner, and
change how citizens react to the player. Tier Knight SHALL require Act II and patron trust of at least 40, and tier
Legend SHALL require Act III or a completed ending.

#### Scenario: Tier thresholds
- **WHEN** Street Cred crosses a tier threshold
- **THEN** the tier changes, the player is notified and higher job tiers become available accordingly

#### Scenario: Weekly cap
- **WHEN** the player completes ten top-tier jobs in one in-game week
- **THEN** Street Cred rises by no more than 6 that week

#### Scenario: Idle decay
- **WHEN** a player at tier Operator completes no job for seven in-game days
- **THEN** Street Cred drops by one

#### Scenario: Story gates
- **WHEN** a player in Act I would reach 70 Street Cred
- **THEN** Street Cred stays at 69 until Act II begins with patron trust of at least 40

#### Scenario: Paced rise
- **WHEN** the playtest bots play 26 weeks on 16 seeds
- **THEN** the median bot reaches Runner between days 21 and 35 and Knight between days 70 and 112
