# Design notes

- `presenter/fvd_minimap.{h,cpp}` paints a top-down image of the recipient
  world (2 px per tile; column = SE, row = map_size - 1 - NE) using the same
  per-tile fog levels as the terrain. The presenter sends it to QML as a PNG
  data URL, repainting at most 4 times a second and only when the world's
  simulation time changed (a 128-tile map is a 256x256 image).
- QML rotates the image 45° and halves its height, which matches the
  isometric camera, so no separate diamond math is needed. The camera box is
  the four window corners projected onto the ground, drawn on a Canvas in
  the same transformed item.
- Clicks: the presenter maps the window position into the transformed item
  (`GUI::map_to_item`, which applies the QML transforms), converts the pixel
  to a tile and centres the camera there. Dragging keeps following until the
  button is released. The panel counts as HUD, so it never selects units.
- Colours come from the same player palette as the HUD flags.

## Not done

- Right-click on the minimap to order units there (AoE2 allows it).
- Flare stars on the minimap (with `builder-flare`).
