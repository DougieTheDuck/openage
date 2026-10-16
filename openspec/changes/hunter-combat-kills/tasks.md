# Tasks

- [x] 1.1 Implement match-relative grace-period scheduling: no spawns before t=20s; one horse at t=20s and every second thereafter.
- [x] 1.2 Keep new Hunter horses free of autonomous orders and resolve spawn-area overlaps; test timing, crowd separation and blocked centre headlessly.
- [ ] 1.3 Add human Hunter owned-horse selection, including group selection, and ordinary Move orders.
- [ ] 1.4 Add AttackMove orders through the existing controller, command queue, pathfinding and simulation; fight eligible enemies en route, then resume travel.
- [ ] 1.5 Enforce Hunter-only horse command ownership in solo and network role assignments; reject Builder-issued Hunter orders.
- [ ] 1.6 Add horse attack/damage behavior and tower targeting, arrow projectiles, damage and deaths for completed towers.
- [ ] 1.7 Add idempotent lethal-damage credit to the owner of a killing tower.
- [ ] 1.8 Expose only the current player's kill total through the HUD.
- [ ] 1.9 Test exact t=19/20/21/22-second spawning, idle groups, command handling, resumed AttackMove and simultaneous lethal hit attribution.
- [ ] 1.10 Keep automatic gather/rally points outside the initial playable scope; run relevant simulation and UI checks.
