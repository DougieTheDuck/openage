// Copyright 2019-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>

#include "coord/pixel.h"
#include "presenter/hud_mouse_capture.h"
#include <vector>

#include "renderer/window.h"
#include "util/path.h"


class QQmlPropertyMap;
class QPointF;

namespace qtgui {
class GuiApplication;
}

namespace openage {

namespace gamestate {
class GameSimulation;
}

namespace input {
class InputManager;

namespace game {
class Controller;
}
}

namespace time {
class TimeLoop;
}

namespace network {
class ReplicationTracker;
class RelayLobbyClient;
}

namespace renderer {
class RenderPass;
class Renderer;
class Texture2d;
class ShaderProgram;
class Window;

namespace camera {
class Camera;
class CameraManager;
} // namespace camera

namespace gui {
class GUI;
}

namespace hud {
class HudRenderStage;
}

namespace screen {
class ScreenRenderStage;
}

namespace skybox {
class SkyboxRenderStage;
}

namespace terrain {
class TerrainRenderStage;
}

namespace world {
class WorldRenderStage;
}

namespace resources {
class AssetManager;
}

} // namespace renderer

namespace presenter {

class Presenter {
public:
	/**
	 * Create a new presenter.
	 *
	 * @param path Root directory path.
	 * @param simulation Game simulation. Can be set later with \p set_engine()
	 * @param time_loop Time loop which controls simulation time. Can be set later with \p set_time_loop()
	 */
	Presenter(const util::Path &path,
	          const std::shared_ptr<gamestate::GameSimulation> &simulation = nullptr,
	          const std::shared_ptr<time::TimeLoop> &time_loop = nullptr);

	~Presenter();

	/**
	 * Start the presenter and initialize subsystems.
	 *
	 * @param window_settings The settings to customize the display window (e.g. size, display mode, vsync).
	 */
	void run(const renderer::window_settings window_settings = {});

	/**
	 * Request that the presenter exits its draw loop.
	 *
	 * This can be called from another thread while the presenter is starting or
	 * running.
	 */
	void stop();

	/**
	 * Set the game simulation controlled by this presenter.
	 *
	 * @param simulation Game simulation.
	 */
	void set_simulation(const std::shared_ptr<gamestate::GameSimulation> &simulation);

	/**
	 * Set the time loop controlled by this presenter.
	 *
	 * @param time_loop Time loop.
	 */
	void set_time_loop(const std::shared_ptr<time::TimeLoop> &time_loop);

	/** Bind the recipient-specific network stream before starting the presenter. */
	void set_fvd_replication(const std::shared_ptr<network::ReplicationTracker> &replication);

	/** Present the relay lobby by default (solo CLI skips it). */
	void set_fvd_lobby_enabled(bool enabled);

	/**
	 * Initialize the Qt application managing the graphical views. Required
	 * for creating windows.
	 *
	 * @returns Pointer to openage's application wrapper,
	 */
	static std::shared_ptr<qtgui::GuiApplication> init_window_system();

protected:
	/**
	 * Initialize all graphics subsystems of the presenter, i.e.
	 *     - window creation
	 *     - main renderer
	 *     - component renderers (Terrain, Game Entities, GUI)
	 */
	void init_graphics(const renderer::window_settings &window_settings = {});

	/**
	 * Initialize the GUI.
	 */
	void init_gui();

	/**
	 * Initialize the input management.
	 */
	void init_input();

	/**
	 * Initialize the final render pass that renders the results of all previous
	 * render passes to the window screen.
	 */
	void init_final_render_pass();

	// void init_audio();

	/**
	 * Render all configured render passes in sequence.
	 */
	void render();

	/** Qt offscreen input needs presenter-level click handling. */
	std::string fvd_lobby_action_at(const QPointF &position) const;
	void handle_fvd_lobby_action(const std::string &action);
	void refresh_fvd_lobby_state();

	// TODO: remove and move into our config/settings system
	util::Path root_dir;

	// graphis components
	/**
	 * Windowing GUI Application wrapper.
	 */
	std::shared_ptr<qtgui::GuiApplication> gui_app;

	/**
	 * Display window.
	 */
	std::shared_ptr<renderer::Window> window;

	/**
	 * openage's graphics renderer.
	 */
	std::shared_ptr<renderer::Renderer> renderer;

	/**
	 * Camera for viewing things.
	 */
	std::shared_ptr<renderer::camera::Camera> camera;

	/**
	 * Qt-based GUI for interface.
	 */
	std::shared_ptr<renderer::gui::GUI> gui;

	/** Qt bridge for FVD player HUD state and build-button actions. */
	std::shared_ptr<QQmlPropertyMap> fvd_hud_state;
	bool fvd_lobby_enabled{false};
	std::shared_ptr<QQmlPropertyMap> fvd_lobby_state;
	std::unique_ptr<network::RelayLobbyClient> fvd_lobby_client;
	std::string fvd_lobby_pressed{};
	std::string fvd_lobby_local_error{};
	bool fvd_lobby_hosting{false};
	bool fvd_bootstrap_requested{false};
	bool fvd_bootstrap_supplied{false};
	/** Guest HUD source; local-only matches continue to read their simulation. */
	std::shared_ptr<network::ReplicationTracker> fvd_replication;
	/** Most recent screen cursor coordinate, owned by the presenter thread. */
	std::optional<coord::input> placement_cursor;
	/** Prevent HUD gestures from leaking game orders through the panel. */
	HudMouseCapture fvd_hud_capture{};

	/**
	 * Camera manager for camera controls.
	 */
	std::shared_ptr<renderer::camera::CameraManager> camera_manager;

	/**
	 * Graphics output for the map background.
	 */
	std::shared_ptr<renderer::skybox::SkyboxRenderStage> skybox_renderer;

	/**
	 * Graphics output for terrain.
	 */
	std::shared_ptr<renderer::terrain::TerrainRenderStage> terrain_renderer;

	/**
	 * Graphics output for units/buildings.
	 */
	std::shared_ptr<renderer::world::WorldRenderStage> world_renderer;

	/**
	 * Graphics output for the HUD.
	 */
	std::shared_ptr<renderer::hud::HudRenderStage> hud_renderer;

	/**
	 * Final graphics output to the window screen.
	 */
	std::shared_ptr<renderer::screen::ScreenRenderStage> screen_renderer;

	/**
	 * Manager for loading/storing asset resources.
	 */
	std::shared_ptr<renderer::resources::AssetManager> asset_manager;

	/**
	 * Render passes in the openage renderer.
	 */
	std::vector<std::shared_ptr<renderer::RenderPass>> render_passes;

	/**
	 * Game simulation.
	 */
	std::shared_ptr<gamestate::GameSimulation> simulation;

	/**
	 * Time loop.
	 */
	std::shared_ptr<time::TimeLoop> time_loop;

	/**
	 * Input manager.
	 */
	std::shared_ptr<input::InputManager> input_manager;

	/**
	 * Controller whose current selection is displayed by the HUD.
	 */
	std::shared_ptr<input::game::Controller> game_controller;

	/**
	 * Set when another engine subsystem requests shutdown.
	 */
	std::atomic<bool> stop_requested{false};
};

} // namespace presenter
} // namespace openage
