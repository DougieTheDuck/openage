# Design notes

- Detect the double click with `QStyleHints::mouseDoubleClickInterval` (the
  controller already includes QStyleHints) on the same picked entity.
- "Visible on screen" means inside the camera's view, using the same frustum the
  world renderer already culls with; never include hidden or remembered
  entities (fog of war).
- New selection kinds for a tower group and a foundation group; existing
  single-tower Delete stays as it is.
- `DEMOLISH_TOWER` currently requires exactly one entity. Either allow several
  (all owned, same kind, each validated, all-or-nothing) or add a group command.
  Each tower still plays its collapse animation.
- Open question: whether towers under construction join a tower group.
