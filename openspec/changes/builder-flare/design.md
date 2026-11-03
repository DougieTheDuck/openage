# Design notes

## Network

- New command `FLARE = 13` (guest → host) carrying the target position only.
  The host stamps owner and time; it rejects the Hunter (player 1), anyone
  not in the match, positions off the map, and flares less than 2 s after
  that player's previous one.
- `WorldState` gains `std::vector<FlareState> flares` with
  `{flare_id, owner_id, position, created_at}`: the last five seconds of
  flares, capped at 64. Like arrows, deltas only send new flares and every
  client drops them after five seconds, so no removals cross the wire. A
  Hunter client refuses any delta that would give it flares. `flare_id` is monotonic, like `arrow_id`, so a
  client shows and plays each flare once even across delta/snapshot changes.
- The host's per-recipient world for player 1 never contains flares, in both
  snapshots and deltas. Builders get every Builder's flares (fog does not
  hide a teammate's flare).
- Protocol: `PROTOCOL_MAJOR` 22 (new command and new world field).
- Known limit: a host who is the Hunter still holds flares in its own
  simulation memory; the local view filter must strip them as
  `fvd_local_view` does for fog.

## Client

- `F` (added to the presenter's forwarded keys) or the HUD button arms flare
  mode, like attack-move targeting. Solo play flares locally.
- Star drawn in QML over the world, like the attack-order markers: a
  five-pointed star in the owner's player colour, pulsing and fading over 5 s.
- `flare` sound cue ("gling", file `flare.wav`/`.opus`): played once per new
  `flare_id`, unplaced (full volume, centre), throttled like other cues.
- Minimap: the same star at the flare's map position (see `minimap`).

## Decisions

- The flarer hears their own gling too, as confirmation.
- Flares can also be placed by clicking the minimap while flare mode is armed.
- Right-click or Esc cancels flare mode.

## Open questions

- Should the camera be able to jump to the last flare (e.g. Space)?
