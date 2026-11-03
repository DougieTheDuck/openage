# Proposal

## Why

FVD is silent. Players get no feedback when an order lands, a horse hits a
tower or a Builder dies off to one side of the screen, and there is no music.

## What Changes

- Music: a looping lobby track and a looping match track, crossfading when the
  match starts or the player returns to the lobby.
- Effects: hit, death (horse, Builder, tower), foundation placed, tower built,
  and an order acknowledgement when you command your own units.
- World sounds play at full volume on screen, fade out just past the screen
  edge, and pan with their position. Each sound is rate-limited, so a
  Builder's 40 towers collapsing at once does not become 40 sounds.
- Foundation placed and order are heard only by the player who acted.
- Master, music and effects volumes in `cfg/audio.oac`
  (`AUDIO_MASTER_VOLUME`, `AUDIO_MUSIC_VOLUME`, `AUDIO_EFFECTS_VOLUME`).
- An Options button at the top right turns music and sound effects on or off
  (remembered between games).
- M mutes and unmutes during a match. `--no-audio` never opens a device.
- The game plays whatever it finds in `assets/fvd/sound/` and is silent for
  anything missing. No sound files ship with this change.
- No protocol change: sounds come from data every client already receives.

## Capabilities

### New Capabilities

- `fvd-sound`: music, game sound effects and volume control.

## Status

Implemented (Dougie, 2026-11-03), without sound files. Open questions below.

## Open questions

- Sound source and licence: converted AoE2 sounds (and music, which the
  converter doesn't handle today) or files we make or license and ship.
- Should allies hear an "under attack" alert for each other's towers?
- Win/lose stingers belong with `match-end-conditions`.
- M only works during a match: in the lobby it would clash with typing.
