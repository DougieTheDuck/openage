# Tasks

- [x] 1.1 Handle Qt structure double-click independently of the trailing single-click release.
- [x] 1.2 Select same-kind visible buildings of the clicked owner's colour inside the camera view; Shift-click adds allied same-type structures.
- [x] 1.3 Three group selection kinds (finished, under construction, foundations) and HUD text; single enemy tower inspection for Hunter.
- [ ] 1.4 Group Delete (separate follow-up; do not allow allied groups to demolish foreign towers).
- [ ] 1.5 Run new selection tests (`tower_placement_mode`) and verify Windows/Linux compilation. Source tests added for cross-owner Shift-click, owner-scope double-click, construction groups and Hunter visibility; group Delete tests deferred with 1.4.
- [ ] 1.6 Playtest double-click timing and on-screen owner filtering on host and guest Builders. **2026-11-02 user smoke checks:** Shift+click PASS; Hunter left-click tower inspection PASS; Hunter structure multi-select disabled PASS; fog blocks input PASS. Double-click failed because `InputManager` lacked a `MOUSE_BUTTON_DBL` routing binding. Fixed on `FVD` and added a routing regression; retest after rebuilding. Group selection on screen and Hunter visibility still need full acceptance.
- [x] 1.7 Unit selection ground rings: shader-generated white 2:1 ellipse with a 1px dark under-shadow, behind selected Builders/Horses (including groups), scaled from nyan selection-box widths and following interpolated positions; no structure rings or gameplay/replication changes.
- [ ] 1.8 Run `bin/run test openage::renderer::world::tests::selection_ground_rings -a --have-assets` and visually check ring alignment, zoom, death, fog and group selection on host/guest Windows/Linux.
