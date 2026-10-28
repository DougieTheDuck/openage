# Design notes

- The host decides the outcome and replicates it once; clients only display it.
- The timer counts from match start (t=0), so the Builders' 20-second head
  start is included. Show the remaining time in the HUD.
- 7000 towers means finished towers alive at the same moment, summed over all
  Builders. Check after each tower completes.
- A Builder's towers collapse with the existing demolition animation; towers
  under construction and foundations are removed too.
- Interaction with the existing disconnect and host-migration rules (see
  ROADMAP "Scope boundaries"): decide whether a disconnected Builder counts as
  dead.
- Garrisoned Builders (if `builder-garrison` lands) count as alive.

## Open questions

- Does the Hunter tower keep firing after the result?
- Does the timer pause while the host migrates?
