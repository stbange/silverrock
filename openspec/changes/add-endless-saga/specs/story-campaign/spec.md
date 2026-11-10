# Spec Delta: story-campaign

## ADDED Requirements

### Requirement: Seasons
Two weeks after an ending the next season SHALL begin: the ending is cleared, the acts run again toward a climax whose
scenario differs from the last one, the season is named by how the last one ended, and the board is shaken accordingly.
A player on a retainer SHALL start in Act II; a player without a patron SHALL get an offer and start in Act I. A
corporation hunting the player SHALL call it off at the reset unless the handler bond is cold. A Sunrise ending SHALL
happen at most once.

#### Scenario: The game goes on in seasons
- **WHEN** the player ends seasons as Loyal, Burn the Board, Kingmaker and Sunrise in turn
- **THEN** each time a new season starts two weeks later with its title, a cleared ending, an opening arc and a
  different climax scenario; a personal grudge survives the reset and an impersonal one does not; the sun stays out and
  Sunrise is not offered again

### Requirement: People ask for help
One of the cast SHALL ask the player for something every five to eight days once the story has started. A request SHALL
wait four days, be answerable on the terminal, count as refused when unanswered, cost a share of the player's cred where
it costs money, and move the bond with that person.

#### Scenario: People ask for help
- **WHEN** requests arrive and the player pays, refuses, cannot afford or ignores them
- **THEN** cred is charged only for a yes the player can afford, bonds rise and fall, favours and refunds follow, and
  over a season at least six kinds of request come up

#### Scenario: Loyalty against friendship
- **WHEN** the handler asks for the reporter's name
- **THEN** refusing costs trust and wins the reporter, and giving the name raises trust, kills the reporter and brings
  a successor who is a stranger

### Requirement: Friends and enemies
A person whose bond reaches 60 SHALL say so once and then help the player with one favour a week at most. A person whose
bond falls to -40 SHALL sell the player out once. The endings Burn the Board and Sunrise SHALL also open through a
friendly reporter and a friendly Ghost.

#### Scenario: Friends stand up for the player
- **WHEN** the cast is close to the player over several weeks
- **THEN** a hunt is put off, a bill disappears, a story is buried, the handler vouches and money is paid back

#### Scenario: Betrayal by someone let down
- **WHEN** a bond falls to -40
- **THEN** that person sells the player out once: hunters, a story in the press or lost trust, by who they are

#### Scenario: Someone close is taken
- **WHEN** someone close is taken by a corporation
- **THEN** paying the ransom brings them back, and not paying ends with their death and an arc against that corporation

### Requirement: Story hygiene
Nobody SHALL say the same line twice within eight lines. A new patron SHALL NOT act on jobs done against a former
patron, and the next offer after a change of patron SHALL take at least two weeks, a week longer each time in a season.
The handler SHALL only brief operations of the patron.

#### Scenario: No repeated lines
- **WHEN** the same line is said four times in a row
- **THEN** it reaches the inbox once

#### Scenario: No chain of betrayals
- **WHEN** a patron drops the player while a job against it is still being looked into
- **THEN** the suspicion is dropped, no offer comes for two weeks, and after the next change three

#### Scenario: Briefings come from the right person
- **WHEN** an arc between two other corporations starts, and one for the patron
- **THEN** the fixer announces the first and the handler the second
