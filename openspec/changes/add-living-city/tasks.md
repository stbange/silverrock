# Tasks

## 1. Appearance and characters

- [x] 1.1 Implement the part-based appearance generator with role, class and faction markers; verify "Unique appearances in a crowd" and "Faction members show their marker"
- [ ] 1.2 Implement rigid-part character meshes, procedural walk cycles and instanced crowd rendering; verify golden images and "Peak crowd on desktop"

## 2. Notable and ambient citizens

- [x] 2.1 Implement notable citizen records, weekly schedules and memory slots; verify "Vendor keeps her hours" and "Notable remembers the player"
- [x] 2.2 Implement ambient crowd seeding by block and time slot with density tables; verify "Same corner, same crowd" and "Strip at peak and off-peak"
- [x] 2.3 Implement promotion from ambient to notable; verify "Trading promotes"

## 3. Level-of-detail simulation

- [x] 3.1 Implement near, mid and far tiers with record/entity transitions and analytic schedule placement; verify "Consistent after leaving view"
- [x] 3.2 Implement spawn and despawn rules outside the view and beyond 30 m; verify "No pop-in nearby"
- [x] 3.3 Implement routines, commuting and shift-change behaviour; verify "Shift-change rush"

## 4. Reactions

- [x] 4.1 Implement stimuli and response tables (flee, cover, gawk, film, report); verify "Gunfire panic"
- [x] 4.2 Implement crime-report events by district; verify "Nobody reports in the Undercity" and "Witness reports in the Spire"
- [x] 4.3 Implement weather reactions (umbrellas, shelter, going indoors); verify "Downpour empties the street"
- [x] 4.4 Implement rumour barks from the event bus; verify "Rumour about a recent event"

## 5. Services and traffic

- [x] 5.1 Implement staffed services with opening hours; verify "Closed shop"
- [ ] 5.2 Implement ground traffic, crossings and aerodyne lanes; verify "Vehicles stop at crossings"
- [x] 5.3 Implement metro timetables and trains; verify "Metro timetable"

## 6. Character creation and start

- [x] 6.1 Implement the character creation screen with backgrounds and appearance randomisation; verify "Background applies starting values" and "Randomised appearance"
- [x] 6.2 Implement the new-game start in the studio with the landlord message; verify "New game start" with a scenario script

## 7. Needs and sleep

- [x] 7.1 Implement hunger and fatigue rates, food items and penalties; verify "Eating restores satiety", "Starvation floor" and "Fatigue penalties"
- [x] 7.2 Implement sleeping at home, in capsules and on the street; verify "Sleeping eight hours", "Hostiles prevent sleep", "Sleeping rough" and "Capsule hotel night"

## 8. Money, rent and housing

- [x] 8.1 Implement cred, vendors, district price indices and the transaction log; verify "Buying food", "Insufficient funds" and "District price differences"
- [x] 8.2 Implement weekly rent, reminders, late fees, eviction and debt repayment; verify "Rent paid automatically", "Missed payment", "Rent reminders", "Two missed payments lead to eviction" and "Paying the debt restores access"
- [ ] 8.3 Implement housing tiers, moving and apartment amenities (bed, stash, terminal, wardrobe); verify "Moving to a better apartment" and "Stash keeps items"
- [x] 8.4 Implement metro and cab travel with fares and travel time; verify "Metro ride" and "Cab costs more than metro"

## 9. Persistence

- [x] 9.1 Implement save format version 1 (sections, CRC32, deltas) and loading; verify "Save and load round trip" and "Save size bound"
- [x] 9.2 Implement atomic writes and corruption detection, and add the save loader to the nightly fuzzing; verify "Interrupted save" and "Corrupted save detected"
- [x] 9.3 Implement autosave triggers and rotation, and manual slots with the combat block; verify "Autosave on sleep", "Autosave rotation" and "Manual save blocked in combat"
- [x] 9.4 Implement IndexedDB-backed web storage with sync; verify "Autosave when the page is hidden" and "Save survives reload" in Playwright
- [x] 9.5 Implement section versioning and the migration chain with a version-0 test fixture; verify "Older save migrated" and "Newer save refused"

## 10. Integration checks

- [x] 10.1 Add the "live a week" autoplayer strategy (eat, sleep, travel, pay rent with injected income) on 50 seeds; verify no soft-locks and no missed rent
- [ ] 10.2 Run the exit demo — survive a week while crowds commute and react — on all targets including a phone; record results in `tests/manual-verification.md`
- [x] 10.3 Confirm every scenario of this change is covered in the traceability report

## Implementation notes (2026-11-05)

- Open: 1.2 (walk cycles: characters are rigid-part figures that glide), 5.2 (aerodyne sky lanes; ground traffic stops at crossings), 8.3 (wardrobe amenity), 10.2 (real-phone run).

## Workflow follow-up

- Archive the change with `openspec archive add-living-city` once CI is green on all targets.
