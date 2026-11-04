# Spec Delta: story-campaign

## Purpose

Gives the faction simulation a story spine: the player's rise from nobody to a key piece, the patron's black-ops
division and its handler, rival offers, procedural story arcs, and an endgame with several endings that always remains
reachable.

## ADDED Requirements

### Requirement: Campaign structure
The campaign SHALL consist of the Prologue, Act I "Pawn", Act II "Knight" and Act III "Promotion", with transitions
triggered by Street Cred, patron trust and world events.

#### Scenario: Act progression
- **WHEN** the player is on the patron's retainer and reaches Street Cred tier Runner with patron trust of at least 40
- **THEN** Act II begins

### Requirement: Patron selection
At the start of a new game one of the city's megacorps SHALL be chosen as the patron, randomly by default or by the
player in the advanced new-game options.

#### Scenario: Random patron
- **WHEN** a new game starts with default options
- **THEN** exactly one megacorp of the city is the patron, chosen from the seed

#### Scenario: Chosen patron
- **WHEN** the player selects a megacorp as patron in the advanced options
- **THEN** that megacorp is the patron

### Requirement: The test and the retainer
One prologue gig SHALL secretly be a test by the patron's black-ops division, and completing it, or the end of the
first week at the latest, SHALL lead to the handler's contact and the retainer offer.

#### Scenario: Retainer offer
- **WHEN** the player completes the test gig
- **THEN** the handler contacts the player within 2 in-game days and offers the retainer

### Requirement: Handler trust
The patron's handler SHALL track trust from 0 to 100, raised by successful and deniable work and lowered by failures,
exposure and actions against the patron; black-op (T4) jobs SHALL be offered only by the handler, at Street Cred tier
Knight and trust of at least 60.

#### Scenario: Trust gates black ops
- **WHEN** the player is at tier Knight with trust 59, and later with trust 60
- **THEN** no T4 job is offered at trust 59, and the handler offers T4 jobs at trust 60

### Requirement: Betrayal
Working against the patron SHALL reduce trust sharply when the patron attributes it to the player, and at zero trust
the patron SHALL become hostile and hunt the player.

#### Scenario: Betrayal discovered
- **WHEN** the patron attributes a job against its interests to the player
- **THEN** trust drops by at least 25, and at 0 the patron's black-ops division starts operations against the player

### Requirement: Counter-offers and defection
During Act II, rival black-ops divisions SHALL make offers to the player; accepting one SHALL make that megacorp the new
patron and turn the old patron hostile.

#### Scenario: Defection
- **WHEN** the player accepts a rival division's offer
- **THEN** that megacorp becomes the patron with a new handler and the previous patron becomes hostile

### Requirement: Procedural arcs
The game SHALL instantiate story arcs from templates with the current factions, keep one to three arcs active, branch
each arc on the outcome of its beats, and continue an arc when the player skips a beat.

#### Scenario: Arc instantiation
- **WHEN** an arc template is instantiated
- **THEN** all of its roles are bound to existing factions, people and places of the current world

#### Scenario: Branching on outcome
- **WHEN** an arc beat succeeds in one run and fails in another with the same seed
- **THEN** the next beats differ as defined by the template

#### Scenario: Skipped beat
- **WHEN** the player declines a beat's job
- **THEN** the beat is resolved off-screen and the arc continues

### Requirement: Endgame trigger
Act III SHALL begin when a megacorp's share of all megacorp resources passes the dominance threshold or Act II exceeds
its maximum duration, with a checkmate scenario chosen to fit the state of the board.

#### Scenario: Dominance triggers the endgame
- **WHEN** a megacorp passes the dominance threshold during Act II
- **THEN** Act III begins with a checkmate scenario involving that megacorp

### Requirement: Endings
The campaign SHALL have at least five endings — Loyal, Defector, Burn the Board, Kingmaker and Sunrise — and the
player SHALL be able to keep playing in the changed city afterwards. Sunrise SHALL be the only ending that shows
daylight.

#### Scenario: Sunrise shows daylight
- **WHEN** the player reaches the Sunrise ending
- **THEN** the sky shows daylight for the first time in the game

#### Scenario: Continue after the ending
- **WHEN** an ending sequence finishes
- **THEN** the player can continue playing in the same city with the ending's consequences applied

### Requirement: Authored cast with succession
The landlord, first fixer, neighbour, handler, rival runner, journalist and the Ghost SHALL be authored roles with
generated identities, and when a character in a critical role dies the role SHALL pass to a successor.

#### Scenario: Handler succession
- **WHEN** the handler dies
- **THEN** a successor handler contacts the player within 3 in-game days and the campaign continues

### Requirement: Campaign always completable
The campaign SHALL reach an ending on every seed without soft-locks.

#### Scenario: Bots complete the campaign
- **WHEN** story-progression bots play the campaign on 20 seeds
- **THEN** every run reaches an ending
