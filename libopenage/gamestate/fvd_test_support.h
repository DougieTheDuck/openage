// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>

namespace openage::gamestate {
class GameState;

namespace tests {

/**
 * Restore the layout older construction, combat and networking tests were
 * written for: Builders in a row beside the map centre and no Hunter
 * tower, whose kill zone would otherwise cover the centre. Call right
 * after creating the game; fvd_scenario and fvd_hunter_tower test the
 * real starting layout.
 */
void use_centre_test_layout(const std::shared_ptr<GameState> &state);

} // namespace tests
} // namespace openage::gamestate
