// Copyright 2026-2026 the openage authors. See copying.md for legal info.

#include "fvd_minimap.h"

#include "testing/testing.h"

namespace openage::presenter::tests {

namespace {

network::EntityState at(gamestate::entity_id_t id, gamestate::player_id_t owner,
                        const std::string &kind, int ne, int se) {
	network::EntityState entity{};
	entity.entity_id = id;
	entity.owner_id = owner;
	entity.definition = "fvd.data.entities." + kind;
	entity.position = coord::phys3{coord::phys_t::from_int(ne) + coord::phys_t{0.5},
	                               coord::phys_t::from_int(se) + coord::phys_t{0.5}, 0};
	return entity;
}

QRgb tile_color(const QImage &image, uint32_t size, int ne, int se) {
	return image.pixel(se * MINIMAP_TILE_PIXELS,
	                   (static_cast<int>(size) - 1 - ne) * MINIMAP_TILE_PIXELS);
}

} // namespace

void fvd_minimap() {
	network::WorldState world{};
	world.map_size = 8;
	world.entities = {at(1, 3, "Tower", 2, 5), at(2, 4, "Builder", 6, 1),
	                  at(3, 4, "Foundation", 0, 0)};
	std::vector<uint8_t> fog(64, 255);
	fog[0 * 8 + 7] = 0;   // ne 0, se 7 unexplored
	fog[7 * 8 + 7] = 128; // ne 7, se 7 explored only

	const auto image = render_minimap(world, fog);
	TESTEQUALS(image.width(), 8 * MINIMAP_TILE_PIXELS);
	TESTEQUALS(tile_color(image, 8, 2, 5), minimap_player_color(3).rgb());
	TESTEQUALS(tile_color(image, 8, 6, 1), minimap_player_color(4).lighter(135).rgb());
	// Units are drawn as a 2x2-tile dot.
	TESTEQUALS(tile_color(image, 8, 5, 2), minimap_player_color(4).lighter(135).rgb());
	TESTEQUALS(tile_color(image, 8, 0, 0), minimap_player_color(4).darker(160).rgb());
	TESTEQUALS(tile_color(image, 8, 0, 7), qRgb(0, 0, 0));
	const auto explored = QColor{tile_color(image, 8, 7, 7)};
	const auto visible = tile_color(image, 8, 4, 4);
	(explored.value() < QColor{visible}.value() and explored.value() > 0) or TESTFAIL;

	// Without fog data everything is visible.
	TESTEQUALS(tile_color(render_minimap(world, {}), 8, 0, 7), visible);

	// Clicks map back to the tile they were drawn on, and stay on the map.
	const auto [x, y] = minimap_position_to_pixel(world.entities[0].position, 8);
	const auto tile = minimap_pixel_to_tile(x, y, 8);
	TESTEQUALS(tile.ne, 2);
	TESTEQUALS(tile.se, 5);
	const auto outside = minimap_pixel_to_tile(-50, 999, 8);
	TESTEQUALS(outside.ne, 0);
	TESTEQUALS(outside.se, 0);
}

} // namespace openage::presenter::tests
