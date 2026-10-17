# Design

## Approach

Use Qt Quick/QML for the view and expose a narrow interface from the
presenter to the game controller and read-only match state. QML must never
spawn entities or calculate valid construction outcomes itself.
A selected owned Builder opens the Build Tower action; this switches the
controller into placement mode and previews a footprint on the map.
Confirm sends the same validated BuildTower command as the B-key placement
mode, while Escape or right-click cancels placement without issuing a Move command.
The Ctrl-left-click shortcut is removed.

Show role and owned selection regardless of whether the local user is the
Hunter or a Builder. For the Hunter, hide/disable all construction actions.
Expose a per-player kill property from simulation state to the local GUI:
the UI requests only the locally assigned player's count, not a leaderboard.

## Integration and verification

The presenter exposes the current local role, selection details, authoritative
local kill total and simulation-derived construction progress through a QML
property map. Buttons only request controller actions and cannot create
construction entities directly.
The QML build panel is found through the Qt Quick visual item tree; mouse
presses begun on it stay captured through their corresponding releases.
Pointer movement and wheel input over the panel do not scroll the world.

See [verification.md](verification.md) for the remaining manual graphical checks.
The Hunter's Attack-move UI uses the same action boundary; no additional Hunter
controls are introduced.
