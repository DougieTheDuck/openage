// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>

#include "network/messages.h"

namespace openage::network {

/** Action a client should take after receiving one world delta. */
enum class DeltaDisposition {
	APPLY,
	REQUEST_SNAPSHOT,
	AWAIT_SNAPSHOT,
};

/**
 * Client-side revision state for authoritative world replication.
 *
 * This class tracks revision continuity only. The client simulation or
 * presentation state remains responsible for applying accepted records.
 */
class ReplicationTracker {
public:
	/** Record a complete initial or recovery snapshot. */
	void accept_snapshot(const WorldState &snapshot);

	/**
	 * Evaluate and consume one decoded delta.
	 *
	 * A delta applies only when a snapshot has established state and its base
	 * revision equals the last applied revision. The first gap requests one
	 * snapshot; later deltas are ignored until that snapshot arrives.
	 */
	DeltaDisposition accept_delta(const WorldState &delta);

	/** Stop accepting replication after the authoritative host disconnects. */
	void host_disconnected();

	/** Whether the authoritative host is still connected. */
	bool is_host_connected() const;

	/** Whether a snapshot has established authoritative state. */
	bool has_state() const;

	/** Whether a recovery snapshot has been requested and not yet received. */
	bool is_awaiting_snapshot() const;

	/** Last snapshot or delta revision accepted by this client. */
	uint64_t get_revision() const;

	/** Build the recovery request for the current replication position. */
	SnapshotRequest make_snapshot_request() const;

private:
	bool host_connected{true};
	bool initialized{false};
	bool awaiting_snapshot{false};
	uint64_t revision{0};
};

} // namespace openage::network
