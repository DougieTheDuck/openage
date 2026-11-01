# FVD Construction: Unit Displacement

## Requirements

### Occupied-unit foundation placement

The game MUST allow Builder tower foundations to be placed on tiles occupied
by live Builders or Hunter Horses, while still rejecting conflicting towers,
foundations, reserved tiles and out-of-bounds positions.

### Clearance at the active queue position

The game MUST attempt to move units off the footprint only when that site's
construction order becomes active, not when the Foundation is first queued.
The building Builder MUST approach an available stand beside the footprint.
Blocking units MUST receive reachable movement destinations outside the
footprint, without being teleported.

### Five-second timeout

If the footprint or stand has not cleared after five seconds of authoritative
simulation time, the game MUST cancel and remove **only** that foundation,
release its reservation and continue with the next queued foundation in the
original FIFO order.

### Multiplayer consistency

The host MUST author all displacement, timeout, construction and resulting
movement curves. Guests MUST display the host-authored state and MUST NOT
independently push units or skip construction orders.
