# Spec Delta: character-progression

## Purpose

Turns experience into a stronger character and better opportunities: attributes, skills that improve by use, levels,
perks, global reputation (Street Cred), standing with each faction, and respecs.

## ADDED Requirements

### Requirement: Attributes
Characters SHALL have five attributes — BODY, REFLEX, TECH, MIND and COOL — with values from 1 to 10.

#### Scenario: Attribute range
- **WHEN** the player tries to raise an attribute that is already 10
- **THEN** the increase is refused and the point is kept

### Requirement: Skills improve by use
Characters SHALL have twelve skills with ranks 0 to 20 that gain experience when used meaningfully, for example by
hitting a hostile target, but not by trivial use such as shooting walls.

#### Scenario: Using a skill raises it
- **WHEN** the player lands enough pistol hits on hostile targets to fill the next rank's experience
- **THEN** the Pistols skill increases by one rank and the player is notified

#### Scenario: Trivial use grants nothing
- **WHEN** the player shoots at walls for 10 minutes
- **THEN** the Pistols skill gains no experience

### Requirement: Character level
Characters SHALL start at level 1, and job experience SHALL raise the level up to 50, granting one perk point per
level gained and one attribute point at every level divisible by three.

#### Scenario: Level-up rewards
- **WHEN** a new character advances from level 1 to level 4
- **THEN** the player has received three perk points and one attribute point

### Requirement: Perks
Each skill SHALL have a perk tree whose tiers unlock at skill ranks 5, 10, 15 and 20.

#### Scenario: Perk locked until rank
- **WHEN** the player tries to buy a rank-10 perk with the skill at rank 9
- **THEN** the purchase is refused and the required rank is shown

### Requirement: Street Cred
Street Cred SHALL range from 0 to 100 in the tiers Nobody, Pawn, Runner, Operator, Knight and Legend, grow with
completed jobs and notable deeds, and change how citizens react to the player.

#### Scenario: Tier thresholds
- **WHEN** Street Cred crosses a tier threshold
- **THEN** the tier changes, the player is notified and higher job tiers become available accordingly

### Requirement: Faction standing
Each faction SHALL track the player's standing from −100 to +100, which SHALL change only when the faction attributes an
action to the player, and which affects prices, job offers and hostility.

#### Scenario: Unattributed action
- **WHEN** the player sabotages a corporate facility without leaving evidence
- **THEN** the player's standing with that corporation is unchanged

#### Scenario: Hostile faction attacks
- **WHEN** the player's standing with a gang falls to Hostile
- **THEN** members of that gang attack the player on sight

### Requirement: Respec
A clinic SHALL offer neural reconditioning that refunds spent attribute and perk points for a price.

#### Scenario: Respec refunds points
- **WHEN** the player pays for reconditioning
- **THEN** all spent attribute and perk points are refunded and can be reassigned
