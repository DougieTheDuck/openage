# Proposal

## Why

Hunter horses only fight while a human is actively issuing AttackMove or
focus orders. A Hunter controlling dozens of horses cannot micromanage
every group, so idle horses ignore Builders and towers next to them and
there is no way to keep a group sweeping an area.

## What Changes

- Give every Hunter horse a persistent stance chosen by the human Hunter:
  - **Observe** (default): never engages on its own initiative. This is
    today's behaviour, so spawned horses still receive no automatic orders.
  - **Aggressive**: while idle, engages any eligible enemy within an
    8-tile sight radius and pursues it without a distance limit.
  - **Defensive**: while idle, engages eligible enemies within its 4-tile
    firing range, gives up beyond 6 tiles from where it engaged, and
    returns to that point afterwards.
- Add a **Patrol** order: the horse moves to a clicked point, then loops
  between its starting position and that point indefinitely. Its stance
  decides how it fights on the way, as in AoE2: Observe never attacks;
  Defensive chases up to 6 tiles from where it left the route (the
  stationary leash), then returns and carries on; Aggressive chases
  anything in sight with no leash and resumes the patrol once the target
  dies or escapes.
- A stance applies only while a horse has no order. Move, AttackMove,
  focus and Patrol orders take precedence; an arrived AttackMove hands
  over to an Aggressive or Defensive stance.
- Replicate stances to the Hunter's client and accept stance and Patrol
  commands from guests with the existing host-authoritative validation.
- Hotkeys for the Hunter: **Z** Aggressive, **X** Defensive, **C** Observe,
  **P** then left-click for Patrol. The HUD shows the selected horses'
  stance and the hotkeys.

## Capabilities

### New Capabilities

- `fvd-hunter-stances`: persistent per-horse Observe/Aggressive/Defensive
  stances, the Patrol order, their validation, replication and HUD.

### Modified Capabilities

- `fvd-hunter-combat`: target acquisition is shared between AttackMove
  and stances; an arrived AttackMove hands over to a non-Observe stance.
- `fvd-synchronized-match`: new SET_STANCE and PATROL command kinds and a
  replicated per-horse stance. This changes the wire format, so the
  protocol major version increases and every client must be rebuilt.

## Product rule change

`openspec/config.yaml` and `openspec/ROADMAP.md` state that only human
Move or AttackMove orders initiate Hunter navigation and attacks.
This change keeps that true for the default Observe stance, but a
human-selected Aggressive or Defensive stance and the Patrol order also
initiate engagements. Spawned horses still never advance automatically.
Both documents need that sentence updated if this proposal is accepted.

## Out of scope

- HUD buttons for stances and Patrol (hotkeys only for now).
- Multi-waypoint patrol routes.
- Moving the sight and leash distances into `scenario.nyan`; they sit
  beside the existing hardcoded 4-tile attack range until combat ranges
  move to data together.
