# Spec Delta: job-system

## Purpose

Lets the player find, negotiate, execute and complete jobs generated from the live world state, with tiers that grow
with reputation, several approaches per job and consequences that feed back into the world.

## ADDED Requirements

### Requirement: Job sources
The game SHALL offer jobs through the public job board on the terminal, fixers in their haunts, personal contacts and
opportunities created by world events.

#### Scenario: Fixer offers jobs
- **WHEN** the player talks to a fixer during the fixer's hours
- **THEN** the fixer offers between one and four jobs of tiers the player has unlocked

#### Scenario: Job board listings
- **WHEN** the player opens the job board on a terminal
- **THEN** it lists the currently available public jobs with tier, client type, pay and deadline

### Requirement: Jobs bound to the world
Every job SHALL have a client, an objective, a target, one or more locations, constraints, a deadline, pay and a risk
tier, and its client, target and locations SHALL be existing entities of the current world.

#### Scenario: Jobs reference real entities
- **WHEN** 1,000 jobs are generated across 50 seeds
- **THEN** every job's client, target and locations exist in that world at the time the job is offered

### Requirement: Job tiers
Jobs SHALL be grouped into tiers T0 to T4 with increasing pay ranges, and a tier SHALL only be offered once the player
has reached its Street Cred tier: T0 Nobody, T1 Pawn, T2 Runner, T3 Operator, T4 Knight.

#### Scenario: Tier gating
- **WHEN** a player at Street Cred tier Nobody looks for jobs
- **THEN** no job above T0 is offered, and after reaching Runner, T2 jobs are offered

#### Scenario: Pay within tier range
- **WHEN** jobs of each tier are generated
- **THEN** their base pay lies within the tier's configured range

### Requirement: Briefings and intel
Each job SHALL come with a briefing that separates known facts from unknowns, and the player SHALL be able to buy or
gather intel that reveals further details such as guard numbers, alternative entrances or security level.

#### Scenario: Buying intel reveals details
- **WHEN** the player buys intel for a job
- **THEN** the briefing shows additional site details that were previously marked unknown

### Requirement: Negotiation
The player SHALL be able to negotiate pay once per job using Persuasion; success raises the pay by up to 30 %, failure
lowers the offer or makes the client withdraw it.

#### Scenario: Successful negotiation
- **WHEN** a negotiation check succeeds
- **THEN** the pay increases by at most 30 %

#### Scenario: Failed negotiation
- **WHEN** a negotiation check fails
- **THEN** the pay decreases or the offer is withdrawn, and the player cannot negotiate this job again

### Requirement: Multiple approaches
Every job SHALL be solvable by at least two distinct approaches among stealth, social engineering, the Matrix and
force, and the generator SHALL verify this before offering the job.

#### Scenario: Two approaches validated
- **WHEN** a job site is generated
- **THEN** autoplayer bots using two different approach strategies can each complete the job

### Requirement: Objectives and payment
The game SHALL track job objectives, mark them in the HUD, verify their completion, pay any upfront share on
acceptance and the remainder on completion, and pay extra for completed bonus objectives.

#### Scenario: Payment on completion
- **WHEN** the player completes all main objectives and reports back
- **THEN** the remaining pay is transferred and the job is marked completed

#### Scenario: Bonus objective
- **WHEN** the player also completes a bonus objective
- **THEN** the bonus amount is added to the payment

### Requirement: Job constraints
Jobs SHALL support constraints such as no kills, no alarms, a deadline or keeping an item intact, and violating a
constraint SHALL reduce the pay or fail the job as stated in the briefing.

#### Scenario: No-kill constraint violated
- **WHEN** the player kills someone during a job with a no-kill constraint
- **THEN** the pay is reduced or the job fails exactly as the briefing stated

### Requirement: Failure without game over
A job SHALL fail when its deadline passes, its objective becomes impossible or the player abandons it; failure SHALL
reduce standing with the client but never end the game.

#### Scenario: Deadline missed
- **WHEN** a job's deadline passes before completion
- **THEN** the job is marked failed, client standing decreases and the game continues

### Requirement: Job consequences
Completed and failed jobs SHALL produce world events describing their outcome, which other systems such as news,
rumours and factions can consume.

#### Scenario: Job outcome becomes a world event
- **WHEN** the player completes a sabotage job against a facility
- **THEN** a world event with the facility, its owner, the outcome and the attribution evidence is emitted

### Requirement: Sustainable starter economy
A new character doing only T0 jobs SHALL be able to pay rent and avoid starvation during the first four in-game weeks.

#### Scenario: Starter economy is sustainable
- **WHEN** the gig-worker autoplayer plays the first four weeks with only T0 jobs on 50 seeds
- **THEN** it misses no rent payment and never starves on at least 95 % of the seeds
