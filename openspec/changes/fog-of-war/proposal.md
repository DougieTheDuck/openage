# Proposal

## Why

Every player currently sees the whole map: the Hunter knows where every
tower is before its horses arrive, and Builders watch horses gather at the
centre from anywhere. Neither side has to scout, and a modified client
receives everything the host knows.

## What Changes

- Each side shares one view of the map. The Hunter sees what its horses
  see; all Builders share what any Builder or completed tower sees.
- Vision ranges come from each entity type's `LineOfSight` ability in
  `entities.nyan`: Builder 6 tiles, Hunter horse 7, Tower 8. Future unit
  and tower types set their own range the same way.
- Tiles are unexplored (black), explored but not currently seen (dimmed)
  or visible. Enemy buildings stay on the map, dimmed like the ground
  around them, where they were last seen until their site is seen again.
  Enemy units in the fog are not shown at all.
- Nothing a side cannot currently see may be attacked, targeted or
  otherwise interacted with: focus attacks on hidden entities are
  rejected, automatic targeting ignores them, a chase ends when the
  target disappears, and towers only fire at horses the Builders see.
  Aggressive horses therefore react within their 7-tile vision, not 8.
- The host sends each player only their own side's entities, the enemy
  entities their side can see, remembered enemy buildings and their
  side's fog. Builders no longer receive Hunter orders, and the Hunter
  only receives Builder plans it can see.
- Hosts and solo players see the same view as a guest would.

## Capabilities

### New Capabilities

- `fvd-fog-of-war`: per-side vision, explored memory, remembered
  buildings, visibility-gated interaction, filtered replication and fog
  rendering.

### Modified Capabilities

- `fvd-hunter-stances`: Aggressive acquisition and the Aggressive patrol
  give-up distance use the horse's vision range; an Aggressive patrol
  whose target vanishes into fog resumes its route.
- `fvd-synchronized-match`: world state gains per-side fog and remembered
  buildings. The protocol major version increases to 16.

## Out of scope

- Restricting where Builders may place towers. Placement stays allowed
  anywhere; the Builder walks into the fog to build.
- Ghosts of buildings the host-local view saw destroyed while unseen.
