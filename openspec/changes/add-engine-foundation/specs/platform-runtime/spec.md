# Spec Delta: platform-runtime

## Purpose

Runs the game from a single codebase on Windows, Linux and web browsers, with a fixed-timestep simulation, a
headless mode for automated tests, persistent user storage and correct behaviour when the game loses focus.

## ADDED Requirements

### Requirement: Supported targets
The game SHALL build from one codebase and run on Windows x64, Linux x64 and web browsers with WebGL 2 support,
including mobile browsers. The web build SHALL NOT require cross-origin isolation or shared-memory threading.

#### Scenario: Desktop build starts
- **WHEN** the release build is launched on Windows x64 or Linux x64 on the reference desktop
- **THEN** a window opens and the first frame is presented within 3 seconds

#### Scenario: Web build starts on a static host
- **WHEN** the web build is served by a static file host without cross-origin isolation headers and opened in a WebGL 2 browser
- **THEN** the main menu is shown within 10 seconds on a broadband connection and no errors are logged to the browser console

#### Scenario: Browser without WebGL 2
- **WHEN** the web build is opened in a browser without WebGL 2 support
- **THEN** a readable message explains that WebGL 2 is required instead of showing a blank page

### Requirement: Fixed simulation timestep
The simulation SHALL advance in fixed steps of 1/60 second, independent of the rendering frame rate, and SHALL run
at most 5 steps per rendered frame.

#### Scenario: Frame rate does not change results
- **WHEN** the same recorded input is replayed once at 30 frames per second and once at 144 frames per second
- **THEN** the simulation state checksums taken every 60 steps are identical

#### Scenario: Long frame is clamped
- **WHEN** a single frame takes 2 seconds
- **THEN** the following update runs at most 5 simulation steps and the next frame is presented

### Requirement: Headless mode
The game SHALL offer a headless mode that runs the full simulation without a window, GPU or audio device, as fast as
possible, and exits with a status code reporting the result of the executed scenario script.

#### Scenario: Headless script passes
- **WHEN** the game runs a scenario script in headless mode and all expectations hold
- **THEN** it exits with status 0 without creating a window

#### Scenario: Headless script fails
- **WHEN** an expectation in the scenario script does not hold
- **THEN** the game exits with a non-zero status and prints the script name, the line number, the expected and the actual value

### Requirement: Focus loss and backgrounding
The game SHALL pause the simulation and mute audio when the window loses focus or the browser page becomes hidden,
and SHALL resume gameplay only after the player confirms.

#### Scenario: Page becomes hidden
- **WHEN** the browser page becomes hidden during gameplay
- **THEN** the in-game clock stops advancing and audio is muted

#### Scenario: Returning to the game
- **WHEN** the window regains focus or the page becomes visible again
- **THEN** the pause menu is shown and gameplay continues only after the player confirms

### Requirement: Persistent user storage
The game SHALL keep settings and save data in per-user storage that survives restarts on desktop, and page reloads
and browser restarts on the web.

#### Scenario: Settings survive a restart
- **WHEN** the player changes a setting and then restarts the game, or reloads the page on the web
- **THEN** the changed setting is still in effect

### Requirement: Display handling
The game SHALL support windowed and fullscreen modes and handle window resizes and device rotation, preserving the
aspect ratio of the rendered image by letterboxing instead of stretching.

#### Scenario: Resize keeps the aspect ratio
- **WHEN** the window is resized to an aspect ratio different from the render target
- **THEN** the image is scaled without distortion and the unused area is filled with black bars

### Requirement: Graphics context loss recovery
The web build SHALL recover from a lost WebGL context by recreating all GPU resources without losing game state.

#### Scenario: Context is restored
- **WHEN** the browser drops and later restores the WebGL context during gameplay
- **THEN** rendering resumes within 2 seconds of the restore and the game state checksum is unchanged

### Requirement: Diagnostics log
The game SHALL write log entries with a level (error, warning, info, debug), timestamp and subsystem name to a log
file on desktop and to the browser console on the web.

#### Scenario: Recoverable error is logged
- **WHEN** a recoverable error occurs, such as failing to write a settings file
- **THEN** an error entry with timestamp and subsystem is logged and the player sees a short message

### Requirement: Web download budget
The initial download of the web release build SHALL NOT exceed 20 MB compressed.

#### Scenario: Release size check
- **WHEN** the web release build is produced in CI
- **THEN** the compressed size of all files needed to reach the main menu is at most 20 MB, otherwise the build fails
