# Spec Delta: matrix-world-link

## Purpose

Defines how actions in the Matrix change the physical world — doors, cameras, turrets, alarms, lights, power, elevators,
records and information — and how the player triggers owned devices from meatspace through Netsight.

## ADDED Requirements

### Requirement: Device mapping
Every controllable device in a networked building — doors, cameras, turrets, drones, alarms, lights, elevators and
comms — SHALL have a device-controller node in that building's host, and changing the node SHALL change the device in
the physical world immediately and until it is reset.

#### Scenario: Every device has a node
- **WHEN** a networked building's interior and host are generated
- **THEN** every controllable device in the interior is linked to exactly one device-controller node

#### Scenario: Unlocking a door from the Matrix
- **WHEN** the player unlocks a door's node in the Matrix
- **THEN** the physical door is unlocked and stays unlocked after the player jacks out, until it is reset

### Requirement: Camera control
The player SHALL be able to loop a camera so it reports nothing, disable it so guards come to check it after a delay,
or view its feed from the Matrix.

#### Scenario: Looped camera sees nothing
- **WHEN** a camera is looped and the player walks through its view
- **THEN** no detection and no evidence is produced by that camera

#### Scenario: Disabled camera draws a check
- **WHEN** a camera is disabled
- **THEN** a guard is sent to inspect it after the site's configured delay

#### Scenario: Viewing a feed
- **WHEN** the player opens a camera's feed from its node
- **THEN** the view from that camera's position in the physical world is shown

### Requirement: Turrets and drones
The player SHALL be able to disable turrets and drones or switch their friend-or-foe recognition.

#### Scenario: Retargeted turret
- **WHEN** a turret's friend-or-foe setting is switched and a guard enters its field of fire
- **THEN** the turret fires at the guard and not at the player

### Requirement: Alarms and comms
The player SHALL be able to cancel alarms, trigger false alarms at a chosen location, and send fake orders that move
guards.

#### Scenario: False alarm diversion
- **WHEN** the player triggers a false alarm in another wing of the building
- **THEN** guards of that site move to investigate the false alarm location

#### Scenario: Cancelling an alarm
- **WHEN** the player cancels an active alarm at the security monitor
- **THEN** the alarm stops and pending reinforcement requests are cancelled

### Requirement: Lights and power
The player SHALL be able to switch off a building's lights, and at the highest tier black out a whole district for a
limited time; darkness SHALL affect perception and citizens SHALL react to blackouts.

#### Scenario: Lights out helps stealth
- **WHEN** the lights of a building are switched off
- **THEN** light levels inside drop and guard detection of the player takes longer than with the lights on

#### Scenario: District blackout
- **WHEN** the player triggers a district blackout from the power grid host
- **THEN** street and building lights in that district switch off except emergency lights, citizens react, and power returns after the configured duration

### Requirement: Elevators
The player SHALL be able to call, stop and send elevators from their nodes.

#### Scenario: Calling an elevator
- **WHEN** the player sends an elevator to a floor from its node
- **THEN** the physical elevator travels to that floor

### Requirement: Records
In government, police and finance hosts the player SHALL be able to erase warrants and evidence, forge credentials and
alter accounts, with effects on Heat, access and money.

#### Scenario: Erasing records reduces Heat
- **WHEN** the player erases their warrant in the police records node
- **THEN** police Heat is reduced and the related evidence records are removed

#### Scenario: Forged credentials grant access
- **WHEN** the player forges credentials for a facility and later presents them at its entrance
- **THEN** guards and locks treat the player as authorised until suspicion rises

### Requirement: Stolen information
Stolen data SHALL reveal intel such as briefing details and faction secrets, and blackmail material SHALL unlock
dialogue options with its subject.

#### Scenario: Stolen data reveals briefing details
- **WHEN** the player steals a site's security data before doing a job there
- **THEN** the job briefing shows the guard count and camera positions as known

### Requirement: Netsight and quick actions
In meatspace, Netsight SHALL show networked devices and their links, and the player SHALL be able to trigger devices
they own through the Matrix within deck range without diving; devices they do not own SHALL NOT offer quick actions.

#### Scenario: Netsight shows devices
- **WHEN** the player activates Netsight in a networked building
- **THEN** controllable devices in view are highlighted with their type

#### Scenario: Quick action on an owned device
- **WHEN** the player owns a camera through a backdoor and triggers its loop quick action within range
- **THEN** the camera is looped without a dive

#### Scenario: No quick action without ownership
- **WHEN** the player targets a device they do not own
- **THEN** no quick action is offered

### Requirement: Countermeasures and attribution
Host security SHALL detect tampering after a time based on the security rating, reset changed devices, raise the alert
and remove backdoors; erasing logs before leaving SHALL prevent the intrusion from being attributed to the player.

#### Scenario: Tampering detected and reset
- **WHEN** the detection time after a device change elapses
- **THEN** the device returns to its original state and the site alert rises

#### Scenario: Erased logs prevent attribution
- **WHEN** the player erases the host's logs before jacking out
- **THEN** the intrusion produces no evidence linked to the player
