# Design: Engine foundation

## Context

Greenfield repository. The constraints (C17, SDL3, OpenGL, three targets, touch on mobile web, ≥ 95 % coverage,
automated game tests) are listed in `openspec/config.yaml` and explained in docs/concept/02-technical-design.md.
See proposal.md for motivation.

## Goals / Non-Goals

**Goals:**
- One main-loop model and one shader dialect for all targets.
- An architecture where almost all code is pure and testable without a window or GPU.
- Quality gates that are active before the first gameplay line is written.

**Non-Goals:**
- Performance tuning beyond the grey-box room.
- Final asset pipeline, audio mixing, UI styling.

## Decisions

### D1 — SDL3 main callbacks on every platform
The entry point uses SDL3's callback model (init, event, iterate, quit). On the web, SDL schedules the callbacks
from `requestAnimationFrame`.
*Alternatives:* a classic `while` loop on desktop plus `emscripten_set_main_loop` on the web (two code paths, harder
to test); Asyncify (larger binaries, slower).

### D2 — OpenGL 3.3 Core and OpenGL ES 3.0 from one GLSL source
Shaders are written in the common subset; a preamble (`#version 330 core` or `#version 300 es` plus precision)
is injected at load time. Geometry shaders, `glPolygonMode` and texture buffers are not used.
*Alternatives:* separate shader sets (duplication); WebGPU (not requested, uneven mobile support, no GL path for
desktop).

### D3 — No threads required on the web
Background work (later: chunk generation, navigation) is written as resumable tasks with a per-frame time budget.
Desktop may add an SDL worker pool; results merge in request order so outcomes never depend on thread count.
*Alternatives:* Emscripten pthreads (needs `SharedArrayBuffer` and COOP/COEP headers, which static hosts and some
mobile browsers make painful).

### D4 — Functional core, imperative shell
Only `platform/`, `render/gl/` and `app/` touch SDL or GL. Everything else is pure C, deterministic and fed through
explicit context structs. A layering script enforces this on every `#include`.
*Alternatives:* conventional engine layering with SDL calls throughout (cannot reach 95 % without a display).

### D5 — In-house test harness, gcovr for coverage
*Revised during implementation:* a ~100-line harness (`tests/sr_test.h`) replaced Unity. It is plain C, runs natively and as WebAssembly under Node, and carries the `SR_SPEC` tags. The coverage gate is
computed with gcovr on the Linux GCC unit-test run. GL backend tests use a real GL 3.3 context on Mesa llvmpipe
through SDL's offscreen driver, so backend code counts towards coverage legitimately.
*Alternatives:* cmocka (fine, but mocking style adds friction), Criterion (no MSVC/Wasm story).

### D6 — Headless runner with a tiny scenario language
Game tests are text scripts (`seed`, `newgame`, `press`, `advance`, `expect`, `autoplay`, …) interpreted by the
game itself in headless mode, using a test-hook query interface compiled only into test builds. The same interface is
exported to JavaScript in web test builds for Playwright.
*Alternatives:* embedding Lua (new dependency, a second language to cover); C-only tests (too verbose for long
gameplay flows).

### D7 — Pinned, vendored dependencies
SDL3 is a git submodule at a fixed tag (release-3.2.8), built from source on every target — including under
Emscripten. *Revised:* GL entry points come from an in-house loader, so glad, stb and Unity are not needed.
*Alternatives:* system packages (version drift between CI images and developers).

### D8 — Spec traceability by tags
Tests reference scenarios by name: `SR_SPEC("capability", "Scenario name")` in C (compiles to nothing), or
`@spec capability: Scenario name` in scripts and Playwright tests. A Python tool matches them against all
`#### Scenario:` headings.

## Risks / Trade-offs

- [llvmpipe differs from real GPUs] → Visual goldens are only compared on llvmpipe; real-device checks are manual per
  milestone and listed in the manual verification file.
- [Strict coverage slows early prototyping] → Prototypes live on branches; only tested code merges.
- [Emscripten SDL3 build quirks] → Pin emsdk; a web smoke test runs on every push.
- [Touch layout can't know physical size] → Sizes in CSS pixels (≥ 48 px) plus a UI-scale setting.

## Migration Plan

Not applicable (greenfield).
