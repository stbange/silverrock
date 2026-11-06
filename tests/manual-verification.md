# Manual verification

Scenarios that need reference hardware or a human eye. Format (read by `tools/spectrace.py`):
`- capability: Scenario name -- why it cannot be fully automated, and how it is checked`

- retro-rendering: Desktop benchmark route -- needs the reference desktop GPU (Iris Xe class); CI renders on software GL where frame times are meaningless. Checked per milestone by playing the benchmark route with the frame-time overlay; the automated part (draw-call and triangle counts, dynamic resolution) is unit-tested.

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
