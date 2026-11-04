// Copyright 2026 the openage authors. See copying.md for legal info.

#include "renderer/stages/world/render_stage.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include <QWindow>

#include "config.h"
#include "coord/phys.h"
#include "coord/scene.h"
#include "renderer/camera/camera.h"
#include "renderer/gui/integration/public/gui_application_with_logger.h"
#include "renderer/render_pass.h"
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/resources/assets/asset_manager.h"
#include "renderer/resources/texture_data.h"
#include "renderer/stages/camera/manager.h"
#include "renderer/stages/terrain/render_entity.h"
#include "renderer/stages/terrain/render_stage.h"
#include "renderer/stages/world/render_entity.h"
#include "renderer/window.h"
#include "testing/testing.h"
#include "time/clock.h"
#include "time/time.h"
#include "util/fslike/directory.h"
#include "util/path.h"


namespace openage::renderer::world::tests {

namespace {

constexpr size_t TEST_TARGET_WIDTH = 512;
constexpr size_t TEST_TARGET_HEIGHT = 768;
constexpr size_t SAMPLE_FRAME_X = 100;
constexpr size_t SAMPLE_FRAME_Y = 300;
constexpr size_t SAMPLE_SIZE = 10;
constexpr size_t FRONT_FRAME_X = 139;
constexpr size_t FRONT_FRAME_Y = 69;

const uint8_t *pixel(const resources::Texture2dData &image,
                     size_t x,
                     size_t y) {
	return image.get_data() + y * image.get_info().get_row_size() + x * 4;
}

} // namespace

void structure_shadow_depth() {
	auto root = util::Path{
		std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto gui_app = std::make_shared<gui::GuiApplicationWithLogger>();
	window_settings settings{};
	settings.width = TEST_TARGET_WIDTH;
	settings.height = TEST_TARGET_HEIGHT;
	settings.vsync = false;
	auto window = Window::create("openage structure shadow test", settings);
	window->get_qt_window()->setVisible(false);
	auto graphics_renderer = window->make_renderer();
	auto camera = std::make_shared<renderer::camera::Camera>(
		graphics_renderer, window->get_size());
	// Match the tower's 0.5 animation scale so one frame pixel maps to one
	// framebuffer pixel. The centered anchor puts its 512px frame at (0, 0).
	camera->set_zoom(0.5f);
	camera->look_at_coord(coord::scene3{0, 0, 0});
	auto camera_manager = std::make_shared<renderer::camera::CameraManager>(camera);
	camera_manager->update();
	auto assets = std::make_shared<resources::AssetManager>(
		graphics_renderer, root / "assets" / "fvd" / "graphics");
	auto clock = std::make_shared<time::Clock>();
	auto stage = std::make_shared<WorldRenderStage>(
		window, graphics_renderer, camera, root / "assets" / "shaders", assets, clock);

	auto render = [&](bool rear_enabled, bool front_enabled, bool team_coloured = false) {
		stage->clear_entities();
		auto add_tower = [&](uint32_t id, const coord::phys3 &position) {
			auto entity = std::make_shared<RenderEntity>();
			entity->update(id, position, "tower.sprite", time::TIME_ZERO);
			if (team_coloured) {
				// Exercise the player-owned world2d shader uniform path.
				// Existing neutral art must still render with no palette copies.
				entity->set_occlusion_mode(occlusion_mode::OCCLUDER, 2);
			}
			stage->add_render_entity(entity);
		};
		if (rear_enabled) {
			add_tower(1, {0, 0, 0});
		}
		if (front_enabled) {
			// Increasing SE by one tile projects towards screen bottom-right.
			add_tower(2, {0, 1, 0});
		}
		stage->update(time::TIME_ZERO);
		graphics_renderer->render(stage->get_render_pass());
		return stage->get_render_pass()->get_target()->into_data();
	};

	auto rear = render(true, false);
	auto combined = render(true, true);
	auto owned = render(true, false, true);
	TESTEQUALS(owned.get_info().get_row_size(), rear.get_info().get_row_size());
	auto luminance_on_white = [](const uint8_t *value) {
		const auto red = static_cast<double>(value[0]) + 255.0 - value[3];
		const auto green = static_cast<double>(value[1]) + 255.0 - value[3];
		const auto blue = static_cast<double>(value[2]) + 255.0 - value[3];
		return 0.2126 * red + 0.7152 * green + 0.0722 * blue;
	};
	double rear_luminance = 0.0;
	double combined_luminance = 0.0;
	// Sample the front frame at (100, 300), translated by its one-tile
	// bottom-right displacement into the shared framebuffer.
	const auto sample_x = FRONT_FRAME_X + SAMPLE_FRAME_X;
	const auto sample_y = FRONT_FRAME_Y + SAMPLE_FRAME_Y;
	for (size_t y = sample_y; y < sample_y + SAMPLE_SIZE; ++y) {
		for (size_t x = sample_x; x < sample_x + SAMPLE_SIZE; ++x) {
			// Texture targets are read from OpenGL's bottom-left origin, while
			// frame offsets use the asset's top-left origin.
			const auto target_y = TEST_TARGET_HEIGHT - 1 - y;
			rear_luminance += luminance_on_white(pixel(rear, x, target_y));
			combined_luminance += luminance_on_white(pixel(combined, x, target_y));
		}
	}
	const auto sample_pixels = static_cast<double>(SAMPLE_SIZE * SAMPLE_SIZE);
	rear_luminance /= sample_pixels;
	combined_luminance /= sample_pixels;
	TESTNOTEQUALS(rear_luminance, 0.0);
	const auto darkening = (rear_luminance - combined_luminance) / rear_luminance;
	if (darkening > 0.20) {
		TESTFAILMSG(__FILE__ << ":" << __LINE__
		                     << ": front tower shadow darkened the rear sample by "
		                     << darkening * 100.0 << "%");
	}

	// Render the smaller Builder at native texture resolution so this test
	// measures team-colour coverage independently of its gameplay scale.
	camera->set_zoom(0.1375f);
	camera_manager->update();
	auto render_builder = [&](uint64_t owner) {
		stage->clear_entities();
		auto entity = std::make_shared<RenderEntity>();
		entity->update(1, coord::phys3{0, 0, 0}, "builder.sprite", time::TIME_ZERO);
		entity->set_occlusion_mode(occlusion_mode::NONE, owner);
		stage->add_render_entity(entity);
		stage->update(time::TIME_ZERO);
		graphics_renderer->render(stage->get_render_pass());
		return stage->get_render_pass()->get_target()->into_data();
	};
	auto blue_builder = render_builder(2);
	auto green_builder = render_builder(3);
	size_t recoloured_pixels = 0;
	for (size_t y = 0; y < TEST_TARGET_HEIGHT; ++y) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
			const auto blue = pixel(blue_builder, x, y);
			const auto green = pixel(green_builder, x, y);
			if (blue[0] == green[0]
			    and blue[1] == green[1]
			    and blue[2] == green[2]) {
				continue;
			}
			++recoloured_pixels;
		}
	}
	TESTEQUALS(recoloured_pixels > 800, true);
	window->close();
}

