# Spec Delta: job-system

## MODIFIED Requirements

### Requirement: Job sources
The game SHALL offer jobs through the public job board on the terminal, fixers in their haunts, personal contacts and
the patron's handler, and the player SHALL choose which offer to take from a list that shows each offer's client,
target, tier, pay, deadline and validated approaches.

#### Scenario: Fixer offers jobs
- **WHEN** the player talks to a fixer during the fixer's hours
- **THEN** a list of one to six offers of unlocked tiers opens and no job is accepted until the player picks one

#### Scenario: Job board listings
- **WHEN** the player opens the Jobs tab on a terminal
- **THEN** it lists the public board, contact and handler offers and the accepted jobs, each with tier, client,
  target, pay and deadline

#### Scenario: Accepting from the terminal
- **WHEN** the player accepts a handler or contact offer on the Jobs tab
- **THEN** the job becomes active without visiting anyone

#### Scenario: Job selection by touch
- **WHEN** a touch player taps an offer in the fixer list
- **THEN** that offer is selected, and a second tap accepts it

### Requirement: Sustainable starter economy
A new character doing only T0 jobs SHALL be able to pay rent and avoid starvation during the first four in-game weeks,
and money SHALL stay meaningful afterwards: the living costs of a successful character SHALL grow with their success.

#### Scenario: Starter economy is sustainable
- **WHEN** the gig-worker autoplayer plays the first four weeks with only T0 jobs on 50 seeds
- **THEN** it misses no rent payment and never starves on at least 95 % of the seeds

#### Scenario: Money keeps mattering
- **WHEN** the playtest bots play 26 weeks on 16 seeds
- **THEN** until Act II the median bot holds no more than 8 weeks of living costs, and no bot holds more than
  150,000 cred at week 26

## ADDED Requirements

### Requirement: Patron warning on offers
Offers whose target is the patron or a faction the patron funds or owns SHALL be marked, and accepting one SHALL need
a confirmation.

#### Scenario: Job against the patron
- **WHEN** an offer targets a facility of the player's patron
- **THEN** it shows a PATRON tag, and the first accept press shows the handler's warning instead of accepting

### Requirement: Risk in resolved missions
Missions resolved without the 3D level SHALL be able to injure the player beyond 1 health and to flatline them, with
the damage capped at 60 % of maximum health on the Story difficulty preset.

#### Scenario: Resolved mission can flatline
- **WHEN** a resolved mission deals more damage than the player's remaining health on Normal difficulty
- **THEN** the player flatlines and wakes up as defined by the flatline rules

#### Scenario: Story preset cap
- **WHEN** a resolved mission is played on the Story preset
- **THEN** it never deals more than 60 % of the player's maximum health

### Requirement: Approach rewards
Clean work SHALL earn a pay bonus, trust and less Heat, and loud work SHALL leave lasting costs, so that every
approach is a viable way through the campaign.

#### Scenario: Clean bonus
- **WHEN** a job is completed with no alarm and no kills
- **THEN** the payment includes a 20 % bonus and Heat with the target's jurisdiction drops by one level

#### Scenario: Approaches stay balanced
- **WHEN** the playtest bots of each approach play 26 weeks
- **THEN** the slowest approach reaches tier Knight within 1.5 times the days of the fastest, and the best approach
  earns at most 1.3 times the median
