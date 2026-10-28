# Design notes

- Detect the double click with `QStyleHints::mouseDoubleClickInterval` (the
  controller already includes QStyleHints) on the same picked entity.
- "Visible on screen" means inside the camera's view, using the same frustum the
  world renderer already culls with; never include hidden or remembered
  entities (fog of war).
- Three new selection kinds: finished towers, towers under construction and
  foundations. Existing single-tower Delete stays as it is.
- `DEMOLISH_TOWER` currently requires exactly one entity. Either allow several
  (all owned, same kind, each validated, all-or-nothing) or add a group command.
  Each tower still plays its collapse animation.
- Towers under construction are their own group (Dougie, 2026-10-28).
  Deleting them cancels each Builder's job, as single Delete does today.
