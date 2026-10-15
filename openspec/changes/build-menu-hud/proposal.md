# Proposal

## Why

FVD currently exposes a Ctrl-click placement shortcut and a help banner,
not a usable tower construction interface or per-player kill indicator.

## What Changes

- Add a Builder build menu (initially a single Tower button) and placement
  preview with valid/invalid feedback and cancellation.
- Expose selected unit and current role, plus construction progress where
  available.
- Show only the **current player's** kill count, initially zero.
- Submit the same validated build order as the existing developer shortcut.
- Ensure UI clicks do not issue unintended world commands.

## Capabilities

### New Capabilities

- `fvd-build-menu-hud`: role-aware Builder actions, placement preview,
  selected-unit information and player-private kill display.

### Modified Capabilities

None.

## Impact

- `assets/fvd/qml/main.qml`, QML-to-controller integration and view models
- `libopenage/presenter/` and `libopenage/input/` integration
- Simulation-backed construction progress and player kill counters

This change does not implement Hunter combat, multiplayer transport,
additional build types or the Hunter's yet-unspecified command UI.
