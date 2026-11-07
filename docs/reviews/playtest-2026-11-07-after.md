# Playtest notes after the playability pass (2026-11-07)

The same 16-run playtest as in `playtest-2026-11-07.md`, repeated after `improve-playability`
(phases 1–4). Each character plays 26 in-game weeks. Half are story bots, which also take the
patron's jobs through the API. The other half do only what the user interface offers: the fixer's
job menu, the terminal's Jobs tab and the story choices.

```bash
bash tests/balance/run_playtests.sh ./build/rel/sr_headless out/   # 16 runs, gated by playtest_bands.txt
```

The nightly workflow now runs this command and fails when a band is broken.

## The runs

Days are game days. Flatlines are counted from the run logs. Defections count every switch of
patron, including switches after the old patron turned hostile; only one voluntary defection is
allowed.

| Run | Bot | Runner | Knight | Retainer | Act II | Act III | Ending (day) | Jobs | Failed | Cash at week 26 | Flatlines | Defections |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | story | d30 | d114 | d5 | d29 | d113 | Loyal (d127) | 455 | 24 | 124,587 | 0 | 0 |
| 2 | ghost | d30 | d105 | d5 | d29 | d113 | Kingmaker (d128) | 474 | 29 | 124,091 | 0 | 0 |
| 3 | story | d29 | d100 | d6 | d29 | d113 | Loyal (d127) | 497 | 23 | 147,151 | 0 | 0 |
| 4 | face | d30 | d88 | d5 | d29 | d113 | Kingmaker (d128) | 445 | 14 | 123,076 | 0 | 0 |
| 5 | story | d30 | d100 | d5 | d29 | d113 | Loyal (d127) | 501 | 30 | 148,041 | 0 | 0 |
| 6 | hammer | d33 | – | d5 | d39 | d123 | Kingmaker (d137) | 374 | 62 | 23,562 | 0 | 4 |
| 7 | story | d30 | d111 | d5 | d32 | d116 | Loyal (d130) | 456 | 28 | 120,767 | 0 | 0 |
| 8 | netrunner | d31 | d118 | d5 | d31 | d115 | Kingmaker (d132) | 351 | 59 | 79,098 | 0 | 0 |
| 9 | story | d30 | d98 | d5 | d29 | d113 | Loyal (d127) | 515 | 34 | 154,851 | 0 | 0 |
| 10 | ghost | d36 | d107 | d5 | d36 | d120 | Kingmaker (d137) | 408 | 17 | 95,469 | 0 | 0 |
| 11 | story | d30 | d107 | d5 | d29 | d113 | Loyal (d127) | 487 | 27 | 129,737 | 0 | 0 |
| 12 | face | d29 | d97 | d6 | d29 | d113 | Kingmaker (d127) | 453 | 23 | 149,960 | 0 | 0 |
| 13 | gig | – | – | d5 | – | – | – | 103 | 0 | 2,462 | 0 | 0 |
| 14 | hammer | d30 | d103 | d5 | d30 | d114 | Kingmaker (d128) | 448 | 62 | 82,209 | 0 | 0 |
| 15 | gig | – | – | d5 | – | – | – | 117 | 0 | 477 | 0 | 0 |
| 16 | netrunner | d31 | d142 | d6 | d31 | d115 | Kingmaker (d129) | 336 | 60 | 13,479 | 5 | 0 |

## Before and after

| Finding (2026-11-07) | Before | After |
|---|---|---|
| F1 Jobs unreachable | Fixer auto-accepted; contract, handler and black-op jobs had no UI | Fixer job menu and terminal Jobs tab; all 8 interface-limited bots finished the campaign through them |
| F2 Jobs unrelated to the board | 0–17 of ~380 jobs per run came from the faction game | Every offer is a step in an operation (contracts, or operations started on demand) |
| F3 Progression | Runner d8–10, Knight d31–55, Legend by d45–75 | Runner d29–36 (median d30), Knight d88–142 (median d105), Legend only in Act III |
| F4 Money | 20–80× weekly rent by week 2; up to 1.9 million at week 26 | Median cash 7 weeks of living costs at Act II; 2k–155k at week 26 (median 124k) |
| F5 Danger | 0 flatlines; hunts did nothing; 1 HP floor | 6 flatlines in 16 runs (the netrunners); hunts become ambushes and raids; Heat 4+ brings raids |
| F6 Patron churn | 7–13 switches in a run | No voluntary defection beyond one; a corp that was betrayed never returns |
| F7 Act pacing | Act III 2–6 days; 80–97 days with no goal after the ending | Act III 14 days with a set piece; endings d127–137; a new cycle two weeks after the ending |
| F8 Story presence | Inbox ~90 % reminders; 30 arcs per run, none told | Arcs announced by the cast, the patron's skipped beats reported, story log on the terminal, one rent reminder a week |
| F9 Headlines | "the Sabotage at Aurelian Grid Site 4", cut off | Templates per event and outlet voice, fitted to 63 characters |
| F10 Approaches | Netrunner Knight d55–88 vs d33–36 | Slowest approach's median Knight day within 1.4× of the fastest |
| F11 Gig players | Stuck with no word | Handler's nudge and a starter job after two weeks of street gigs only |
| F12 Endings | Hidden requirements; Kingmaker without choice | Requirements on the Story tab, the player crowns a megacorp, a short sequence per ending |

## What a player now reads

The inbox of run 5 during Act I and Act II (excerpt):

> Bren Calloway: We liked your work. A retainer: steady pay, better jobs. Accept on the terminal.
> Bren Calloway: Takeover season: Rahman-Dumont against Vantor Biosystems. Plans first, at Rook Civic Center.
> Bren Calloway: We have their layout. Now Rahman-Dumont takes Rook Civic Center.
> Bren Calloway: You're one of ours now. Real operations from here on. Rivals will call. Don't answer.
> Celeste Iwasaki: Council seats are for sale and Aurelian Cybernetics is shopping.
> Zora Calloway: A scientist at Vantor Biosystems is talking to Rahman-Dumont. We need proof first.

Headlines from the same week:

> Thieves raid Vantor Biosystems offices in Rook Civic Center
> Kestrel Grid confirms breach at Kilroy Campus site
> Who is the runner everyone in Rook Exchange talks about?

## Still open

1. **Endings by bot type.** Story bots end Loyal, while the interface-limited bots mostly end
   Kingmaker. The bots pick from the offered endings by seed, and Kingmaker now offers one row per
   megacorp, so it wins most draws. This is a bot effect; human choices will differ. Watch it in
   manual play.
2. **Hammer runs still lose their patron.** One hammer run turned its patrons hostile four times.
   Each switch came after a discovered betrayal, not a voluntary defection. Loud play against the
   patron's interests is expensive, which is the intent, but the warning on offers may need to be
   louder.
3. **Generic headline wording.** Headlines about gangs and the city government read oddly
   ("Blast halts City operations"), and a missing subject shows as "a firm". Templates for
   non-corporate victims would fix this.
4. **Black ops are rare by design.** With the weekly limit, a Knight sees about one black op a
   week. That matches the intent; check that it still feels worth chasing.
