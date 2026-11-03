// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <QColor>
#include <QImage>

#include "network/messages.h"

namespace openage::presenter {

/** Minimap pixels per map tile, so a one-tile tower is still visible. */
constexpr int MINIMAP_TILE_PIXELS = 2;

/**
 * Player colours for the minimap; keep aligned with flagColor() in
 * assets/fvd/qml/main.qml and the world outline palette.
 */
QColor minimap_player_color(gamestate::player_id_t player);

/**
 * Paint the minimap for one viewer, top-down in tile space:
 * column = SE, row = map_size - 1 - NE, so that QML rotating the image 45°
 * clockwise and halving its height matches the isometric camera.
 *
 * Only what the viewer may see is drawn: the world must already be the
 * recipient's (fog-filtered) state, and `fog_levels` is the viewer's
 * per-tile fog (0 unexplored, 128 explored, 255 visible, index
 * ne * map_size + se). Without fog everything counts as visible.
 *
 * - unexplored: black; explored: dark grass; visible: grass
 * - towers, foundations and the Hunter tower: owner colour (foundations dimmer,
 *   fog memories dimmer still)
 * - Builders and horses: owner colour, brightened, as a 2x2-tile dot
 */
QImage render_minimap(const network::WorldState &world, const std::vector<uint8_t> &fog_levels);

/**
 * Tile under a minimap image pixel (as returned by mapping a click into the
 * rotated image item). Clamped to the map.
 */
coord::tile minimap_pixel_to_tile(double x, double y, uint32_t map_size);

/** Image pixel for a world position (inverse of minimap_pixel_to_tile). */
std::pair<double, double> minimap_position_to_pixel(const coord::phys3 &position, uint32_t map_size);

} // namespace openage::presenter
