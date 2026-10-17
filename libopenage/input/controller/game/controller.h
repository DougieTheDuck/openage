// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <vector>

#include "coord/pixel.h"
#include "curve/discrete.h"
#include "gamestate/types.h"
#include "input/event.h"


namespace openage {

namespace gamestate {
class GameSimulation;
}

namespace time {
class TimeLoop;
}

namespace renderer::world {
class RenderEntity;
}

namespace input::game {

class BindingContext;

/** Kind of the currently selected FVD entity, as resolved by game simulation. */
enum class SelectionKind {
	NONE,
	BUILDER,
	TOWER,
	HORSE,
	GROUP_HORSES,
	OTHER,
};

/** A controller-owned snapshot; health is read from its render entity. */
struct SelectionDetails {
	std::vector<gamestate::entity_id_t> ids;
	SelectionKind kind = SelectionKind::NONE;
	std::shared_ptr<renderer::world::RenderEntity> render_entity;
};

/**
 * Interface for game controllers.
 *
 * Controllers handle inputs from outside of a game (e.g. GUI, AI, scripts, ...)
 * and pass the resulting events to game entities. They also act as a form of
 * access control for using in-game functionality of game entities.
 */
class Controller : public std::enable_shared_from_this<Controller> {
public:
	Controller(const std::unordered_set<size_t> &controlled_factions,
	           size_t active_faction_id);

	~Controller() = default;

	/**
	 * Switch the actively controlled faction by the controller.
	 * The ID must be in the list of controlled factions.
	 *
	 * @param faction_id ID of the new active faction.
	 */
	void set_control(size_t faction_id);

	/**
	 * Get the ID of the faction actively controlled by the controller.
	 *
	 * @return ID of the active faction.
	 */
	size_t get_controlled() const;

	/**
	 * Get the currently selected entities.
	 *
	 * @return Selected entities.
	 */
	std::vector<gamestate::entity_id_t> get_selected() const;

	/**
	 * Set the currently selected entities.
	 *
	 * @param ids Selected entities.
	 */
	void set_selected(
		const std::vector<gamestate::entity_id_t> ids,
		SelectionKind kind = SelectionKind::NONE,
		const std::shared_ptr<renderer::world::RenderEntity> &render_entity = nullptr);

	/** Start an asynchronous selection update and return its generation. */
	uint64_t begin_selection_update();

	/** Apply a selection result only if it is the latest pending update. */
	bool set_selected_if_current(
		uint64_t generation,
		const std::vector<gamestate::entity_id_t> ids,
		SelectionKind kind = SelectionKind::NONE,
		const std::shared_ptr<renderer::world::RenderEntity> &render_entity = nullptr);

	/**
	 * Defer an input action while the latest selection update is pending.
	 *
	 * Only the most recent deferred action is retained.
	 *
	 * @return Whether the action was deferred.
	 */
	bool defer_until_selection_applied(std::function<void()> action);

	/** Get the selected units and resolved type as a single thread-safe snapshot. */
	SelectionDetails get_selection_details() const;

	/** Enter FVD tower placement mode when a Builder has been selected. */
	bool begin_tower_placement();

	/** Cancel a tower placement without issuing a world command. */
	void cancel_tower_placement();

	/** Prepare a human Hunter attack-move destination from the next map click. */
	bool begin_attack_move();
	void cancel_attack_move();
	bool is_attack_move_active() const;

	/** Whether the next normal map click should place a tower. */
	bool is_tower_placement_active() const;

	/**
	 * Process an input event from the input manager.
	 *
	 * @param ev_args Input event and arguments.
	 * @param ctx Binding context for looking up the event transformation.
	 *
	 * @return true if the event is accepted, else false.
	 */
	bool process(const event_arguments &ev_args, const std::shared_ptr<BindingContext> &ctx);

	/**
	 * Set the start position of a drag selection.
	 *
	 * @param start Start position of the drag selection.
	 */
	void set_drag_select_start(const coord::input &start);

	/**
	 * Get the start position of a drag selection.
	 *
	 * @return Start position of the drag selection.
	 */
	const coord::input get_drag_select_start() const;

	/**
	 * Reset the drag select start position.
	 */
	void reset_drag_select();

private:
	/**
	 * Factions controllable by this controller.
	 */
	std::unordered_set<size_t> controlled_factions;

	/**
	 * ID of the currently active faction.
	 */
	size_t active_faction_id;

	/**
	 * Currently selected entities.
	 */
	std::vector<gamestate::entity_id_t> selected;

	/** Simulation-resolved selection kind and thread-safe health source. */
	SelectionKind selected_kind = SelectionKind::NONE;
	std::shared_ptr<renderer::world::RenderEntity> selected_render_entity;
	uint64_t selection_generation = 0;
	bool selection_pending = false;
	std::function<void()> deferred_selection_action{};

	/** Controller-local input mode, not authoritative simulation state. */
	bool placing_tower = false;
	bool attacking = false;

	/**
	 * Start position of a drag selection.
	 *
	 * TODO: Move this into an input event.
	 */
	std::optional<coord::input> drag_select_start;

	/**
	 * Mutex for threaded access.
	 */
	mutable std::recursive_mutex mutex;
};

/**
 * Setup default controller action bindings:
 *
 * - CTRL + Left Mouse click: Create game entity.
 * - Right Mouse click: Move game entity.
 *
 * @param ctx Binding context the actions are added to.
 * @param time_loop Time loop for getting simulation time.
 * @param simulation Game simulation.
 * @param camera Active game camera.
 * @param pick_entity Callback for finding the visible entity under a window pixel.
 */
void setup_defaults(const std::shared_ptr<BindingContext> &ctx,
                    const std::shared_ptr<time::TimeLoop> &time_loop,
                    const std::shared_ptr<openage::gamestate::GameSimulation> &simulation,
                    const std::shared_ptr<renderer::camera::Camera> &camera,
	                const std::function<void(
		                const coord::input &,
		                const std::function<void(std::optional<gamestate::entity_id_t>)> &)> &pick_entity);

} // namespace input::game
} // namespace openage
