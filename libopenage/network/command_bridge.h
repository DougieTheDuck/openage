// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <functional>
#include <memory>

#include "gamestate/types.h"
#include "network/command_mailbox.h"
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
 * Translate an authenticated network command into a simulation event.
 *
 * Ownership comes from the immutable accepted-session identity paired with
 * the intent by CommandMailbox.
 */
void submit_authenticated_command(
	const AuthenticatedCommand &command,
	const std::shared_ptr<::openage::event::EventLoop> &event_loop,
	const std::shared_ptr<gamestate::event::Commander> &commander,
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	command_result_callback_t result_callback);

/** Capture complete authoritative FVD state at a simulation time. */
WorldState capture_fvd_state(
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	uint64_t base_revision,
	uint64_t world_revision);

} // namespace openage::network
