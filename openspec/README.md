# FVD development specs

The user-edited [roadmap](ROADMAP.md) is the authority for the first playable
FVD match. The specs below translate its milestones into behavioral contracts,
implementation decisions and testable tasks. A change is not considered
implemented merely because it has a spec.

## Changes in roadmap order

1. [Builder construction](changes/builder-construction/proposal.md): move to a
   valid adjacent tile, reserve the site and complete timed construction.
2. [Build menu and HUD](changes/build-menu-hud/proposal.md): tower placement
   UI, selection details and only the current player's kill count.
3. [Hunter combat and kills](changes/hunter-combat-kills/proposal.md): a
   20-second Builder head start, one horse per second thereafter, human
   Hunter selection/attack-move, tower arrows and kill attribution.
4. [Multiplayer lobby](changes/multiplayer-lobby/proposal.md): 2-8 humans,
   a randomly assigned human Hunter, half-weight host draw, host Start.
5. [Synchronized match](changes/synchronized-match/proposal.md): authoritative
   state, player ownership, concurrent commands and disconnect handling.
6. [Hunter spawn tower](changes/hunter-spawn-tower/proposal.md): permanent central
   Hunter tower, horses spawning around it, and 10-tile / 100-damage missiles
   activated when the 20-second Hunter timer expires.

The last four plans capture product requirements now. Hunter control is
human selection, Move and AttackMove; automatic spawn movement is excluded.
Networking transport and combat tuning remain engineering decisions.

### Player model

Player 1 is always the **human Hunter role in a multiplayer match**, not an
AI player. The other 1-7 humans each own one Builder in slots 2-8. A lobby
host may become the Hunter, with half the selection weight of any guest.
In solo development mode, one tester switches between roles 1 and 2 with a
hotkey; no AI player participates. Hunter horses first spawn at match second
20 and once per second thereafter. Passive contact forces spread the idle
spawn crowd outward; horses only travel or attack on human Hunter orders.
A future rally/gather point is deliberately outside the initial scope.

## Working with OpenSpec

Run `openspec list`, `openspec show <change>`, and
`openspec validate <change>` if the OpenSpec CLI is installed. These Markdown
and YAML files can also be reviewed without the CLI.

After a change is implemented and reviewed, archive it through the OpenSpec
workflow so its accepted requirements become part of `openspec/specs/`.

See [AGENTS.md](../AGENTS.md) for development conventions and
[assets/fvd/README.md](../assets/fvd/README.md) for the existing prototype.
