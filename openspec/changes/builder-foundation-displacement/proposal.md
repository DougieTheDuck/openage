# Proposal: Builder foundation displacement

## Why

FVD currently rejects tower foundations placed beneath a Builder or Hunter
horse, so wall construction is unnecessarily fiddly and some deliberate
foundation queues cannot be entered. A failed occupied site can also disrupt
the rest of the Builder's planned work.

## What changes

- Accept otherwise valid foundations beneath living Builder and Hunter units.
- Keep permanent towers, other foundations, map boundaries and existing
  reservations as placement blockers.
- When the foundation reaches the head of its FIFO queue, move the builder
  to a reachable stand and try to push blocking units off the footprint.
- Wait up to five seconds of simulation time for clearance. If still blocked,
  delete only that foundation, release its reservation and proceed to the
  next queued foundation.
- Preserve the existing queue order, construction animation timing, unit
  ownership and authoritative multiplayer replication.

## Capability

Extend `fvd-construction` with occupied foundation clearance and a
queue-preserving timeout.

## Status

Implementation committed on FVD; requires compilation, focused tests and
host/guest gameplay verification before marking complete.
