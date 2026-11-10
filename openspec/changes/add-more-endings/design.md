# Design: More endings

## Conditions

- **Where:** `sr_saga_endings` adds the new conditions. It reads only the chess state.
- **Inputs it cannot see directly** are set beforehand:
  - The game sets the player's cash and share value (`saga.cash`, `saga.portfolio`) each minute.
  - The saga tick finds the gang holding the most districts (`saga.street_gang`, `saga.street_districts`).

## Effects

- **Where:** `sr_story_end` applies each new ending, like the old ones.
- **Winner:**
  - The Chair crowns the patron; The Shareholder and Street Kings crown `camp.king`.
  - A crowned corporation takes the police contract. A gang does not.
  - Exodus and City Hall have no winner: curfews are lifted, police contract to the city.
- **Exodus** is charged by the game: half the player's cred, and Heat cleared.
- **The Chair's stipend** is paid each week through the saga's refund, while the player is on a retainer.

## Next season

`begin_season` gives each new ending a title, a shake and an opening arc:

- **The Chair:** rivals gain intel. The Mole arc against the patron's worst rival.
- **Exodus:** a Supply Shortage arc anywhere.
- **City Hall:** corporations gain capital to buy the reformers back. An Election arc.
- **Street Kings:** corporations gain force. A Proxy War between gangs.
- **The Shareholder:** a rival of the corporation the player sits on starts a Scandal against it.

## Saves

`SrSaga` grows only at its end. The saga section is read as a prefix: the stored length is read, the rest is zeroed and
the strongest gang reset to none. So saves made before this change keep their saga.
