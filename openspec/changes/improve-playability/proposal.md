# Proposal: Playability pass after M5

## Why

Sixteen 26-week playtests after M5 (`docs/reviews/playtest-2026-11-07.md`) found that the systems
work but the game they add up to does not hold together:

- **The faction game is out of reach.** The player cannot choose jobs, and nothing in the UI offers
  the corporate game's contracts, handler jobs or black ops.
- **Most work is disconnected.** Fixer jobs are random templates with no link to the faction game.
- **Progression is too fast.** Knight comes by about day 35 and Legend by day 45–75 of a six-month
  save.
- **Money stops mattering.** Cash reached 20–80 times the weekly rent by week 2, and up to 1.9
  million by week 26.
- **There is no danger.** Simulated missions floor health at 1 HP, Heat has no lasting effect, and
  a hostile patron's hunt does nothing.
- **Loyalty means nothing.** Players switched patrons up to 13 times, sometimes back to a corp that
  was still hunting them.
- **The acts are badly paced.** Act II always runs to its time limit, Act III lasts 2–6 days, and
  80–97 days follow the ending with no goal.
- **The story stays invisible.** About 30 arcs per run never reach the player. The inbox is mostly
  repeated reminders, and headlines read like debug text.
- **Smaller issues:** the approaches are unbalanced, players who only take street gigs stall in
  Act I, and the endings have hidden requirements and no real choice.

This change fixes all of them before M6 adds content on top. Roadmap position: between **M5** and
**M6**.

## What Changes

- **Job selection.** Fixers show a list of offers, and the terminal gets a Jobs tab (board,
  contacts, handler, black ops, accepted jobs). Every offer shows client, target, tier, pay,
  deadline and approaches, and warns when it targets the patron.
- **Every job from the faction game.** Fixer offers come from open contracts in the fixer's
  district. When there are not enough, the district's factions start small operations on demand,
  so every job the player can take is a step in an operation.
- **A slower progression curve.** Street Cred comes from notable deeds, not volume. Each week has a
  cap, Street Cred decays when the player is idle above Runner, and the Knight and Legend tiers are
  tied to the story.
- **Money pressure that lasts.**
  - Living costs grow with success: housing, gear and cyberware upkeep, medical bills, the fixer's
    cut, and bribes to clean Heat.
  - T3 and T4 pay less; black ops become a weekly event paid partly in intel and favours.
  - Rent rises with notoriety.
- **Real danger.**
  - Simulated missions can injure and kill.
  - Hostile patrons and high Heat bring ambushes, raids and bounties.
- **Loyalty that holds.**
  - Trust is lost only when the patron learns of the job.
  - A corp the player betrayed never takes them back.
  - The player can defect once per campaign, and starts with low trust afterwards.
- **Paced acts.**
  - Minimum lengths for the Prologue and Act I.
  - The dominance trigger becomes reachable.
  - Act III becomes a climax of at least two weeks, with a final beat the player must play.
  - Act transitions get staged scenes.
  - The epilogue gives the player new goals.
- **A story that reaches the player.** Fewer arcs at a time, each announced by a cast member who
  names the people and places. The terminal gets a story log, the inbox stops repeating itself, and
  headlines are written for each outlet's voice.
- **Fair approaches.** Clean work earns bonuses and trust; loud work leaves lasting costs; netrunner
  paydata is worth more. Street-gig players get a nudge from the handler.
- **Endings with choices.** Kingmaker lets the player choose whom to crown. Sunrise is hinted by the
  Ghost. Each ending gets a short sequence and a visible change in the city.
- **Playtest bands in the nightly run.** Pacing and economy bands are checked on 16 runs, so these
  problems cannot come back unnoticed.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `job-system`: job selection, offers from contracts, risk in resolved missions, approach rewards,
  pay bands, economy bands.
- `faction-simulation`: contracts drawn by fixers, operations started on demand, reachable dominance.
- `character-progression`: the Street Cred curve.
- `player-life`: living costs, rent and notoriety, one reminder per week.
- `law-and-heat`: hunters (ambushes, raids, bounties).
- `story-campaign`:
  - handler trust and betrayal;
  - defection limits;
  - arcs announced through the cast;
  - act pacing, the Act III climax and the epilogue;
  - a nudge for stalled players;
  - ending choices and sequences;
  - pacing bands.
- `news-media`: written headlines, coverage of notoriety.

## Out of scope

- 3D hit squads in the street. Hunts resolve as encounters in this change; 3D street combat
  against hunters comes in M6.
- New arc templates (M6).
- Localisation of the new text (M6).
