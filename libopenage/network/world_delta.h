// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <optional>
#include <vector>

#include "network/messages.h"

namespace openage::network {

/**
 * A WorldDelta contains only changed records and explicit removals.
 * All record sets use their stable identity (entity ID, order ID, tile, etc.).
 * The header and the optional lifetime-score change are authoritative.
 */
struct WorldDelta {
	WorldState changed{};
	bool local_kills_changed{false};
	std::vector<gamestate::player_id_t> removed_players{};
	std::vector<gamestate::entity_id_t> removed_entities{};
	std::vector<coord::tile> removed_reservations{};
	std::vector<uint64_t> removed_constructions{};
	std::vector<gamestate::entity_id_t> removed_attack_orders{};
	std::vector<uint64_t> removed_arrows{};
};

/** Compare two complete, recipient-specific states and produce a true patch. */
WorldDelta make_world_delta(const WorldState &previous, const WorldState &current);

/** Apply a contiguous patch, validating identity and all removals atomically. */
WorldState apply_world_delta(const WorldState &previous, const WorldDelta &delta);

/** Distinct wire format for WorldDelta (not the full snapshot encoding). */
std::vector<uint8_t> encode_world_delta(const WorldDelta &delta);
WorldDelta decode_world_delta(const std::vector<uint8_t> &payload);

/** Host-side per-recipient baseline. Seed on snapshots and advance on deltas. */
class WorldDeltaBaseline {
public:
	void accept_snapshot(WorldState snapshot);
	WorldDelta advance(WorldState complete);
	bool initialized() const;

private:
	std::optional<WorldState> previous{};
};

} // namespace openage::network
