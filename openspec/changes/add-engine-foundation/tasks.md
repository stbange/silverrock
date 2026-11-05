# Tasks

## 1. Repository and build

- [x] 1.1 Add the CMake project with presets `linux-gcc-debug`, `linux-clang-asan`, `linux-coverage`, `windows-msvc`, `web-debug` and `web-release`; verify every preset configures and builds an empty executable
- [x] 1.2 Vendor and pin SDL3, glad 2, stb and Unity under `third_party/`; verify a clean clone builds offline from the pinned sources on all presets
- [x] 1.3 Enable warnings-as-errors for first-party code on GCC, Clang and MSVC, and add clang-format and clang-tidy configurations; verify a deliberately misformatted fixture fails the lint step
- [x] 1.4 Add the layer dependency check script; verify it fails on a fixture with a forbidden include (quality-gates: Forbidden include)

## 2. Test framework and coverage

- [x] 2.1 Add Unity-based test executables per module for native and WebAssembly (Node) builds; verify `ctest` passes in every preset (quality-gates: WebAssembly unit tests)
- [x] 2.2 Add the coverage preset with gcovr gates (95 % overall, 90 % per directory, third-party excluded); verify fixtures for "Coverage below threshold", "Directory below minimum" and "Third-party code is not counted"
- [x] 2.3 Add the coverage-exclusion budget script; verify fixtures for "Exclusion without a reason" and "Exclusion budget exceeded"
- [x] 2.4 Add the spec traceability tool (`SR_SPEC` macro, `@spec` tags, manual verification list); verify fixtures for "Untested scenario" and "Change in progress"
- [x] 2.5 Add the Playwright project with desktop browsers and mobile emulation; verify it runs an empty smoke test in CI

## 3. Core library

- [x] 3.1 Implement vectors, matrices, quaternions and Q16.16 fixed-point math with unit tests; verify golden results are identical on Linux, Windows and Web
- [x] 3.2 Implement PCG32 with named stream derivation and 64-bit hashing; verify cross-platform golden sequences in unit tests
- [x] 3.3 Implement arena and pool allocators with fault injection; verify out-of-memory paths are covered by unit tests
- [x] 3.4 Implement dynamic arrays, an ordered hash map, ring buffers, string helpers and logging; verify unit tests and ≥ 95 % coverage of the core directory
- [x] 3.5 Implement binary serialization helpers, CRC32 and the state-checksum utility; verify round-trip unit tests

## 4. Platform runtime

- [x] 4.1 Implement the SDL3 main-callback entry point, window and GL 3.3 / GLES 3.0 context creation; verify "Desktop build starts" and "Web build starts on a static host"
- [x] 4.2 Implement the fixed-timestep loop with the 5-step clamp and interpolation factor; verify "Frame rate does not change results" and "Long frame is clamped" with a fake clock
- [x] 4.3 Implement pause on focus loss and page visibility, audio mute and the pause-menu resume; verify "Page becomes hidden" and "Returning to the game" with injected events and a Playwright test
- [x] 4.4 Implement the per-user storage abstraction (desktop user path, IndexedDB-backed mount with sync on the web) and the settings file; verify "Settings survive a restart" natively and with a page reload in Playwright
- [x] 4.5 Implement windowed/fullscreen modes, resize and letterboxing; verify "Resize keeps the aspect ratio" with unit tests of the viewport math
- [x] 4.6 Implement WebGL context-loss recovery; verify "Context is restored" in Playwright using the lose-context browser extension API
- [x] 4.7 Implement log sinks and the "WebGL 2 required" message in the HTML shell; verify "Recoverable error is logged" and "Browser without WebGL 2"
- [x] 4.8 Add the web download size check; verify "Release size check" fails on an oversized fixture

## 5. Rendering bootstrap

