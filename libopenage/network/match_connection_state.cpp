// Copyright 2026 the openage authors. See copying.md for legal info.

#include "match_connection_state.h"

#include <algorithm>

#include "error/error.h"

namespace openage::network {

MatchConnectionState::MatchConnectionState(size_t player_count) :
	participants{player_count} {
	if (player_count == 0 or player_count > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "FVD match connection state requires between 1 and "
		                     << MAX_STATE_PLAYERS << " players"};
	}
}

void MatchConnectionState::bind_remote(const SessionIdentity &identity) {
	if (identity.session_id == 0 or identity.player_id == 0
	    or identity.player_id > this->participants.size()) {
		throw Error{MSG(err) << "FVD match connection binding has an invalid identity"};
	}
	if (std::any_of(
		    this->participants.begin(), this->participants.end(),
		    [&](const auto &participant) {
			    return participant.remote_identity
			       and participant.remote_identity->session_id == identity.session_id;
		    })) {
		throw Error{MSG(err) << "FVD session " << identity.session_id
		                     << " is already bound to a match participant"};
	}

	auto &participant = this->participants.at(identity.player_id - 1);
	if (not participant.connected) {
		throw Error{MSG(err) << "Disconnected FVD player " << identity.player_id
		                     << " cannot be assigned another session"};
	}
	if (participant.remote_identity) {
		throw Error{MSG(err) << "FVD player " << identity.player_id
		                     << " already has a bound remote session"};
	}
	participant.remote_identity = identity;
}

bool MatchConnectionState::accepts_commands(const SessionIdentity &identity) const {
	if (identity.session_id == 0 or identity.player_id == 0
	    or identity.player_id > this->participants.size() or not this->is_match_active()) {
		return false;
	}

	const auto &participant = this->participants.at(identity.player_id - 1);
	return participant.connected
	   and participant.remote_identity
	   and participant.remote_identity->session_id == identity.session_id
	   and participant.remote_identity->player_id == identity.player_id;
}

ParticipantDisconnected MatchConnectionState::disconnect_remote(
	const SessionIdentity &identity,
	DisconnectReason reason) {
	if (not is_known_disconnect_reason(reason)) {
		throw Error{MSG(err) << "Unknown FVD disconnect reason "
		                     << static_cast<uint16_t>(reason)};
	}
	if (not this->accepts_commands(identity)) {
		throw Error{MSG(err) << "FVD disconnect does not match an active session"};
	}

	this->participants.at(identity.player_id - 1).connected = false;
	return {identity.player_id, reason};
}

bool MatchConnectionState::is_connected(
	gamestate::player_id_t player_id) const {
	if (player_id == 0 or player_id > this->participants.size()) {
		throw Error{MSG(err) << "FVD player " << player_id
		                     << " is outside the match participant range"};
	}
	return this->participants.at(player_id - 1).connected;
}

bool MatchConnectionState::is_match_active() const {
	// Player 1 is always the Hunter. Other players may disconnect without
	// ending the match; only Hunter loss makes further orders invalid.
	return this->participants.front().connected;
}

size_t MatchConnectionState::get_player_count() const {
	return this->participants.size();
}

} // namespace openage::network
