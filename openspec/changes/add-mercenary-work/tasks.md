# Tasks

## 1. Items and equipment

- [ ] 1.1 Implement item tuples, weapon archetype tables and stat computation from manufacturer, rarity and seed; verify "Archetype coverage", "Same item seed, same weapon", "Manufacturer style is consistent" and "Rarity stat ranges"
- [ ] 1.2 Implement weapon mods and ammunition types; verify "Suppressor reduces noise" and "Gel rounds are non-lethal"
- [ ] 1.3 Implement inventory, carry weight, quick slots and the gadget wheel; verify "Overburdened" and "Quick slot use"
- [ ] 1.4 Implement gadgets (throwables, deployables, tools, consumables); verify "Grapple line to a rooftop" and "Holo-decoy distracts"
- [ ] 1.5 Implement salvage, blueprints and workbench crafting; verify "Crafting a gadget"
- [ ] 1.6 Implement vendors, black-market rules and fences; verify "Stolen goods refused by a shop"
- [ ] 1.7 Implement cyberware slots, ripperdocs, neural capacity, glitches, malfunctions and jailbreaks; verify "Cyberware ability works", "Over capacity causes glitches" and "Jailbreak removes the backdoor"

## 2. Combat

- [ ] 2.1 Implement hitscan and projectile weapons, hit zones, armour and damage types; verify "Headshot multiplier", "Armour reduces damage" and "EMP against a drone"
- [ ] 2.2 Implement melee, blocking and takedowns; verify "Silent non-lethal takedown"
- [ ] 2.3 Implement knockouts, restraints and waking up; verify "Knockout is not a kill" and "Unconscious target wakes up"
- [ ] 2.4 Implement health, armour, healing and the regeneration cap; verify "Natural regeneration cap"
- [ ] 2.5 Implement flatlining with and without MedEvac, and Hardcore death; verify "Flatline with MedEvac", "Flatline without insurance" and "Hardcore death deletes the save"
- [ ] 2.6 Implement carrying and hiding bodies; verify "Body discovered"
- [ ] 2.7 Implement difficulty presets; verify "Difficulty changes enemy accuracy" with a combat replay

## 3. Enemy AI and stealth

- [ ] 3.1 Implement light-grid perception, hearing and awareness meters; verify "Darkness hides the player" and "Noise draws investigation"
- [ ] 3.2 Implement the alert state machine and search behaviour; verify "Alert decays"
- [ ] 3.3 Implement combat tactics with utility scoring, comms and morale; verify "Reinforcements called", "Jammed comms" and "Surrender at low morale"
- [ ] 3.4 Implement the HUD visibility meter; verify "Visibility meter reacts to light"

## 4. Progression

- [ ] 4.1 Implement attributes, skills with use-based experience and levels; verify "Attribute range", "Using a skill raises it", "Trivial use grants nothing" and "Level-up rewards"
- [ ] 4.2 Implement perk trees and respec; verify "Perk locked until rank" and "Respec refunds points"
- [ ] 4.3 Implement Street Cred tiers and faction standing with attribution; verify "Tier thresholds", "Unattributed action" and "Hostile faction attacks"

## 5. Law and Heat

- [ ] 5.1 Implement evidence records, Heat per jurisdiction and detection; verify "Separate jurisdictions", "Unwitnessed crime" and "Camera recording raises Heat"
- [ ] 5.2 Implement escalating responses with district response times; verify "Response time by district"
- [ ] 5.3 Implement Heat decay, bribes, appearance-based recognition and arrests; verify "Heat decays out of sight", "Changing appearance breaks recognition" and "Surrender to police"
- [ ] 5.4 Implement disguises, restricted areas and trespassing; verify "Uniform grants access", "Suspicious behaviour breaks the disguise" and "Trespass detected"

## 6. Jobs

- [ ] 6.1 Implement job templates, world binding and tiers; verify "Jobs reference real entities", "Tier gating" and "Pay within tier range"
- [ ] 6.2 Implement job sources (job board, fixers, contacts, event opportunities); verify "Fixer offers jobs" and "Job board listings"
- [ ] 6.3 Implement briefings, intel and negotiation; verify "Buying intel reveals details", "Successful negotiation" and "Failed negotiation"
- [ ] 6.4 Implement approach planners and the two-approach check; verify "Two approaches validated" with ghost, face and hammer autoplayer strategies
- [ ] 6.5 Implement objectives, constraints, payment, bonus objectives and failure; verify "Payment on completion", "Bonus objective", "No-kill constraint violated" and "Deadline missed"
- [ ] 6.6 Implement job outcome events and the autosave on job completion; verify "Job outcome becomes a world event"

## 7. Persistence

- [ ] 7.1 Extend the save format with inventory, progression, jobs, evidence and Heat sections plus migration from version 1; verify round-trip and migration tests

## 8. Integration checks

- [ ] 8.1 Run the gig-worker autoplayer for four weeks on 50 seeds; verify "Starter economy is sustainable"
- [ ] 8.2 Add the nightly balance report (200 seeds, all strategies) with threshold bands; verify the report is produced and gated
- [ ] 8.3 Play the exit demo on all targets including touch on a phone; record results in `tests/manual-verification.md`
- [ ] 8.4 Confirm every scenario of this change is covered in the traceability report

## Workflow follow-up

- Archive the change with `openspec archive add-mercenary-work` once CI is green on all targets.
