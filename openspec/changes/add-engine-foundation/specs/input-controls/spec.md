# Spec Delta: input-controls

## Purpose

Translates keyboard and mouse, gamepad and touch input into device-independent game actions, and switches
automatically to touch controls on touch devices such as phones and tablets.

## ADDED Requirements

### Requirement: Device-independent actions
Game logic SHALL react only to abstract actions — movement and look axes, fire, aim, jump, crouch, sprint, interact,
reload, gadget, quick slots, inventory, map, jack-in, Netsight and pause — and never to raw device input.

#### Scenario: Same action from every device
- **WHEN** jump is triggered by the keyboard binding, the gamepad binding or the touch jump button
- **THEN** the game receives the same Jump action in each case

### Requirement: Rebinding
The game SHALL let the player rebind every keyboard, mouse and gamepad action and SHALL persist the bindings.

#### Scenario: Rebound key
- **WHEN** the player rebinds Jump from Space to C
- **THEN** pressing C triggers Jump and pressing Space no longer does, also after a restart

#### Scenario: Conflicting binding
- **WHEN** the player assigns a key that is already bound to another action
- **THEN** the game offers to swap the two bindings or cancel, and never leaves two actions on the same key silently

### Requirement: Keyboard and mouse scheme
The game SHALL provide default keyboard and mouse bindings and relative mouse look; in desktop browsers it SHALL
request pointer lock when the player clicks the game.

#### Scenario: Pointer lock in a desktop browser
- **WHEN** the player clicks the game canvas in a desktop browser during gameplay
- **THEN** pointer lock is requested and mouse movement rotates the camera

### Requirement: Gamepad scheme
The game SHALL support standard gamepads that are connected or disconnected at any time without a restart.

#### Scenario: Gamepad hot-plug
- **WHEN** a gamepad is connected during gameplay
- **THEN** it controls the game immediately

#### Scenario: Active gamepad disconnected
- **WHEN** the gamepad currently in use is disconnected during gameplay
- **THEN** the game pauses and shows a reconnect message

### Requirement: Automatic touch scheme
The game SHALL start with the touch scheme on devices whose primary pointer is coarse, and SHALL switch at runtime to
the scheme of the most recently used input device.

#### Scenario: Mobile browser start
- **WHEN** the web build starts on a device whose primary pointer is coarse
- **THEN** the touch overlay is shown and pointer lock is never requested

#### Scenario: Desktop browser start
- **WHEN** the web build starts on a device whose primary pointer is fine
- **THEN** the touch overlay is hidden

#### Scenario: Hybrid device switches schemes
- **WHEN** the touch scheme is inactive and the player touches the screen
- **THEN** the touch overlay appears, and it hides again as soon as mouse or keyboard input follows

### Requirement: Virtual movement stick
The touch scheme SHALL show a floating movement stick where a finger touches down in the left half of the screen,
map finger displacement to movement direction and strength up to a maximum radius, and stop movement on release.

#### Scenario: Stick appears under the thumb
- **WHEN** a finger touches down in the left half of the screen
- **THEN** the stick is centred at that point and dragging moves the player in the drag direction

#### Scenario: Edge push sprints
- **WHEN** the finger is dragged beyond 90 % of the stick radius
- **THEN** the player sprints in that direction

#### Scenario: Release stops movement
- **WHEN** the finger controlling the stick is lifted
- **THEN** movement input returns to zero in the same simulation step and the stick disappears

### Requirement: Touch look
The touch scheme SHALL rotate the camera when a finger drags in the right half of the screen, proportionally to the
drag distance and the sensitivity setting, and SHALL allow moving and looking at the same time.

#### Scenario: Simultaneous move and look
- **WHEN** one finger drags the movement stick and another finger drags in the right half at the same time
- **THEN** the player moves and the camera rotates in the same simulation step

### Requirement: Touch buttons
The touch overlay SHALL provide buttons for fire, aim, jump, crouch, interact, reload, gadget, jack-in, map and menu,
each at least 48 CSS pixels in size and placed entirely inside the device's safe area.

#### Scenario: Safe area respected
- **WHEN** the device reports non-zero safe-area insets, for example because of a notch
- **THEN** no touch button overlaps the inset regions

#### Scenario: Gadget wheel by long press
- **WHEN** the gadget button is held for at least 400 ms
- **THEN** the gadget wheel opens and sliding to a segment selects that gadget

#### Scenario: Context icon on interact
- **WHEN** the player looks at an interactive object
- **THEN** the interact button shows the icon of the available interaction

### Requirement: Aim assistance
The game SHALL offer adjustable aim assistance (slow-down and gentle magnetism near targets) for touch and gamepad
aiming only, and SHALL never apply it to mouse aiming.

#### Scenario: Slow-down near a target
- **WHEN** aim assistance is enabled and the crosshair driven by touch or gamepad passes over a hostile target
- **THEN** the look speed is reduced while the crosshair is on the target

#### Scenario: No assistance for the mouse
- **WHEN** the player aims with a mouse
- **THEN** no aim assistance is applied regardless of the setting

### Requirement: Mobile fullscreen and orientation
On mobile browsers the game SHALL request fullscreen and landscape orientation on the first tap where the browser
supports it, and SHALL pause and show a rotate prompt while the device is in portrait orientation.

#### Scenario: Portrait orientation
- **WHEN** a phone is held in portrait orientation during gameplay
- **THEN** gameplay pauses and a prompt asks the player to rotate the device

### Requirement: Hold or toggle actions
The game SHALL let the player choose between hold and toggle behaviour for aim, crouch and sprint.

#### Scenario: Toggle crouch
- **WHEN** crouch is set to toggle and the player presses and releases crouch once
- **THEN** the player stays crouched until crouch is pressed again
