# Proposal: The game never ends, and it gets personal

## Why

Sixteen 26-week test games (see `docs/reviews/playtest-stories-2026-11-09.md`) showed that the story stops too soon
and stays impersonal:

- After the ending around day 127 the game ran on for two months with no goal.
- Careful players sat on 150,000 cred with nothing to spend it on.
- Loyal players always got the same ending; three of the five endings never happened.
- The people in the story (handler, fixer, neighbour, reporter, rival) only delivered briefings.

The same games also showed four defects: repeated messages, broken headlines about sites nobody owns, a chain of
betrayals within days, and the patron's handler briefing other corporations' operations.

## What Changes

- **Seasons.** Two weeks after an ending the next season begins.
  - The acts run again toward a new climax, with a scenario different from the last one.
  - How the last season ended decides what the next is about:
    - Loyal: **The Reckoning**. The winner is everybody's target.
    - Defector: **Old Debts**.
    - Burn the Board: **Ashes**. The corporations rebuild and the gangs move.
    - Kingmaker: **The Crown**. The crown gets heavy.
    - Sunrise: **Daylight**. The machine is being rebuilt.
  - Old grudges fade with the old board, unless the handler made it personal.
  - Every ending can be reached again in every season. Sunrise happens once: the sun stays out.
- **People.** The landlord, fixer, neighbour, handler, rival runner, reporter and the Ghost each have a bond with the
  player.
  - **Requests.** Every five to eight days one of them asks for something. It waits four days on the terminal's new
    People tab. Silence counts as a no. A yes or a no moves the bond and has consequences. The requests:
    - Pay a debt, post bail, or lend money for the lease.
    - Leak a file to the reporter.
    - Give the handler the reporter's name.
    - Split a job with the rival.
    - Hide someone on the run.
    - Carry the Ghost's packet.
  - **Friends.** Someone close says so once, then helps week by week: a hunt sent to the wrong district, a clinic
    bill that disappears, a story about the player buried, a word upstairs, money paid back.
  - **Betrayal.** Someone let down too often sells the player out:
    - The fixer or the neighbour sends hunters and skips town.
    - The reporter prints a story about the player.
    - The handler stops covering for the player.
    - The rival takes a contract.
  - **Loss.** Someone close can be taken by a corporation. Pay the ransom, or they die and an arc of revenge begins.
    Giving the handler the reporter's name buys trust and costs the reporter's life. Successors are strangers.
  - **Endings.** A reporter who trusts the player opens Burn the Board; a Ghost who listens opens Sunrise.
- **Money matters late.** Debts, bail, the lease and ransoms scale with what the player has (5 to 20 %, with caps),
  never more than half.
- **Fixes:**
  - **Repeated lines.** Nobody says the same line twice in a row; an arc beat picks another wording when its line was
    said lately. "While you were away" comes for the first and last beat of an arc only.
  - **Betrayal chains.** A new patron does not inherit the old patron's suspicions. After being dropped, the next
    offer comes after two weeks, three the next time, and so on. Trust with a new patron starts at 35.
  - **Briefings.** The handler only briefs the patron's own operations; other corporations' wars come from the fixer.
  - **Headlines.** Sites nobody owns get headlines of their own ("Unmarked facility in Rook Yards hit by saboteurs").

## Capabilities

### Modified Capabilities

- `story-campaign`: seasons, people, the fixes.
- `news-media`: headlines without an owner.
- `game-persistence`: the saga is saved (version 6).

## Impact

- **Saves:** version 6 adds a saga section. Version 5 saves load with an empty saga (season 1, neutral bonds).
- **Code:**
  - `src/sim/sr_saga.c` (new).
  - `sr_story.c`: hooks, fixes, scenario variety.
  - `sr_news_text.c`.
  - `sr_game_chess.c`: answers, the People tab's rows, refunds.
  - `sr_ui.c`: People tab, season on the Story tab.
  - `sr_guide.c`: the goal names who is waiting.
  - `sr_script.c` and `tools/headless_main.c`: the test players answer requests; `SR_PLAYTEST_WEEKS` for longer runs;
    the event log prints the story log.
- **Tests:** five new unit tests in `tests/unit/test_play.c`; the playtest and balance bands.
