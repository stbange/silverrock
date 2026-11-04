# Tasks

## 1. Faction simulation core

- [ ] 1.1 Implement faction resources with bounds and mean-reverting income; verify "Resources stay in bounds" in a 52-week soak
- [ ] 1.2 Implement goals, operation templates and the strategic tick; verify "Deterministic strategy" and "Doctrine shapes choices"
- [ ] 1.3 Implement contracts from operation steps and channel selection; verify "Operation step becomes a contract"
- [ ] 1.4 Implement off-screen resolution by NPC crews; verify "Unclaimed contract resolved"
- [ ] 1.5 Implement counterplay, attribution and framing; verify "Security rises after an attack", "Retaliation follows attribution" and "Framing redirects blame"

## 2. Consequences in the city

- [ ] 2.1 Apply outcomes as modifiers on ownership, security, patrols, prices and advertising; verify "Sabotage raises prices"
- [ ] 2.2 Implement gang turf wars, dissolution and replacement; verify "Turf change is visible" and "Gang replaced"
- [ ] 2.3 Implement megacorp protection floors; verify "Long soak keeps every megacorp"
- [ ] 2.4 Implement the intel Board; verify "Intel reveals a link"

## 3. Politics

- [ ] 3.1 Implement elections with influence, image, scandals and tampering; verify "Election held on schedule", "Scandal costs votes" and "Vote tampering"
- [ ] 3.2 Implement council policies and their world effects; verify "Curfew in effect" and "Police contract privatised"
- [ ] 3.3 Implement bribery and exposure; verify "Exposed councillor resigns"

## 4. Market and news

- [ ] 4.1 Implement share prices and trading; verify "Price reacts to an outcome" and "Buying shares"
- [ ] 4.2 Implement insider-trading suspicion; verify "Suspicious trade flagged"
- [ ] 4.3 Implement news generation, owner spin and distribution; verify "Event becomes news", "Owner protection" and "Billboards show headlines"
- [ ] 4.4 Implement planted and killed stories and anonymous reporting; verify "Planted story lowers public image" and "Anonymous reporting"

## 5. Story campaign

- [ ] 5.1 Implement patron selection, the prologue test gig and the retainer; verify "Random patron", "Chosen patron" and "Retainer offer"
- [ ] 5.2 Implement handler trust, T4 gating and betrayal; verify "Trust gates black ops" and "Betrayal discovered"
- [ ] 5.3 Implement acts and transitions; verify "Act progression"
- [ ] 5.4 Implement counter-offers and defection; verify "Defection"
- [ ] 5.5 Implement arc templates (the initial eight), the story director and branching; verify "Arc instantiation", "Branching on outcome" and "Skipped beat"
- [ ] 5.6 Implement the authored cast with dialogue templates and succession; verify "Handler succession"
- [ ] 5.7 Implement the endgame director and checkmate scenarios; verify "Dominance triggers the endgame"
- [ ] 5.8 Implement the five endings, the daylight mode for Sunrise and the post-ending sandbox; verify "Sunrise shows daylight" and "Continue after the ending"

## 6. Persistence

- [ ] 6.1 Extend the save format with faction, politics, market, news and story state plus migration; verify round-trip and migration tests

## 7. Integration checks

- [ ] 7.1 Add story-progression bots and the nightly campaign run on 20 seeds; verify "Bots complete the campaign"
- [ ] 7.2 Review generated histories from 10 soak runs for coherence and log fixes
- [ ] 7.3 Play the full campaign manually on at least two seeds across desktop and phone; record results in `tests/manual-verification.md`
- [ ] 7.4 Confirm every scenario of this change is covered in the traceability report

## Workflow follow-up

- Archive the change with `openspec archive add-corporate-chess` once CI is green on all targets.