- [x] 5.1 Implement render command lists and the null backend; verify unit tests on generated command streams
- [x] 5.2 Implement the minimal GL backend (shader preamble injection, meshes, texture arrays, low-resolution framebuffer, nearest-neighbour upscale); verify unit tests against a software GL context in CI
- [x] 5.3 Build the grey-box room with a first-person camera and a few coloured lights; verify a visual-regression golden image (quality-gates: Unintended visual change)

## 6. Input and controls

- [x] 6.1 Implement the device-agnostic input queue, action map, rebinding and persistence; verify "Same action from every device", "Rebound key" and "Conflicting binding"
- [x] 6.2 Implement the keyboard and mouse scheme with relative mouse mode and web pointer lock; verify "Pointer lock in a desktop browser" in Playwright
- [x] 6.3 Implement the gamepad scheme with hot-plug and disconnect pause; verify "Gamepad hot-plug" and "Active gamepad disconnected" with injected events
- [x] 6.4 Implement the touch overlay as pure layout and hit-testing (floating stick, look zone, buttons, safe areas, long press, context icon); verify every touch scenario of input-controls in unit tests
- [x] 6.5 Implement scheme detection (coarse-pointer query on the web) and last-device switching; verify "Mobile browser start", "Desktop browser start" and "Hybrid device switches schemes"
- [x] 6.6 Implement aim assistance and hold/toggle options; verify "Slow-down near a target", "No assistance for the mouse" and "Toggle crouch"
- [x] 6.7 Implement fullscreen and orientation handling with the rotate prompt; verify "Portrait orientation" in Playwright mobile emulation

## 7. Game-test infrastructure

- [x] 7.1 Implement headless mode, the test-hook command and query interface (test builds only) and the scenario-script runner; verify "Headless script passes" and "Headless script fails"
- [x] 7.2 Implement input recording and replay with periodic checksums; verify "Replay matches" and "Divergence is reported"
- [x] 7.3 Implement the visual-regression runner and golden-image updater; verify a deliberate colour change fails the comparison
- [x] 7.4 Add the sanitizer configuration and nightly fuzz targets for the scenario parser, settings parser and seed codes; verify "Memory error detected" and "Crash found by the fuzzer" with planted fixtures
- [x] 7.5 Add the release symbol check; verify "Release symbol check" on all targets

## 8. Continuous integration and deployment

- [ ] 8.1 Create GitHub Actions workflows for lint, Linux GCC with coverage, Linux Clang with sanitizers, Windows MSVC, Web build and tests, and Playwright; verify all jobs are green on the main branch (quality-gates: Failing game test blocks the merge, New compiler warning)
- [x] 8.2 Add the strict OpenSpec validation job; verify "Malformed scenario" with a fixture
- [ ] 8.3 Deploy the web build from the main branch to GitHub Pages; verify the deployed page boots in a desktop browser and on a phone
- [ ] 8.4 Add the nightly workflow (fuzzing, extended seed sweeps as they arrive); verify a scheduled run completes

## 9. Integration checks

- [ ] 9.1 Walk the grey-box room on Windows, Linux, desktop Chromium, Firefox and Safari, and on a real Android and iOS phone with touch; record the results in `tests/manual-verification.md`
- [x] 9.2 Run the traceability report and confirm every scenario of this change has a test or a justified manual entry

## Implementation notes (2026-11-05)

- Unit tests use a small in-house C harness (`tests/sr_test.h`) instead of Unity: it compiles to WebAssembly unchanged and carries the `SR_SPEC` traceability tags. GL entry points are loaded by an in-house X-macro loader instead of glad; stb is not needed (golden images are PPM).
- Open: 8.1/8.3/8.4 need the branch merged to main (green CI on main, GitHub Pages deployment, a scheduled nightly run; the nightly also runs on pushes that change it). 9.1 needs real Windows hardware and phones.
- Coverage on the unit and platform suites: 98.8 % lines, all directories above 96 %, exclusions 0.58 % (2026-11-05, Linux GCC).

## Workflow follow-up

- Archive the change with `openspec archive add-engine-foundation` once CI is green on all targets.
