// Copyright 2019-2024 the openage authors. See copying.md for legal info.

#include "presenter.h"

#include <eigen3/Eigen/Dense>
#include <algorithm>
#include <QEvent>
#include <QMouseEvent>
#include <QVariantList>
#include <QVariantMap>
#include <QQmlPropertyMap>
#include <QVariant>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <string>
#include <vector>

#include "error/error.h"
#include "gamestate/fvd.h"
#include "gamestate/game.h"
#include "gamestate/game_state.h"
#include "gamestate/simulation.h"
#include "input/controller/camera/binding_context.h"
#include "input/controller/camera/controller.h"
#include "input/controller/game/binding_context.h"
#include "input/controller/game/controller.h"
#include "input/controller/hud/binding_context.h"
#include "input/controller/hud/controller.h"
#include "input/input_context.h"
#include "input/input_manager.h"
#include "log/log.h"
#include "network/replication.h"
#include "network/lobby_relay_client.h"
#include "renderer/camera/camera.h"
#include "renderer/gui/gui.h"
#include "renderer/gui/guisys/public/gui_input.h"
#include "renderer/gui/integration/public/gui_application_with_logger.h"
#include "renderer/render_factory.h"
#include "renderer/render_pass.h"
#include "renderer/render_target.h"
#include "renderer/resources/assets/asset_manager.h"
#include "renderer/resources/shader_source.h"
#include "renderer/resources/texture_info.h"
#include "renderer/stages/camera/manager.h"
#include "renderer/stages/hud/render_stage.h"
#include "renderer/stages/screen/render_stage.h"
#include "renderer/stages/skybox/render_stage.h"
#include "renderer/stages/terrain/render_stage.h"
#include "renderer/stages/world/render_entity.h"
#include "renderer/stages/world/render_stage.h"
#include "time/clock.h"
#include "time/time_loop.h"
#include "util/path.h"
#include "util/stall_profiler.h"


