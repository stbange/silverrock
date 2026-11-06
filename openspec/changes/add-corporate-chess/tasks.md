# Tasks

## 1. Faction simulation core

- [x] 1.1 Implement faction resources with bounds and mean-reverting income; verify "Resources stay in bounds" in a 52-week soak
- [x] 1.2 Implement goals, operation templates and the strategic tick; verify "Deterministic strategy" and "Doctrine shapes choices"
- [x] 1.3 Implement contracts from operation steps and channel selection; verify "Operation step becomes a contract"
- [x] 1.4 Implement off-screen resolution by NPC crews; verify "Unclaimed contract resolved"
- [x] 1.5 Implement counterplay, attribution and framing; verify "Security rises after an attack", "Retaliation follows attribution" and "Framing redirects blame"

## 2. Consequences in the city

- [x] 2.1 Apply outcomes as modifiers on ownership, security, patrols, prices and advertising; verify "Sabotage raises prices"
- [x] 2.2 Implement gang turf wars, dissolution and replacement; verify "Turf change is visible" and "Gang replaced"
- [x] 2.3 Implement megacorp protection floors; verify "Long soak keeps every megacorp"
- [x] 2.4 Implement the intel Board; verify "Intel reveals a link"

## 3. Politics

- [x] 3.1 Implement elections with influence, image, scandals and tampering; verify "Election held on schedule", "Scandal costs votes" and "Vote tampering"
- [x] 3.2 Implement council policies and their world effects; verify "Curfew in effect" and "Police contract privatised"
- [x] 3.3 Implement bribery and exposure; verify "Exposed councillor resigns"

## 4. Market and news

- [x] 4.1 Implement share prices and trading; verify "Price reacts to an outcome" and "Buying shares"
- [x] 4.2 Implement insider-trading suspicion; verify "Suspicious trade flagged"
- [x] 4.3 Implement news generation, owner spin and distribution; verify "Event becomes news", "Owner protection" and "Billboards show headlines"
- [x] 4.4 Implement planted and killed stories and anonymous reporting; verify "Planted story lowers public image" and "Anonymous reporting"

## 5. Story campaign

- [x] 5.1 Implement patron selection, the prologue test gig and the retainer; verify "Random patron", "Chosen patron" and "Retainer offer"
- [x] 5.2 Implement handler trust, T4 gating and betrayal; verify "Trust gates black ops" and "Betrayal discovered"
- [x] 5.3 Implement acts and transitions; verify "Act progression"
- [x] 5.4 Implement counter-offers and defection; verify "Defection"
- [x] 5.5 Implement arc templates (the initial eight), the story director and branching; verify "Arc instantiation", "Branching on outcome" and "Skipped beat"
- [x] 5.6 Implement the authored cast with dialogue templates and succession; verify "Handler succession"
- [x] 5.7 Implement the endgame director and checkmate scenarios; verify "Dominance triggers the endgame"
- [x] 5.8 Implement the five endings, the daylight mode for Sunrise and the post-ending sandbox; verify "Sunrise shows daylight" and "Continue after the ending"

## 6. Persistence

- [x] 6.1 Extend the save format with faction, politics, market, news and story state plus migration; verify round-trip and migration tests

## 7. Integration checks

- [x] 7.1 Add story-progression bots and the nightly campaign run on 20 seeds; verify "Bots complete the campaign"
- [x] 7.2 Review generated histories from 10 soak runs for coherence and log fixes
- [ ] 7.3 Play the full campaign manually on at least two seeds across desktop and phone; record results in `tests/manual-verification.md`
- [x] 7.4 Confirm every scenario of this change is covered in the traceability report

## Implementation notes

- Code: `src/sim/sr_chess.h` with `sr_board.c` (factions, operations, contracts, counterplay, gangs, Board),
  `sr_politics.c`, `sr_market.c`, `sr_news.c`, `sr_story.c`; game glue in `src/game/sr_game_chess.c`.
- Contracts reach the player as jobs on the matching channel (fixer, board, gang contact, handler) when
  their tier is unlocked; anything else goes to NPC crews at the deadline. Job results (alarms or kills
  = evidence, planted evidence = framing) resolve the contract.
- Effects reuse existing systems: building owners, facility security, district price index (shops),
  gang turf (graffiti in the chunk meshes), police contract (officer colours, Heat jurisdiction),
  curfew (crowd density at night, police stops).
- Matrix: media hosts offer *Plant a story* and *Kill the next story*; city hall offers *Alter the vote
  count*; a corp whose councillor is bought offers *Leak bribery evidence*; downloaded paydata reveals
  the owner's links on the Board.
- Terminal tabs: Home, News, Market (E buy 10, R sell 10), Board, Story (retainer, defection, endings).
  Street HUD shows the district's billboard headline.
- Save format v4 adds sections 10–14; older saves start the corporate game at load time.
- Bots: `sr_headless --campaign N` (nightly 20 seeds), `sr_headless --soak N` (histories); review in
  `docs/reviews/m5-soak-review.md`.
- 7.3 needs manual play on desktop and phone (see `tests/manual-verification.md`).

## Workflow follow-up

- Archive the change with `openspec archive add-corporate-chess` once CI is green on all targets.
