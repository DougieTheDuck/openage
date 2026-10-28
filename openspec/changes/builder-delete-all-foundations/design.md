# Design notes

- Reuse `delete_owned_foundation` per site so reservations, queue order and
  stale selections behave exactly as single Delete does.
- Order: queued sites first, then the walked-to site, so the Builder never
  starts a site that is about to be deleted.
- A new `DELETE_FOUNDATIONS` command naming the Builder (exactly one entity);
  the host validates that the Builder is the sender's. Wire kind 10.
- Abandoned foundations (in nobody's queue) belong to the player, not to a
  Builder. With one Builder per player, include them; revisit if a player ever
  has several Builders.
- Open question: a shortcut key (e.g. Shift+Delete) as well as the button.
