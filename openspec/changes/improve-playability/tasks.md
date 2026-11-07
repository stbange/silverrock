# Tasks

Phases are ordered by impact. Every phase ends green on CI. In "(spec: …)", a name alone refers to
a scenario in this change's deltas, and "D1"…"D12" refer to design decisions.

## 1. Quick wins (about 2–3 days)

- [x] 1.1 One landlord reminder per week; merge repeated messages within 24 h with a count; pin story messages; inbox of 64 (spec: "Rent reminders", "Repeated message merged")
- [x] 1.2 Headline templates by event type, outcome and outlet voice, fitted to 63 characters, in `sr_news_text.c` (spec: "Headline from a job", "Outlet voice")
- [x] 1.3 PATRON tag and handler warning on offers against the patron or factions it funds or owns (spec: "Job against the patron")
- [x] 1.4 Blacklist of betrayed megacorps; one voluntary defection per campaign; trust 25 after defecting (spec: "Defection", "No way back", "One defection")
- [x] 1.5 Remove the 1 HP floor from resolved missions; route deaths through `sr_game_flatline`; 60 % cap on the Story preset (spec: "Resolved mission can flatline", "Story preset cap")

## 2. Make M5 playable (about 1–1.5 weeks)

- [x] 2.1 Fixer job menu (rows: client, target, tier, pay, deadline, approaches; E accept, second press for the briefing; touch tap) (spec: "Fixer offers jobs", "Job selection by touch")
- [x] 2.2 Terminal Jobs tab (handler, gang contacts, public board refreshed daily, accepted jobs); accept from anywhere (spec: "Job board listings", "Accepting from the terminal")
- [x] 2.3 Fixer offers from open contracts in the district and its neighbours; `sr_chess_demand_op` tops up to three offers; street gigs become steps of gang or minor-faction operations (spec: "Fixer offers come from contracts", "Operations on demand")
- [x] 2.4 Discovery-based betrayal: roll 1–3 days later, 25 % + 25 % per evidence point; framing prevents discovery (spec: "Betrayal discovered", "Framed betrayal stays hidden")
- [x] 2.5 Hunt encounters: ambush outdoors or raid at home, fight / flee / pay off through the resolved-mission path; Heat 4+ triggers raids; bounties in the gang channel (spec: "Hostile patron sends hunters", "Paying off the hunters", "Heat brings raids", "Bounty listing")
- [x] 2.6 Switch the playtest bots to the UI paths: job menu and Jobs tab for the interface-limited group (spec: D12)

## 3. Balance (about 1 week)

- [x] 3.1 Street Cred from notable deeds, weekly cap of 4, idle decay above Runner, story gates at 69 and 89 (spec: "Weekly cap", "Idle decay", "Story gates")
- [x] 3.2 Living costs: upkeep 2 %, fixer cut 10 %, medical bills 15 cred per HP with medical debt, rent +10 % per tier above Runner, Heat clean-up at fixers (spec: "Upkeep with rent", "Medical bill", "Notoriety raises the rent")
- [x] 3.3 New T3/T4 pay ranges; black ops at most weekly, half paid in rewards (Board reveal, Heat wipe, cyberware voucher) (spec: "Black ops are rare")
- [x] 3.4 Approach rewards: clean bonus +20 % and −1 Heat, +2 trust on patron jobs, paydata ×1.5 at fences, Matrix steps count for netrunners (spec: "Clean bonus")
- [x] 3.5 Long-run capital +8 per takeover; dominance threshold `clamp(2.2 / corps, 0.25, 0.40)` (spec: "Takeovers accumulate", "Threshold scales with the board")
- [x] 3.6 Retune with the playtest harness until the bands hold; update `tests/balance/bands.txt` (spec: "Money keeps mattering", "Paced rise", "Approaches stay balanced")

## 4. Story pacing and presence (about 1.5 weeks)

- [x] 4.1 Act minimums (retainer from day 3, Act I ≥ 3 weeks, Act II 6–12 weeks); staged transitions (handler meeting, three news items, a city change per act) (spec: "Act progression", "Staged transition", "Retainer offer")
- [x] 4.2 Act III climax: four beats with the last as a set piece that never resolves off-screen; ending choice after the set piece; abandoning picks Kingmaker for the rival (spec: "The set piece waits for the player")
- [x] 4.3 Arc limits (1 in Act I, 2 in Act II), beats at least 3 days apart, cast messages from `sr_story_text.c` (3 variants per beat and branch), reports on skipped beats (spec: "Beat announced", "Skipped beat")
- [x] 4.4 Story log in `SrStory` (32 entries) shown on the Story tab; trust levels and ending requirements shown (spec: "Beat announced")
- [x] 4.5 Nudge for stalled players after 14 days at Pawn (spec: "Stalled at Pawn")
- [x] 4.6 Endings: Kingmaker chooses the megacorp; Sunrise hint from the Ghost; a sequence per ending (three messages, news, city change); a new patron cycle two weeks after the ending (spec: "Choosing a king", "Continue after the ending")
- [x] 4.7 Notoriety stories on each new tier and after three reports in a week (spec: "A name on the street")

## 5. Persistence and verification

- [ ] 5.1 Save format v5 (blacklist, defections, story log, weekly Street Cred, set piece, capital bonus, medical debt, patron flag on jobs) with migration from v4; round-trip and migration tests (spec: design: Save format)
- [ ] 5.2 `--playtest … --bands tests/balance/playtest_bands.txt` in the nightly workflow (spec: "Paced campaign")
- [ ] 5.3 Re-run the 16 playtests and write `docs/reviews/playtest-<date>.md` comparing against the 2026-11-07 notes
- [ ] 5.4 Update every test the new rules change (the starter economy, M5 trust and defection tests, campaign bot timing), and keep coverage at 95 % or above
- [ ] 5.5 Manual playtest on desktop and phone: job menu, Jobs tab, one hunt, the climax set piece, one ending; record in `tests/manual-verification.md`
- [ ] 5.6 Confirm every scenario of this change is covered in the traceability report

## Workflow follow-up

- Archive the change with `openspec archive improve-playability` once CI is green on all targets, after the
  M1–M5 changes are archived (its MODIFIED requirements target the specs they create).
