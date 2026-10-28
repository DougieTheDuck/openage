# Design notes

- Labels live in `Presenter` (`kills_label`): "Total kills: " and
  "Tower kills: ". Rename both to make the unit count explicit.
- Add a regression test that a tower destroyed by horses never changes either
  counter, and that one horse killed by a tower adds exactly one to both.
- Before changing counting code, reproduce what was seen in play: which counter,
  which numbers and what happened.
