# Spec Delta: law-and-heat

## Purpose

Makes risk real: crimes are detected through witnesses, cameras and evidence, raise Heat with the responsible
jurisdiction, trigger escalating responses, and can be reduced by lying low, disguises, bribes and changing appearance.

## ADDED Requirements

### Requirement: Heat per jurisdiction
The game SHALL track Heat from 0 to 5 separately for the city police and for each megacorp's security.

#### Scenario: Separate jurisdictions
- **WHEN** the player is seen committing a crime inside a facility of one megacorp
- **THEN** only that megacorp's Heat rises and the Heat of other megacorps is unchanged

### Requirement: Crime detection
A crime SHALL raise Heat only when it is detected — witnessed and reported, recorded by a working camera whose footage
reaches security, or discovered through evidence.

#### Scenario: Unwitnessed crime
- **WHEN** the player commits a crime with no witness, no working camera and no evidence left behind
- **THEN** no Heat is added

#### Scenario: Camera recording raises Heat
- **WHEN** a working camera records the player committing a crime and the footage is reviewed
- **THEN** Heat rises with the camera owner's jurisdiction

### Requirement: Escalating response
Responses SHALL escalate with Heat from questioning and patrols to searches, armed response, heavy units and kill
teams, with response times depending on the district.

#### Scenario: Response time by district
- **WHEN** the same Heat level is reached in the Corporate Spire and in a Residential district
- **THEN** the first responders arrive sooner in the Spire

### Requirement: Heat decay
Heat SHALL decay over time while no observer of the jurisdiction sees the player, and decay faster while the player
hides at home.

#### Scenario: Heat decays out of sight
- **WHEN** the player stays unseen by police for 24 in-game hours at police Heat 2
- **THEN** police Heat is lower than 2

### Requirement: Reducing Heat
The player SHALL be able to reduce Heat by bribing police contacts and by changing appearance through clothes, hair or
a ripperdoc face job, which breaks recognition.

#### Scenario: Changing appearance breaks recognition
- **WHEN** the player changes outfit and hairstyle while having police Heat
- **THEN** police observers no longer recognise the player on sight until new evidence links them

### Requirement: Arrest
Police SHALL offer arrest at Heat 1 and 2; surrendering SHALL lead to a fine or jail time that skips time. Corporate
security SHALL NOT arrest.

#### Scenario: Surrender to police
- **WHEN** the player surrenders to police at Heat 1
- **THEN** a fine is charged or time is skipped in jail, and police Heat is reset to 0

### Requirement: Disguises and access
Uniforms and forged IDs SHALL grant access to restricted areas until suspicion rises through out-of-role behaviour or
close inspection.

#### Scenario: Uniform grants access
- **WHEN** the player wears a matching maintenance uniform in a facility's maintenance area
- **THEN** guards do not treat the player as a trespasser

#### Scenario: Suspicious behaviour breaks the disguise
- **WHEN** a disguised player is seen drawing a weapon or picking a lock
- **THEN** the disguise is blown for that site and observers react as to an intruder

### Requirement: Restricted areas
Restricted areas SHALL be marked to the player, and being detected inside one without authorisation SHALL count as
trespassing.

#### Scenario: Trespass detected
- **WHEN** a guard sees the unauthorised player inside a restricted area
- **THEN** the guard becomes suspicious and challenges or attacks according to the site's rules
