# Design

## Simulation and placement

Spawn one special Hunter-owned tower at the map's logical center at match initialization. Give it an explicit entity definition or immutable tower type distinguishable from ordinary Builder towers; avoid relying only on HP or UI labels for invulnerability. Reserve its permanent footprint in the pathfinding/occupancy grid. No Builder construction may overlap this footprint.

Reuse the existing Hunter wave schedule: the first horse still spawns at t=20 and one per second afterwards. Choose deterministic valid spawn positions around the tower's footprint (fixed ordering and tie breaks), respecting bounds, obstacles, reservations and nearby units. Contact separation may spread horses further without issuing Move or AttackMove orders. A blocked preferred position must not cause a horse to spawn inside the tower or skip its scheduled spawn.

## Combat

The spawn tower has a range of 10 map tiles measured consistently with current combat distance conventions. It must never select, launch at, or damage a Builder before t=20, and activates precisely at t=20 independently of whether the first horse can be rendered. After activation it automatically acquires living enemy Builder units within range and launches missiles for 100 damage per confirmed impact, with the same authoritative projectile scheduling, duplicate-impact protection and world-state replication used by existing combat. Existing projectile travel/cooldown conventions can be reused initially; make those values explicit in implementation and tests rather than inventing a new firing cadence. The tower does not attack Hunter horses or other friendly entities.

The tower is invulnerable to all damage paths, selection commands and demolition (including the Builder Delete shortcut), and is exempt from ordinary destroyed-tower cleanup. It remains present until the match ends. It does not credit Builder tower kills for its attacks.

## Verification decisions

Test the precise 19.999s / 20s boundary, range-edge hit/miss, 100 HP loss per missile impact, immunity to combat and deletion, deterministic spawn positions over many waves, and authoritative guest replication. Keep the special tower present across the full match, including reconnect/resnapshot. Do not accidentally make the Hunter tower a selectable ordinary Builder tower.
