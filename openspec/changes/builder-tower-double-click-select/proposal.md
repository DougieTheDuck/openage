# Proposal

## Why

Builders need to inspect and group their own or allied structures, and Hunters
need to inspect visible enemy towers without being able to command them.

## What Changes

- A Builder may Shift+left-click additional allied or own structures of the
  same type (finished tower, tower under construction, or foundation), even
  when the structures have different Builder owners.
- As a Builder, double-clicking a **finished tower** selects all finished towers
  belonging to that clicked tower's owner that are visible on screen.
- Double-clicking a **foundation** selects the on-screen foundations of its owner.
- Double-clicking a **tower under construction** selects its owner's on-screen
  towers under construction.
- The three groups never mix: only the kind double-clicked is selected.
- Group selection is inspection-only for now: it never sends orders to an
  allied owner's structures. Group demolition remains a separate TODO.
- The Hunter can single-click a visible enemy tower to inspect its health and
  tower stats but cannot multi-select any structures or select remembered fog silhouettes.
- The HUD shows "N towers selected", "N towers under construction selected" or
  "N foundations selected".

## Capabilities

### Modified Capabilities

- `fvd-build-menu-hud`: group selection of buildings by double-click.
- `fvd-builder-construction`: Delete accepts a group of towers or foundations.

## Status

Selection and Hunter tower inspection code implemented on FVD, 2026-11-02;
manual gameplay/Windows-Linux build verification outstanding. Group Delete remains proposed.
