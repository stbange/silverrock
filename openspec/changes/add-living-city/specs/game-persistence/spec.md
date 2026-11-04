# Spec Delta: game-persistence

## Purpose

Saves and loads the game reliably on every target: complete and compact saves, autosaves, manual slots, persistence
in browsers, protection against corruption, and compatibility across versions.

## ADDED Requirements

### Requirement: Complete saves
Loading a save SHALL restore the exact game state at the moment of saving.

#### Scenario: Save and load round trip
- **WHEN** the game is saved and the save is loaded again
- **THEN** the simulation state checksum after loading equals the checksum at the moment of saving

### Requirement: Compact saves
Saves SHALL store the seed, generator version and the changes to the generated world instead of the world itself.

#### Scenario: Save size bound
- **WHEN** a game is saved after 10 in-game weeks of autoplayer play
- **THEN** the save file is at most 4 MB

### Requirement: Autosave
The game SHALL autosave after sleeping, when entering the player's home, after finishing a job, and when the window
loses focus or the page becomes hidden, keeping the three most recent autosaves.

#### Scenario: Autosave on sleep
- **WHEN** the player wakes up after sleeping
- **THEN** a new autosave exists

#### Scenario: Autosave when the page is hidden
- **WHEN** the browser page becomes hidden during gameplay
- **THEN** an autosave is written before the page is suspended

#### Scenario: Autosave rotation
- **WHEN** a fourth autosave is written
- **THEN** the oldest autosave is replaced and three autosaves remain

### Requirement: Manual saves
The player SHALL be able to save manually into three slots at any time except during combat.

#### Scenario: Manual save blocked in combat
- **WHEN** the player tries to save while an enemy is in combat state with them
- **THEN** saving is refused with a message and existing saves are unchanged

### Requirement: Browser persistence
Saves in the web build SHALL survive page reloads and browser restarts.

#### Scenario: Save survives reload
- **WHEN** the player saves in the web build and reloads the page
- **THEN** the save is listed and loads with the same state checksum

### Requirement: Corruption safety
An interrupted save SHALL never damage the previous save, and corrupted save files SHALL be detected and reported
without crashing the game.

#### Scenario: Interrupted save
- **WHEN** writing a save is interrupted before it completes
- **THEN** the previous save in that slot still loads correctly

#### Scenario: Corrupted save detected
- **WHEN** the player tries to load a save file with altered bytes
- **THEN** the game reports the save as damaged, does not crash, and other saves remain loadable

### Requirement: Version compatibility
Saves from older format versions SHALL be migrated when loaded, and saves from newer versions or from generator
versions the build cannot reproduce SHALL be refused with an explanation.

#### Scenario: Older save migrated
- **WHEN** a save written in an older supported format version is loaded
- **THEN** it is migrated and loads successfully

#### Scenario: Newer save refused
- **WHEN** a save written by a newer game version is loaded
- **THEN** the game refuses it with a message naming the required version
