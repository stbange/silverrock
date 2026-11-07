# Spec Delta: story-campaign

## MODIFIED Requirements

### Requirement: Campaign structure
The campaign SHALL consist of the Prologue, Act I "Pawn", Act II "Knight", Act III "Promotion" and an Epilogue, with
transitions triggered by Street Cred, patron trust and world events, minimum durations (Act I at least 3 in-game
weeks, Act II at least 6, Act III at least 14 days) and a staged scene at every transition.

#### Scenario: Act progression
- **WHEN** the player is on the patron's retainer, has spent at least 3 weeks in Act I, and reaches Street Cred tier
  Runner with patron trust of at least 40
- **THEN** Act II begins

#### Scenario: Staged transition
- **WHEN** a new act begins
- **THEN** the handler sends a meeting message, three news items about the board are published and the act's city
  change becomes visible

### Requirement: The test and the retainer
One prologue gig SHALL secretly be a test by the patron's black-ops division, and completing it, or the end of the
first week at the latest, SHALL lead to the handler's contact and the retainer offer, never before day 3.

#### Scenario: Retainer offer
- **WHEN** the player completes the test gig on day 1
- **THEN** the handler contacts the player with the retainer offer on day 3

### Requirement: Handler trust
The patron's handler SHALL track trust from 0 to 100, raised by successful and deniable work and lowered by failures,
discovered actions against the patron and exposure; black-op (T4) jobs SHALL be offered only by the handler, at Street
Cred tier Knight and trust of at least 60, at most once per in-game week, and paid half in cred and half in a reward
(a Board reveal, a Heat wipe or a cyberware voucher). The story tab SHALL show trust and what each level unlocks.

#### Scenario: Trust gates black ops
- **WHEN** the player is at tier Knight with trust 59, and later with trust 60
- **THEN** no T4 job is offered at trust 59, and the handler offers T4 jobs at trust 60

#### Scenario: Black ops are rare
- **WHEN** the player completes a black op
- **THEN** the next black op is offered no sooner than seven in-game days later

### Requirement: Betrayal
Working against the patron SHALL reduce trust sharply only when the patron discovers it, with a discovery chance of
25 % plus 25 % per piece of evidence rolled one to three days later, framing another faction SHALL prevent discovery,
and at zero trust the patron SHALL become hostile and hunt the player.

#### Scenario: Betrayal discovered
- **WHEN** the patron discovers a job against its interests
- **THEN** trust drops by at least 25, and at 0 the patron's black-ops division starts operations against the player

#### Scenario: Framed betrayal stays hidden
- **WHEN** the player completes a job against the patron with evidence planted against another faction
- **THEN** the patron never discovers it and trust is unchanged

### Requirement: Counter-offers and defection
During Act II, rival black-ops divisions SHALL make offers to the player; accepting one SHALL make that megacorp the
new patron with a new handler and trust of 25, turn the old patron hostile and blacklist it. A blacklisted megacorp
SHALL never make an offer or become the patron again, and only one voluntary defection SHALL be possible per campaign;
further offers come only while the current patron is hostile.

#### Scenario: Defection
- **WHEN** the player accepts a rival division's offer
- **THEN** that megacorp becomes the patron with a new handler and trust 25, and the previous patron becomes hostile
  and blacklisted

#### Scenario: No way back
- **WHEN** the player has defected from a megacorp
- **THEN** that megacorp makes no further offers for the rest of the campaign

#### Scenario: One defection
- **WHEN** the player has already defected once and the new patron is not hostile
- **THEN** no rival makes an offer

### Requirement: Procedural arcs
The game SHALL instantiate story arcs from templates with the current factions, keep one arc active in Act I and up
to two in Act II, space beats at least three in-game days apart, announce every beat through a cast member's message
that names the people, place and stakes, branch each arc on the outcome of its beats, continue an arc when the player
skips a beat and report what happened when the beat was the patron's, and record acts, beats and trust changes in a
story log on the terminal.

#### Scenario: Arc instantiation
- **WHEN** an arc template is instantiated
- **THEN** all of its roles are bound to existing factions, people and places of the current world

#### Scenario: Branching on outcome
- **WHEN** an arc beat succeeds in one run and fails in another with the same seed
- **THEN** the next beats differ as defined by the template

#### Scenario: Skipped beat
- **WHEN** the player declines a beat's job
- **THEN** the beat is resolved off-screen, the arc continues and a message tells the player what happened

#### Scenario: Beat announced
- **WHEN** an arc beat starts
- **THEN** a cast member's message names the faction, the place and what is at stake, and the story log gains an entry

### Requirement: Endgame trigger
Act III SHALL begin when a megacorp's share of all megacorp resources passes the dominance threshold or Act II exceeds
its maximum duration, with a checkmate scenario chosen to fit the state of the board. The climax SHALL be an arc of
four beats whose last beat is a set piece the player must take part in, and the ending choice SHALL open only after
the set piece.

#### Scenario: Dominance triggers the endgame
- **WHEN** a megacorp passes the dominance threshold during Act II
- **THEN** Act III begins with a checkmate scenario involving that megacorp

#### Scenario: The set piece waits for the player
- **WHEN** the climax reaches its last beat and the player has not taken the job
- **THEN** the beat does not resolve off-screen and its deadline extends until the player acts or abandons it

### Requirement: Endings
The campaign SHALL have at least five endings (Loyal, Defector, Burn the Board, Kingmaker and Sunrise), each with a
short sequence of messages and news and a visible change in the city, after which the player SHALL be able to keep
playing in the changed city with new goals. Sunrise SHALL be the only ending that shows daylight, Kingmaker SHALL let
the player choose the megacorp to crown, and the story tab SHALL show what each ending requires.

#### Scenario: Sunrise shows daylight
- **WHEN** the player reaches the Sunrise ending
- **THEN** the sky shows daylight for the first time in the game

#### Scenario: Continue after the ending
- **WHEN** an ending sequence finishes
- **THEN** the player can continue playing in the same city with the ending's consequences applied, and a new patron
  cycle opens two in-game weeks later

#### Scenario: Choosing a king
- **WHEN** the player picks the Kingmaker ending
- **THEN** the player chooses which megacorp is crowned, and that megacorp gains the ending's advantage

### Requirement: Campaign always completable
The campaign SHALL reach an ending on every seed without soft-locks, within paced bounds, for both story bots and bots
limited to what the user interface offers.

#### Scenario: Bots complete the campaign
- **WHEN** story-progression bots play the campaign on 20 seeds
- **THEN** every run reaches an ending

#### Scenario: Paced campaign
- **WHEN** the playtest bots play 26 weeks on 16 seeds
- **THEN** at least 7 of the 8 interface-limited bots reach an ending, endings fall between days 120 and 200, Act III
  lasts at least 14 days and no run defects more than once

## ADDED Requirements

### Requirement: A nudge for stalled players
A player at tier Pawn who has done nothing but street gigs (T0) for 14 days in Act I SHALL get a guided T1 contract
from the handler, together with an explanation of what Act II needs.

#### Scenario: Stalled at Pawn
- **WHEN** the player has been at tier Pawn in Act I for 14 in-game days without completing a job above T0
- **THEN** the handler offers a T1 contract and says that Act II needs tier Runner and trust of 40
