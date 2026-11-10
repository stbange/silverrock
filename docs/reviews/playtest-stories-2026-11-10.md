# A year in the city: test games after the endless saga (2026-11-10)

Eight computer-played characters each lived **52 weeks** with the new saga: the corporate game no longer stops at an
ending. Two weeks after each ending a new season begins, and the people around the player ask for things, remember
what the player did, help, betray and die. Every run went through **four seasons** and three endings. Below are
two of them as plot summaries, then what changed against the 26-week runs of 2026-11-09.

The raw logs come from `SR_PLAYTEST_WEEKS=52 sr_headless --playtest 8 <dir>`.

---

## Port Tern: the soldier who kept the reporters alive (and once didn't)

*An ex-soldier playing the story. First patron: Orison Cybernetics.*

**Season 1, The Climb.** Orison's handler, Oskar Brandt, hires the soldier in the first week. The neighbour, Mara
Nakamura, is behind with a loan shark, and the soldier pays. A month later Mara has people from Sable Mutual looking
for her, and the soldier hides her. In return Mara says what nobody else in the building says: "You're the only one in
this building I trust."

Then comes the first hard choice. The reporter Lior Varga needs one file from Orison, and the soldier leaks it. On day
46 the handler asks for the reporter's name, and the soldier gives it. Lior is pulled out of the canal two days later.
Trust with Orison goes up; the fixer and the neighbour hear about it.

Uma Xu, a rival runner the soldier once turned down, sells the soldier out on day 69. A new reporter, Greta Falk, takes
Lior's desk, and this time the soldier feeds her files. On day 107 Sable Mutual takes Mara and asks for a ransom. The
soldier pays. On day 113, with Greta's trust, the soldier chooses **Burn the Board**: every corporation's files on the
street.

**Season 2, Ashes.** The corporations rebuild, and the gangs smell blood. Oskar Brandt asks again for the reporter's
name, and this time the soldier refuses and keeps Greta alive. Greta is taken by Sable Mutual on day 174; the soldier
pays for her life. The landlord, Sol Moreau, borrows twice to keep the building out of a corporation's hands and comes
to trust the soldier. The season ends as **Kingmaker**.

**Season 3, The Crown.** Oskar asks a third time, at the start of the season, and the soldier gives Greta up. The
reporter after her, Cass Sorensen, becomes the soldier's closest source, is taken in turn on day 308, and is ransomed.
The season ends in **Burn the Board** again.

**Season 4, Ashes.** The soldier refuses Oskar once more and keeps Cass. The year ends with the landlord, the neighbour,
the rival and the reporter all close to the soldier, and the handler cool. Two reporters dead, three people ransomed,
one betrayal.

## Saint Marrow: the hammer nobody would keep

*A street kid who solves things with violence. First patron: Rubicon Industries.*

**Season 1.** The kid pays a neighbour's 300-cred debt and posts the fixer's bail in the first three weeks, and then
loses Rubicon's trust on day 20. Vantor Biosystems takes the kid on two weeks later. A month later, Lattice
Atmospherics. In between the kid refuses to hide the neighbour, Celeste Quintero, and Rubicon catches her.

The next neighbour, Ada Iwasaki, is luckier at first: the kid pays her debts twice and hides her from Vantor. On day 95
Vantor takes her and asks 3,700 cred. The kid doesn't pay, and Ada is found the next morning. The kid twice refuses to
give a handler the reporter's name. The rival runner Tomas Ostrander shares jobs with the kid all season and is hidden
by the kid in turn. The season ends as **Kingmaker** with the kid working for Kestrel Pharma.

**Season 2, The Crown.** The old hunts are called off. The kid's rival is taken by Rubicon, and this time the kid pays.
Then the reporter, Tomas Santangelo, who now trusts the kid, is taken by Argent Consolidated, and the kid pays again.
The landlord, Rafa Whitlock, is helped four times and comes to trust the kid. Patrons still come and go (Lattice in
season 2, then Argent), but weeks apart now, not days. The season ends as **Kingmaker** again.

**Season 3, The Crown.** The kid starts over as a pawn, keeps leaking to the reporter, keeps bailing out the fixer, and
ends the year in **Burn the Board**. The reporter, the landlord, the neighbour and the rival end the year close: a
violent career with loyal friends.

---

## Across the eight year-long runs

- **Endings:** every run reached three endings, 24 in all: Kingmaker 13, Burn the Board 8, Defector 2, Loyal 1. No
  climax scenario repeated twice in a row. Sunrise did not happen: none of the test players dives deep enough into the
  Matrix or befriends the Ghost. Loyal is rare because the test players change patrons or crown someone else.
- **People:** about 55 requests a year per run, with all nine kinds. The test players helped about two thirds of the
  time. Each run lost one to eight people (ransoms not paid, people turned away, a reporter given up), and was sold out
  at most once.
- **Money:** debts, bail, leases and ransoms scale with the player's cred. Rich players paid 5,000 to 17,000 cred
  ransoms; poor ones 300-cred debts.
- **Bonds:** they spread out (warm, friend, close, cold) instead of all ending at 100, because strong feelings fade a
  little every week unless kept up.

## The issues from 2026-11-09, and what happened to them

1. **Repeated messages:** fixed. Nobody says the same line within eight lines, and arc beats choose another wording.
2. **"a firm" in headlines:** fixed. Sites nobody owns get their own headlines ("Unmarked facility in Rook Yards hit by
   saboteurs").
3. **Defection whiplash:** fixed. A new patron doesn't inherit the old one's suspicions, and offers come two, three,
   four weeks after a change. The hammer above still changes patrons often, but never within days.
4. **Same endings every time:** fixed. Endings vary by season. A reporter who trusts the player opens Burn the Board,
   and a Ghost who trusts them opens Sunrise.
5. **Money stops mattering:** better. Late requests cost thousands, and ransoms up to a fifth of the player's cred.
   Rich players still end the year with 70,000 to 150,000 cred.
6. **The long epilogue:** fixed. Two weeks after an ending a new season begins with its own conflict.
7. **Messages about strangers:** fixed. The handler only briefs the patron's operations; other corporations' wars come
   from the fixer.

## Still open

- **Sunrise in long games:** it needs a player who hacks deep or befriends the Ghost; the test players never do.
- **The test players' choices:** they answer requests with fixed odds. A human will be more selective, so the number of
  deaths and betrayals in real play will differ.
- **Pace:** a request every five to eight days, plus arc briefings, makes a busy inbox. It reads well in the logs; it
  needs a look in real play.