// Render the shader itself, not a CPU approximation of its blend. Existing
// per-tile fog data must produce dark interiors and soft tile-edge transitions.
void terrain_fog_soft_edges() {
	auto root = util::Path{
		std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto gui_app = std::make_shared<gui::GuiApplicationWithLogger>();
	window_settings settings{};
	settings.width = TEST_TARGET_WIDTH;
	settings.height = TEST_TARGET_HEIGHT;
	settings.vsync = false;
	auto window = Window::create("openage softened terrain fog test", settings);
	window->get_qt_window()->setVisible(false);
	auto graphics_renderer = window->make_renderer();
	auto camera = std::make_shared<renderer::camera::Camera>(
		graphics_renderer, window->get_size());
	camera->set_zoom(1.0f);
	camera->look_at_coord(coord::scene3{1.5, 1.5, 0});
	auto camera_manager = std::make_shared<renderer::camera::CameraManager>(camera);
	camera_manager->update();
	auto assets = std::make_shared<resources::AssetManager>(
		graphics_renderer, root / "assets" / "fvd" / "graphics");
	auto clock = std::make_shared<time::Clock>();
	auto stage = std::make_shared<terrain::TerrainRenderStage>(
		window, graphics_renderer, camera, root / "assets" / "shaders", assets, clock);

	terrain::RenderEntity::tiles_t tiles{};
	for (int i = 0; i < 9; ++i) {
		tiles.emplace_back(terrain::RenderEntity::terrain_elevation_t{0}, "grass.terrain");
	}
	auto entity = std::make_shared<terrain::RenderEntity>();
	entity->update(util::Vector2s{3, 3}, tiles, time::TIME_ZERO);
	stage->add_render_entity(entity, util::Vector2s{3, 3}, coord::scene2_delta{0, 0});

	auto render = [&](const std::vector<uint8_t> &fog_levels, float edge_width) {
		stage->set_fog(3, fog_levels);
		stage->set_fog_edge_width(edge_width);
		stage->update();
		graphics_renderer->render(stage->get_render_pass());
		return stage->get_render_pass()->get_target()->into_data();
	};
	const std::vector<uint8_t> all_visible(9, 255);
	// Fog rows are NE (height), columns SE (width): first SE column visible.
	const std::vector<uint8_t> revealed{
		255, 0, 0,
		255, 0, 0,
		255, 0, 0,
	};
	auto full = render(all_visible, 0.55f);
	auto soft = render(revealed, 0.55f);
	auto hard = render(revealed, 0.0f);
	auto sample = [camera](const resources::Texture2dData &image,
	                       float se, float ne) {
		const Eigen::Vector3f world = coord::scene3{ne, se, 0}.to_world_space();
		const auto matrix = camera->get_projection_matrix() * camera->get_view_matrix();
		const Eigen::Vector4f projected = matrix * Eigen::Vector4f{
			world.x(), world.y(), world.z(), 1.0f};
		const auto x = static_cast<size_t>(std::clamp(
			std::lround((projected.x() / projected.w() * 0.5 + 0.5) * TEST_TARGET_WIDTH),
			0l, static_cast<long>(TEST_TARGET_WIDTH - 1)));
		const auto y = static_cast<size_t>(std::clamp(
			std::lround((projected.y() / projected.w() * 0.5 + 0.5) * TEST_TARGET_HEIGHT),
			0l, static_cast<long>(TEST_TARGET_HEIGHT - 1)));
		const uint8_t *rgba = pixel(image, x, y);
		return 0.2126 * rgba[0] + 0.7152 * rgba[1] + 0.0722 * rgba[2];
	};
	const double full_center = sample(full, 0.5f, 1.5f);
	const double full_edge = sample(full, 1.0f, 1.5f);
	const double full_hidden = sample(full, 1.5f, 1.5f);
	TESTEQUALS(full_center > 0.0, true);
	TESTEQUALS(full_edge > 0.0, true);
	TESTEQUALS(full_hidden > 0.0, true);
	// Fog softness doesn't wash out well inside visible and unseen tiles.
	TESTEQUALS(sample(soft, 0.5f, 1.5f) > full_center * 0.95, true);
	TESTEQUALS(sample(soft, 1.5f, 1.5f) < full_hidden * 0.05, true);
	// Between different tile centres, the new shader renders a partial fade.
	TESTEQUALS(sample(soft, 1.0f, 1.5f) > full_edge * 0.20, true);
	TESTEQUALS(sample(soft, 1.0f, 1.5f) < full_edge * 0.85, true);
	// A width of zero restores the earlier hard tile boundary.
	TESTEQUALS(sample(hard, 0.88f, 1.5f) > full_edge * 0.95, true);
	TESTEQUALS(sample(hard, 1.12f, 1.5f) < full_edge * 0.05, true);
	window->close();
}

// Selection rings must appear under selected movable units and disappear
// immediately on deselection or when the unit is hidden by fog.
void selection_ground_rings() {
	auto root = util::Path{
		std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto gui_app = std::make_shared<gui::GuiApplicationWithLogger>();
	window_settings settings{};
	settings.width = TEST_TARGET_WIDTH;
	settings.height = TEST_TARGET_HEIGHT;
	settings.vsync = false;
	auto window = Window::create("openage selection ring test", settings);
	window->get_qt_window()->setVisible(false);
	auto graphics_renderer = window->make_renderer();
	auto camera = std::make_shared<renderer::camera::Camera>(
		graphics_renderer, window->get_size());
	camera->set_zoom(0.5f);
	camera->look_at_coord(coord::scene3{0, 0, 0});
	auto camera_manager = std::make_shared<renderer::camera::CameraManager>(camera);
	camera_manager->update();
	auto assets = std::make_shared<resources::AssetManager>(
		graphics_renderer, root / "assets" / "fvd" / "graphics");
	auto clock = std::make_shared<time::Clock>();
	auto stage = std::make_shared<WorldRenderStage>(
		window, graphics_renderer, camera, root / "assets" / "shaders", assets, clock);

	auto builder = std::make_shared<RenderEntity>();
	builder->update(17, coord::phys3{0, 0, 0}, "builder.sprite", time::TIME_ZERO);
	builder->set_occlusion_mode(occlusion_mode::OUTLINED, 2);
	stage->add_render_entity(builder);
	auto draw = [&] {
		stage->update(time::TIME_ZERO);
		graphics_renderer->render(stage->get_render_pass());
		return stage->get_render_pass()->get_target()->into_data();
	};
	stage->set_selected_unit_rings({}, 0.0f);
	auto plain = draw();
	stage->set_selected_unit_rings({17}, 0.5f);
	auto ringed = draw();
	size_t ring_pixels = 0;
	for (size_t y = 0; y < TEST_TARGET_HEIGHT; ++y) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
			const auto *a = pixel(plain, x, y);
			const auto *b = pixel(ringed, x, y);
			if (a[0] != b[0] or a[1] != b[1]
			    or a[2] != b[2] or a[3] != b[3]) {
				++ring_pixels;
			}
		}
	}
	TESTEQUALS(ring_pixels > size_t{40}, true);

	// No stale renderable persists after the selection layer is replaced.
	stage->set_selected_unit_rings({}, 0.0f);
	auto deselected = draw();
	for (size_t y = 0; y < TEST_TARGET_HEIGHT; y += 7) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; x += 7) {
			const auto *a = pixel(plain, x, y);
			const auto *b = pixel(deselected, x, y);
			TESTEQUALS(a[0] == b[0] and a[1] == b[1]
			           and a[2] == b[2] and a[3] == b[3], true);
		}
	}

	stage->set_selected_unit_rings({17}, 0.5f);
	stage->set_shown(std::unordered_set<uint32_t>{});
	auto hidden = draw();
	size_t shown_pixels = 0;
	for (size_t y = 0; y < TEST_TARGET_HEIGHT; ++y) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
			if (pixel(hidden, x, y)[3] > 5) ++shown_pixels;
		}
	}
	TESTEQUALS(shown_pixels == size_t{0}, true);
	window->close();
}

