// Copyright 2013-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <shared_mutex>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <cstdint>
#include <vector>
#include "network/messages.h"
#include "network/command_mailbox.h"

#include "util/path.h"

namespace openage {

namespace assets {
class ModManager;
}

namespace cvar {
class CVarManager;
}

namespace event {
class EventLoop;
} // namespace event

namespace renderer {
class RenderFactory;
}

namespace time {
class TimeLoop;
} // namespace time

namespace gamestate {
class EntityFactory;
class Game;
class TerrainFactory;

namespace event {
class Commander;
class Spawner;
} // namespace event

/**
 * Gameplay subsystem of the engine.
 *
 * Manages the state of the game world and the event loop.
 */
class GameSimulation final {
public:
	/**
	 * Create the game simulation subsystems depending on the requested run mode.
	 *
	 * @param root_dir openage root directory.
	 * @param cvar_manager Environment variable manager.
	 * @param time_loop Time management loop.
	 */
	GameSimulation(const util::Path &root_dir,
	               const std::shared_ptr<cvar::CVarManager> &cvar_manager,
	               const std::shared_ptr<openage::time::TimeLoop> time_loop);

	// game simulation should not be copied or moved
	GameSimulation(const GameSimulation &copy) = delete;
	GameSimulation &operator=(const GameSimulation &copy) = delete;
	GameSimulation(GameSimulation &&other) = delete;
	GameSimulation &operator=(GameSimulation &&other) = delete;
	~GameSimulation() = default;

	/**
	 * Run the simulation loop.
	 */
	void run();

	/**
	 * Start the simulation loop.
	 */
	void start();

	/**
	 * Stop of the simulation loop.
	 */
	void stop();

	/**
	 * Get the root directory of the simulation executable.
	 */
	const util::Path &get_root_dir();

	/**
	 * Get this simulation's cvar manager.
	 *
	 * @return CVarManager instance.
	 */
	const std::shared_ptr<cvar::CVarManager> get_cvar_manager();

	/**
	 * Get the game running in the simulation.
	 *
	 * @return Game instance.
	 */
	const std::shared_ptr<gamestate::Game> get_game();

	/**
	 * Get the event loop for the gamestate.
	 *
	 * @return Event loop.
	 */
	const std::shared_ptr<openage::event::EventLoop> get_event_loop();

	/**
	 * Get the event entity for spawing game entities.
	 *
	 * TODO: Move somewhere else or remove.
	 *
	 * @return Spawner for entity creation.
	 */
	const std::shared_ptr<gamestate::event::Spawner> get_spawner();

	/**
	 * Get the event entity for sending commands.
	 *
	 * TODO: Move somewhere else or remove.
	 *
	 * @return Commander for sending commands.
	 */
	const std::shared_ptr<gamestate::event::Commander> get_commander();

	/**
	 * Attach a renderer to the simulation.
	 *
	 * @param factory Factory for creating render entities.
	 */
	void attach_renderer(const std::shared_ptr<renderer::RenderFactory> &render_factory);

	/**
	 * Set the modpacks to load for a game.
	 *
	 * @param modpacks IDs of the modpacks to load.
	 */
	void set_modpacks(const std::vector<std::string> &modpacks);

	/**
	 * Request an authoritative initial FVD snapshot, captured by the
	 * simulation thread after completing its event loop at time zero.
	 * The lobby must remain paused while this request is processed.
	 */
	void request_network_bootstrap(gamestate::player_id_t recipient);

	/** Bind snapshots to the admitted players, rather than eight scenario slots. */
	void configure_network_participants(size_t admitted_players);
	/** Persist a departed Builder's connection status in later snapshots. */
	void report_network_participant_disconnect(gamestate::player_id_t player);

	/** Retrieve the completed bootstrap snapshot, if ready. */
	std::optional<network::WorldState> take_network_bootstrap();

	/**
	 * Queue a later recipient-specific state capture on the simulation thread.
	 * The caller allocates increasing revisions (starting at 2 after bootstrap).
	 * Only one request/result may be outstanding to bound memory use.
	 */
	void request_network_update(gamestate::player_id_t recipient, uint64_t revision);
	std::optional<network::WorldState> take_network_update();

	/** Enqueue guest input for owner-thread authentication and execution. */
	bool submit_network_command(network::AuthenticatedCommand command,
	                            std::function<void(bool)> result_callback,
	                            std::function<bool()> session_is_active = {});

	/**
	 * Recreate the FVD game and its event/entity factories on the owning
	 * simulation thread after the multiplayer clock has reset to paused zero.
	 * The presenter must clear renderer stages before calling this, then
	 * reattach a new render factory after consume_match_reset_completed().
	 */
	void request_match_reset();
	bool consume_match_reset_completed();

	/** Invalidate any pending/ready captures when the user leaves the lobby. */
	void cancel_network_bootstrap();

	/**
	 * current simulation state variable.
	 * to be set to false to stop the simulation loop.
	 */
	bool running;

private:
	/**
	 * Initialize event handlers.
	 */
	void init_event_handlers();

	/**
	 * The simulation root directory.
	 * Uses the openage fslike path abstraction that can mount paths into one.
	 *
	 * This means that this path does simultaneously lead to global assets,
	 * home-folder-assets, settings, and basically the whole filesystem access.
	 *
	 * TODO: move this to a settings class, which then also hosts cvar and the options system.
	 */
	util::Path root_dir;

	/**
	 * the simulation's cvar manager.
	 */
	std::shared_ptr<cvar::CVarManager> cvar_manager;

	/**
	 * Time loop for getting the current simulation time and changing speed.
	 */
	std::shared_ptr<openage::time::TimeLoop> time_loop;

	/**
	 * Event loop for processing events in the game.
	 */
	std::shared_ptr<openage::event::EventLoop> event_loop;

	/**
	 * Factory for creating game entities.
	 */
	std::shared_ptr<gamestate::EntityFactory> entity_factory;

	/**
	 * Factory for creating terrain.
	 */
	std::shared_ptr<gamestate::TerrainFactory> terrain_factory;

	/**
	 * Mod manager.
	 */
	std::shared_ptr<assets::ModManager> mod_manager;

	// TODO: move somewhere sensible or remove
	std::shared_ptr<gamestate::event::Spawner> spawner;
	std::shared_ptr<gamestate::event::Commander> commander;

	// TODO: The game run by the engine
	std::shared_ptr<gamestate::Game> game;

	/**
	 * Mutex for thread-safe access to the simulation.
	 */
	std::shared_mutex mutex;

	std::mutex bootstrap_mutex;
	struct BootstrapRequest {
		gamestate::player_id_t recipient;
		uint64_t generation;
	};
	uint64_t bootstrap_generation{0};
	size_t network_admitted_players{0};
	std::vector<gamestate::player_id_t> network_disconnected_players{};
	std::optional<BootstrapRequest> bootstrap_requested{};
	std::optional<network::WorldState> bootstrap_ready{};
	struct UpdateRequest {
		gamestate::player_id_t recipient;
		uint64_t revision;
		uint64_t generation;
	};
	std::optional<UpdateRequest> update_requested{};
	std::optional<network::WorldState> update_ready{};
	std::mutex network_command_mutex;
	struct NetworkCommandRequest {
		network::AuthenticatedCommand command;
		std::function<void(bool)> callback;
		std::function<bool()> session_is_active;
	};
	std::deque<NetworkCommandRequest> network_commands{};

	std::mutex reset_mutex;
	bool reset_requested{false};
	bool reset_completed{false};
};

} // namespace gamestate
} // namespace openage
