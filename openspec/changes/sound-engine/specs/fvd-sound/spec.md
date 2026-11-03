# Spec Delta

## ADDED Requirements

### Requirement: Music

The game SHALL loop a lobby track while the online lobby is shown and a match
track during a match, crossfading over about two seconds when switching. A
missing track SHALL be silence.

#### Scenario: Match starts
- **WHEN** the host starts the match from the lobby
- **THEN** the lobby music fades out as the match music fades in

### Requirement: Game sound effects

The game SHALL play a sound when a visible horse, Builder or tower is hit or
dies, when a visible tower finishes construction, when the local player places
their own foundation, and when the local player orders their own units.
Sounds SHALL be derived only from state the player is already allowed to see.

#### Scenario: Hidden death
- **WHEN** a Builder dies inside the Hunter's fog
- **THEN** the Hunter hears nothing

#### Scenario: Another player's foundation
- **WHEN** an ally places a foundation the local player can see
- **THEN** the local player hears nothing until it is completed

#### Scenario: Revealed tower
- **WHEN** fog lifts from a tower that was finished earlier
- **THEN** no tower-built sound plays

### Requirement: Placement and limits

World sounds SHALL be full volume on screen, fade to silence just beyond the
screen edge, and pan towards their side of the screen. Each sound SHALL be
rate-limited so a burst of identical events plays only a few times.

#### Scenario: Mass tower loss
- **WHEN** a killed Builder's 40 visible towers are removed in one update
- **THEN** only a few tower-destroyed sounds play

### Requirement: Volume control

Players SHALL be able to set master, music and effects volumes (0-100) in
`audio.oac`, mute with M during a match, and start with `--no-audio` to use no
audio device at all. The game SHALL run normally without an audio device or
sound files.

#### Scenario: No sound files
- **WHEN** `assets/fvd/sound/` holds no sounds and no placeholders could be made
- **THEN** the game runs silently and logs that once

### Requirement: Placeholder sounds (temporary)

Until real recordings exist, every game start (solo, host or guest) without
`--no-audio` SHALL generate synthesised placeholder sounds for every cue and
music state into a per-user cache folder, writing only missing files and never
into `assets/fvd/sound/`. A cue or music state SHALL use the placeholders only
when `assets/fvd/sound/` has no file for it. Failing to write placeholders
SHALL be logged and SHALL NOT stop the game. This requirement is removed once
every cue has a real sound.

#### Scenario: Partly recorded
- **WHEN** `assets/fvd/sound/` holds only `hit.opus`
- **THEN** hits play `hit.opus` and every other cue plays its placeholder

#### Scenario: Second start
- **WHEN** the placeholders already exist in the cache folder
- **THEN** nothing is regenerated

### Requirement: Options menu

An Options button SHALL sit at the top right of the match HUD. It SHALL open a
panel with separate on/off switches for music and for sound effects, which
SHALL be remembered between games. Clicking outside the panel SHALL close it.

#### Scenario: Music off
- **WHEN** a player turns Music off in Options and restarts the game
- **THEN** no music plays, sound effects still play, and the switch still reads OFF
