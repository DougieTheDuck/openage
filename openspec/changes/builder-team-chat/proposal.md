# Proposal

## Why

Builders win by coordinating where to wall and when to retreat. There is no
way to talk in game.

## What Changes

- Builders press **Enter** to open a chat line, type up to **140 characters**,
  and press Enter to send (Esc cancels). While the line is open, keys type
  text instead of issuing orders.
- Messages go to **all Builders only** (team chat). The Hunter never sees
  them and cannot chat.
- Messages appear on the left of the screen, starting at about **75% of the
  window height**, newest at the bottom, **bold, in the sender's player
  colour**, e.g. **Player 3: wall the north gap**. Each fades after about
  10 seconds; the last few are kept.
- A short rate limit stops spam.

## Capabilities

### New Capabilities

- `fvd-builder-chat`: team text chat for Builders.

## Status

Proposed (Dougie, 2026-11-03). Not implemented. Needs a protocol bump.
