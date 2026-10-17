// Copyright 2026 the openage authors. See copying.md for legal info.

#include "replication.h"

#include "error/error.h"

namespace openage::network {

ReplicationTracker::ReplicationTracker(gamestate::player_id_t assigned_player) :
	assigned_player{assigned_player} {
	if (assigned_player == 0 or assigned_player > MAX_STATE_PLAYERS) {
		throw Error{MSG(err) << "FVD replication requires an assigned playable player"};
	}
}

gamestate::player_id_t ReplicationTracker::get_local_player_id() const {
	return this->assigned_player;
}

std::optional<uint64_t> ReplicationTracker::get_local_kills() const {
	if (not this->displayed_kills_valid.load()) {
		return std::nullopt;
	}
	return this->displayed_kills.load();
}

void ReplicationTracker::accept_snapshot(const WorldState &snapshot) {
	if (not this->host_connected) {
		throw Error{MSG(err) << "FVD replication cannot resume after host disconnect"};
	}
	if (snapshot.local_player_id != this->assigned_player) {
		throw Error{MSG(err) << "FVD snapshot is for a different player"};
	}
	if (snapshot.base_revision != 0) {
		throw Error{MSG(err) << "FVD world snapshot must have base revision zero"};
	}
	if (this->initialized and snapshot.world_revision < this->revision) {
		throw Error{MSG(err) << "FVD world snapshot revision "
		                     << snapshot.world_revision
		                     << " is older than applied revision " << this->revision};
	}

	this->revision = snapshot.world_revision;
	this->initialized = true;
	this->awaiting_snapshot = false;
	this->displayed_kills.store(snapshot.local_kill_count);
	this->displayed_kills_valid.store(true);
}

DeltaDisposition ReplicationTracker::accept_delta(const WorldState &delta) {
	if (not this->host_connected) {
		throw Error{MSG(err) << "FVD replication cannot accept deltas after host disconnect"};
	}
	if (delta.local_player_id != this->assigned_player) {
		throw Error{MSG(err) << "FVD delta is for a different player"};
	}
	if (delta.world_revision <= delta.base_revision) {
		throw Error{MSG(err) << "FVD world delta revision must advance beyond its base"};
	}
	if (this->awaiting_snapshot) {
		return DeltaDisposition::AWAIT_SNAPSHOT;
	}
	if (not this->initialized or delta.base_revision != this->revision) {
		this->awaiting_snapshot = true;
		return DeltaDisposition::REQUEST_SNAPSHOT;
	}

	this->revision = delta.world_revision;
	this->displayed_kills.store(delta.local_kill_count);
	this->displayed_kills_valid.store(true);
	return DeltaDisposition::APPLY;
}

void ReplicationTracker::host_disconnected() {
	this->host_connected = false;
	this->initialized = false;
	this->awaiting_snapshot = false;
	this->displayed_kills_valid.store(false);
}

bool ReplicationTracker::is_host_connected() const {
	return this->host_connected;
}

bool ReplicationTracker::has_state() const {
	return this->initialized;
}

bool ReplicationTracker::is_awaiting_snapshot() const {
	return this->awaiting_snapshot;
}

uint64_t ReplicationTracker::get_revision() const {
	return this->revision;
}

SnapshotRequest ReplicationTracker::make_snapshot_request() const {
	if (not this->host_connected) {
		throw Error{MSG(err) << "FVD cannot request a snapshot after host disconnect"};
	}
	return {this->revision};
}

} // namespace openage::network
