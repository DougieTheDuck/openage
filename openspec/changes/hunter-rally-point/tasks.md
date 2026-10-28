# Tasks

- [x] 1.1 Add the `HunterRallyPoint` marker entity and red arrow sprite; reference it from the scenario.
- [x] 1.2 Validate and store the rally point; replace its marker; clear both if the Hunter tower is removed.
- [x] 1.3 Send newly spawned horses to the rally point with a Move order.
- [x] 1.4 Add the `SET_RALLY_POINT` command (simulation, wire kind 9, relay label) with exactly one entity.
- [x] 1.5 Hide the marker from Builder recipients and the local Builder view; let guests draw the Hunter tower and marker.
- [x] 1.6 Select a lone Hunter tower as `HUNTER_TOWER`; right-click sets the rally point; boxes prefer horses; the host skips the tower in horse orders.
- [x] 1.7 Tests: `fvd_hunter_rally_point`, `horse_selection_priority`, rally wire format in `stance_messages`.
- [ ] 1.8 Playtest on a guest Hunter client.
