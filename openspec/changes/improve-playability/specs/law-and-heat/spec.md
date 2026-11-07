# Spec Delta: law-and-heat

## ADDED Requirements

### Requirement: Hunters
A hostile former patron and any faction at Heat 4 or more SHALL hunt the player: an ambush on the next street trip
when the player is outdoors, or a night raid at home, resolved as an encounter with the choices fight, flee or pay off.
Active bounties SHALL be listed in the gang channel.

#### Scenario: Hostile patron sends hunters
- **WHEN** the former patron is hostile and its hunt operation completes
- **THEN** the player meets an ambush within one in-game day of their next time outdoors, and the outcome changes
  health, cash or Heat

#### Scenario: Paying off the hunters
- **WHEN** the player chooses to pay off an ambush and has enough cred
- **THEN** the cred is deducted, nobody is hurt and that hunt ends

#### Scenario: Heat brings raids
- **WHEN** Heat with a faction reaches 4 and the player sleeps at home
- **THEN** a raid encounter happens that night unless the player sleeps elsewhere

#### Scenario: Bounty listing
- **WHEN** a hunt against the player is active
- **THEN** the terminal's Jobs tab lists the bounty in the gang channel
