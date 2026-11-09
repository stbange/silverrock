# Spec Delta: retro-rendering

## ADDED Requirements

### Requirement: Chrome limbs
Characters with chrome limbs SHALL show them as metal arms or legs with a glowing seam, and the player's own chrome
arms SHALL show in first person.

#### Scenario: Chrome limbs show
- **WHEN** a character has a chrome right arm, or chrome arms and legs
- **THEN** that arm, or all four limbs, are drawn in metal with a glowing seam, and an unaugmented body has no metal
  parts

### Requirement: Feminine faces
Characters with a feminine frame SHALL use a feminine face set in every style and expression: thinner arched brows set
higher, lined eyes with lower lashes and a flick at the outer corner, a smaller nose, fuller tinted lips and a narrower
chin. The expressions SHALL stay as serious as the masculine set.

#### Scenario: Women have feminine faces
- **WHEN** a woman and a man of the same face style are drawn with the same expression
- **THEN** the woman's head uses the feminine cell of that style and expression, whose brows are arched and one
  texel thin, whose eyes are lined and whose lips are tinted, and the man's uses the masculine cell
