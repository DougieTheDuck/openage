# Proposal

## Why

The moment the Hunter's horses start spawning changes the whole match, but
nothing marks it apart from the countdown quietly disappearing.

## What Changes

- When the Hunter countdown reaches zero, everyone sees a large
  **HUNTER UNLEASHED** banner in the upper middle of the screen for about
  three seconds, then it fades.
- At the same moment everyone hears the `hunter_unleashed` sound (see
  `sound-engine`); silent if the file is missing or sound effects are off.
- Skipping the countdown with F9 also triggers it. Joining or resetting after
  the horses already started does not.
- No protocol change: every client already receives the first spawn time.

## Capabilities

### Modified Capabilities

- `fvd-sound`: one more cue.
- `fvd-hud`: the banner.

## Status

Implemented (Dougie, 2026-11-03). Depends on `sound-engine`.
