# Spec Delta: equipment

## Purpose

Provides the player's tools: weapons with procedural variants, mods and ammunition, gadgets, quick slots, inventory,
crafting, trading and cyberware.

## ADDED Requirements

### Requirement: Weapon archetypes
The game SHALL provide at least 20 weapon archetypes across pistols, long guns, heavy and exotic weapons, non-lethal
weapons and melee weapons.

#### Scenario: Archetype coverage
- **WHEN** the weapon data is loaded
- **THEN** there are at least 20 archetypes and every category contains at least two

### Requirement: Procedural weapon variants
Each weapon SHALL be defined by archetype, manufacturer, rarity, item seed and mods, with deterministic stats.
Manufacturers SHALL be the city's megacorps with arms or cyberware sectors plus street makers, each with its own stat
tendency and visual style.

#### Scenario: Same item seed, same weapon
- **WHEN** a weapon is created twice from the same archetype, manufacturer, rarity and item seed
- **THEN** both have identical stats and appearance

#### Scenario: Manufacturer style is consistent
- **WHEN** 100 weapons of one manufacturer are generated
- **THEN** all of them share the manufacturer's visual style and their stats deviate from the archetype base in the manufacturer's tendency direction

### Requirement: Rarity
Weapons SHALL come in the rarities street, standard, military, prototype and signature, with stat ranges that rise
with rarity.

#### Scenario: Rarity stat ranges
- **WHEN** weapons of the same archetype are generated in each rarity
- **THEN** each stat lies within the range configured for its rarity

### Requirement: Weapon mods
Weapons SHALL have mod slots for muzzle, optic, magazine, underbarrel and chip, and installing or removing mods at a
workbench SHALL change the weapon's stats and return removed mods to the inventory.

#### Scenario: Suppressor reduces noise
- **WHEN** a suppressor is installed on a pistol
- **THEN** the pistol's noise radius decreases by the suppressor's value

### Requirement: Ammunition types
Firearms SHALL accept the ammunition types standard, armour-piercing, hollow-point, EMP, incendiary, gel and tracker,
and the type in use SHALL determine the damage type and effects.

#### Scenario: Gel rounds are non-lethal
- **WHEN** an enemy's health is reduced to zero by gel rounds
- **THEN** the enemy is knocked out instead of killed

### Requirement: Gadgets
The game SHALL provide at least 16 gadgets across throwables, deployables, tools and consumables, including frag, EMP,
flash, smoke and sleep-gas grenades, a holo-decoy, a grapple line, a recon drone and a comms jammer.

#### Scenario: Grapple line to a rooftop
- **WHEN** the player uses the grapple line while aiming at a grapple point in range
- **THEN** the player is pulled to that grapple point

#### Scenario: Holo-decoy distracts
- **WHEN** a holo-decoy is deployed in an enemy's view
- **THEN** the enemy reacts to the decoy as if it were the player until it is revealed

### Requirement: Quick slots and gadget wheel
The player SHALL have four quick slots and a gadget wheel for equipping and using items without opening the inventory.

#### Scenario: Quick slot use
- **WHEN** the player presses quick slot 2 holding a medkit
- **THEN** the medkit is used and the slot count decreases by one

### Requirement: Inventory and carry weight
The inventory SHALL be limited by carry weight based on BODY, and carrying more than the limit SHALL slow the player
down.

#### Scenario: Overburdened
- **WHEN** the carried weight exceeds the limit
- **THEN** the player's movement speed is reduced and sprinting is disabled

### Requirement: Crafting
The player SHALL be able to salvage items into components and craft items from components and known blueprints at a
workbench.

#### Scenario: Crafting a gadget
- **WHEN** the player crafts a smoke grenade at a workbench with the blueprint and enough components
- **THEN** the components are consumed and a smoke grenade is added to the inventory

### Requirement: Vendors and fences
Vendors SHALL buy and sell items; black-market goods SHALL require a minimum Streetwise rank or standing, and stolen
corporate goods SHALL only be accepted by fences.

#### Scenario: Stolen goods refused by a shop
- **WHEN** the player tries to sell a stolen corporate item at a regular shop
- **THEN** the shop refuses, and a fence accepts the item

### Requirement: Cyberware
Cyberware SHALL be installed by ripperdocs into body slots, limited by neural capacity based on BODY and TECH;
exceeding the capacity SHALL cause glitches, and low-grade cyberware SHALL have a malfunction chance.

#### Scenario: Cyberware ability works
- **WHEN** thermal optics are installed and activated
- **THEN** warm bodies are highlighted through smoke and darkness

#### Scenario: Over capacity causes glitches
- **WHEN** installed cyberware exceeds the player's neural capacity
- **THEN** glitch effects occur at the configured rate until capacity is respected

#### Scenario: Jailbreak removes the backdoor
- **WHEN** a ripperdoc jailbreaks corporate cyberware
- **THEN** the item no longer carries its manufacturer backdoor
