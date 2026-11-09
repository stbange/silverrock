# Spec Delta: city-generation

## ADDED Requirements

### Requirement: Floors fit the building
Each building that is not a mission site or the player's home SHALL have a deterministic ground floor of a kind that
fits its use: shop, diner, bar, clinic, capsule hotel, fixer, booth hall, residential lobby, office, warehouse or civic
lobby. Every room SHALL be reachable from the front door, and the door, the counter and the people inside SHALL not
overlap walls or solid furniture.

#### Scenario: Floors fit the building
- **WHEN** the floors of every building of a city are generated twice
- **THEN** each has its kind, the same layout both times, every room reachable from the front door, a counter with
  someone behind it where something is sold, nobody standing in a wall or furniture, and homes are the most common
  kind
