# Spec Delta: game-persistence

## ADDED Requirements

### Requirement: Saga persistence
Saves SHALL store the saga (season, past endings, bonds, the open request) from version 6. A version 5 save SHALL load
with an empty saga.

#### Scenario: The saga is saved
- **WHEN** a game in its third season with an open request is saved and loaded, and a version 5 save is loaded
- **THEN** the season, endings, bonds and request come back, the sun stays out after a Sunrise, and the version 5 save
  starts in season one with neutral bonds
