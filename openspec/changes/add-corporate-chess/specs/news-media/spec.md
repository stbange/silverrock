# Spec Delta: news-media

## Purpose

Turns significant world events into news and rumours, shaped by who owns the outlet, and lets the player read, plant
and kill stories — making the corporate game visible on screens and in the streets.

## ADDED Requirements

### Requirement: News from world events
Significant world events SHALL be published as news items by the city's media outlets.

#### Scenario: Event becomes news
- **WHEN** a significant world event occurs, such as a facility raid or an election result
- **THEN** at least one outlet publishes a news item about it within 2 in-game hours

### Requirement: Owner spin
Outlets owned by a megacorp SHALL report events in favour of their owner and SHALL NOT report their owner's scandals
unless an independent outlet does first.

#### Scenario: Owner protection
- **WHEN** evidence of a scandal involving an outlet's owner exists but no independent outlet has reported it
- **THEN** the owned outlet does not publish it

### Requirement: Distribution
News SHALL reach the player on terminals, billboards and radio, and citizens SHALL pass it on as rumours.

#### Scenario: Billboards show headlines
- **WHEN** a news item is published
- **THEN** billboards in the affected districts show its headline in their rotation

### Requirement: Planted and killed stories
Through a media outlet's host the player SHALL be able to plant a story, which changes the subject's public image, or
kill a pending story.

#### Scenario: Planted story lowers public image
- **WHEN** the player plants a damaging story about a megacorp
- **THEN** the story is published and the megacorp's public image decreases

### Requirement: The player in the news
News about the player's deeds SHALL describe the perpetrator only as far as the evidence allows.

#### Scenario: Anonymous reporting
- **WHEN** the player completes a job without leaving attributable evidence
- **THEN** news about it names no perpetrator