namespace openage::presenter {

Presenter::Presenter(const util::Path &root_dir,
                     const std::shared_ptr<gamestate::GameSimulation> &simulation,
                     const std::shared_ptr<time::TimeLoop> &time_loop) :
	root_dir{root_dir},
	render_passes{},
	simulation{simulation},
	time_loop{time_loop},
	game_controller{nullptr} {}

Presenter::~Presenter() = default;

void Presenter::set_fvd_lobby_enabled(bool enabled) {
	this->fvd_lobby_enabled = enabled;
	if (enabled) {
		this->fvd_lobby_client = std::make_unique<network::RelayLobbyClient>();
	}
}


void Presenter::run(const renderer::window_settings window_settings) {
	log::log(INFO << "Presenter: Launching subsystems...");

	this->init_graphics(window_settings);

	this->init_input();

	while (not this->stop_requested and not this->window->should_close()) {
		{
			util::StallProfiler profile{"presenter.process_events"};
			this->gui_app->process_events();
		}

		{
			util::StallProfiler profile{"presenter.render"};
			this->render();
		}

		{
			util::StallProfiler profile{"presenter.check_renderer_error"};
			this->renderer->check_error();
		}

		{
			util::StallProfiler profile{"presenter.window_update"};
			this->window->update();
		}
	}

	log::log(MSG(info) << "Presenter: Draw loop exited");

	if (this->simulation) {
		this->simulation->stop();
	}

	if (this->time_loop) {
		this->time_loop->stop();
	}

	this->window->close();
}

void Presenter::stop() {
	this->stop_requested = true;
}

void Presenter::set_simulation(const std::shared_ptr<gamestate::GameSimulation> &simulation) {
	this->simulation = simulation;
	auto render_factory = std::make_shared<renderer::RenderFactory>(
		this->terrain_renderer, this->world_renderer, this->hud_renderer);
	this->simulation->attach_renderer(render_factory);
}

void Presenter::set_time_loop(const std::shared_ptr<time::TimeLoop> &time_loop) {
	this->time_loop = time_loop;
}

void Presenter::set_fvd_replication(const std::shared_ptr<network::ReplicationTracker> &replication) {
	this->fvd_replication = replication;
}

std::shared_ptr<qtgui::GuiApplication> Presenter::init_window_system() {
	return std::make_shared<renderer::gui::GuiApplicationWithLogger>();
}

void Presenter::init_graphics(const renderer::window_settings &window_settings) {
	log::log(INFO << "Presenter: Initializing graphics subsystems...");

	// Start up rendering framework
	this->gui_app = this->init_window_system();

	// Window and renderer
	this->window = renderer::Window::create("openage presenter test", window_settings);
	this->renderer = this->window->make_renderer();

	// Asset mangement
	this->asset_manager = std::make_shared<renderer::resources::AssetManager>(
		this->renderer,
		this->root_dir / "assets" / "converted");
	auto missing_tex = this->root_dir / "assets" / "test" / "textures" / "test_missing.sprite";
	this->asset_manager->set_placeholder_animation(missing_tex);

	// Camera
	this->camera = std::make_shared<renderer::camera::Camera>(this->renderer, this->window->get_size());
	this->camera->look_at_coord(coord::scene3{10.0, 10.0, 0}); // Center camera on the map
	this->window->add_resize_callback([this](size_t w, size_t h, double /*scale*/) {
		this->camera->resize(w, h);
	});

	// Camera manager
	this->camera_manager = std::make_shared<renderer::camera::CameraManager>(this->camera);
	// TODO: Make boundaries dynamic based on map size.
	this->camera_manager->set_camera_boundaries(
		renderer::camera::CameraBoundaries{
			renderer::camera::X_BOUND_MIN,
			renderer::camera::X_BOUND_MAX,
			renderer::camera::Y_BOUND_MIN,
			renderer::camera::Y_BOUND_MAX,
			renderer::camera::Z_BOUND_MIN,
			renderer::camera::Z_BOUND_MAX});

	// Skybox
	this->skybox_renderer = std::make_shared<renderer::skybox::SkyboxRenderStage>(
		this->window,
		this->renderer,
		this->root_dir["assets"]["shaders"]);
	this->skybox_renderer->set_color(1.0f, 0.5f, 0.0f, 1.0f);
	this->render_passes.push_back(this->skybox_renderer->get_render_pass());

	// Terrain
	this->terrain_renderer = std::make_shared<renderer::terrain::TerrainRenderStage>(
		this->window,
		this->renderer,
		this->camera,
		this->root_dir["assets"]["shaders"],
		this->asset_manager,
		this->time_loop->get_clock());
	this->render_passes.push_back(this->terrain_renderer->get_render_pass());

	// Units/buildings
	this->world_renderer = std::make_shared<renderer::world::WorldRenderStage>(
		this->window,
		this->renderer,
		this->camera,
		this->root_dir["assets"]["shaders"],
		this->asset_manager,
		this->time_loop->get_clock());
	this->render_passes.push_back(this->world_renderer->get_render_pass());

	// HUD
	this->hud_renderer = std::make_shared<renderer::hud::HudRenderStage>(
		this->window,
		this->renderer,
		this->camera,
		this->root_dir["assets"]["shaders"],
		this->asset_manager,
		this->time_loop->get_clock());
	this->render_passes.push_back(this->hud_renderer->get_render_pass());

	this->init_gui();
	this->init_final_render_pass();

	if (this->simulation) {
		auto render_factory = std::make_shared<renderer::RenderFactory>(
			this->terrain_renderer, this->world_renderer, this->hud_renderer);
		this->simulation->attach_renderer(render_factory);
		auto fvd = this->simulation->get_game()->get_state()->get_fvd();
		if (fvd) {
			this->terrain_renderer->set_grid_enabled(true);
			this->camera->look_at_coord(fvd->get_centre().to_scene3());
			auto size = fvd->get_centre().ne.to_float() * 2;
			auto eye = this->camera->get_position();
			auto centre = fvd->get_centre().to_scene3().to_world_space();
			auto offset = eye - centre;
			this->camera_manager->set_camera_boundaries({
				offset.x(), offset.x() + size, offset.y(), offset.y(),
				offset.z() - size, offset.z()});
			this->skybox_renderer->set_color(0.08f, 0.12f, 0.08f, 1.0f);
		}
	}

	log::log(INFO << "Presenter: Graphics subsystems initialized");
}

void Presenter::init_gui() {
	log::log(INFO << "Presenter: Initializing GUI with Qt backend");

	//// -- gui initialization
	// TODO: Do not use test GUI
	util::Path qml_root = this->root_dir / "assets" / "test" / "qml";
	if (this->simulation and this->simulation->get_game()->get_state()->get_fvd()) {
		qml_root = this->root_dir / "assets" / "fvd" / "qml";
	}
	log::log(INFO << "Presenter: Setting QML root to " << qml_root.resolve_native_path());
	if (not qml_root.is_dir()) {
		throw Error{ERR << "could not find qml root folder " << qml_root};
	}

	util::Path qml_assets = this->root_dir / "assets";
	log::log(INFO << "Presenter: Setting QML asset path to " << qml_assets.resolve_native_path());
	if (not qml_assets.is_dir()) {
		throw Error{ERR << "could not find asset root folder " << qml_assets};
	}

	util::Path qml_root_file = qml_root / "main.qml";
	log::log(INFO << "Presenter: Setting QML root file to " << qml_root_file.resolve_native_path());
	if (not qml_root_file.is_file()) {
		throw Error{ERR << "could not find main.qml file " << qml_root_file};
	}

	// TODO: in order to support qml-mods, the fslike and filelike
	//       library has to be integrated into qt. For now,
	//       figure out the absolute paths here and pass them in.

	this->gui = std::make_shared<renderer::gui::GUI>(
		this->gui_app, // Qt application wrapper
		this->window,  // window for the gui
		qml_root_file, // entry qml file, absolute path.
		qml_root,      // directory to watch for qml file changes
		qml_assets,    // qml data: Engine *, the data directory, ...
		this->renderer // openage renderer
	);

	auto gui_pass = this->gui->get_render_pass();
	this->render_passes.push_back(gui_pass);
}

std::string Presenter::fvd_lobby_action_at(const QPointF &position) const {
	if (not this->gui) return {};
	for (const auto &[item, action] : {
		std::pair{"fvdLobbyHostButton", "host"},
		std::pair{"fvdLobbyJoinButton", "join"},
		std::pair{"fvdLobbyStartButton", "start"},
		std::pair{"fvdLobbyBackButton", "disconnect"},
	}) {
		if (this->gui->contains_item(QString::fromUtf8(item), position)) {
			return action;
		}
	}
	return {};
}

void Presenter::handle_fvd_lobby_action(const std::string &action) {
	if (not this->fvd_lobby_state or not this->fvd_lobby_client) return;
	if (action == "disconnect") {
		this->fvd_lobby_local_error.clear();
		this->fvd_lobby_client->close();
		return;
	}
	if (action == "start") {
		this->fvd_lobby_client->request_start();
		return;
	}
	if (action != "host" and action != "join") return;
	this->fvd_lobby_local_error.clear();
	try {
		auto val = [&](const char *name) {
			return this->fvd_lobby_state->value(name).toString().trimmed();
		};
		bool ok_port = false;
		bool ok_match = false;
		bool ok_pair = false;
		const auto port = val("port").toUInt(&ok_port);
		const auto match = val("matchId").toULongLong(&ok_match);
		const auto pair = val("pairId").toULongLong(&ok_pair);
		if (not ok_port or port == 0 or port > 65535
		    or not ok_match or match == 0 or not ok_pair or pair == 0) {
			throw std::invalid_argument{"Relay port, match ID and pair ID must be valid positive numbers"};
		}
		network::RelayLobbyConfig config{};
		config.address = val("address").toStdString();
		config.port = static_cast<uint16_t>(port);
		config.match_id = match;
		config.pair_id = pair;
		config.token_hex = val("token").toStdString();
		config.name = val("name").toStdString();
		config.hosting = action == "host";
		this->fvd_lobby_client->connect(std::move(config));
	}
	catch (const std::exception &error) {
		this->fvd_lobby_local_error = error.what();
	}
}

void Presenter::refresh_fvd_lobby_state() {
	if (not this->fvd_lobby_enabled or not this->fvd_lobby_state
	    or not this->fvd_lobby_client) return;
	const auto state = this->fvd_lobby_client->status();
	auto update = [&](const char *key, const QVariant &value) {
		if (this->fvd_lobby_state->value(key) != value) {
			this->fvd_lobby_state->insert(key, value);
		}
	};
	update("phase", QString::fromStdString(
		this->fvd_lobby_local_error.empty() ? state.phase : "error"));
	update("status", QString::fromStdString(
		this->fvd_lobby_local_error.empty() ? state.message : this->fvd_lobby_local_error));
	update("participants", QString::fromStdString(state.participants));
	update("canStart", state.can_start);
}

void Presenter::init_input() {
	log::log(INFO << "Presenter: Initializing input subsystem...");

	this->input_manager = std::make_shared<input::InputManager>();

	this->window->add_key_callback([&](const QKeyEvent &ev) {
		if (this->fvd_lobby_enabled) {
			if (this->gui) {
				this->gui->get_input_handler()->process(
					std::shared_ptr<QEvent>{ev.clone()});
			}
			return;
		}
		this->input_manager->process(ev);
	});
	this->window->add_mouse_button_callback([&](const QMouseEvent &ev) {
		if (this->fvd_lobby_enabled) {
			if (this->gui) {
				this->gui->get_input_handler()->process(
					std::shared_ptr<QEvent>{ev.clone()});
			}
			if (ev.button() == Qt::LeftButton) {
				const auto action = this->fvd_lobby_action_at(ev.position());
				if (ev.type() == QEvent::MouseButtonPress) {
					this->fvd_lobby_pressed = action;
				}
				else if (ev.type() == QEvent::MouseButtonRelease) {
					if (not action.empty() and action == this->fvd_lobby_pressed) {
						this->handle_fvd_lobby_action(action);
					}
					this->fvd_lobby_pressed.clear();
				}
			}
			return;
		}
		const bool over_hud = this->fvd_hud_state and this->gui
		                      and this->gui->contains_item("fvdBuildPanel", ev.position());
		const bool over_action = over_hud and this->gui->contains_item(
			"fvdActionButton", ev.position());
		const auto hud_input = this->fvd_hud_capture.process(
			ev.type(), ev.button(), over_hud, over_action);
		// An offscreen Qt Quick view cannot be relied upon to deliver a
		// MouseArea click. Deliver the panel's action once at the presenter
		// boundary while preventing that gesture from selecting the world.
		if (this->gui) {
			this->gui->get_input_handler()->process(
				std::shared_ptr<QEvent>{ev.clone()});
		}
		if (hud_input.activated_button and this->game_controller) {
			if (this->game_controller->is_tower_placement_active()
			    or this->game_controller->is_attack_move_active()) {
				this->game_controller->cancel_tower_placement();
				this->game_controller->cancel_attack_move();
			}
			else if (this->game_controller->get_controlled() == 1) {
				this->game_controller->begin_attack_move();
			}
			else {
				this->game_controller->begin_tower_placement();
			}
		}
		if (hud_input.consumed) {
			if (this->game_controller) {
				this->game_controller->reset_drag_select();
			}
			return;
		}
		this->input_manager->process(ev);
	});
	this->window->add_mouse_move_callback([&](const QMouseEvent &ev) {
		if (this->fvd_lobby_enabled) {
			if (this->gui) {
				this->gui->get_input_handler()->process(
					std::shared_ptr<QEvent>{ev.clone()});
			}
			return;
		}
		this->placement_cursor = coord::input{
			static_cast<coord::pixel_t>(ev.position().x()),
			static_cast<coord::pixel_t>(ev.position().y())};
		this->input_manager->set_mouse(ev.position().x(), ev.position().y());
		if (this->gui) {
			this->gui->get_input_handler()->process(
				std::shared_ptr<QEvent>{ev.clone()});
		}
		const bool over_hud = this->fvd_hud_state and this->gui
		                      and this->gui->contains_item("fvdBuildPanel", ev.position());
		if (not over_hud and not this->fvd_hud_capture.is_capturing()) {
			this->input_manager->process(ev);
		}
	});
	this->window->add_mouse_wheel_callback([&](const QWheelEvent &ev) {
		if (this->fvd_lobby_enabled) {
			if (this->gui) {
				this->gui->get_input_handler()->process(
					std::shared_ptr<QEvent>{ev.clone()});
			}
			return;
		}
		if (this->gui) {
			this->gui->get_input_handler()->process(
				std::shared_ptr<QEvent>{ev.clone()});
		}
		if (this->fvd_hud_state and this->gui
		    and this->gui->contains_item("fvdBuildPanel", ev.position())) {
			return;
		}
		this->input_manager->process(ev);
	});

	auto input_ctx = this->input_manager->get_global_context();
	input::setup_defaults(input_ctx);

	// setup simulation controls
	if (this->simulation) {
		log::log(INFO << "Loading game simulation controls");

		// TODO: Remove hardcoding
		auto game_controller = std::make_shared<input::game::Controller>(
			std::unordered_set<size_t>{0, 1, 2, 3}, 0);
		auto fvd = this->simulation->get_game()->get_state()->get_fvd();
		if (fvd) {
			std::unordered_set<size_t> players;
			if (this->fvd_replication) {
				auto assigned = this->fvd_replication->get_local_player_id();
				if (assigned > fvd->get_player_count()) {
					throw Error{MSG(err) << "FVD assigned player is outside the current match"};
				}
				players.insert(assigned);
			}
			else {
				for (size_t id = 1; id <= fvd->get_player_count(); ++id) {
					players.insert(id);
				}
			}
			for (size_t id : players) {
				input_ctx->bind(
					input::Event{input::event_class::KEYBOARD, static_cast<int>(Qt::Key_0 + id),
					             Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
					input::input_action{input::input_action_t::GAME});
			}
			game_controller = std::make_shared<input::game::Controller>(
				players, this->fvd_replication
					? this->fvd_replication->get_local_player_id()
					: 2);

			// The global input context must forward placement keys to
			// the FVD-specific game binding context.
			for (auto key : {Qt::Key_B, Qt::Key_A, Qt::Key_Escape}) {
				input_ctx->bind(
					input::Event{input::event_class::KEYBOARD, key,
					             Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
					input::input_action{input::input_action_t::GAME});
			}
		}
		auto engine_context = std::make_shared<input::game::BindingContext>();
		input::game::setup_defaults(
			engine_context, this->time_loop, this->simulation, this->camera,
			[this](
				const coord::input &position,
				const std::function<void(std::optional<gamestate::entity_id_t>)> &callback) {
				this->world_renderer->pick_entity(
					position,
					[callback](std::optional<uint32_t> id) {
						if (id.has_value()) {
							callback(static_cast<gamestate::entity_id_t>(*id));
							return;
						}
						callback(std::nullopt);
					});
			});
		this->input_manager->set_game_controller(game_controller);
		this->game_controller = game_controller;
		input_ctx->set_game_bindings(engine_context);
		if (fvd and this->gui) {
			this->fvd_hud_state = std::make_shared<QQmlPropertyMap>();
			this->fvd_hud_state->insert("controlled", static_cast<int>(game_controller->get_controlled()));
			this->fvd_hud_state->insert("selection", QString{"No unit selected"});
			this->fvd_hud_state->insert("isTower", false);
			this->fvd_hud_state->insert("health", QString{"—"});
			this->fvd_hud_state->insert("damage", QString{"Not defined"});
			this->fvd_hud_state->insert("armor", QString{"Not defined"});
			this->fvd_hud_state->insert("canBuild", false);
			this->fvd_hud_state->insert("placementFootprint", QVariantList{});
			this->fvd_hud_state->insert("placementValid", false);
			this->fvd_hud_state->insert("queuedTowers", 0);
			this->fvd_hud_state->insert("constructionStatus", QString{});
			this->fvd_hud_state->insert("constructionPercent", -1);
			this->fvd_hud_state->insert("canAttack", false);
			this->fvd_hud_state->insert("attacking", false);
			this->fvd_hud_state->insert("placing", false);
			this->fvd_hud_state->insert("kills", 0);
			this->fvd_hud_state->insert("killsAvailable", !this->fvd_replication);
			this->fvd_hud_state->insert("killsLabel", QString{});
			this->fvd_hud_state->insert("arrowStreaks", QVariantList{});
			this->fvd_hud_state->insert("hunterCountdown", 0);
			this->gui->set_context_property("fvdHud", this->fvd_hud_state.get());
			if (this->fvd_lobby_enabled) {
				this->fvd_lobby_state = std::make_shared<QQmlPropertyMap>();
				for (const auto &[key, value] : {
					std::pair{"address", "127.0.0.1"},
					std::pair{"port", "32456"},
					std::pair{"matchId", ""},
					std::pair{"pairId", ""},
					std::pair{"name", "Player"},
					std::pair{"token", ""},
					std::pair{"phase", "menu"},
					std::pair{"status", "Enter the relay pair details supplied by the relay administrator."},
					std::pair{"participants", ""},
				}) {
					this->fvd_lobby_state->insert(key, QString::fromUtf8(value));
				}
				this->fvd_lobby_state->insert("canStart", false);
				this->fvd_lobby_state->insert("enabled", true);
				this->gui->set_context_property("fvdLobby", this->fvd_lobby_state.get());
			}
		}
	}

	// attach GUI if it's initialized
	if (this->gui) {
		log::log(INFO << "Loading GUI controls");
		this->input_manager->set_gui(this->gui->get_input_handler());
	}

	// setup camera controls
	if (this->camera) {
		log::log(INFO << "Loading camera controls");
		auto camera_controller = std::make_shared<input::camera::Controller>();
		auto camera_context = std::make_shared<input::camera::BindingContext>();
		input::camera::setup_defaults(camera_context, this->camera, this->camera_manager);
		this->input_manager->set_camera_controller(camera_controller);
		input_ctx->set_camera_bindings(camera_context);
	}

	// setup HUD controls
	if (this->hud_renderer) {
		log::log(INFO << "Loading HUD controls");
		auto hud_controller = std::make_shared<input::hud::Controller>();
		auto hud_context = std::make_shared<input::hud::BindingContext>();
		input::hud::setup_defaults(hud_context, this->hud_renderer);
		this->input_manager->set_hud_controller(hud_controller);
		input_ctx->set_hud_bindings(hud_context);
	}

	log::log(INFO << "Presenter: Input subsystem initialized");
}

void Presenter::init_final_render_pass() {
	// Final output to window
	this->screen_renderer = std::make_shared<renderer::screen::ScreenRenderStage>(
		this->window,
		this->renderer,
		this->root_dir["assets"]["shaders"]);
	std::vector<std::shared_ptr<renderer::RenderTarget>> targets{};
	for (auto pass : this->render_passes) {
		targets.push_back(pass->get_target());
	}
	this->screen_renderer->set_render_targets(targets);
	this->render_passes.push_back(this->screen_renderer->get_render_pass());

	// Update final render pass if the textures are reassigned on resize
	// TODO: This REQUIRES that all other render passes have already been
	//       resized
	this->window->add_resize_callback([this](size_t, size_t, double /*scale*/) {
		// Acquire the render targets for all previous passes
		std::vector<std::shared_ptr<renderer::RenderTarget>> targets{};
		for (size_t i = 0; i < this->render_passes.size() - 1; ++i) {
			targets.push_back(this->render_passes[i]->get_target());
		}
		this->screen_renderer->set_render_targets(targets);
	});
}

void Presenter::render() {
	this->refresh_fvd_lobby_state();
	// TODO: Pass current time to update() instead of fetching it in renderer
	{
		util::StallProfiler profile{"render.camera_update"};
		this->camera_manager->update();
	}
	{
		util::StallProfiler profile{"render.terrain_update"};
		this->terrain_renderer->update();
	}
	{
		util::StallProfiler profile{"render.world_update"};
		this->world_renderer->update();
	}
	if (this->game_controller) {
		auto selected = this->game_controller->get_selection_details();
		this->hud_renderer->set_selected(selected.ids);
		if (this->fvd_hud_state) {
			auto controlled = this->game_controller->get_controlled();
			auto tower = selected.kind == input::game::SelectionKind::TOWER;
			auto builder = selected.kind == input::game::SelectionKind::BUILDER;
			auto hunter_horse = selected.kind == input::game::SelectionKind::HORSE
			                 or selected.kind == input::game::SelectionKind::GROUP_HORSES;
			QString selection_label{"No unit selected"};
			if (selected.ids.size() > 1) {
				selection_label = QString{"%1 units selected"}.arg(static_cast<int>(selected.ids.size()));
			}
			else if (builder) {
				selection_label = QString{"Builder"};
			}
			else if (tower) {
				selection_label = QString{"Tower"};
			}
			else if (hunter_horse) {
				selection_label = QString{"Horse"};
			}
			else if (not selected.ids.empty()) {
				selection_label = QString{"Unit selected"};
			}

			QString health{"—"};
			if (selected.render_entity and selected.render_entity->is_active()
			    and selected.render_entity->has_health()) {
				auto current = selected.render_entity->get_health_at(
					this->time_loop->get_clock()->get_time());
				health = QString{"%1 / %2"}
				             .arg(static_cast<qlonglong>(current))
				             .arg(selected.render_entity->get_max_health());
			}
			this->fvd_hud_state->insert("controlled", static_cast<int>(controlled));
			this->fvd_hud_state->insert("selection", selection_label);
			this->fvd_hud_state->insert("isTower", tower);
			this->fvd_hud_state->insert("health", health);
			this->fvd_hud_state->insert("canBuild", controlled >= 2 and builder);
			this->fvd_hud_state->insert("canAttack", controlled == 1 and hunter_horse);
			this->fvd_hud_state->insert("attacking", this->game_controller->is_attack_move_active());
			auto fvd = this->simulation->get_game()->get_state()->get_fvd();
			if (fvd) {
				const auto now = this->time_loop->get_clock()->get_time();
				std::vector<gamestate::FVD::ArrowTrail> arrows{};
				if (this->fvd_replication) {
					// Replicate each launch once. The guest animates the flight and
					// hides it at impact_time without an arrow removal packet.
					auto replicated = this->fvd_replication->get_world_state();
					if (replicated) {
						arrows.reserve(replicated->arrows.size());
						for (const auto &arrow : replicated->arrows) {
							if (not network::arrow_visible_at(arrow, now)) {
								continue;
							}
							const auto duration = (arrow.impact_time - arrow.launch_time).to_double();
							if (duration <= 0.0) {
								continue;
							}
							const auto progress = std::clamp(
								(now - arrow.launch_time).to_double() / duration, 0.0, 1.0);
							auto position_at = [&](double fraction) {
								auto lerp = [fraction](coord::phys_t a, coord::phys_t b) {
									return coord::phys_t::from_double(
										a.to_double() + (b.to_double() - a.to_double()) * fraction);
								};
								return coord::phys3{
									lerp(arrow.start.ne, arrow.destination.ne),
									lerp(arrow.start.se, arrow.destination.se),
									lerp(arrow.start.up, arrow.destination.up)
										+ coord::phys_t::from_double(2.0 * fraction * (1.0 - fraction))};
							};
							arrows.push_back({
								position_at(std::max(0.0, progress - 0.22)),
								position_at(progress), progress});
						}
					}
				}
				else {
					arrows = fvd->get_visible_arrows(now);
				}
				QVariantList arrow_streaks;
				arrow_streaks.reserve(arrows.size());
				if (not arrows.empty()) {
					const Eigen::Matrix4f camera_matrix = this->camera->get_projection_matrix()
					                                     * this->camera->get_view_matrix();
					const auto viewport = this->camera->get_viewport_size();
					const auto screen_width = static_cast<float>(viewport[0]);
					const auto screen_height = static_cast<float>(viewport[1]);
					auto project = [&](const coord::phys3 &position) {
						const auto world = position.to_scene3().to_world_space();
						const Eigen::Vector4f clip = camera_matrix
						                           * Eigen::Vector4f{world.x(), world.y(), world.z(), 1.0f};
						return Eigen::Vector2f{
							(clip.x() / clip.w() * 0.5f + 0.5f) * screen_width,
							(0.5f - clip.y() / clip.w() * 0.5f) * screen_height};
					};
					for (const auto &arrow : arrows) {
						auto tail = project(arrow.tail);
						auto head = project(arrow.head);
						// Keep QML Canvas work bounded to visible shots.
						if (not std::isfinite(head.x()) or not std::isfinite(head.y())
						    or (head.x() < -100 and tail.x() < -100)
						    or (head.x() > screen_width + 100 and tail.x() > screen_width + 100)
						    or (head.y() < -100 and tail.y() < -100)
						    or (head.y() > screen_height + 100 and tail.y() > screen_height + 100)) {
							continue;
						}
						arrow_streaks.push_back(QVariantMap{
							{"x0", tail.x()}, {"y0", tail.y()},
							{"x1", head.x()}, {"y1", head.y()},
							{"alpha", std::min(1.0, (1.0 - arrow.progress) * 4.0)}});
					}
				}
				// No Canvas repaint when nothing is on screen.
				if (not arrow_streaks.empty()
				    or not this->fvd_hud_state->value("arrowStreaks").toList().empty()) {
					this->fvd_hud_state->insert("arrowStreaks", arrow_streaks);
				}
				// Preview the exact footprint checked by can_place_tower().
				// The preview is cosmetic; the simulation validates the
				// authoritative command again at its execution time.
				QVariantList footprint;
				bool valid = false;
				if (this->game_controller->is_tower_placement_active()
				    and controlled >= 2 and builder
				    and this->placement_cursor and selected.ids.size() == 1) {
					auto state = this->simulation->get_game()->get_state();
					auto mouse = this->placement_cursor->to_phys3(this->camera);
					auto site = mouse.to_tile();
					valid = fvd->can_place_tower(
						state, static_cast<gamestate::player_id_t>(controlled),
						selected.ids, mouse, now);
					const Eigen::Matrix4f camera_matrix =
						this->camera->get_projection_matrix() * this->camera->get_view_matrix();
					const auto viewport = this->camera->get_viewport_size();
					auto project_tile = [&](double ne, double se) {
						const auto position = coord::phys3{
							coord::phys_t::from_double(ne),
							coord::phys_t::from_double(se), 0};
						const auto world = position.to_scene3().to_world_space();
						const Eigen::Vector4f clip = camera_matrix
							* Eigen::Vector4f{world.x(), world.y(), world.z(), 1.0f};
						return Eigen::Vector2f{
							(clip.x() / clip.w() * 0.5f + 0.5f) * viewport[0],
							(0.5f - clip.y() / clip.w() * 0.5f) * viewport[1]};
					};
					for (const auto &tile : fvd->get_tower_footprint(site)) {
						const auto a = project_tile(tile.ne, tile.se);
						const auto b = project_tile(tile.ne + 1.0, tile.se);
						const auto c = project_tile(tile.ne + 1.0, tile.se + 1.0);
						const auto d = project_tile(tile.ne, tile.se + 1.0);
						footprint.push_back(QVariantMap{
							{"ax", a.x()}, {"ay", a.y()},
							{"bx", b.x()}, {"by", b.y()},
							{"cx", c.x()}, {"cy", c.y()},
							{"dx", d.x()}, {"dy", d.y()}});
					}
				}
				if (not footprint.isEmpty()
				    or not this->fvd_hud_state->value("placementFootprint").toList().empty()) {
					this->fvd_hud_state->insert("placementFootprint", footprint);
				}
				this->fvd_hud_state->insert("placementValid", valid);
				this->fvd_hud_state->insert("queuedTowers",
					builder and selected.ids.size() == 1
						? static_cast<int>(fvd->get_queued_build_count(selected.ids.front()))
						: 0);
				QString construction_status{};
				int construction_percent = -1;
				if (builder and controlled >= 2 and selected.ids.size() == 1) {
					const auto orders = fvd->get_construction_state();
					const auto current = std::find_if(
						orders.begin(), orders.end(),
						[&](const auto &order) {
							return order.builder_id == selected.ids.front()
							       and order.owner == controlled;
						});
					if (current != orders.end()) {
						if (current->phase == gamestate::FVDConstructionPhase::PLANNED) {
							construction_status = QString{"Walking to tower site"};
						}
						else {
							construction_percent = current->progress_percent(now);
							construction_status = QString{"Building: %1%"}.arg(construction_percent);
						}
					}
				}
				this->fvd_hud_state->insert("constructionStatus", construction_status);
				this->fvd_hud_state->insert("constructionPercent", construction_percent);
				auto countdown = fvd->get_hunter_spawn_countdown(
					this->time_loop->get_clock()->get_time());
				this->fvd_hud_state->insert("hunterCountdown",
					countdown.value_or(0));
				QString kills_label{};
				std::optional<uint64_t> visible_kills{};
				if (builder and selected.ids.size() == 1 and controlled >= 2) {
					kills_label = QString{"Total kills: "};
					if (this->fvd_replication) {
						visible_kills = this->fvd_replication->get_local_kills();
					}
					else {
						visible_kills = fvd->get_kills(controlled);
					}
				}
				else if (tower and selected.ids.size() == 1 and controlled >= 2) {
					kills_label = QString{"Tower kills: "};
					if (this->fvd_replication) {
						visible_kills = this->fvd_replication->get_tower_kills(
							selected.ids.front());
					}
					else {
						visible_kills = fvd->get_tower_kills(selected.ids.front(), controlled);
					}
				}
				this->fvd_hud_state->insert("killsLabel", kills_label);
				this->fvd_hud_state->insert("killsAvailable", visible_kills.has_value());
				this->fvd_hud_state->insert("kills",
					static_cast<qulonglong>(visible_kills.value_or(0)));
				this->fvd_hud_state->insert("damage",
					static_cast<qlonglong>(fvd->get_tower_damage()));
				this->fvd_hud_state->insert("armor",
					static_cast<qlonglong>(fvd->get_tower_armor()));
			}
			this->fvd_hud_state->insert(
				"placing", this->game_controller->is_tower_placement_active());
		}
	}
	{
		util::StallProfiler profile{"render.hud_update"};
		this->hud_renderer->update();
	}
	{
		util::StallProfiler profile{"render.gui"};
		this->gui->render();
	}

	{
		util::StallProfiler profile{"render.passes"};
		for (auto &pass : this->render_passes) {
			this->renderer->render(pass);
		}
	}
}

} // namespace openage::presenter
