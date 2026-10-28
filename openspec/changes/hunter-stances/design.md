# Design

## Context

Hunter combat lives in `FVD::combat_tick`, which runs on a fixed
simulation-time repeat event. Each horse with an `AttackOrder` scans
for eligible targets within the 4-tile firing range, gives completed
towers priority over Builders, pursues with `ranged_approach` and
schedules hits at the attack animation's hit frame. Horses without an
order are skipped entirely, which is why idle horses never fight.

## Stance storage

`HunterStance` (`OBSERVE`, `AGGRESSIVE`, `DEFENSIVE`) is a gamestate type
beside `AnimationAction`, because the network layer replicates it too.
FVD keeps a host-authoritative map from horse ID to stance. A horse
absent from the map is Observe, so spawning needs no change, and a dead
horse's entry is removed with its attack order in `apply_damage`.

`FVD::set_hunter_stance` validates the selection with the existing
`resolve_command_units` (Hunter-only, owned, living horses, stale dead
horses pruned) and rejects the whole command otherwise. Changing a
horse's stance cancels any engagement that the previous stance started,
so a horse switched to Observe stops chasing immediately. Command-driven
orders are left alone.

## Stance engagements reuse AttackOrder

A stance does not get a parallel combat loop. When `combat_tick` reaches
a living horse without an order, and the horse is not moving, its stance
may create an ordinary `AttackOrder` with `stance_engagement = true` and
`destination` set to the horse's current position. From then on the
existing pursuit, firing and hit-scheduling code handles it unchanged.

Target acquisition is factored into one member function shared by
AttackMove and stances: nearest completed tower within a radius, else
nearest Builder within that radius, with the existing deterministic ID
order and tie behaviour. An optional anchor restricts candidates to
those within a given distance of a point.

| Source | Acquisition radius | Anchor restriction | After the target is gone |
|---|---|---|---|
| AttackMove | 4 tiles (firing range) | none | resume destination |
| Patrol, Observe | never acquires | — | — |
| Patrol, Defensive | 4 tiles | candidates within 10 tiles of where it left the route | walk back there, then resume the leg |
| Patrol, Aggressive | 8 tiles (sight) | none | resume the leg if the target died; stop patrolling if it escaped beyond 8 tiles |
| Aggressive | 8 tiles (sight) | none | drop the order where it stands; next tick rescans at 8 tiles |
| Defensive | 4 tiles | candidates within 6 tiles of the anchor | walk back to the anchor, still defending; drop the order on arrival |

A Defensive engagement also ends when the horse or its target moves more
than 6 tiles from the anchor. The anchor restriction on reacquisition
prevents a returning horse from turning straight back to the same
escaping target.

Moving horses are skipped by stance acquisition, so a right-click Move
travels without stopping to fight, matching the existing difference
between Move and AttackMove.

## AttackMove hand-over

An arrived AttackMove currently stays alert at its destination until the
next command. Observe horses keep exactly that behaviour. Aggressive and
Defensive horses instead drop the arrived order so their stance takes
over, which is a strict superset (Aggressive sees further; Defensive adds
the return-to-anchor rule at the same spot).

## Patrol

`FVD::issue_patrol` mirrors `issue_attack_move` validation: Hunter owner,
in-bounds ground destination, resolved survivors and a found path for
every horse, all or nothing. A destination on a completed tower is
rejected, since a Patrol endpoint inside a blocked footprint can never be
reached. Each horse's order stores its own starting position as
`patrol_return`. On arrival the endpoints swap and the horse moves on,
so the order never sets `reached_destination`. Engagements en route use
the AttackMove pursuit and firing code, with the stance choosing the
acquisition radius and leash as in the table above. A Defensive patrol
records `patrol_engagement` where it first left the route and walks back
there before resuming its leg. Switching a patrolling horse to Observe
drops its current target immediately.

## Commands and networking

Two command kinds join the simulation (`command_t::SET_STANCE`,
`command_t::PATROL`) and the wire protocol (`CommandKind::SET_STANCE = 6`,
`CommandKind::PATROL = 7`). SET_STANCE carries its stance in a new
optional intent field signalled by a command flag; it is required for
SET_STANCE and forbidden otherwise. The host validates ownership exactly
as for AttackMove.

Each Hunter horse's stance travels in `EntityState::hunter_stance`,
signalled by a new entity flag and stored in the entity record's
previously reserved 16-bit field. Like tower kills, it is only sent to
the owning player: the host strips it for Builder recipients and the
decoder rejects it on any entity the recipient does not own. World
deltas treat a stance change as an entity change. The local presentation
snapshot includes it, so solo and host HUDs read the same field as guests.

Older clients would reject the new entity flag and command kinds, so the
protocol major version increases to 14 and incompatible clients are
refused at the handshake rather than failing mid-match.

## Input and HUD

The game controller's AttackMove targeting flag becomes a small targeting
mode (`NONE`, `ATTACK_MOVE`, `PATROL`), so the existing left-click path
serves both and one cancel clears either. Z, X and C send SET_STANCE for
the selected horses through the same local or guest command path as
AttackMove. The HUD adds a stance line for Hunter horse selections
(`Mixed` for a group with different stances) and Patrol help text.
