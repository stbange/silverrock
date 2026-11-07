# Spec Delta: player-life

## MODIFIED Requirements

### Requirement: Weekly rent
Rent SHALL be charged automatically every Sunday at 23:59 together with the weekly living costs when the player has
enough cred, with one reminder the day before; otherwise the payment is missed and a late fee is added to the rent
debt. Rent SHALL rise by 10 % for every Street Cred tier above Runner.

#### Scenario: Rent paid automatically
- **WHEN** rent falls due and the player has enough cred
- **THEN** the rent and living costs are deducted and the next due date is one week later

#### Scenario: Missed payment
- **WHEN** rent falls due and the player does not have enough cred
- **THEN** the payment is recorded as missed and the rent plus a late fee is added to the debt

#### Scenario: Rent reminders
- **WHEN** the clock reaches one day before the due date
- **THEN** the landlord sends one reminder, and no other rent reminder that week

#### Scenario: Notoriety raises the rent
- **WHEN** the player reaches tier Operator
- **THEN** the next rent is 10 % higher than the housing tier's base rent

## ADDED Requirements

### Requirement: Living costs
Each week the player SHALL pay upkeep of 2 % of the value of installed cyberware and the equipped weapon, fixers SHALL
keep 10 % of the pay of fixer jobs, sleep SHALL heal part of the damage (60 % at home, 40 % in a capsule, 15 % on the
street) at 5 cred per healed point, a clinic's treatment SHALL cost 15 cred per point, and a bill SHALL take at most
half of the player's cash, the rest being owed and collected with the weekly costs.

#### Scenario: Upkeep with rent
- **WHEN** rent falls due for a player with 10,000 cred of installed cyberware
- **THEN** 200 cred of upkeep is charged together with the rent

#### Scenario: Medical bill
- **WHEN** a player missing 40 health with plenty of cash sleeps eight hours at home
- **THEN** 24 health is healed and a medical bill of 120 cred is charged

### Requirement: A readable inbox
Identical messages from the same sender within 24 in-game hours SHALL be merged into one with a count, story messages
SHALL be pinned, and the inbox SHALL hold 64 messages.

#### Scenario: Repeated message merged
- **WHEN** the handler sends the same line twice in one day
- **THEN** the inbox shows it once, marked ×2
