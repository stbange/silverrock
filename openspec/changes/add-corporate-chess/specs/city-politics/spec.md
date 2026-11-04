# Spec Delta: city-politics

## Purpose

Simulates the city's government as pieces in the corporate game: councillors and the mayor, scheduled elections,
policies that change the world, and corruption that can be exposed.

## ADDED Requirements

### Requirement: Elections
Elections SHALL be held on a fixed schedule — a rotating group of council seats every 8 in-game weeks and the mayor
every 24 — with results driven by influence, public image, scandals and tampering.

#### Scenario: Election held on schedule
- **WHEN** the clock reaches the scheduled election date
- **THEN** the election is resolved, results are announced and seats are reassigned

#### Scenario: Scandal costs votes
- **WHEN** a candidate is hit by a published scandal before an election
- **THEN** their vote share is lower than it would have been without the scandal

#### Scenario: Vote tampering
- **WHEN** the player alters results in the vote host without being detected
- **THEN** the announced result reflects the alteration

### Requirement: Policies
The council SHALL vote on policies — police budget, curfews, corporate licences, zoning and the police contract — and
passed policies SHALL change the world.

#### Scenario: Curfew in effect
- **WHEN** a curfew policy passes for a district
- **THEN** during curfew hours most citizens stay indoors and police stop people on the street there

#### Scenario: Police contract privatised
- **WHEN** the council awards the police contract to a megacorp
- **THEN** police officers wear that megacorp's colours and police Heat is handled by that megacorp

### Requirement: Corruption and exposure
Megacorps SHALL be able to buy councillors with influence, and a councillor exposed by published evidence SHALL resign,
triggering a by-election.

#### Scenario: Exposed councillor resigns
- **WHEN** evidence of a councillor's bribery is published
- **THEN** the councillor resigns within 3 in-game days and a by-election is scheduled
