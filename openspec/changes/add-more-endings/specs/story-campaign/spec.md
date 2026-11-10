# Spec Delta: story-campaign

## ADDED Requirements

### Requirement: More endings
Besides Loyal, Defector, Burn the Board, Kingmaker and Sunrise, the climax SHALL offer The Chair, Exodus, City Hall,
Street Kings and The Shareholder when their conditions hold, each with its own effect on the city, three closing lines
and the next season's title. The terminal SHALL list all endings and mark the open ones.

#### Scenario: Ten endings
- **WHEN** every ending's name, hint and closing lines are read
- **THEN** each is filled in, and an unknown ending is refused

#### Scenario: The Chair
- **WHEN** a player loyal to the first patron has trust 90 and a handler bond of 35 and chooses The Chair
- **THEN** a stipend of 1500 cred a week is paid, and the next season, "The Chair", starts in Act II at trust 100

#### Scenario: Exodus
- **WHEN** a player with two people at bond 60 and 20000 cred chooses Exodus
- **THEN** half the cred is spent, Heat, the hunt, the bounty and the blacklist are cleared, and the next season is
  "The Return"

#### Scenario: City Hall
- **WHEN** a councillor is exposed or the scenario is the Coup, the reporter's bond is 15, and the player chooses City Hall
- **THEN** bribed councillors are exposed, the police go back to the city, curfews end, and the next season is "Reform"

#### Scenario: Street Kings
- **WHEN** a gang holds three districts, the fixer's bond is 35, and the player chooses Street Kings
- **THEN** that gang is crowned with more force, the police contract ends, and the next season is "The Street"

#### Scenario: The Shareholder
- **WHEN** the player's shares are worth 100000 cred and the player chooses The Shareholder
- **THEN** the corporation they hold most of is crowned, its price rises by a quarter, and the next season is
  "Dividends"
