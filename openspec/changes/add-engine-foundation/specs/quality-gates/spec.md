# Spec Delta: quality-gates

## Purpose

Defines the automated quality bar every change must pass in continuous integration: unit-test coverage, spec-to-test
traceability, automated game tests, sanitizers, cross-platform builds and spec validation.

## ADDED Requirements

### Requirement: Unit-test coverage gate
CI SHALL fail when line coverage of first-party source code, measured on the unit-test suite alone, is below 95 %, or
when any first-party source directory is below 90 %. Vendored third-party code SHALL NOT be counted.

#### Scenario: Coverage below threshold
- **WHEN** a change lowers overall first-party line coverage to 94.9 %
- **THEN** CI fails and lists the files with uncovered lines

#### Scenario: Directory below minimum
- **WHEN** overall coverage is at least 95 % but one source directory is at 89 %
- **THEN** CI fails and names that directory

#### Scenario: Third-party code is not counted
- **WHEN** coverage is computed
- **THEN** vendored third-party code appears neither in the numerator nor in the denominator

### Requirement: Bounded coverage exclusions
Every region excluded from coverage SHALL carry a written reason, and excluded lines SHALL NOT exceed 1.5 % of
first-party source lines.

#### Scenario: Exclusion without a reason
- **WHEN** a change adds a coverage exclusion without a reason
- **THEN** CI fails and points to the exclusion

#### Scenario: Exclusion budget exceeded
- **WHEN** excluded lines exceed 1.5 % of first-party source lines
- **THEN** CI fails and reports the current share

### Requirement: Spec-to-test traceability
CI SHALL fail when a scenario in an implemented spec has neither an automated test tagged with its capability and
scenario name nor an entry with a justification in the manual verification list.

#### Scenario: Untested scenario
- **WHEN** an implemented spec contains a scenario that no test references and that is not on the manual list
- **THEN** CI fails and names the capability and scenario

#### Scenario: Change in progress
- **WHEN** a scenario belongs only to a change that has not been archived yet
- **THEN** the traceability report lists it as pending without failing CI

### Requirement: Automated game tests in CI
CI SHALL run all headless scenario scripts and autoplayer test runs on Linux and Windows for every change and block
merging when one fails.

#### Scenario: Failing game test blocks the merge
- **WHEN** a scenario script fails in CI
- **THEN** the change cannot be merged and the job log shows the failing script line

### Requirement: Deterministic replay
Replaying a recorded input stream SHALL produce identical simulation checksums at every checkpoint on the same build
and platform.

#### Scenario: Replay matches
- **WHEN** a recorded session is replayed on the build that recorded it
- **THEN** every checkpoint checksum matches the recording

#### Scenario: Divergence is reported
- **WHEN** a replay diverges from its recording
- **THEN** the test fails and reports the first diverging step

### Requirement: Cross-platform build and test matrix
Every change SHALL be built and unit-tested in CI on Windows with MSVC, on Linux with GCC and Clang, and for the web
with Emscripten, where the unit tests run as WebAssembly in a JavaScript runtime.

#### Scenario: WebAssembly unit tests
- **WHEN** CI runs for a change
- **THEN** the complete unit-test suite compiled to WebAssembly executes and passes

### Requirement: Visual regression tests
CI SHALL render reference scenes with a software rasteriser and fail when the result differs from the approved
reference images beyond the configured tolerance, publishing difference images.

#### Scenario: Unintended visual change
- **WHEN** a change alters the rendering of a reference scene beyond the tolerance
- **THEN** CI fails and publishes the reference, actual and difference images

### Requirement: Browser end-to-end tests
CI SHALL run browser tests against the web build in desktop browsers and in emulated mobile devices with touch.

#### Scenario: Desktop boot
- **WHEN** the web build is opened in each desktop browser of the test matrix
- **THEN** the main menu appears and no errors are logged to the console

#### Scenario: Mobile emulation uses touch controls
- **WHEN** the web build is opened in an emulated phone with touch
- **THEN** a new game shows the touch overlay and a simulated drag on the movement stick changes the player position

### Requirement: Sanitizers and warnings
Unit and game tests SHALL run under address and undefined-behaviour sanitizers with zero findings, and first-party
code SHALL compile without warnings at the strictest configured warning level.

#### Scenario: Memory error detected
- **WHEN** a test triggers an out-of-bounds access or a leak
- **THEN** the sanitizer job fails and reports the location

#### Scenario: New compiler warning
- **WHEN** a change introduces a compiler warning in first-party code on any target
- **THEN** the build fails

### Requirement: Spec validation
CI SHALL validate all specs and changes in strict mode and fail on any validation error or warning.

#### Scenario: Malformed scenario
- **WHEN** a spec contains a requirement without a scenario
- **THEN** the validation job fails and names the requirement

### Requirement: Layer dependency check
CI SHALL fail when a module includes headers from a higher architectural layer, or when platform or graphics API
headers are included outside the modules allowed to use them.

#### Scenario: Forbidden include
- **WHEN** a simulation source file includes a platform or graphics API header
- **THEN** the layer check fails and names the file and the include

### Requirement: Nightly fuzzing
A nightly job SHALL fuzz every parser of external input — save files, settings, data tables, scenario scripts and
seed codes — and report any crash with a reproducing input.

#### Scenario: Crash found by the fuzzer
- **WHEN** the fuzzer finds an input that crashes a parser
- **THEN** the nightly job fails and attaches the reproducing input

### Requirement: Test hooks excluded from release builds
Release builds SHALL NOT contain the test command and query interface.

#### Scenario: Release symbol check
- **WHEN** a release build is produced for any target
- **THEN** a check confirms that the test interface is neither compiled in nor exported
