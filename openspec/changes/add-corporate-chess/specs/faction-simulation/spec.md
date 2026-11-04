# Spec Delta: faction-simulation

## Purpose

Makes factions act: they pursue goals according to their doctrine and resources, turn operations into contracts,
react to outcomes, and visibly change the city — while the player only sees what their intel reveals.

## ADDED Requirements

### Requirement: Faction resources
Each faction SHALL track capital, influence, force, intel, Matrix defence and public image, each kept within its
configured bounds.

#### Scenario: Resources stay in bounds
- **WHEN** the faction simulation runs for 52 in-game weeks without the player on 20 seeds
- **THEN** every resource of every faction stays within its bounds

### Requirement: Strategic decisions
Every 6 in-game hours each faction SHALL evaluate its goals against its doctrine, resources and relationships and start
or advance operations, deterministically for a given seed and history of player actions.

#### Scenario: Deterministic strategy
- **WHEN** two games with the same seed receive the same player actions
- **THEN** all factions start the same operations at the same times

#### Scenario: Doctrine shapes choices
- **WHEN** the simulation runs for 52 in-game weeks on 50 seeds
- **THEN** megacorps with the Shark doctrine start more sabotage and takeover operations on average than those with the Fortress doctrine

### Requirement: Contracts from operations
Each operation step that needs work in the city SHALL become a contract offered through a channel fitting the client:
the patron's handler, fixers, gang contacts or the job board.

#### Scenario: Operation step becomes a contract
- **WHEN** a faction starts an operation whose next step requires stealing data from a facility
- **THEN** a matching job appears in a channel fitting that faction

### Requirement: Off-screen resolution
Contracts nobody takes before their deadline SHALL be attempted by NPC crews and resolved by odds based on the
client's resources and the target's defences, with the same kinds of world effects as player outcomes.

#### Scenario: Unclaimed contract resolved
- **WHEN** the player ignores a contract until its deadline
- **THEN** the contract is resolved off-screen and its outcome is applied to the world

### Requirement: Counterplay
Factions targeted by an operation SHALL respond by raising security at affected facilities, investigating, and
retaliating against the faction or person the evidence points to.

#### Scenario: Security rises after an attack
- **WHEN** a facility is successfully attacked
- **THEN** its security level is higher at the next strategic tick

#### Scenario: Retaliation follows attribution
- **WHEN** evidence attributes an attack to a faction
- **THEN** the victim starts an operation against that faction within the next 7 in-game days

### Requirement: Attribution and framing
Factions SHALL blame whoever the available evidence points to, and evidence planted by the player SHALL redirect blame
to the framed faction.

#### Scenario: Framing redirects blame
- **WHEN** the player plants a gang's evidence at a sabotaged facility and leaves no own evidence
- **THEN** the facility owner attributes the attack to that gang

### Requirement: Visible consequences
Operation outcomes SHALL change things the player can see in the city: asset ownership, facility security, patrol
density, prices, gang turf, the fate of people, and advertising.

#### Scenario: Sabotage raises prices
- **WHEN** a synthfood plant is sabotaged successfully
- **THEN** food prices in the districts it supplies rise at the next strategic tick

#### Scenario: Turf change is visible
- **WHEN** a gang takes over a district from another gang
- **THEN** graffiti and gang members in that district change to the new gang

### Requirement: Gang turf wars
Gangs SHALL contest districts, and a gang that loses all its turf SHALL be dissolved and replaced by a newly generated
gang after a cooldown.

#### Scenario: Gang replaced
- **WHEN** a gang loses its last district
- **THEN** it is dissolved and a new gang with a new identity appears within 4 in-game weeks

### Requirement: Stable board
No megacorp SHALL be eliminated before the endgame.

#### Scenario: Long soak keeps every megacorp
- **WHEN** the simulation runs for 52 in-game weeks without the player
- **THEN** every megacorp still exists and owns its headquarters

### Requirement: Intel Board
The player's Board SHALL show factions, links and operations only after the player has acquired intel revealing them.

#### Scenario: Intel reveals a link
- **WHEN** the player reads a stolen file proving that a megacorp funds a gang
- **THEN** the Board shows a funding link between them, and before that it did not
