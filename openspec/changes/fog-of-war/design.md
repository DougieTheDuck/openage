# Design

## Sides and vision sources

`FVD::team_of` maps player 1 to the Hunter side and players 2–8 to the
Builders side. A vision source is any living entity of the side whose type
has a `LineOfSight` ability; towers only count once completed. The range
is read from nyan once per type and cached (`FVD::vision_range`), so new
types need no code change.

## Visibility is tile based

A tile is visible to a side when its centre lies within a source's range.
An entity is visible to a side when it belongs to that side or stands on a
visible tile. One rule serves simulation, replication and rendering, so a
unit that is drawn can always be targeted and vice versa.

`FVD::tile_visible` and `FVD::can_see` evaluate the rule on demand from
current positions, so commands validated between combat ticks see the
present. The combat tick first runs `update_vision`, which recomputes
both sides' visible bitsets, ORs them into the explored bitsets and
updates remembered buildings; queries made during the tick reuse those
bitsets instead of rescanning every source.

## Remembered buildings

Per side, `update_vision` records each visible enemy tower or foundation
(ID, owner, definition, position). A record is dropped only when its tile
is visible again and the entity no longer exists, so a tower destroyed in
the fog stays on the map until scouted.

## Interaction rules

`hunter_target_eligible` requires Hunter visibility, so automatic
acquisition, the tower-priority switch and pursuit all stop at the fog
edge. `issue_focus_attack` rejects hidden targets; a focus whose target
becomes hidden ends like one whose target died. Towers only choose horses
visible to the Builders. Aggressive stance acquisition and the Aggressive
patrol leash use the horse type's vision range.

Placement is unchanged: Builders may order towers anywhere. A placement
preview still turns red over a hidden horse standing on the footprint,
which reveals that tile's occupant; occupancy is needed to keep units out
of footprints.

## Replication

`WorldState::fog` holds `TeamFog` records: explored and visible bitsets,
one bit per tile in (ne, se) row-major order. On the wire a state carries
exactly the recipient's side; a delta carries it only when it changed.
The local presentation snapshot carries both sides plus remembered IDs.

`capture_fvd_state` removes enemy entities on hidden tiles, appends
remembered buildings flagged `remembered` (no health or motion), drops
Hunter orders for Builder recipients and, for the Hunter, drops Builder
reservations and constructions it cannot see. Projectiles are not
filtered: each one starts or ends at the recipient's own side, so it
reveals nothing new, and filtering would stall the per-recipient
projectile delivery cursor.

Entities leaving view are removed from the recipient's world. Combat cues
only treat a removal as a death if the entity's tile is still visible.

## Presentation

Guests draw exactly what they receive. For the host and solo play the
presenter reduces the local snapshot to the controlled side's view with
the same filter and conceals the hidden entities' sprites
(`WorldRenderStage::set_concealed`), which also excludes them from
picking. The terrain shader darkens ground through a per-tile fog texture:
black when unexplored, 45% when explored, full brightness when visible,
with linear filtering softening the edge. Remembered buildings are
drawn at the same 45% through a `brightness` uniform on the sprite
shader (`WorldRenderStage::set_dimmed`); the local view turns hidden
remembered buildings into the same bare `remembered` records a guest
receives, so both paths dim them alike.
