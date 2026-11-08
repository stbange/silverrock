# Spec Delta: population-simulation

## ADDED Requirements

### Requirement: Body variety
Citizens SHALL come in eight builds (average, slender, athletic, heavy, thin, muscular, curvy and stocky), two frames
and four height classes, with builds weighted by role, and notable citizens' frames SHALL match their given names.

#### Scenario: Bodies of every kind
- **WHEN** 2,000 citizens are generated across all roles
- **THEN** every build occurs, between 40 % and 60 % have a feminine frame, and the skeletons differ in shoulder,
  waist, hip and limb widths and in height

#### Scenario: Builds fit the role
- **WHEN** police officers and drifters are generated
- **THEN** at least 40 % of officers are athletic or muscular and at least 30 % of drifters are thin

#### Scenario: Bodies match names
- **WHEN** a notable citizen has a given name commonly read as female or male
- **THEN** their frame is feminine or masculine to match
