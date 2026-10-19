// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "network/messages.h"
#include "network/session.h"

namespace openage::network {

/**
 * Authoritative connection status for the participants in one running match.
 *
 * The simulation thread owns this state. Connection threads report closures
 * through ConnectionEventMailbox; the simulation thread applies them here
 * before publishing the next world revision.
 */
class MatchConnectionState {
public:
	/** Create connected participant slots for one started match. */
	explicit MatchConnectionState(size_t player_count);

	/** Bind one accepted remote session to its fixed participant slot. */
	void bind_remote(const SessionIdentity &identity);

	/** Whether this exact accepted session may still submit commands. */
	bool accepts_commands(const SessionIdentity &identity) const;

	/**
	 * Mark an accepted remote session disconnected.
	 *
	 * The returned notification is sent once to every remaining connection.
	 */
	ParticipantDisconnected disconnect_remote(
		const SessionIdentity &identity,
		DisconnectReason reason);

	/** Whether a participant is still connected to the running match. */
	bool is_connected(gamestate::player_id_t player_id) const;

	/** Hunter/player 1 is mandatory; no remote orders after Hunter loss. */
	bool is_match_active() const;

	/** Number of fixed participant slots in the running match. */
	size_t get_player_count() const;

private:
	struct ParticipantConnection {
		bool connected{true};
		std::optional<SessionIdentity> remote_identity{};
	};

	std::vector<ParticipantConnection> participants;
};

} // namespace openage::network
