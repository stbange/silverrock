# Manual verification

Scenarios that need reference hardware or a human eye. Format (read by `tools/spectrace.py`):
`- capability: Scenario name -- why it cannot be fully automated, and how it is checked`

- retro-rendering: Desktop benchmark route -- needs the reference desktop GPU (Iris Xe class); CI renders on software GL where frame times are meaningless. Checked per milestone by playing the benchmark route with the frame-time overlay; the automated part (draw-call and triangle counts, dynamic resolution) is unit-tested.
- world-environment: Night at noon -- superseded by improve-city-atmosphere: noon is now a short, dark overcast day. Its scenarios "Night at midnight", "Noon stays dark" and "Short days" are tested in tests/unit/test_world.c.

## Results log

| Date | Build | Platform | Check | Result |
|---|---|---|---|---|
| 2026-11-05 | 65533d5 | Linux, Xvfb + Mesa llvmpipe | Platform tests, golden street image | pass |
| 2026-11-05 | 65533d5 | Chromium (SwiftShader), desktop and Pixel 7 emulation | Browser end-to-end suite | pass |
| pending | — | Windows 11, real GPU | Walk the city, keyboard/mouse and gamepad | not yet run |
| pending | — | Android and iOS phones | Touch controls, rotate prompt, 30 FPS | not yet run |

## M4 — The Matrix (add-matrix 7.1)

| Target | Exit demo: prepare a run in the Matrix, then execute it in meatspace | Result | Date | Tester |
|---|---|---|---|---|
| Windows 11 desktop | jack in at the home rig, breach a host, loop its cameras and unlock a door, then walk in | not yet run | | |
| Linux desktop | same | not yet run | | |
| Chromium / Firefox / Safari desktop | same | not yet run | | |
| Android phone (touch) | same, with the touch flight controls and breach by touch | not yet run | | |
| iPhone (touch, home-screen app) | same | not yet run | | |


## M5 — Corporate chess (add-corporate-chess 7.3)

Automated proxy: `sr_headless --campaign 20` (story bots reach an ending on every seed, nightly) and
`tests/unit/test_chess.c` (every scenario). The manual run judges pacing and whether the story reads well.

| Target | Exit demo: the full campaign, from rent-is-due to an ending, on two seeds | Result | Date | Tester |
|---|---|---|---|---|
| Linux or Windows desktop, seed A | prologue test gig, retainer, Act II, a defection or betrayal, Act III, an ending | not yet run | | |
| Linux or Windows desktop, seed B | same, aiming for a different ending (Sunrise needs the Pall Protocol and deep Matrix work) | not yet run | | |
| Phone (touch) | terminal tabs by stick, buying shares, story choices, billboard ticker readable | not yet run | | |

## Playability pass (improve-playability 5.5)

Automated proxy: `bash tests/balance/run_playtests.sh` (16 runs of 26 weeks, 12 bands, nightly) and
`tests/unit/test_play.c`. The manual run judges whether the jobs, the danger and the story feel right.

| Target | Check | Result | Date | Tester |
|---|---|---|---|---|
| Linux or Windows desktop | take a job from the fixer's job menu and one from the terminal's Jobs tab; accept a job against the patron after the warning | not yet run | | |
| Linux or Windows desktop | reach Heat 4 or a patron hunt and survive one ambush (fight, flee or pay) | not yet run | | |
| Linux or Windows desktop | reach Act III, take the climax set piece, crown a king or pick another ending, continue playing | not yet run | | |
| Phone (touch) | Jobs tab by tap (select, then tap again to accept), abandon a job, read the Story tab log | not yet run | | |

## City atmosphere (improve-city-atmosphere 11.8)

Automated proxy: the reference views and image metrics in `tests/platform/test_platform.c`
(`test_atmosphere_*`, run in CI on Mesa llvmpipe), the budget test in `tests/unit/test_atmos.c`, and
the sheets in `docs/screenshots/atmosphere/`. The manual look judges the mood and the frame rate on
real hardware.

| Target | Check | Result | Date | Tester |
|---|---|---|---|---|
| Linux or Windows desktop | walk six districts at night in rain; dawn, noon and dusk; fog and smog; under a highway; a rooftop | not yet run | | |
| Linux or Windows desktop | benchmark route at street detail high: 60 FPS | not yet run | | |
| Android phone | the same walk at the phone defaults (medium detail): 30 FPS on the 95th percentile | not yet run | | |
| iPhone (home-screen app) | the same walk; brightness setting at +2 in a bright room | not yet run | | |
