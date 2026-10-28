# Spec Delta

## ADDED Requirements

### Requirement: Each side shares one view of the map

The Hunter SHALL see what any of its horses sees, and every Builder
player SHALL see what any Builder or completed tower sees. A tile SHALL be
visible to a side when its centre lies within the vision range of one of
that side's units or completed towers. Vision ranges SHALL come from each
entity type's `LineOfSight` ability: 6 tiles for a Builder, 7 for a Hunter
horse and 8 for a Tower.

#### Scenario: Within a Builder's vision
- **WHEN** a horse stands on a tile whose centre is 5 tiles from a Builder
- **THEN** every Builder player can see it

#### Scenario: Beyond a Builder's vision
- **WHEN** a horse stands 7 tiles from the nearest Builder and no tower sees it
- **THEN** no Builder player can see it

#### Scenario: Own side
- **WHEN** a Builder stands far from every other Builder and tower
- **THEN** the Builders still see it, and its own vision still applies

### Requirement: Explored ground and remembered buildings

Tiles a side has seen before but cannot currently see SHALL be shown as
explored, and tiles never seen as unexplored. An enemy building seen
before SHALL remain shown where it was last seen until the side sees its
tile again without it.

#### Scenario: Leaving an area
- **WHEN** the only Builder near a tile walks away
- **THEN** the tile is explored but not visible to the Builders

#### Scenario: Tower destroyed in the fog
- **WHEN** a tower the Hunter saw is destroyed while no horse can see it
- **THEN** the Hunter still sees it remembered until a horse sees its site again

### Requirement: Nothing hidden can be attacked or interacted with

A side SHALL NOT attack, target or interact with an entity it cannot
currently see. Automatic targeting SHALL ignore hidden entities, a
pursuit SHALL end when its target becomes hidden, focus attacks on
hidden entities SHALL be rejected, and towers SHALL only fire at horses
the Builders can see. Builders MAY still order towers anywhere on the map.

#### Scenario: Focus attack on a hidden Builder
- **WHEN** the Hunter orders a focus attack on a Builder no horse can see
- **THEN** the command is rejected and no horse moves

#### Scenario: Target escapes into the fog
- **WHEN** a focus-attacked Builder moves beyond every horse's vision
- **THEN** the attacking horses stop pursuing it

#### Scenario: Aggressive horse
- **WHEN** a Builder stands 7.5 tiles from an idle Aggressive horse and no other horse sees it
- **THEN** the horse does not engage it

### Requirement: Players only receive what their side can see

The host SHALL send each player only their own side's entities, enemy
entities on tiles their side can see, remembered enemy buildings without
health or movement, and their side's fog. Builders SHALL NOT receive the
Hunter's orders. The Hunter SHALL only receive Builder construction plans
on tiles it can see. The host and solo players SHALL be shown the same
view a guest of their side would receive. Clients built before this
change SHALL be refused at connection.

#### Scenario: Hidden horse
- **WHEN** the host sends world state to a Builder while a horse is outside the Builders' vision
- **THEN** the state does not contain that horse

#### Scenario: Remembered tower
- **WHEN** the host sends world state to the Hunter after its horses lose sight of a tower
- **THEN** the state contains the tower marked as remembered, without health

#### Scenario: Unit walks into the fog
- **WHEN** an enemy unit leaves a guest's view
- **THEN** it disappears without a death effect

### Requirement: Fog is drawn on the ground

The map SHALL be drawn black where the viewer's side has never seen,
dimmed where it has seen before but cannot see now, and normally where it
can see. Remembered enemy buildings SHALL be drawn dimmed like the ground
they stand on. Hidden enemy units SHALL not be drawn or selectable.

#### Scenario: Remembered tower
- **WHEN** the Hunter's horses leave a tower they saw
- **THEN** the tower stays drawn at its last position, dimmed like the explored ground

#### Scenario: Unit in the fog
- **WHEN** an enemy Builder stands on explored ground the Hunter cannot currently see
- **THEN** the Builder is not drawn

#### Scenario: Switching sides in solo play
- **WHEN** a solo player switches from a Builder to the Hunter
- **THEN** the fog and visible units change to the Hunter's view
