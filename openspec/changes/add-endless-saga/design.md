# Design: The game never ends, and it gets personal

## Where it lives

- **Structure:** `SrSaga` is a new block in `SrChess`, advanced by `sr_saga_tick` after the story's tick. Like the rest
  of the corporate game it is plain data and pure functions over (city, time), so replays and saves stay
  deterministic.
- **Save:** the saga has its own save section, so the story and campaign blobs keep their layout.

## Seasons

- **Start:** an ending sets the epilogue as before. Two weeks later (`SR_INTERLUDE_S`) `begin_season` runs.
- **History:** the ending, the scenario and the winner go into rings of eight entries.
- **Reset:** the story's machinery is cleared: ending, scenario, set piece, arcs, suspects, leaks, the count of
  voluntary defections.
- **Patron:**
  - A player still on a retainer starts the season in Act II.
  - A player without a patron gets an offer from the last winner, or from any corporation that is not hunting them,
    and starts in Act I. In later seasons Act I lasts one week at least, not three.
  - Loyalty is counted from the season's first patron.
- **Grudges:** a corporation hunting the player calls it off at the reset unless the handler bond is cold (-15 or
  below). The blacklist is reduced to that corporation.
- **Shake:** resources shift by how the season ended, and an opening arc starts between the season's two sides.
- **Scenario:** when Act III begins and the scenario equals the last season's, it rotates to the next one. The Pall
  Protocol is skipped after a Sunrise.
- **Street Cred** above Knight stays open in later seasons.

## People

- **Bond:** -100 to 100 per cast role.
  - -40 and below: enemy. -15 and below: cold.
  - 35 and above: friend. 60 and above: close.
  - A successor starts at 0.
- **Requests:** one at a time, from `make_ask`.
  - A hash of the seed, the day and the count picks the kind. The last kind is never repeated. A kind is skipped when
    its person is gone or its condition is not met (Act II for the leak, the name and hiding; Matrix depth 10 for the
    Ghost).
  - A ransom takes priority when someone is close and a corporation hunts the player, or in any later season; at most
    once in five weeks.
- **Costs:** a percentage of the player's cred between a floor and a cap, never more than half:
  - Debt: 5 %, 300 to 8,000.
  - Bail: 8 %, 500 to 12,000.
  - Lease: 10 %, 1,000 to 20,000.
  - Ransom: 20 %, 2,000 to 40,000.
  - The game charges the cost before the saga applies the answer.
- **Answers:** `sr_saga_answer` moves bonds, trust and flags and says the lines. A no can cost a life: the neighbour
  loses the flat, someone turned away is caught.
- **Favours:** one a week, from the first person who is close or owes the player and has something to give.
- **Betrayal:** once per person and generation, at -40 or below. The fixer and the neighbour leave afterwards.
- **Hunters** use the existing bounty (`camp.hunter`).
- **Money paid back** goes through `saga.refund`, collected by the game each minute.

## Fixes

- **Repeats:** the saga keeps hashes of the last eight lines said. `sr_story_say` drops a repeat. A beat tries its
  three wordings in turn.
- **Betrayal chains:** `sr_saga_patron_changed` clears suspects and sets the next rival offer to (1 + switches this
  season) weeks away.
- **Briefings:** the teller of a handler-type arc is the fixer when the arc's client is not the patron.
- **Headlines:** events at sites without an owner use `k_anon` templates.

## Test players

- **Requests:** bots say yes 70 % of the time when the cost is at most a third of their cred. They pay ransoms they
  can afford 80 % of the time. Only the story strategy gives up the reporter.
- **Endings** are picked by a hash of seed and season.
