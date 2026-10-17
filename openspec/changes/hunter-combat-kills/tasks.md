# Tasks

- [x] 1.1 Implement match-relative grace-period scheduling: no spawns before t=20s; one horse at t=20s and every second thereafter.
- [x] 1.2 Keep new Hunter horses free of autonomous orders and resolve spawn-area overlaps; test timing, crowd separation and blocked centre headlessly.
- [x] 1.3 Add human Hunter owned-horse selection, including group selection, and ordinary Move orders.
- [ ] 1.4 Add AttackMove orders through the existing controller, command queue, pathfinding and simulation; fight eligible enemies en route, then resume travel.
- [ ] 1.5 Enforce Hunter-only horse command ownership in solo and network role assignments; reject Builder-issued Hunter orders.
- [ ] 1.6 Add horse attack/damage behavior and tower targeting, arrow projectiles, damage and deaths for completed towers.
- [x] 1.7 Add idempotent lethal-damage credit to the owner of a killing tower.
- [x] 1.8 Expose only the current player's kill total through the local HUD (network delivery follows synchronized-match work).
- [ ] 1.9 Test exact t=19/20/21/22-second spawning, idle groups, command handling, resumed AttackMove and simultaneous lethal hit attribution.
- [ ] 1.10 Keep automatic gather/rally points outside the initial playable scope; run relevant simulation and UI checks.

- [x] Add a local Hunter focus-attack command for deliberately targeting a Builder despite a nearby tower, with ownership validation and cancellation on target death. Remote command forwarding is part of the networking work.
- [x] 1.11 Show the shared simulation-time countdown to the first Hunter horse in the top-centre HUD.
