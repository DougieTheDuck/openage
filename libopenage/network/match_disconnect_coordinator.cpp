// Copyright 2026 the openage authors. See copying.md for legal info.

#include "match_disconnect_coordinator.h"

#include <limits>
#include <utility>

#include "error/error.h"
#include "network/command_bridge.h"

namespace openage::network {

MatchDisconnectCoordinator::MatchDisconnectCoordinator(
	MatchConnectionState &connections) :
	connections{connections} {}

void MatchDisconnectCoordinator::register_remote(
	const SessionIdentity &identity, OutboundMailbox &outbound) {
	if (this->remotes.contains(identity.player_id)) {
		throw Error{MSG(err) << "FVD participant already registered with an outbound mailbox"};
	}
	this->connections.bind_remote(identity);
	this->remotes.emplace(identity.player_id, Remote{identity, &outbound});
}

bool MatchDisconnectCoordinator::accepts_command(
	const AuthenticatedCommand &command) const {
	return this->connections.accepts_commands(command.identity);
}

size_t MatchDisconnectCoordinator::process_closures(
	ConnectionEventMailbox &events,
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &simulation_time,
	uint64_t &world_revision,
	size_t maximum_events) {
	size_t disconnected_count = 0;
	for (const auto &closed : events.drain(maximum_events)) {
		auto remote = this->remotes.find(closed.identity.player_id);
		if (remote == this->remotes.end()
		    or remote->second.identity.session_id != closed.identity.session_id
		    or remote->second.identity.player_id != closed.identity.player_id) {
			throw Error{MSG(err) << "FVD connection closure names an unknown session"};
		}
		if (not this->connections.accepts_commands(closed.identity)) {
			// Multiple terminal socket errors may report the same closure.
			// A repeated event must not increment world revision again.
			continue;
		}
		if (world_revision == std::numeric_limits<uint64_t>::max()) {
			throw Error{MSG(err) << "FVD world revision exhausted during disconnect"};
		}
		// No silent loss: check space for BOTH the notification and delta
		// before changing authoritative state. The connection thread only
		// drains its outbound mailbox, never adds messages.
		for (const auto &[player_id, recipient] : this->remotes) {
			if (player_id == closed.identity.player_id
			    or not this->connections.accepts_commands(recipient.identity)) {
				continue;
			}
			auto pending = recipient.outbound->size();
			auto capacity = recipient.outbound->capacity();
			if (capacity - pending < 2) {
				throw Error{MSG(err) << "FVD remote outbound mailbox cannot carry "
				                     "disconnect notification and world delta"};
			}
		}

		auto notification = this->connections.disconnect_remote(
			closed.identity, closed.reason);
		const auto base_revision = world_revision;
		++world_revision;
		for (const auto &[player_id, recipient] : this->remotes) {
			if (not this->connections.accepts_commands(recipient.identity)) {
				continue;
			}
			auto delta = capture_fvd_state(
				state, this->connections, simulation_time,
				base_revision, world_revision, player_id);
			if (not recipient.outbound->try_push(notification)
			    or not recipient.outbound->try_push(WorldDeltaMessage{std::move(delta)})) {
				throw Error{MSG(err) << "FVD outbound mailbox overflow during disconnect"};
			}
		}
		++disconnected_count;
	}
	return disconnected_count;
}

} // namespace openage::network
