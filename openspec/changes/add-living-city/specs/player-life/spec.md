# Spec Delta: player-life

## Purpose

Covers the player's everyday life as a city dweller: creating a character, the humble starting situation, hunger and
fatigue, sleeping, money, rent and eviction, housing, and getting around the city.

## ADDED Requirements

### Requirement: Character creation
The game SHALL let the player choose a name, pronouns, an appearance from the citizen generator and one of five
backgrounds, which sets starting attributes, trained skills, a starting contact, kit and cash.

#### Scenario: Background applies starting values
- **WHEN** the player creates a character with the Dropout Netrunner background
- **THEN** the character starts with Netrunning and Electronics trained, the background's contact, kit and cash

#### Scenario: Randomised appearance
- **WHEN** the player presses the randomise button
- **THEN** a new valid appearance is generated and previewed

### Requirement: Starting situation
A new game SHALL start on a Monday evening in the player's rented studio, with the background's cash and kit, rent of
the studio due on Sunday at 23:59, and a message from the landlord on the terminal.

#### Scenario: New game start
- **WHEN** a new game begins
- **THEN** the player is inside the studio, the clock shows Monday evening, rent is due on Sunday 23:59 and an unread landlord message exists

### Requirement: Hunger
Hunger SHALL increase over time so that a full stomach lasts about 36 in-game hours; low satiety SHALL reduce stamina
regeneration, and starving SHALL drain health slowly, but never below 25 % of maximum health.

#### Scenario: Eating restores satiety
- **WHEN** the player eats a meal
- **THEN** satiety increases by the meal's value

#### Scenario: Starvation floor
- **WHEN** the player has been starving for 48 in-game hours
- **THEN** health has decreased but is not below 25 % of maximum

### Requirement: Fatigue
Fatigue SHALL increase over time and with stimulants, and high fatigue SHALL increase aim sway and slow down Matrix
actions until the player sleeps.

#### Scenario: Fatigue penalties
- **WHEN** fatigue exceeds the high threshold
- **THEN** aim sway and Matrix action durations increase by the configured penalties

### Requirement: Sleep
The player SHALL be able to sleep in their home, a capsule hotel or on the street for a chosen duration, which
advances time, reduces fatigue and triggers an autosave, but not while hostiles are nearby.

#### Scenario: Sleeping eight hours
- **WHEN** the player sleeps for 8 hours in their own bed
- **THEN** the clock advances by 8 hours, fatigue is reset and an autosave is written

#### Scenario: Hostiles prevent sleep
- **WHEN** the player tries to sleep while an alerted hostile is within 50 m
- **THEN** sleeping is refused with a message

#### Scenario: Sleeping rough
- **WHEN** the player sleeps on the street
- **THEN** there is a chance, determined by the seed and district, of waking up with part of the cash stolen

### Requirement: Money and trading
The game SHALL use a single integer currency, cred, for buying and selling, with prices that depend on the district,
and SHALL keep a transaction log.

#### Scenario: Buying food
- **WHEN** the player buys a meal with enough cred
- **THEN** cred decreases by exactly the displayed price and the purchase appears in the transaction log

#### Scenario: Insufficient funds
- **WHEN** the player tries to buy an item that costs more than their cred
- **THEN** the purchase is refused and nothing changes

#### Scenario: District price differences
- **WHEN** the same item is offered in the Corporate Spire and in an Undercity district
- **THEN** its price in the Spire is higher

### Requirement: Weekly rent
Rent SHALL be charged automatically every Sunday at 23:59 when the player has enough cred, with reminders two days
and one day before; otherwise the payment is missed and a late fee is added to the rent debt.

#### Scenario: Rent paid automatically
- **WHEN** rent falls due and the player has enough cred
- **THEN** the rent is deducted and the next due date is one week later

#### Scenario: Missed payment
- **WHEN** rent falls due and the player does not have enough cred
- **THEN** the payment is recorded as missed and the rent plus a late fee is added to the debt

#### Scenario: Rent reminders
- **WHEN** the clock reaches two days and one day before the due date
- **THEN** the landlord sends a reminder message each time

### Requirement: Eviction
After two consecutive missed rent payments the player SHALL be evicted: the home is locked, the stash inside is kept
untouched, and paying the full debt restores access.

#### Scenario: Two missed payments lead to eviction
- **WHEN** two consecutive rent payments are missed
- **THEN** the home is locked for the player and all items in its stash are kept

#### Scenario: Paying the debt restores access
- **WHEN** an evicted player pays the full debt to the landlord
- **THEN** the home is unlocked and the stash contents are unchanged

### Requirement: Capsule hotels
Capsule hotels SHALL rent a bed and a locker per night.

#### Scenario: Capsule hotel night
- **WHEN** the player pays for a capsule and sleeps there
- **THEN** the nightly price is deducted and sleeping works as in the player's home

### Requirement: Housing tiers
The player SHALL be able to rent better housing tiers with more amenities and higher rent; moving SHALL transfer the
stash and end the previous lease.

#### Scenario: Moving to a better apartment
- **WHEN** the player rents a mid-block apartment while living in the studio
- **THEN** the new rent applies from the next due date, the studio lease ends and all stash items are in the new home

### Requirement: Apartment amenities
The player's home SHALL provide a bed, a stash, a terminal with messages, news and job listings, and a wardrobe,
with further amenities such as a workbench in higher tiers.

#### Scenario: Stash keeps items
- **WHEN** the player stores items in the stash, plays several days and loads a save
- **THEN** the items are still in the stash

### Requirement: Getting around
The player SHALL be able to walk, ride the metro between stations for a fare and travel time, or take a cab door to
door for a higher, distance-based price.

#### Scenario: Metro ride
- **WHEN** the player rides the metro from one station to another
- **THEN** the fare is deducted, the clock advances by the travel time and the player arrives at the destination station

#### Scenario: Cab costs more than metro
- **WHEN** the same trip between two stations is priced by cab and by metro
- **THEN** the cab fare is higher
