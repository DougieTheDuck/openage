# Design notes

- Use Qt's `MouseButtonDblClick` event and suppress the following release so
  normal single-click cannot erase a freshly selected group.
- "Visible on screen" means inside the camera's viewport projection; never
  include fog-hidden or remembered buildings. The clicked owner (not the
  controlling Builder) determines which structures the double-click collects.
- Shift-click accumulates same-kind structures across allied Builder owners.
- Hunter clicks select just one visible tower and expose authoritative HP.
- Three new selection kinds: finished towers, towers under construction and
  foundations. Existing single-tower Delete stays as it is.
- `DEMOLISH_TOWER` currently requires exactly one entity. Either allow several
  (all owned, same kind, each validated, all-or-nothing) or add a group command.
  Each tower still plays its collapse animation.
- Towers under construction are their own group (Dougie, 2026-10-28).
  Deleting them cancels each Builder's job, as single Delete does today.
