# Proposal

## Why

Builders have no way to hide from horses. Garrisoning lets a Builder shelter in
any finished friendly tower and hop along allied walls. One Builder
occupies each tower; the occupant's player-colour flag is public.

## What Changes

- With a Builder selected, **T** enables garrison mode; clicking one of your
  finished allied towers sends the Builder there and puts him inside.
- A garrisoned Builder is hidden and cannot be targeted.
- An occupied tower visibly raises the occupant's player-colour flag for
  everyone who can see the tower, without disclosing the private Builder ID.
- **G** is permitted only to the Builder's actual player. Other players
  attempting it see "Can only ungarrison your own builder."
- A shortcut key (**G**) ungarrisons the Builder. With no ungarrison
  point set, he steps straight out onto a free tile next to the tower and
  stays there, even if horses are nearby.
- Every Builder player has an independent **ungarrison point per tower**,
  set like the Hunter's rally point: select a friendly tower and right-click.
  It uses the same red arrow sprite, visible only to the point's player
  while its source tower is selected. When one is
  set, the Builder walks there on ungarrisoning.
- If the ungarrison point is on an allied finished tower **adjacent** to the
  current one, ungarrisoning moves the Builder straight into that tower instead
  (hopping between towers).

## Capabilities

### New Capabilities

- `fvd-builder-garrison`: garrison, ungarrison point, hopping.

## Status

Implemented on FVD, pending compilation and multiplayer playtests.
