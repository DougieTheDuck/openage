// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <mutex>
#include <unordered_map>

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
	/** Bind replication to the player assigned by the host session. */
	explicit ReplicationTracker(gamestate::player_id_t assigned_player);

	/** Assigned player identity, fixed for this match. */
	gamestate::player_id_t get_local_player_id() const;

	/** Displayable authoritative kills, unavailable until sync or after host loss. */
	std::optional<uint64_t> get_local_kills() const;

	/** Displayable kills for one of the assigned player's owned Towers. */
	std::optional<uint64_t> get_tower_kills(gamestate::entity_id_t tower) const;

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
	const gamestate::player_id_t assigned_player;
	// The presenter may read these while the connection thread applies updates.
	std::atomic<uint64_t> displayed_kills{0};
	std::atomic<bool> displayed_kills_valid{false};
	mutable std::mutex tower_kills_mutex;
	std::unordered_map<gamestate::entity_id_t, uint64_t> displayed_tower_kills{};
	bool host_connected{true};
	bool initialized{false};
	bool awaiting_snapshot{false};
	uint64_t revision{0};
};

} // namespace openage::network
