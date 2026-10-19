// Copyright 2026 the openage authors. See copying.md for legal info.

#include "authority_succession.h"

#include <algorithm>
#include <limits>

#include "error/error.h"
#include "network/lobby.h"

namespace openage::network {

AuthoritySuccession::AuthoritySuccession(
	const StartMatch &assignment, uint64_t original_host_participant_id) {
	if (assignment.host_participant_id != original_host_participant_id) {
		throw Error{MSG(err) << "Authority succession requires the immutable lobby host"};
	}
	// player_for_participant also verifies the complete Hunter/Builder
	// assignment, unique player slots, and exactly one Hunter.
	this->current = {
		1, player_for_participant(assignment, original_host_participant_id), 0};
	this->participants.reserve(assignment.assignments.size());
	for (const auto &entry : assignment.assignments) {
		this->participants.push_back(entry.player_id);
	}
}

gamestate::player_id_t AuthoritySuccession::hunter_player() const {
	return 1;
}

gamestate::player_id_t AuthoritySuccession::authority_player() const {
	return this->current.authority;
}

const AuthorityLease &AuthoritySuccession::lease() const {
	return this->current;
}

bool AuthoritySuccession::host_is_builder() const {
	return this->current.epoch == 1 and this->current.authority != this->hunter_player();
}

AuthorityLossAction AuthoritySuccession::disconnected(gamestate::player_id_t player) {
	const auto it = std::find(this->participants.begin(), this->participants.end(), player);
	if (it == this->participants.end()) {
		throw Error{MSG(err) << "Unknown participant in FVD authority loss decision"};
	}
	this->participants.erase(it);
	this->acknowledgements.erase(
		std::remove(this->acknowledgements.begin(), this->acknowledgements.end(), player),
		this->acknowledgements.end());
	if (player == this->hunter_player()) {
		this->hunter_connected = false;
		this->restored_revision.reset();
		return AuthorityLossAction::END_MATCH;
	}
	if (player != this->current.authority) {
		return AuthorityLossAction::CONTINUE;
	}
	this->original_host_lost = true;
	this->restored_revision.reset();
	return AuthorityLossAction::REQUIRE_RESTORED_CHECKPOINT;
}

void AuthoritySuccession::attest_restored_checkpoint(
	uint64_t source_epoch, uint64_t restored_world_revision,
	bool full_simulation_restored) {
	if (not this->original_host_lost or not this->hunter_connected
	    or this->current.authority == this->hunter_player()
	    or this->current.epoch != source_epoch or restored_world_revision == 0
	    or not full_simulation_restored) {
		throw Error{MSG(err) << "Cannot resume FVD with an unverified or incomplete "
		                     << "simulation checkpoint"};
	}
	if (this->restored_revision
	    and restored_world_revision < *this->restored_revision) {
		throw Error{MSG(err) << "Cannot roll back the FVD takeover checkpoint revision"};
	}
	this->restored_revision = restored_world_revision;
}

AuthorityLease AuthoritySuccession::commit_hunter_takeover() {
	if (not this->hunter_connected or not this->original_host_lost
	    or this->current.authority == this->hunter_player()
	    or not this->restored_revision
	    or this->current.epoch == std::numeric_limits<uint64_t>::max()) {
		throw Error{MSG(err) << "Hunter takeover requires a restored authoritative simulation"};
	}
	this->current = {
		this->current.epoch + 1, this->hunter_player(), *this->restored_revision};
	this->restored_revision.reset();
	this->original_host_lost = false;
	this->acknowledgements.clear();
	return this->current;
}

bool AuthoritySuccession::acknowledge_pregame_rehost(
	SessionIdentity reconnected, uint64_t proposed_epoch) {
	if (this->bootstrap_started or this->original_host_lost
	    or not this->hunter_connected or this->current.authority == 1
	    or this->current.epoch != 1 or proposed_epoch != 2
	    or reconnected.session_id == 0
	    or std::find(this->participants.begin(), this->participants.end(),
	                 reconnected.player_id) == this->participants.end()
	    or this->rehost_session_ids.contains(reconnected.player_id)) {
		return false;
	}
	// Reusing a session across different players would permit one socket
	// to cast multiple votes for a successor. Each participant must have a
	// distinct authenticated transport session in the Hunter-owned room.
	for (const auto &[id, session_id] : this->rehost_session_ids) {
		if (session_id == reconnected.session_id) return false;
	}
	this->rehost_session_ids.emplace(reconnected.player_id, reconnected.session_id);
	return true;
}

bool AuthoritySuccession::pregame_rehost_ready() const {
	return not this->bootstrap_started and not this->original_host_lost
	    and this->hunter_connected and this->current.epoch == 1
	    and this->current.authority != 1
	    and this->rehost_session_ids.size() == this->participants.size();
}

AuthorityLease AuthoritySuccession::commit_pregame_hunter_rehost() {
	if (not this->pregame_rehost_ready()) {
		throw Error{MSG(err) << "FVD Hunter authority rehost requires every "
		                     << "participant to establish a new authenticated session "
		                     << "before the world bootstrap"};
	}
	this->current = {2, this->hunter_player(), 0};
	this->rehost_session_ids.clear();
	this->acknowledgements.clear();
	return this->current;
}

void AuthoritySuccession::mark_bootstrap_started() {
	this->bootstrap_started = true;
	this->rehost_session_ids.clear();
}

bool AuthoritySuccession::accepts_authority_epoch(
	uint64_t epoch, gamestate::player_id_t claimed_authority) const {
	return this->hunter_connected and not this->original_host_lost
	    and epoch == this->current.epoch
	    and claimed_authority == this->current.authority;
}

bool AuthoritySuccession::acknowledge_lease(
	gamestate::player_id_t player, uint64_t epoch) {
	if (not this->accepts_authority_epoch(epoch, this->current.authority)
	    or std::find(this->participants.begin(), this->participants.end(), player)
	       == this->participants.end()
	    or std::find(this->acknowledgements.begin(), this->acknowledgements.end(), player)
	       != this->acknowledgements.end()) {
		return false;
	}
	this->acknowledgements.push_back(player);
	return true;
}

bool AuthoritySuccession::has_acknowledged(gamestate::player_id_t player) const {
	return std::find(this->acknowledgements.begin(), this->acknowledgements.end(), player)
	    != this->acknowledgements.end();
}

} // namespace openage::network
