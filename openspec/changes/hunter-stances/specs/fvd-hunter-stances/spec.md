# Spec Delta

## ADDED Requirements

### Requirement: Hunter horses have a persistent stance

Every Hunter horse SHALL have exactly one stance: Observe, Aggressive or
Defensive. A newly spawned horse SHALL be Observe. Only the human Hunter
SHALL change a stance, and the stance SHALL persist until changed again
or the horse dies.

#### Scenario: Spawned horse
- **WHEN** a Hunter horse spawns
- **THEN** its stance is Observe and it receives no automatic order

#### Scenario: Hunter changes a group's stance
- **WHEN** the Hunter sets Aggressive for three owned living horses
- **THEN** all three horses become Aggressive

#### Scenario: Builder attempts a stance change
- **WHEN** a Builder player sends a stance command for any horse
- **THEN** the simulation rejects it and no stance changes

#### Scenario: Invalid selection
- **WHEN** a stance command includes an unknown entity, a Builder or a duplicated ID
- **THEN** the simulation rejects the whole command and no stance changes

#### Scenario: Stale selection
- **WHEN** a stance command includes a horse that has already died alongside living horses
- **THEN** the living horses change stance and the dead horse is ignored

### Requirement: Observe horses never engage on their own initiative

A horse in Observe stance without an order SHALL NOT start any engagement.

#### Scenario: Enemy within range of an idle Observe horse
- **WHEN** a Builder stands within 4 tiles of an idle Observe horse
- **THEN** the horse does not attack

### Requirement: Aggressive horses engage anything in sight

An idle Aggressive horse without an order SHALL engage the nearest
completed enemy tower within 8 tiles, otherwise the nearest Builder
within 8 tiles, and SHALL pursue it until it dies or becomes unreachable.
It SHALL then look for another target from where it stands.

#### Scenario: Builder in sight
- **WHEN** a Builder stands 7 tiles from an idle Aggressive horse
- **THEN** the horse approaches it and attacks

#### Scenario: Tower priority
- **WHEN** a completed tower and a closer Builder are both within 8 tiles
- **THEN** the Aggressive horse targets the tower

#### Scenario: Out of sight
- **WHEN** the nearest enemy is 9 tiles away
- **THEN** the idle Aggressive horse does not move

### Requirement: Defensive horses guard their position

An idle Defensive horse without an order SHALL engage the nearest eligible
enemy within its 4-tile firing range, using the same tower priority. It
SHALL abandon a target when either the horse or the target is more than
6 tiles from the point where the engagement began, and SHALL return to
that point once it has no target, defending it on the way.

#### Scenario: Enemy comes close
- **WHEN** a Builder comes within 4 tiles of an idle Defensive horse
- **THEN** the horse attacks it

#### Scenario: Enemy beyond firing range
- **WHEN** the nearest enemy is 5 tiles from an idle Defensive horse
- **THEN** the horse does not move

#### Scenario: Target flees
- **WHEN** a Defensive horse's target moves more than 6 tiles from the engagement point
- **THEN** the horse abandons it and returns to the engagement point

#### Scenario: Target killed
- **WHEN** a Defensive horse kills its target away from its engagement point
- **THEN** the horse returns to the engagement point and becomes idle there

### Requirement: Orders take precedence over stances

A stance SHALL only start engagements for a horse without an active order
that is not moving. Move, AttackMove, focus and Patrol orders SHALL run
unchanged regardless of stance. When an AttackMove reaches its destination
without a target, an Aggressive or Defensive horse SHALL continue under its
stance, while an Observe horse SHALL keep the AttackMove's alert behaviour.

#### Scenario: Moving through enemies
- **WHEN** an Aggressive horse is ordered to Move past a Builder
- **THEN** it does not stop to attack while travelling

#### Scenario: Arrived AttackMove under Aggressive
- **WHEN** an Aggressive horse's AttackMove arrives and a Builder is 7 tiles away
- **THEN** the horse engages the Builder

#### Scenario: Stance change during a stance engagement
- **WHEN** an Aggressive horse is chasing a target and the Hunter sets it to Observe
- **THEN** the horse stops and does not complete further attacks on that target

### Requirement: Patrol loops between two points

A Patrol order SHALL move each selected horse to the clicked point and then
back to its own starting position, repeating until another order replaces
it. The horse's stance SHALL decide how it fights while patrolling:
an Observe horse SHALL never attack; a Defensive horse SHALL engage
enemies within 4 tiles, abandon a target once the horse or the target is
more than 6 tiles from where it left its route (the same leash as a
stationary Defensive horse), then return there and resume the patrol; an
Aggressive horse SHALL engage enemies within its sight, with no leash, and
SHALL resume the patrol once the target dies or escapes beyond that sight.
Patrol SHALL decide only the route; a stance change during a patrol SHALL
take effect immediately. A Patrol command SHALL
be rejected entirely unless it comes from the Hunter, every horse is owned
and alive or known dead, the destination is an in-bounds ground position
outside completed tower footprints, and every horse has a path to it.

#### Scenario: Loop
- **WHEN** a horse on Patrol reaches the clicked point
- **THEN** it turns back toward its starting position, and on reaching that, heads out again

#### Scenario: Observe patrol
- **WHEN** a Builder comes within 2 tiles of an Observe horse on Patrol
- **THEN** the horse keeps patrolling without attacking

#### Scenario: Defensive patrol, enemy en route
- **WHEN** a Builder comes within 4 tiles of a Defensive horse on Patrol
- **THEN** the horse attacks it and afterwards resumes its patrol

#### Scenario: Defensive patrol, enemy flees
- **WHEN** a Defensive patrolling horse's target runs more than 6 tiles from where the horse left its route
- **THEN** the horse gives up, returns to that point and resumes its patrol

#### Scenario: Aggressive patrol, enemy escapes
- **WHEN** an Aggressive patrolling horse's target moves beyond 8 tiles from it
- **THEN** the horse gives up and carries on along its patrol route, still Aggressive

#### Scenario: Unreachable destination
- **WHEN** any selected horse has no path to the Patrol destination
- **THEN** no horse receives the Patrol order

### Requirement: Stances are replicated privately and commanded remotely

The host SHALL accept stance and Patrol commands from a guest Hunter with
the same validation as local commands. Each horse's stance SHALL be sent
only to the player who owns it. Clients with an incompatible protocol
version SHALL be refused at connection time.

#### Scenario: Guest Hunter sets a stance
- **WHEN** a guest Hunter presses Z with horses selected
- **THEN** the host applies Aggressive and the guest HUD shows Aggressive after the next world update

#### Scenario: Builder recipient
- **WHEN** the host sends world state to a Builder
- **THEN** it contains no horse stances

#### Scenario: Old client
- **WHEN** a client built before this change connects
- **THEN** the handshake rejects it as incompatible

### Requirement: Hunter controls for stances and Patrol

With Hunter horses selected, Z SHALL set Aggressive, X Defensive and C
Observe, and P SHALL enter Patrol targeting, where the next left-click on
the map issues the Patrol order and right-click or Escape cancels. The
HUD SHALL show the selection's stance, or Mixed when the selected horses
differ, and the available hotkeys.

#### Scenario: Patrol targeting cancelled
- **WHEN** the Hunter presses P and then Escape
- **THEN** no Patrol order is sent

#### Scenario: Mixed selection
- **WHEN** the Hunter selects one Aggressive and one Defensive horse
- **THEN** the HUD shows Stance: Mixed
