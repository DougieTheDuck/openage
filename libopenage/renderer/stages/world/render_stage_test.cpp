// Copyright 2026 the openage authors. See copying.md for legal info.

#include "renderer/stages/world/render_stage.h"

#include <cstddef>
#include <cstdint>
#include <memory>

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

	auto render = [&](bool rear_enabled, bool front_enabled) {
		stage->clear_entities();
		auto add_tower = [&](uint32_t id, const coord::phys3 &position) {
			auto entity = std::make_shared<RenderEntity>();
			entity->update(id, position, "tower.sprite", time::TIME_ZERO);
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
	window->close();
}

} // namespace openage::renderer::world::tests
