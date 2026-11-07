# Design: Playability pass after M5

## Context

The findings, with numbers, are in `docs/reviews/playtest-2026-11-07.md`, from 16 runs of 26 weeks
each. The tool that produced them, `sr_headless --playtest`, also verifies this change: its runs
gain pacing and economy bands that the nightly workflow enforces (D12).

Finding numbers (F1–F12) refer to the playtest notes.

## Goals / Non-Goals

**Goals:**
- Everything M5 built can be reached and understood by a human player, on desktop and touch.
- A six-month save has an arc: scraping by in the first weeks, rising through the middle, a climax,
  and an epilogue worth staying for.
- Choices stick: loyalty, betrayal, loud work and clean work all have consequences the player can
  see.
- The fixes stay data-driven and deterministic, and keep the existing tests green or update them
  deliberately.

**Non-Goals:**
- New content: arc templates, new districts or weapons (M6).
- 3D street combat against hunters (M6). Hunts resolve as encounters here.
- Rebalancing combat in the 3D levels. This change only touches the simulated mission path, the
  economy and the campaign.

## Decisions

### D1 — Job selection UI (F1)

**At a fixer.** Interacting with a fixer opens a job menu, reusing `SrMenu` with job rows. It shows
up to six offers:

- each row gives title, client, target, tier, pay, deadline and the approaches validated for it;
- E accepts and Esc leaves;
- touch taps a row;
- a second press on a row opens the briefing and intel purchase.

**On the terminal**, a new **Jobs** tab sits between Home and News. Its rows are grouped by channel:

- the handler's work (patron contracts and black ops);
- gang contacts;
- the public board (refreshed daily with `sr_jobs_board_refresh`, now actually called);
- accepted jobs.

Accepting from the terminal works anywhere. The handler and gang channels do not need a visit.

**Patron warning.** Any offer whose target is the patron, or a faction the patron funds or owns,
carries a red **PATRON** tag. Accepting it asks for a second press with a handler warning.

### D2 — Every job is a step in an operation (F2)

`sr_jobs_fixer_offers` gets its offers from the corporate game:

1. **Open contracts first.** It lists open contracts in the FIXER or BOARD channel whose target
   building lies in the fixer's district or a neighbouring one.
2. **Operations on demand.** If fewer than three are found, the factions active in that district
   start small operations: `sr_chess_demand_op(district, tier)` picks a client by doctrine weight
   and a fitting target. Each operation has one step, a normal deadline and normal effects.
3. **Street gigs are a step too.** Delivery, debt collection and find-item gigs become steps of
   gang or minor-faction operations, with effects scaled down to match.

This replaces the free-standing templates. Every completed job moves the board, so the news, prices
and turf react to the player's own work.

### D3 — Street Cred curve (F3)

The 0–100 scale and the tier thresholds stay. Only how Street Cred is gained and lost changes:

| Source | Change |
|---|---|
| A clean job (no alarm, no kills) at the player's top tier | +1 |
| A high-profile target (a megacorp facility or HQ, or the job made the news) | +1 extra |
| A failed job, or one attributed to the player | −1 |
| Idle above Runner (no job for 7 days) | −1 per week |

- **Weekly cap:** at most +4 per in-game week.
- **Story gates:**
  - Knight needs Act II and patron trust of at least 40; until then Street Cred stops at 69.
  - Legend needs Act III or a completed ending; until then it stops at 89.

| Tier | Target (playtest band) | Today |
|---|---|---|
| Runner | weeks 3–5 | days 8–10 |
| Operator | weeks 6–9 | days 14–36 |
| Knight | weeks 10–16 | days 31–88 |

### D4 — Economy (F4)

**Living costs, charged each Sunday with rent:**

| Cost | Rule |
|---|---|
| Housing | Rent rises by 10 % per Street Cred tier above Runner (the landlord knows who you are) |
| Gear upkeep | 2 % of the value of installed cyberware and the equipped weapon |
| Fixer's cut | 10 % of the pay of every fixer job (handler and contact jobs are exempt) |

**Costs that come with events:**

- **Medical bills** after a mission that left the player injured: 15 cred per HP below maximum,
  charged when the player sleeps or visits a clinic.
- **Heat clean-up:** a fixer can lower Heat by one level for 500 × the level in cred.

**Pay and black ops:**

- T3 pays 1,500–5,000 instead of 3,000–12,000; T4 pays 5,000–15,000 instead of 10,000–50,000.
- Black ops come at most once per in-game week. Half the pay arrives as a reward instead:
  a Board reveal, a Heat wipe with one jurisdiction, or a cyberware voucher.

**Bands.** Until Act II, the median bot holds no more than 8 weeks of living costs. At week 26 it
holds no more than 150,000 cred. The gig bot keeps the existing starter-economy band.

### D5 — Risk in resolved missions (F5)

`sr_job_execute` stops clamping health at 1 HP. Damage beyond the player's health flatlines them
through `sr_game_flatline` (clinic or alley, as specified in `combat`). The resolved damage is
capped at 60 % of maximum health on the Story difficulty preset, so death there needs bad luck
twice in a row.

### D6 — Hunters (F5)

A hostile patron's `SR_OP_HUNT` operation, and Heat of 4 or more with any faction, schedule a
**hunt encounter**:

- **when the player is outdoors:** an ambush on their next street trip;
- **when the player is at home:** a raid at night.

The encounter resolves through the simulated mission path, using the hunting faction's force as
the difficulty and offering three choices: fight, flee or pay off. A short toast and a message
describe it. The outcome changes:

