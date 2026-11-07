# Spec Delta: story-campaign

## MODIFIED Requirements

### Requirement: Endings
The campaign SHALL have at least five endings (Loyal, Defector, Burn the Board, Kingmaker and Sunrise), each with a
short sequence of messages and news and a visible change in the city, after which the player SHALL be able to keep
playing in the changed city with new goals. Sunrise SHALL be the only ending in which the clouds break, the rain stops
and the sun is seen, Kingmaker SHALL let the player choose the megacorp to crown, and the story tab SHALL show what each
ending requires.

#### Scenario: Sunrise shows daylight
- **WHEN** the player reaches the Sunrise ending
- **THEN** the cloud deck breaks, the rain stops, the fog lifts and the sky shows a clear blue sky with the sun for the
  first time in the game

#### Scenario: Continue after the ending
- **WHEN** an ending sequence finishes
- **THEN** the player can continue playing in the same city with the ending's consequences applied, and a new patron
  cycle opens two in-game weeks later

#### Scenario: Choosing a king
- **WHEN** the player picks the Kingmaker ending
- **THEN** the player chooses which megacorp is crowned, and that megacorp gains the ending's advantage
