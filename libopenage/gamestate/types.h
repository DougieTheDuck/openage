// Copyright 2023-2023 the openage authors. See copying.md for legal info.

#pragma once

#include <cstdint>

namespace openage::gamestate {

/**
 * Game entity IDs.
 */
using entity_id_t = uint64_t;

/**
 * Player IDs.
 */
using player_id_t = uint64_t;

/** State chosen by the authoritative simulation for rendered unit animation. */
enum class AnimationAction : uint16_t {
	IDLE = 0,
	MOVE = 1,
	ATTACK = 2,
};

/** Standing behaviour of an FVD Hunter horse that has no active order. */
enum class HunterStance : uint16_t {
	/** Never start an engagement without a human order. */
	OBSERVE = 0,
	/** Engage any eligible enemy within sight and pursue it. */
	AGGRESSIVE = 1,
	/** Engage enemies within firing range and return to the engagement point. */
	DEFENSIVE = 2,
};

} // namespace openage::gamestate