// Adjacent towers must overlap like AoE2 buildings: whatever is nearer the
// camera is in front, whatever order the towers were built in.
void adjacent_tower_draw_order() {
	auto root = util::Path{
		std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto gui_app = std::make_shared<gui::GuiApplicationWithLogger>();
	window_settings settings{};
	settings.width = TEST_TARGET_WIDTH;
	settings.height = TEST_TARGET_HEIGHT;
	settings.vsync = false;
	auto window = Window::create("openage tower draw order test", settings);
	window->get_qt_window()->setVisible(false);
	auto graphics_renderer = window->make_renderer();
	auto camera = std::make_shared<renderer::camera::Camera>(graphics_renderer, window->get_size());
	camera->set_zoom(1.0f);
	camera->look_at_coord(coord::scene3{1, 1, 0});
	auto camera_manager = std::make_shared<renderer::camera::CameraManager>(camera);
	camera_manager->update();
	auto assets = std::make_shared<resources::AssetManager>(
		graphics_renderer, root / "assets" / "fvd" / "graphics");
	auto clock = std::make_shared<time::Clock>();
	auto stage = std::make_shared<WorldRenderStage>(
		window, graphics_renderer, camera, root / "assets" / "shaders", assets, clock);

	using positions_t = std::vector<coord::phys3>;
	auto render_positions = [&](const positions_t &positions) {
		stage->clear_entities();
		uint32_t id = 1;
		for (const auto &position : positions) {
			auto entity = std::make_shared<RenderEntity>();
			entity->update(id++, position, "tower.sprite", time::TIME_ZERO);
			entity->set_occlusion_mode(occlusion_mode::OCCLUDER, 2);
			stage->add_render_entity(entity);
		}
		stage->update(time::TIME_ZERO);
		graphics_renderer->render(stage->get_render_pass());
		return stage->get_render_pass()->get_target()->into_data();
	};
	using tiles_t = std::vector<std::pair<int, int>>;
	auto render = [&](const tiles_t &tiles) {
		positions_t positions;
		positions.reserve(tiles.size());
		for (auto [ne, se] : tiles) {
			positions.emplace_back(
				coord::phys_t::from_int(ne) + coord::phys_t{0.5},
				coord::phys_t::from_int(se) + coord::phys_t{0.5}, 0);
		}
		return render_positions(positions);
	};
	auto same = [](const resources::Texture2dData &a, const resources::Texture2dData &b) {
		for (size_t y = 0; y < TEST_TARGET_HEIGHT; ++y) {
			for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
				if (std::memcmp(pixel(a, x, y), pixel(b, x, y), 4) != 0) return false;
			}
		}
		return true;
	};

	// A 3x3 block and a row of equal-depth towers, built in both orders.
	tiles_t grid{{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {2, 0}, {2, 1}, {2, 2}};
	tiles_t row{{0, 0}, {1, 1}, {2, 2}};
	for (const auto &tiles : {grid, row}) {
		auto reversed = tiles;
		std::reverse(reversed.begin(), reversed.end());
		same(render(tiles), render(reversed)) or TESTFAILMSG("tower overlap depends on build order");
	}

	// The nearer tower (higher SE) is drawn completely over the rear one,
	// even when it was built first.
	const auto front_alone = render({{0, 1}});
	const auto both = render({{0, 1}, {0, 0}});
	size_t front_pixels = 0;
	size_t covered = 0;
	for (size_t y = 0; y < TEST_TARGET_HEIGHT; ++y) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
			const auto *front = pixel(front_alone, x, y);
			if (front[3] < 250) continue;
			++front_pixels;
			if (std::memcmp(front, pixel(both, x, y), 4) == 0) ++covered;
		}
	}
	(front_pixels > 10000) or TESTFAIL;
	if (covered < front_pixels * 99 / 100) {
		TESTFAILMSG(__FILE__ << ":" << __LINE__ << ": rear tower drawn over the front one ("
		                     << front_pixels - covered << " of " << front_pixels << " pixels)");
	}

	// Equal-depth towers are ordered left to right. Picking must use that same
	// tie-break instead of whichever tower happened to be built last.
	const coord::phys3 pick_rear_position{coord::phys_t{0.5}, coord::phys_t{0.5}, 0};
	const coord::phys3 pick_front_position{coord::phys_t{0.75}, coord::phys_t{0.75}, 0};
	const auto pick_rear = render_positions({pick_rear_position});
	const auto pick_front = render_positions({pick_front_position});
	std::optional<coord::input> overlap;
	for (size_t y = 0; y < TEST_TARGET_HEIGHT and not overlap.has_value(); ++y) {
		for (size_t x = 0; x < TEST_TARGET_WIDTH; ++x) {
			if (pixel(pick_front, x, y)[3] >= 250 and pixel(pick_rear, x, y)[3] >= 250) {
				overlap = coord::input{
					static_cast<coord::pixel_t>(x),
					static_cast<coord::pixel_t>(TEST_TARGET_HEIGHT - 1 - y)};
				break;
			}
		}
	}
	overlap.has_value() or TESTFAILMSG("adjacent towers have no opaque overlap to pick");
	for (const auto &positions : {positions_t{pick_front_position, pick_rear_position},
	                              positions_t{pick_rear_position, pick_front_position}}) {
		render_positions(positions);
		std::optional<uint32_t> picked;
		stage->pick_entity(*overlap, [&](std::optional<uint32_t> id) {
			picked = id;
		});
		const uint32_t front_id = positions.front() == pick_front_position ? 1 : 2;
		TESTEQUALS(picked.value_or(0), front_id);
	}
	window->close();
}

} // namespace openage::renderer::world::tests
