# Spec Delta: navigation

## ADDED Requirements

### Requirement: Routes
The game SHALL plan a route along the pedestrian graph from the player to a chosen building, metro entrance or point,
replan it once a second, and report arrival within 8 m. Inside a site the route SHALL be a straight line to the target.
The route SHALL NOT be saved.

#### Scenario: Route to a chosen place
- **WHEN** a destination is set
- **THEN** the route runs from the player to it along walking lines, its length is the sum of its legs, it keeps only
  corners, and clearing it removes it

#### Scenario: Arrival
- **WHEN** the player comes within 8 m of the destination
- **THEN** an "ARRIVED" message shows and the destination is cleared

### Requirement: Turn hints
The game SHALL describe the next turn of the route with its side and distance rounded to 10 m, "CONTINUE" when there is
none, and "DESTINATION AHEAD" for the last 25 m, ignoring slight bends and the step from a door onto the walking line.

#### Scenario: Turn hints
- **WHEN** the route turns right 50 m ahead, left 9 m ahead, bends slightly, or runs straight
- **THEN** the hint reads "TURN RIGHT IN 50 M", "TURN LEFT NOW", or "CONTINUE ..."

### Requirement: Following the job
When no destination is set, the route SHALL follow the active job to its target, and to the client once the work is
done. A job route cleared by hand SHALL NOT be followed again, and a destination set by hand SHALL take precedence.

#### Scenario: Route follows the job
- **WHEN** the player takes a job, sets a point, clears routes and finishes the work
- **THEN** the route goes to the target, yields to the point, stays cleared once the player declines it, and goes to
  the client after the work

### Requirement: City map
The game SHALL offer a city map that pauses time and shows blocks, districts, buildings, the metro, places by filter,
home, the job's target, the route and the player. The map SHALL pan, zoom between a street corner and the whole city,
cycle filters, set the place under the cursor or pointer as the destination, and clear the route. It SHALL be
reachable by keyboard, mouse, gamepad and touch.

#### Scenario: City map
- **WHEN** the player opens the map, pans, zooms, changes the filter and picks a place, a station or empty ground
- **THEN** time stands still, the view moves within its limits, and the destination becomes that place, the station or
  a marked point

### Requirement: Navigation HUD
With a destination, the HUD SHALL show its direction on the compass with the distance and the next instruction. With
the minimap setting on, it SHALL show a heading-up minimap of streets, places, the route and the destination.

#### Scenario: Minimap and compass
- **WHEN** the HUD is built with a route on desktop and touch, and with the minimap and guidance off
- **THEN** the compass marker, minimap and goal are drawn when on and left out when off

### Requirement: Route in the world
With the route setting on, the street scene SHALL show glowing chevrons on the ground along the next 60 m of the route
with a pulse running toward the destination, and a shaft of light over the destination that is not hidden by fog.

#### Scenario: Route on the pavement
- **WHEN** a route is set and the scene is built with the setting on and off
- **THEN** chevrons lie flat along the route within 60 m with a moving pulse, a beacon stands over the destination,
  and nothing is drawn with the setting off
