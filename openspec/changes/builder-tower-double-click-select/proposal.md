# Proposal

## Why

A Builder with dozens of towers cannot select them together, for example to
delete a badly placed line.

## What Changes

- As a Builder, double-clicking one of your **finished towers** selects all
  of your finished towers visible on screen.
- Double-clicking one of your **foundations** selects all of your foundations
  visible on screen.
- The two never mix: only the kind double-clicked is selected. Towers under
  construction count as neither for now (open question).
- A group of your towers or foundations can be deleted together with Delete.
- The HUD shows "N towers selected" or "N foundations selected".

## Capabilities

### Modified Capabilities

- `fvd-build-menu-hud`: group selection of buildings by double-click.
- `fvd-builder-construction`: Delete accepts a group of towers or foundations.

## Status

Proposed (Dougie, 2026-10-28). Not implemented.
