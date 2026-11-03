# Spec Delta

## ADDED Requirements

### Requirement: Builder team chat

A Builder SHALL be able to open a chat line with Enter, type up to 140
characters and send it with Enter. Every Builder SHALL see the message on the
left of the screen at about 75% of the window height, in bold and in the
sender's player colour. The Hunter SHALL NOT see team chat or receive it
from the host. While the chat line is open, typed keys SHALL NOT issue game
commands.

#### Scenario: Message sent
- **WHEN** player 4 presses Enter, types "help north" and presses Enter
- **THEN** players 2-8 see "Player 4: help north" in player 4's colour, in bold

#### Scenario: Length limit
- **WHEN** a Builder types a 141st character
- **THEN** the input does not accept it, and the host rejects any longer message

#### Scenario: Typing does not command units
- **WHEN** a Builder with a unit selected types "b" in the chat line
- **THEN** no tower placement starts

#### Scenario: Hunter excluded
- **WHEN** Builders chat
- **THEN** the Hunter's client receives no chat data and shows nothing

#### Scenario: Cancel
- **WHEN** a Builder presses Esc in the chat line
- **THEN** it closes and nothing is sent
