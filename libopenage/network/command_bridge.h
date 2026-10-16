// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <functional>
#include <memory>

#include "gamestate/types.h"
#include "network/messages.h"

namespace openage::event {
class EventLoop;
}

namespace openage::gamestate {
class GameState;

namespace event {
class Commander;
}
}

namespace openage::network {

/** Result callback invoked when the authoritative host decides the command. */
using command_result_callback_t = std::function<void(bool)>;

/**
 * Translate a player-independent network intent into a simulation event.
 *
 * Ownership is supplied by the authenticated connection and never read from
 * the payload.
 */
void submit_command_intent(
	const CommandIntent &intent,
	gamestate::player_id_t assigned_player,
	const std::shared_ptr<::openage::event::EventLoop> &event_loop,
	const std::shared_ptr<gamestate::event::Commander> &commander,
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	command_result_callback_t result_callback);

/** Capture authoritative positions for replication at a simulation time. */
WorldState capture_positions(
	const std::shared_ptr<gamestate::GameState> &state,
	const std::vector<gamestate::entity_id_t> &entity_ids,
	const time::time_t &time,
	uint64_t base_revision,
	uint64_t world_revision);

} // namespace openage::network
