# Spec Delta: news-media

## ADDED Requirements

### Requirement: Adverts follow the board
Billboards and holo-ads SHALL advertise products of the city's megacorporations in their colours and logos, SHALL be
vandalised after a recent scandal, SHALL multiply for a megacorp that dominates, and SHALL be lost in a district where
the megacorp lost a takeover.

#### Scenario: Adverts for the city's corporations
- **WHEN** a billboard slot is filled
- **THEN** it shows a product name from the name tables with the colour and logo of an existing megacorp

#### Scenario: Scandal vandalises adverts
- **WHEN** a megacorp suffered a scandal within the last 7 in-game days
- **THEN** about half of its billboards show the vandalised variant, and none do once 7 days have passed without one

#### Scenario: Winners multiply
- **WHEN** a megacorp holds more than 25 % of all megacorp resources
- **THEN** it fills billboard slots that would otherwise stay empty

### Requirement: The news in the street
LED tickers on media buildings and over metro entrances SHALL scroll the latest headline, and when the player has a
bounty, wanted posters SHALL appear on walls near the player's home district.

#### Scenario: Ticker shows the news
- **WHEN** a new headline is published
- **THEN** tickers within view scroll that headline within one in-game minute

#### Scenario: Wanted posters
- **WHEN** the player's Heat is 3 or more
- **THEN** wanted posters in the player's outfit colour appear on blank walls in the player's home district, and they
  disappear once Heat falls below 3
