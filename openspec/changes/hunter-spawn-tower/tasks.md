# Tasks

- [ ] 1.1 Define and instantiate exactly one distinguishable, permanent Hunter-owned center tower at match initialization.
- [ ] 1.2 Reserve the tower's full footprint in occupancy and pathfinding; reject conflicting Builder construction and keep the tower indestructible across all damage and demolition paths.
- [ ] 1.3 Redirect scheduled Hunter horse spawning to deterministic free positions around the central tower; never spawn inside its footprint, skip a wave due to crowding, or issue automatic movement orders.
- [ ] 1.4 Gate the central tower's attacks on the same match-relative 20-second Hunter spawn deadline; no targeting or projectile launch earlier.
- [ ] 1.5 After activation, acquire enemy living Builders within 10 tiles and fire missiles for 100 damage per confirmed impact; do not target Hunter-owned entities.
- [ ] 1.6 Replicate tower presence, missile launches/impacts, resulting HP changes and deaths to all clients through host-authoritative state updates.
- [ ] 1.7 Test permanent existence, invalid Builder placement/demolition, projectile damage/one-hit semantics, range boundary, no pre-20-second shots, and subsequent wave timings.
- [ ] 1.8 Test crowd/blocked spawn positions, connected guest snapshots, and no divergent local/remote damage or removal.
- [ ] 1.9 Playtest the 10-tile radius and missile cadence with multiple Builder players, then document any tuning separately.
