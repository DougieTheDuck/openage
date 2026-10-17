# Build menu and HUD verification

The local build menu can be tested without networking or other players.
Run the FVD modpack in a graphical build, then verify these interactions at
the default window size, after resizing the window, and with the mouse released
outside the HUD after pressing inside it.

- [ ] Select player 2's Builder; the panel shows Builder, current HP and Build Tower.
- [ ] Click Build Tower. The Builder stays selected, placement mode begins, and
  no movement or terrain selection occurs behind the button. Press B to compare.
- [ ] Click the same button when it reads Cancel; placement mode exits without
  moving the Builder. Check the Hunter Attack-move and Cancel button too.
- [ ] Hover a clear tile and an invalid/occupied tile; the preview changes
  between valid and invalid without creating a construction order.
- [ ] Click a valid tile; one Builder construction begins. The panel shows
  "Walking to tower site" where relevant, then a building percentage.
- [ ] Select the Builder again and Shift-click at least two sites. Verify
  foundations and reservations persist, then each queued job executes in order.
- [ ] Cancel with Escape, right-click and the panel's Cancel action. None of
  these cancellation actions may move the Builder or create a tower.
- [ ] Press and drag from the Build Tower panel into the game world before
  release; selection and the world must remain unchanged.
- [ ] Click and release in empty world terrain after using the panel; normal
  selection and movement still work.
- [ ] Select a Tower; inspect its HP, damage and armour, and confirm it has no
  Build Tower action. Press B and verify no placement mode starts.
- [ ] Switch to Hunter player 1; only human Hunter actions appear, with no
  construction control.
- [ ] Select an owned Tower: "Tower kills" starts at zero and increases only
  when that particular tower scores the lethal hit, not another Tower.
- [ ] Select the Builder: "Total kills" starts at zero and includes the kills
  from all that player's towers, even destroyed towers.
- [ ] Destroy a Tower with kills, then select its Builder: the total stays.
  A killing arrow that was in flight when the Tower died must still be credited.
- [ ] In a guest match, verify the assigned player's Builder lifetime total
  and selected Tower count match the host after snapshots and deltas.
  Another player's tower kill totals must not be sent or displayed.
- [ ] Verify that ending or canceling construction clears the progress display.

## Headless/automated checks

From a configured build environment, run:

```sh
make -j$(nproc)
bin/run test openage::input::game::tests::tower_placement_mode
bin/run test openage::presenter::tests::hud_mouse_capture
bin/run test openage::gamestate::tests::fvd_construction -a --have-assets
bin/run test openage::gamestate::tests::fvd_hunter_combat -a --have-assets
bin/run test openage::network::tests::replication
make tests
make checkmerge
```

The simulation cases require the FVD modpack and converted engine assets.
The Qt Quick mouse hit-test and button delivery must be exercised with the
graphical client; controller-only tests cannot prove that events were routed
through the QML scene.
