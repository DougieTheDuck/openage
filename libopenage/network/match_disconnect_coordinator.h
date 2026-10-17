// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

#include "network/connection_mailbox.h"
#include "network/command_mailbox.h"
#include "network/match_connection_state.h"
#include "network/world_delta.h"
#include "time/time.h"

namespace openage::gamestate {
class GameState;
}

namespace openage::network {

/**
 * Simulation-thread owner of mid-match connection closures and notifications.
 *
 * Connection threads enqueue ConnectionClosed and consume their own outbound
 * mailbox. The authoritative simulation thread alone calls process_closures,
 * so ownership, world revisions and disconnect broadcasts stay ordered.
 *
 * OutboundMailbox references must outlive this coordinator or be unregistered
 * after the session is closed. An overflowing outbound mailbox is fatal to the
 * host session: continuing would silently diverge the affected peer.
 */
class MatchDisconnectCoordinator {
public:
	explicit MatchDisconnectCoordinator(MatchConnectionState &connections);

	/** Bind a remote session and its dedicated outbound mailbox. */
	void register_remote(const SessionIdentity &identity, OutboundMailbox &outbound);

	/** Seed a recipient's baseline when its full snapshot is transmitted. */
	void record_snapshot(WorldState snapshot);

	/** Diff the newest authoritative capture against that recipient's last sent state. */
	WorldDelta make_delta(WorldState complete);

	/** Check a command again when the simulation drains the command mailbox. */
	bool accepts_command(const AuthenticatedCommand &command) const;

	/**
	 * Drain lifecycle events before newly arrived gameplay commands.
	 *
	 * Each effective closure advances world_revision exactly once, queues one
	 * notification per surviving remote, then a contiguous authoritative delta
	 * containing the updated persistent participant-connection status.
	 * Duplicate closure events from the same session are ignored.
	 *
	 * @return Number of newly disconnected sessions (not duplicate events).
	 * @throws Error for unknown/mismatched sessions, revision overflow, or
	 * outbound backpressure. Callers must end the host session on fatal error.
	 */
	size_t process_closures(ConnectionEventMailbox &events,
	                        const std::shared_ptr<gamestate::GameState> &state,
	                        const time::time_t &simulation_time,
	                        uint64_t &world_revision,
	                        size_t maximum_events);

private:
	struct Remote {
		SessionIdentity identity;
		OutboundMailbox *outbound;
		WorldDeltaBaseline baseline{};
	};

	MatchConnectionState &connections;
	std::map<gamestate::player_id_t, Remote> remotes;
};

} // namespace openage::network
