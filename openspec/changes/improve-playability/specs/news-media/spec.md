# Spec Delta: news-media

## ADDED Requirements

### Requirement: Written headlines
Headlines SHALL come from templates for each event type and outcome, written in the voice of the outlet, and SHALL
fit within 63 characters without truncation.

#### Scenario: Headline from a job
- **WHEN** a sabotage at a megacorp facility is reported
- **THEN** the headline is a full sentence from a template (no raw job type or site number) and ends with a complete
  word

#### Scenario: Outlet voice
- **WHEN** the same event is reported by an owned outlet and by the pirate feed
- **THEN** the two headlines differ in wording and slant

### Requirement: Notoriety coverage
When the player reaches a new Street Cred tier or is reported three times in one in-game week, an outlet SHALL run a
story about the unknown runner, slanted by its owner.

#### Scenario: A name on the street
- **WHEN** the player reaches tier Operator
- **THEN** within one in-game day an outlet publishes a story about a runner of that reputation
