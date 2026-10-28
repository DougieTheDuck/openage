# Proposal

## Why

Delete demolishes a Builder's selected tower, finished or under construction,
but a foundation (a planned or queued tower site) could not be removed: the
only way to drop one was to retask the Builder, which abandons the whole queue.

## What Changes

- Delete with one of your foundations selected removes it. Its reserved tiles
  are freed, and a selected Builder's "Queued towers" count drops by one.
- If it was the site the Builder was walking to, the Builder moves on to his
  next queued site instead of abandoning the queue.
- Abandoned foundations (no longer in any queue) can be deleted too.
- Towers keep Ryan's behaviour: immediate demolition with the collapse
  animation. Foundations have no tower to collapse, so they simply vanish.
- Box selections prefer units: a Builder's box that also catches his towers or
  foundations selects the Builder, so he can still be ordered.

No new command: the existing `DEMOLISH_TOWER` command (Delete) accepts a
foundation ID and the host routes it.

## Capabilities

### Modified Capabilities

- `fvd-builder-construction`: foundations can be deleted; queue continues.
