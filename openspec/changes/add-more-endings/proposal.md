# Proposal: More endings

## Why

The campaign had five endings, and in the year-long test games three of them (Loyal, Burn the Board, Kingmaker) made
up almost all of them. The game now builds relationships, money, politics and gang turf, but none of them led to an
ending of its own.

## What Changes

Five new endings, each opened by something the player built, each with its own effect, three closing voices and the
next season's title:

| Ending | Opens when | Effect | Next season |
|---|---|---|---|
| **The Chair** | loyal to the first patron, trust 90, a handler who is a friend | head of the patron's black ops: patron +40 capital, 1,500 cred a week, the next season starts at trust 100 | *The Chair* |
| **Exodus** | two people close to the player and 20,000 cred | the player leaves with them: half their cred, all Heat, the hunt, bounties and the blacklist are gone | *The Return* |
| **City Hall** | an exposed councillor or the Coup scenario, and a reporter with a bond of 15 or more | bribed seats exposed, police back to the city, curfews lifted, corporations lose influence | *Reform* |
| **Street Kings** | a gang holding three districts and a fixer who is a friend | the gang is crowned: corporations lose force, the gang gains it, the police stand aside | *The Street* |
| **The Shareholder** | shares worth 100,000 cred | a seat on the board of the corporation the player owns most of: it is crowned, its share price +25 % | *Dividends* |

The terminal's Story tab lists all ten endings in two lines, marks the ones open now, and names what the next locked
new ending needs.

## Capabilities

### Modified Capabilities

- `story-campaign`: five more endings.

## Impact

- **Saves:** still version 6. The saga gains fields at its end; a saga saved before them loads into the front of the
  new one.
- **Code:** `sr_chess.h` (endings, saga fields), `sr_story.c` (conditions, effects, news), `sr_story_text.c` (voices),
  `sr_saga.c` (bonds, the strongest gang, the stipend, season titles and openers), `sr_game_chess.c` (portfolio, the
  cost of leaving), `sr_ui.c`, `sr_save.c`, the test players.
- **Tests:** `test_more_endings`; in 16 year-long test games 8 of the 10 endings happened (The Shareholder and Sunrise
  need trading and deep hacking, which the test players do not do).
