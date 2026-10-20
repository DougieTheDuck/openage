# Complete gameplay replication

## Why

FVD is ready for gameplay expansion, but every gameplay feature must be
shared between the Hunter authority and all eligible clients. A feature
that works only in the host's local simulation is incomplete, even when
commands and world revisions are otherwise synchronized. Sending discrete
positions every capture introduces avoidable interpolation delay and
bandwidth when the simulation already has scheduled position curves.

## What changes

- Make multiplayer-visible gameplay state and effects an acceptance
  requirement of every new feature, rather than deferred GUI polish.
- Prioritize authoritative **time-based curves and events** whenever the
  engine already knows their future evolution: unit movement segments,
  projectiles, construction intervals, timed spawns and animation phase.
- Keep the Hunter as the only decision-making simulation. Guests evaluate
  received presentation trajectories but never simulate autonomous damage,
  target choice, pathfinding or RNG to invent gameplay outcomes.
- Retain privacy rules: everybody sees world actions and entity health;
  per-Builder kill totals and individual tower kills stay visible only to
  their owning Builder.
- Validate snapshots, deltas, late joins/recovery and per-recipient routing
  through 2, 4 and 8-client tests, including Windows/Linux peers.
- Make all visuals work on both Hunter and guest screens, with bounds on
  serialized curves, queue sizes, memory and per-frame rendering work.

## Scope and relation to other changes

This follows `synchronized-match`, which establishes host authority,
room handoff and command transport. Existing networking acceptance remains
open independently. This change covers rendering parity, event/curve
replication and ongoing feature-by-feature gameplay acceptance.
