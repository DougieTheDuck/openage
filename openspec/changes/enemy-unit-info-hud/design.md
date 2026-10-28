# Design notes

- Guest selection currently ignores entities the player does not own, and
  local selection treats a lone horse as commandable. Add an `ENEMY` selection
  kind used for inspection only; right-click and keys do nothing with it.
- HP is already in each `EntityState` for entities the recipient can see.
- Attack figures come from scenario settings: horse melee damage
  (`horse_melee_damage`) and the Hunter tower's damage and range. Send them in
  the match settings rather than per entity.
- Deselect when the entity leaves the recipient's view.
- The Hunter inspects Builders and towers the same way (Dougie, 2026-10-28).
  Tower figures: `tower_arrow_damage`, the 7-tile tower range and
  `tower_armor`. Builders have no attack.
