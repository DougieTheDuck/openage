// Copyright 2026 the openage authors. See copying.md for legal info.

#include "replication.h"

#include "error/error.h"

namespace openage::network {

void ReplicationTracker::accept_snapshot(const WorldState &snapshot) {
	if (not this->host_connected) {
		throw Error{MSG(err) << "FVD replication cannot resume after host disconnect"};
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
}

DeltaDisposition ReplicationTracker::accept_delta(const WorldState &delta) {
	if (not this->host_connected) {
		throw Error{MSG(err) << "FVD replication cannot accept deltas after host disconnect"};
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
	return DeltaDisposition::APPLY;
}

void ReplicationTracker::host_disconnected() {
	this->host_connected = false;
	this->initialized = false;
	this->awaiting_snapshot = false;
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
