# Spec Delta: corporate-market

## Purpose

Gives each megacorp a share price driven by the faction simulation and lets the player trade shares — with the
temptation and risk of trading on knowledge from their own jobs.

## ADDED Requirements

### Requirement: Share prices
Each megacorp SHALL have a share price that is updated every strategic tick from its resources and recent events.

#### Scenario: Price reacts to an outcome
- **WHEN** a sabotage operation against a megacorp succeeds
- **THEN** that megacorp's share price is lower after the next strategic tick than before

### Requirement: Trading
The player SHALL be able to buy and sell shares on the terminal for a fee, and the portfolio's value SHALL follow the
share prices.

#### Scenario: Buying shares
- **WHEN** the player buys 10 shares
- **THEN** cred decreases by 10 times the price plus the fee and the portfolio holds 10 more shares

### Requirement: Insider-trading suspicion
Large trades placed shortly before an operation the player takes part in SHALL raise suspicion with Finance & Insurance
megacorps, which can lead to standing loss or retaliation.

#### Scenario: Suspicious trade flagged
- **WHEN** the player short-sells a megacorp's shares within 24 in-game hours before sabotaging it, above the size threshold
- **THEN** a suspicion record is created with the Finance & Insurance megacorps
