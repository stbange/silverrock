# Spec Delta: player-life

## ADDED Requirements

### Requirement: Every building has an inside
Every building in the city SHALL be enterable through its front door, except places with opening hours outside those
hours. Entering and leaving SHALL NOT pass game time.

#### Scenario: Every building has an inside
- **WHEN** the player stands at the door of a diner, a fixer, a shop, an office or a home and interacts
- **THEN** the player is inside that building's ground floor, and leaving puts them back on the street

### Requirement: Service at the counter
Inside, the counter SHALL offer what the place sells: food at diners, bars and groceries, goods at shops, treatment at
clinics, a capsule at capsule hotels and jobs at fixers. Desks with nothing to sell SHALL answer with a line, and
mailboxes, lifts and flat doors SHALL be lookable. In a Matrix booth a booth SHALL be a public deck.

#### Scenario: Service at the counter
- **WHEN** the player faces the counter of each kind of place and interacts
- **THEN** the shop list, the meal, the clinic, the capsule, the fixer's jobs or a spoken line follows, with the
  keeper standing behind the counter
