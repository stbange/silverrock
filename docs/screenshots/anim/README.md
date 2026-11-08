# Animation strips

Rendered by the platform tests at the default 480 × 270 internal resolution:

```bash
SR_ANIM_SHOTS=out xvfb-run -a build/linux-gcc-debug/sr_platform_tests test_anim_shots
```

```bash
SR_ANIM_SHOTS=out xvfb-run -a build/linux-gcc-debug/sr_platform_tests test_anim_street
```

`tools/anim_sheet.py out dir` then builds the sheets and GIFs. The selection below was cut down to
keep the repository small.

| File | What it shows |
|---|---|
| `clips_sheet.png` | Every clip on three body types (worker, executive, ganger), four frames each, near profile |
| `clip_walk.gif`, `clip_crouch_walk.gif` | Gaits with knees, elbows and hip bob; the third figure runs |
| `clip_sit.gif`, `clip_smoke.gif`, `clip_talk.gif`, `clip_dance.gif` | Street activities |
| `clip_umbrella.gif`, `clip_flee_arms.gif` | Overlays: an umbrella held up; an arm over the head in flight |
| `clip_aim.gif` | A guard's rifle aim |
| `clip_die_back.gif`, `clip_slump.gif`, `clip_stagger.gif` | A fall back from a shot, a knockout, a heavy hit |
| `street_crossing.gif` | A commercial street at 22:00 in drizzle |
| `street_shelter.gif` | A downpour: people under cover keep moving |
| `street_flee.gif` | Shots fired on the Strip: the crowd runs, cowers and films |
| `street_junction.gif` | Traffic in fog at an intersection: headlights, braking, turns |
| `view_model.png` | Pistol, rifle and knife: idle, shot, reload, swing, throw, reach |
| `firefight_flash.png` | A dark room before and during a guard's shot: the flash lights the walls |
