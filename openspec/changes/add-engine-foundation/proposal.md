# Proposal: Engine foundation (M0)

## Why

Nothing exists yet. Before any gameplay, Silverrock needs a codebase that builds and runs on Windows, Linux and the
web (including phones), reads input from keyboard, mouse, gamepad and touch, and enforces the quality bar from day
one: ≥ 95 % unit-test coverage, automated game tests and spec traceability. Adding these gates later would mean
retrofitting tests onto untested code. Roadmap milestone: **M0 Foundation** (docs/concept/03-roadmap.md).

## What Changes

- CMake project with presets for Windows (MSVC), Linux (GCC, Clang), Web (Emscripten), coverage and sanitizers.
- Pinned, vendored dependencies: SDL3, glad 2, stb, Unity.
- Core library: math, fixed-point, RNG streams, hashing, allocators, containers, serialization, logging.
- Platform runtime: SDL3 main callbacks, GL 3.3 / GLES 3.0 context, fixed-timestep loop, headless mode,
  pause on focus loss, persistent storage, resize and letterbox handling, WebGL context-loss recovery.
- Minimal renderer: command lists, null backend, GL backend with a low-resolution render target.
- Input: device-agnostic actions, rebinding, keyboard and mouse, gamepad, automatic touch scheme with a floating
  stick, look zone and buttons, aim assistance, mobile fullscreen and orientation handling.
- Test infrastructure: unit tests on all targets, coverage gate, scenario-script runner, test hooks, input replay,
  visual regression on software GL, browser end-to-end tests, sanitizers, fuzzing, spec traceability.
- CI on GitHub Actions for all targets, plus web deployment to GitHub Pages.
- A grey-box room to walk around in, as the exit demo.

## Capabilities

### New Capabilities

- `platform-runtime`: running the game on all targets — startup, fixed timestep, headless mode, focus handling,
  storage, display handling, context loss, logging, web download size.
- `input-controls`: device-independent actions, rebinding, keyboard and mouse, gamepad, the touch scheme and its
  automatic activation, aim assistance, mobile orientation.
- `quality-gates`: the automated quality bar enforced in CI — coverage, exclusions, traceability, game tests,
  replays, visual regression, browser tests, sanitizers, cross-platform matrix, spec validation, layering, fuzzing.

### Modified Capabilities

None (first change).

## Out of scope

City generation, gameplay systems, audio beyond a silent device, final art and UI styling. These belong to later
milestones.

## Impact

- Creates the whole repository structure (`src/`, `tests/`, `tools/`, `third_party/`, `web/`, CI workflows).
- New dependencies: SDL3, glad 2, stb, Unity (vendored, pinned); Playwright, OpenSpec CLI and Python 3 for tooling.
- Every later change builds on these gates.
