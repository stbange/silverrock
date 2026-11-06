# M5 soak review: generated histories (task 7.2)

Ten 52-week runs of the corporate game without the player (`sr_headless --soak 10`, seeds 500 +
k·104729), read for coherence. The nightly workflow writes the same histories to `soak.txt`.

## Before the fixes

| Finding | Evidence | Fix |
|---|---|---|
| Retaliation spiral: every attack was answered and every answer answered again | 3,210 operations in one city-year (≈9 a day) | A strike back is never itself answered; only 40 % of attributed attacks are answered; strategy rolls 25 % per tick with a 3–7 day cooldown |
| Gangs wiped out within days of the start | Two gangs "finished" by day 13 on seed 500 | A gang defending its last district gets +45 defence; gang weights favour expansion (650) over turf war (350) |
| A reborn gang could share a district with a gang that held only that district | "Ghost Knives takes Corran Stacks" twice, from two different gangs | Rebirth only into a free district or one taken from a gang holding several; otherwise it retries a week later |
| Bribery read as an attack on the city | "Paragon Works: bribery on City of Lower Lanyard succeeds" | Now "Paragon Works buys councillor *name*" |
| Curfews never happened | 0 curfews in 10 runs: turf wars logged no street violence | A won turf war logs gunfire in the district; curfew votes are noted in the history |

## After the fixes (10 runs, 52 weeks each)

- Operations: about 4 a day citywide; 1,316 succeeded and about 2.7× as many failed. Failures are
  frequent because facility security rises after each attack.
- Retaliation: 380 of 1,316 successes (29 %). This is high but reads as feuds, with clear pairs
  such as Hexis Capital vs Umbra.
- Gang turf: 204 districts changed hands and 74 gangs were wiped out (about 7 per city-year).
  Every wiped-out gang was replaced 3 weeks later under a new name and colour.
- Politics:
  - Elections followed the schedule: 12–20 per city-year, and 2 mayoral elections per city-year.
  - 13 curfews were declared and 12 lifted.
  - The police contract was privatised 11 times.
- Megacorps: no megacorp dropped below its protection floor or lost its HQ. The leading corp held
  11–21 % of all megacorp resources.

## Open observations (not fixed)

1. **Dominance never reaches 40 % without the player.** Income reverts to the mean, so the 40 %
   threshold from the spec is out of reach with 5–10 corps. In bot campaigns, Act III therefore
   always starts at the 12-week Act II limit. Dominance can still be reached through the player's
   takeovers, and it is tested with a forced board. Revisit in playtests: either make takeovers
   raise a corp's long-run capital, or scale the threshold with the number of corps.
2. **Similar faction names in one city.** Examples: "Umbra Systems" next to "Umbra Cybernetics",
   and "Hollow Hands" next to "Hollow Knives". This comes from the M1 name generator; a follow-up
   should reject shared first words within a city.
3. **Bribes are frequent.** There are about 20 a year per city, so councillors change hands
   often. It is coherent with Spider doctrines but may feel busy on the Board. Watch in playtests.

## Bots (task 7.1)

`sr_headless --campaign 20`: every seed reaches an ending in 98–126 in-game days. The endings
were Loyal, Defector and Kingmaker. Bots do not hack, so Sunrise does not occur, and Burn the
Board did not occur either. These two endings are covered by unit tests.
