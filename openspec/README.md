# FVD development specs

The [roadmap](ROADMAP.md) describes the first playable FVD multiplayer game.
Work is tracked as small OpenSpec changes with a proposal (why), a spec delta
(what users can rely on), a design (how), and tasks (implementation checklist).

## Current change

- [builder-construction](changes/builder-construction/proposal.md) — make a Builder
  walk to a valid adjacent tile and complete a timed tower construction order.

The later changes in the roadmap should be proposed individually after their
dependencies and integration risks are reviewed. Only approved, observable
behavior belongs in the specs; implementation decisions can be revised.

To use the OpenSpec CLI, install it separately, then run these commands from
the repository root:

    openspec list
    openspec validate builder-construction
    openspec show builder-construction

These files are plain Markdown/YAML and do not require the CLI to read or edit.
When a change is implemented and reviewed, archive it using the OpenSpec
workflow so its capability spec becomes part of openspec/specs/.

For engine-specific conventions, see [AGENTS.md](../AGENTS.md), and for the
existing FVD behavior see [assets/fvd/README.md](../assets/fvd/README.md).
