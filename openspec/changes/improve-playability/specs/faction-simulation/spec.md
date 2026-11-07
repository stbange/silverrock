# Spec Delta: faction-simulation

## MODIFIED Requirements

### Requirement: Contracts from operations
Each operation step that needs work in the city SHALL become a contract offered through a channel fitting the client:
the patron's handler, fixers, gang contacts or the job board. Fixers SHALL offer only contracts, and when a fixer's
district has too few open contracts, its factions SHALL start operations on demand, so that every job the player can
take is a step in some faction's operation.

#### Scenario: Operation step becomes a contract
- **WHEN** a faction starts an operation whose next step requires stealing data from a facility
- **THEN** a matching job appears in a channel fitting that faction

#### Scenario: Fixer offers come from contracts
- **WHEN** the player opens a fixer's offers
- **THEN** every offer belongs to an open contract, and completing it advances that contract's operation

#### Scenario: Operations on demand
- **WHEN** a fixer's district and its neighbours have fewer than three open contracts of unlocked tiers
- **THEN** factions active there start operations until three offers exist, at most one per district per day

## ADDED Requirements

### Requirement: Reachable dominance
Successful takeovers SHALL raise the owner's long-run capital level, and the dominance threshold SHALL scale with the
number of megacorps as clamp(2.2 / megacorps, 25 %, 40 %).

#### Scenario: Takeovers accumulate
- **WHEN** a megacorp completes three takeovers
- **THEN** its long-run capital level is 24 higher than at the start

#### Scenario: Threshold scales with the board
- **WHEN** a city has ten megacorps
- **THEN** the dominance threshold is 25 %
