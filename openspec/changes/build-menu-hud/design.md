# Design

## Approach

Use Qt Quick/QML for the view and expose a narrow interface from the
presenter to the game controller and read-only match state. QML must never
spawn entities or calculate valid construction outcomes itself.
A selected owned Builder opens the Build Tower action; this switches the
controller into placement mode and previews a footprint on the map.
Confirm sends the same BuildTower intent as Ctrl-click, while Escape or
right-click cancels placement without issuing a Move command.

Show role and owned selection regardless of whether the local user is the
Hunter or a Builder. For the Hunter, hide/disable all construction actions.
Expose a per-player kill property from simulation state to the local GUI:
the UI requests only the locally assigned player's count, not a leaderboard.

## Risks / Open Questions

- The QML-to-C++ presenter bridge currently does not send the FVD banner's
  buttons into the game controller; inspect event forwarding and input focus.
- Progress may have to be derived from construction start/end simulation
  times; do not duplicate authoritative timers in QML.
- The Hunter may eventually have unit controls, but this UI change does not
  invent them.