- **health:** injury or flatline;
- **cash:** the pay-off amount;
- **Heat;**
- **trust with the hunter.**

A bounty board in the gang channel lists active bounties on the player. Completing a hunt resolves
the hunter's operation.

### D7 — Loyalty (F6)

- **Discovery, not instant blame.** A job against the patron only costs trust when the patron
  learns of it. The patron learns with a chance of 25 % plus 25 % per evidence point (alarms,
  kills, a camera sighting, a witness), rolled after 1–3 days. Framing another faction prevents
  discovery.
- **Betrayed corps never return.** A corp the player betrayed (defected from or turned hostile)
  goes on a blacklist, makes no further offers and cannot become the patron again.
- **Defection limits:**
  - One voluntary defection per campaign. A second rival offer comes only while the current patron
    is hostile.
  - Trust after a defection starts at 25 (it was 40).
- **Trust levels are shown** on the Story tab, along with what each level unlocks.

### D8 — Act pacing (F7)

| Act | Rule |
|---|---|
| Prologue | The retainer offer comes no earlier than day 3 |
| Act I | Lasts at least 3 weeks |
| Act II | Lasts 6–12 weeks |
| Act III | A climax of at least 14 days |

**Act II and dominance.** Each successful takeover raises the owner's long-run capital level by 8
(remembered in `SrFactionSim`). The dominance threshold becomes `clamp(2.2 / corp_count, 0.25,
0.40)`, so a corp that snowballs can trigger the endgame before the time limit.

**Act III climax.**
- The finale arc has four beats.
- The last beat is a **set piece** the player must take part in: it never resolves off-screen, and
  its deadline extends until the player acts or abandons it (abandoning picks the Kingmaker ending
  for the rival).
- The ending choice opens after the set piece.

**Staged transitions.** Each act change triggers:
- a meeting message from the handler;
- three news items about the board;
- a visible city change for the new act: curfews for Act II, the patron's billboards everywhere
  for Act III.

**Epilogue goals.**
- After an ending, a new patron cycle opens two weeks later, starting from the board left by the
  ending.
- "Continue with a new character" keeps the changed city.

### D9 — Story presence (F8, F11)

**Arcs.**
- At most 1 arc runs in Act I and 2 in Act II; the climax arc runs alone.
- Beats are at least 3 days apart.
- Each arc type has a cast member who announces each beat in a message naming the people, the
  place and the stakes. The templates live in `sr_story_text.c`, with three variants per beat and
  branch.
- Skipped beats are reported afterwards ("While you were away…").

**Story log.** The terminal's Story tab keeps a log of acts, arcs, beats with their outcomes, and
trust changes. It is kept in `SrStory` (32 entries) and saved.

**Inbox.**
- The landlord sends one reminder, the day before rent is due.
- The same message from the same sender within 24 hours is merged and shown with a count.
- Story messages are pinned, and the inbox holds 64 messages.

**Nudge for stalled players.** After 14 days at Pawn in Act I, the handler offers a guided T1
contract and says what Act II needs.

### D10 — News writing (F9)

- Headline templates for each event type and outcome, with a voice for each outlet: corporate
  channels are clean and favour their owner, the pirate feed is angry.
- Templates are chosen so they fit within 63 characters. Long names fall back to their short form,
  meaning the corp name without its suffix.
- When the player reaches a new Street Cred tier or makes the news three times in a week, an outlet
  runs a story about the unknown runner, slanted by who owns it.

### D11 — Approach rewards (F10)

| Kind of work | Effect |
|---|---|
| Clean work (ghost, face or netrunner with no alarm) | +20 % pay bonus, +2 trust when it is the patron's job, −1 Heat |
| Loud work (any alarm) | Medical risk (D5), +1 Heat, and lasting effects in the target building (raised security, D2) |
| Paydata | Sells to fences at 1.5× its current price |
| Netrunner approach | Its plans count Matrix steps toward Street Cred |

The existing balance band (best approach / median ≤ 1.3) stays. A new band adds that the slowest
approach reaches Knight within 1.5× the time of the fastest.

### D12 — Playtest bands (verification)

`sr_headless --playtest 16 DIR --bands tests/balance/playtest_bands.txt` checks bands like the
balance report does:

| Band | Limit |
|---|---|
| Runner day | 21–35 |
| Knight day | 70–112 |
| Act III length | at least 14 days |
| Ending day | 120–200 |
| Defections per run | at most 1 |
| Median cash/living costs until Act II | at most 8 |
| Cash at week 26 | at most 150,000 |
| Flatlines over 16 runs | at least 1 (risk exists) |
| Reachable-play bots reaching an ending | at least 7 of 8 |

The playtest bots switch to the new UI paths (job menu, terminal Jobs tab), so the "reachable"
group exercises what a human can do. The nightly workflow runs the bands.

## Save format

Version 5:
- Story section: blacklist bitmask, voluntary defections used, story log, weekly Street Cred gain,
  set-piece state.
- Board section: long-run capital bonus per faction.
- Player section: medical debt.
- Jobs section: patron flag.

Version 4 saves migrate with an empty blacklist and log, and zero bonuses and debt.

## Risks / Trade-offs

- **Slower progression may feel grindy.** Mitigated by the notable-deed rewards and by fewer
  filler jobs (D2). Watch the manual playtests.
- **Death in resolved missions can feel unfair.** Mitigated by the cap on the Story preset, and a
  briefing that shows a risk estimate.
- **Operations on demand could flood the board.** At most one per district per day; they use the
  same strategy weights and cooldowns.
- **Text templates are new content.** They stay in one file, with three variants per beat, ready
  for localisation in M6.
