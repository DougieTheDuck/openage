# Tasks

- [x] 1.1 Add `HunterStance` and the authoritative per-horse stance map with Hunter-only validated `set_hunter_stance`; default Observe; clear on death.
- [x] 1.2 Share tower-first target acquisition between AttackMove and stances, with radius and optional anchor restriction.
- [x] 1.3 Start Aggressive (8-tile sight, no leash) and Defensive (4-tile range, 6-tile leash, return to anchor) engagements for idle horses without orders.
- [x] 1.4 Hand an arrived AttackMove over to a non-Observe stance; keep Observe behaviour unchanged.
- [x] 1.5 Add the Patrol order with all-or-nothing validation and endpoint swapping; engage en route with AttackMove rules.
- [x] 1.6 Add SET_STANCE and PATROL simulation commands and network command kinds, with the stance intent field and validation.
- [x] 1.7 Replicate each horse's stance to its owner only; include it in world deltas and the local presentation snapshot; bump the protocol major version.
- [x] 1.8 Bind Z/X/C stances and P-then-click Patrol for local and guest Hunters; show stance and hotkeys in the HUD.
- [x] 1.9 Test stance defaults, rejection of foreign and Builder stance commands, Aggressive and Defensive acquisition, leash and return, hand-over, Patrol looping and engagement, and stance wire round trips and privacy.
- [x] 1.10 Verify interactively in solo, then on two processes with the Hunter as a guest.
- [x] 1.11a Document the stances, Patrol and protocol v14 in `assets/fvd/README.md`.
- [ ] 1.11b Update the Hunter rule in `openspec/config.yaml` and `openspec/ROADMAP.md` once this proposal is accepted.
