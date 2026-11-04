# Spec Delta: combat

## Purpose

Defines fighting and sneaking: weapon damage, melee and takedowns, non-lethal play, how enemies perceive and fight the
player, health and healing, what happens when the player goes down, and difficulty.

## ADDED Requirements

### Requirement: Hit zones and armour
Weapon hits SHALL apply zone multipliers for head, torso and limbs, and armour SHALL reduce damage according to the
weapon's penetration.

#### Scenario: Headshot multiplier
- **WHEN** an unarmoured target is hit in the head and in the torso by the same weapon
- **THEN** the head hit deals twice the torso damage

#### Scenario: Armour reduces damage
- **WHEN** an armoured and an unarmoured target are hit in the torso by the same non-piercing weapon
- **THEN** the armoured target takes less damage

### Requirement: Damage types
Damage SHALL be kinetic, thermal, EMP, shock or toxin, and targets SHALL have resistances per type; EMP SHALL be
strong against drones and cyberware and weak against unaugmented humans.

#### Scenario: EMP against a drone
- **WHEN** a drone and an unaugmented human are hit by the same EMP grenade
- **THEN** the drone takes at least three times the damage of the human

### Requirement: Melee and takedowns
The player SHALL be able to perform light and heavy melee attacks, block, and silently take down unaware enemies from
behind, choosing lethal or non-lethal.

#### Scenario: Silent non-lethal takedown
- **WHEN** the player performs a non-lethal takedown on an unaware enemy from behind
- **THEN** the enemy is knocked out without making noise above the takedown's noise level

### Requirement: Non-lethal play
Every lethal tool class SHALL have a non-lethal alternative, knockouts SHALL be tracked separately from kills, and
knocked-out targets SHALL wake up after a while unless restrained.

#### Scenario: Knockout is not a kill
- **WHEN** an enemy is knocked out with gel rounds
- **THEN** the knockout counter increases and the kill counter does not

#### Scenario: Unconscious target wakes up
- **WHEN** a knocked-out enemy is left unrestrained for 30 in-game minutes
- **THEN** the enemy wakes up and is in searching state

### Requirement: Enemy perception
Enemies SHALL detect the player by sight, affected by light, distance, movement, stance, weather and camouflage, and by
hearing, moving through the states unaware, suspicious, searching and combat, and back when the stimulus is gone.

#### Scenario: Darkness hides the player
- **WHEN** the player crouches motionless 15 m in front of an unaware guard in darkness, and in a second test in bright light
- **THEN** the guard does not detect the player within 5 seconds in darkness but does in bright light

#### Scenario: Noise draws investigation
- **WHEN** a noise is made within an unaware guard's hearing range
- **THEN** the guard becomes suspicious and moves to investigate the noise location

#### Scenario: Alert decays
- **WHEN** a searching guard finds nothing for the configured search time
- **THEN** the guard returns to unaware or a heightened patrol state

### Requirement: Enemy tactics
Enemies in combat SHALL use cover, flank, suppress, call reinforcements over comms unless jammed, and retreat or
surrender when their morale breaks.

#### Scenario: Reinforcements called
- **WHEN** a guard enters combat and comms are not jammed
- **THEN** reinforcements are requested and arrive according to the site's security level

#### Scenario: Jammed comms
- **WHEN** a comms jammer is active in range
- **THEN** enemies cannot call reinforcements

#### Scenario: Surrender at low morale
- **WHEN** an enemy's morale falls below the surrender threshold
- **THEN** the enemy stops fighting and surrenders or flees

### Requirement: Visibility feedback
The HUD SHALL show a visibility meter reflecting the player's current light level, movement, stance and camouflage.

#### Scenario: Visibility meter reacts to light
- **WHEN** the player moves from a neon-lit street into a dark alley
- **THEN** the visibility meter decreases

### Requirement: Health and healing
The player SHALL have health and armour, heal with medkits, stimulants and clinics, and regenerate naturally only up
to 30 % of maximum health.

#### Scenario: Natural regeneration cap
- **WHEN** the player rests at 10 % health without healing items
- **THEN** health regenerates up to 30 % of maximum and no further

### Requirement: Flatline and recovery
When the player's health reaches zero, the player SHALL wake up in a clinic with a bill if they have a MedEvac plan,
or in an alley with part of their cash stolen if not; the home stash SHALL never be lost.

#### Scenario: Flatline with MedEvac
- **WHEN** the player flatlines with an active MedEvac plan
- **THEN** the player wakes up in a clinic, the treatment bill is charged and carried items are kept

#### Scenario: Flatline without insurance
- **WHEN** the player flatlines without a MedEvac plan
- **THEN** the player wakes up in an alley nearby with part of the carried cash missing and the home stash untouched

### Requirement: Hardcore mode
In Hardcore mode the game SHALL keep one save per character and delete it when the character dies.

#### Scenario: Hardcore death deletes the save
- **WHEN** a Hardcore character's health reaches zero
- **THEN** the character's save is deleted and the game shows the death summary

### Requirement: Bodies and evidence
Bodies and unconscious enemies SHALL be carryable and hideable, and a discovered body SHALL raise the alert of the
discovering guard's site.

#### Scenario: Body discovered
- **WHEN** a patrolling guard sees an unhidden body
- **THEN** the guard enters searching state and the site alert level rises

### Requirement: Difficulty presets
Difficulty presets SHALL change enemy accuracy, damage and perception speed.

#### Scenario: Difficulty changes enemy accuracy
- **WHEN** the same combat replay is run on Story and on Street difficulty
- **THEN** enemy hit rates are lower on Story than on Street
