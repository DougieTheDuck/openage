// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <optional>
#include <vector>

#include "network/messages.h"

namespace openage::network {

/** One active Hunter order resolved to authoritative world positions. */
struct AttackOrderPresentation {
	gamestate::entity_id_t horse_id;
	coord::phys3 horse_position;
	coord::phys3 marker_position;
	std::optional<gamestate::entity_id_t> target_id;
	bool focus_target;
	bool reached_destination;
};

/**
 * Resolve Hunter attack destinations and targets from one authoritative world.
 *
 * Target markers follow the target's replicated position. Destination markers
 * use the simulation-authored order position. Invalid references fail instead
 * of producing a plausible but divergent overlay.
 */
std::vector<AttackOrderPresentation> make_attack_order_presentation(
	const WorldState &world,
	const time::time_t &presentation_time);

} // namespace openage::network
