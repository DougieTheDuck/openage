# Tasks

- [x] 1.1 Define and instantiate exactly one distinguishable, permanent Hunter-owned center tower at match initialization.
- [x] 1.2 Reserve the tower's full footprint in occupancy and pathfinding; reject conflicting Builder construction and keep the tower indestructible across all damage and demolition paths.
- [x] 1.3 Redirect scheduled Hunter horse spawning to deterministic free positions around the central tower; never spawn inside its footprint, skip a wave due to crowding, or issue automatic movement orders.
- [x] 1.4 Gate the central tower's attacks on the same match-relative 20-second Hunter spawn deadline; no targeting or projectile launch earlier.
- [x] 1.5 After activation, acquire enemy living Builders within 10 tiles and fire missiles for 100 damage per confirmed impact; do not target Hunter-owned entities.
- [x] 1.6 Replicate tower presence, missile launches/impacts, resulting HP changes and deaths to all clients through host-authoritative state updates.
- [x] 1.7 Test permanent existence, invalid Builder placement/demolition, projectile damage/one-hit semantics, range boundary, no pre-20-second shots, and subsequent wave timings.
- [ ] 1.8 Test crowd/blocked spawn positions, connected guest snapshots, and no divergent local/remote damage or removal.
- [ ] 1.9 Playtest the 10-tile radius and missile cadence with multiple Builder players, then document any tuning separately.

## Implementation notes (2026-10-28, Linux developer)

- `fvd.data.entities.HunterTower` is a separate definition from the Builders' `Tower`. It has 1000 health but nothing can reduce it. Horses cannot focus-attack it, and `demolish_owned_tower` rejects it because it is neither a Builder tower nor Builder-owned. The placeholder sprite is a crimson recolour of the medieval tower.
- It blocks the same footprint as a Builder tower. Horses spawn on the first free tile beside it (`get_horse_spawn_point`). The existing crowd solver moves a horse off that tile if it is ever blocked, so no wave is skipped.
- `hunter_tower_active_from` is set to the first horse's spawn time in `start_hunter_wave`. Before then the tower never targets or fires. Combat ticks every 0.2 s, so the first shot comes on the first tick at or after t=20.
- Settings live in `scenario.nyan`: `hunter_tower_range = 10.0`, `hunter_tower_damage = 100`, `hunter_tower_cooldown = 1.0` (one shot a second; Builder towers fire every 0.8 s). Shots reuse `arrow_travel_time` and the existing replicated `ArrowVisual` launch records. Damage applies once, on the scheduled `game.fvd_hunter_tower_hit` event, and earns no kill credit.
- Targets are living Builder units only, chosen by `nearest_hunter_target` with no towers, and must be visible to the Hunter side under fog of war. Its `LineOfSight` gives the Hunter 15 tiles of vision around the centre.
- Builders start on a 20-tile ring (`builder_start_radius`), outside its reach.
- Tests: `fvd_hunter_tower` covers silence at t=19.999, the first shot at 20, 100 damage per hit, the 10-tile range edge, towers never targeted, no kill credit, focus-attack and demolition rejection, its blocked footprint and the spawn tile. 1.8 (guest snapshot coverage) and 1.9 (playtest) remain open.
