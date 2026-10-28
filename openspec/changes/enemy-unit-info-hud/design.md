# Design notes

- Guest selection currently ignores entities the player does not own, and
  local selection treats a lone horse as commandable. Add an `ENEMY` selection
  kind used for inspection only; right-click and keys do nothing with it.
- HP is already in each `EntityState` for entities the recipient can see.
- Attack figures come from scenario settings: horse melee damage
  (`horse_melee_damage`) and the Hunter tower's damage and range. Send them in
  the match settings rather than per entity.
- Deselect when the entity leaves the recipient's view.
- Open question: should the Hunter be able to inspect Builders and towers the
  same way? Easy to add with the same panel.
