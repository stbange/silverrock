# What happened in the long test games (2026-11-09)

Sixteen computer-played characters each lived 26 weeks (182 days) in their own generated city, with the current game
(interiors, heavy fog, guidance). Each played a style: *story* follows the patron's campaign, *ghost* sneaks, *face*
talks, *hammer* fights, *netrunner* hacks, *gig* only does small street jobs. Below are four of them as plot summaries,
then the patterns across all sixteen and what looked wrong.

The raw logs come from `sr_headless --playtest 16 <dir>` (a daily CSV and an event log per run).

---

## 1. The loyal soldier

*Port Tern. An ex-soldier, playing the story. Patron: Orison Cybernetics.*

A nobody with 4 points of street cred takes small jobs around Port Tern. Within a week Orison Cybernetics' black-ops
handler, Oskar Brandt, likes the work and offers a retainer: "steady pay, better jobs". By day 25 the soldier is a
Runner, Orison trusts them, and the real operations begin.

Through the summer the soldier works Orison's war against **Sable Mutual**. They sabotage, steal data and smear in the
press, and Orison's takeover bids on Sable Mutual start to land. On day 101 Orison's board makes its move: **The
Merger**. By day 115 the soldier is a Legend, the city's news asks "Who is the runner everyone in Rook Exchange talks
about?", and the corporations are "on edge as a Legend works the city".

At the finale the soldier stays loyal. Orison wins, and the long tail of the war follows. An arc called **The Mole**
hunts a leak between Orison and Sable Mutual and is won on day 190. Curfews come and go in two districts. Two new gangs,
the Static Owls and the Neon Monks, carve out turf in the docks. Five hundred jobs, 147,000 cred, never evicted, never
dead.

## 2. The turncoat with a hammer

*Saint Marrow. A street kid who solves things with violence. Patron: Rubicon Industries.*

Rubicon Industries hires the kid on day 3. By day 20 the kid's loud methods have burned Rubicon's trust to zero, and a
rival, **Vantor Biosystems**, makes an offer. The kid takes it. Rubicon declares the kid hostile, and its hitters start
blocking the streets.

The pattern repeats:
- Day 41: the kid defects to **Argent Consolidated** and Vantor turns hostile.
- Day 69: to **Lattice Atmospherics**.
- Day 70: to **Kestrel Pharma**, one day later.

By midsummer four of the city's five megacorps want the kid dead. In between, a journalist named Tomas Santangelo keeps
asking for proof of council bribes: "Council seats are for sale and Vantor Biosystems is shopping."

In Act III Kestrel Pharma launches **The Merger**, then turns on the kid as well. With no patron left, the kid chooses
the **Kingmaker** ending: the one who decides who rules the city. They end the half-year poor (23,000 cred after 374
jobs and 62 failures), notorious (25 news stories about them) and alive.

## 3. The netrunner who kept dying

*East Vesper. A street kid who hacks. Patron: Nadir Grid.*

Nadir Grid hires the hacker on day 4. Money is tight from the start: cash falls from 2,400 to under 900 by week 3, and
the hacker **flatlines on day 36**. They wake up poorer and keep going. Meanwhile a street war between the Neon Hands
and three rival gangs spreads across three districts. A fixer named Ren Ferreira keeps reporting that "Neon Hands got
pushed back" and "the war is not over".

The hacker recovers in the autumn. A defector at Lattice Cybernetics wants out, the data checks out, and Nadir Grid
pulls them across. On day 102 Nadir Grid stages **The Coup**. On day 115 the hacker picks the **Kingmaker** ending and,
shortly after, reaches Knight, then Legend.

The epilogue goes badly: they flatline four more times in a month (days 121, 139, 145 and 151), and the savings drop
from 28,000 to 6,700. The run ends as a Legend with 13,000 cred, five deaths and 60 failed jobs.

## 4. The errand runner who never rose

*East Harrow. An ex-soldier who only does small gigs. Patron: Rubicon Capital.*

Rubicon Capital offers a retainer on day 3, and the ex-soldier takes it, then keeps doing errands. They never reach
Runner, so the campaign never starts. Over 26 weeks the inbox fills with the handler's frustration:
- "You're still doing errands. To move up you need Runner-level cred and our trust at 40."
- "While you were away, Rubicon Capital's crew went after Vantor Grid without you. It failed."
- "While you were away, someone else did Rubicon Capital's job on Kestrel Orbital. It worked. You weren't paid."

Outside, Rubicon Capital's shadow war plays out without them. Supply lines get cut, and the press blames Rubicon:
"Investigation: Rubicon Capital behind attacks". The ex-soldier makes rent every week on 2,500 cred at the most, and the
world moves on without them.

---

## Across all sixteen

- **Pacing:** 14 of 16 reach the finale, at day 127–137. The patron's retainer comes in the first week, Act II on day
  29–39, Act III on day 113–123. Only the two *gig* characters never leave Act I.
- **Endings:** all six *story* players end **Loyal**. Every *ghost*, *face*, *hammer* and *netrunner* chooses
  **Kingmaker**. No run sees Defector, Revolution or Sunrise.
- **Money:** *story* and *face* players end with 120,000–155,000 cred, *ghosts* with 95,000–124,000, the violent and
  the hackers with 13,000–82,000.
- **Hard times:** nobody is evicted. One character flatlines (the netrunner, five times).
- **Fame:** the *hammer* characters make the news 25–29 times; the quietest players two or three times.

## What looked wrong

1. **Repeated messages.** Handlers often send the same message twice in a row. Examples: "The envelope landed. Now
   Lattice Cybernetics's candidate needs a scandal." and "Clean grab. Next, Lattice Cybernetics's production line…".
2. **"a firm" in headlines.** When the owner of a site is unknown, headlines read "Blast halts a firm operations in Rook
   Yards" and "a firm facility in Tern Sump hit by saboteurs": broken grammar, and a lower-case start.
3. **Defection whiplash.** The hammer character defected twice in two days (day 69 and day 70). A new rival offer
   arrives straight after a defection, and nothing slows a chain of betrayals.
4. **Same endings every time.** Revolution, Defector and Sunrise never happened. This is partly how the test players
   choose, but the six loyal runs had nothing else on offer.
5. **Money stops mattering.** Careful players sit on 150,000 cred in the last two months with nothing left to buy. The
   balance bands accept this, but the late game has no money pressure.
6. **The long epilogue.** After the ending (around day 127) the runs keep playing two more months of Act IV with no new
   story goal. The world's war goes on: new arcs, curfews and gangs.
7. **Messages about strangers.** The patron's handler briefs operations for *other* corporations, for example Nadir
   Grid's handler on "Aurelian Atmospherics is buying Lattice Cybernetics the hard way". These are contract jobs, but
   from the handler they read as if the player switched sides.
