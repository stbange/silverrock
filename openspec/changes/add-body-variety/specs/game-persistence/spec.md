# Spec Delta: game-persistence

## ADDED Requirements

### Requirement: Stable appearance encoding
New body features SHALL be stored in the existing appearance bytes, so saves keep their format and older appearances
keep their look and their recognition keys.

#### Scenario: Older looks stay the same
- **WHEN** an appearance from an older save (body 0..3, cyber 0..5) is read
- **THEN** it has the build of the same index, a masculine frame, average height, chrome arms only at level 4 or
  more, and the same appearance key as before
