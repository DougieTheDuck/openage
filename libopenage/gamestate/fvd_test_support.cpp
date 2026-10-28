// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd_test_support.h"

#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/fvd.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"

namespace openage::gamestate::tests {

void use_centre_test_layout(const std::shared_ptr<GameState> &state) {
	auto fvd = state->get_fvd();
	fvd->remove_hunter_tower(state, time::TIME_ZERO);
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() != "fvd.data.entities.Builder") {
			continue;
		}
		auto owner = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP))->get_owners().get(time::TIME_ZERO);
		auto offset = static_cast<int>(owner) - static_cast<int>((fvd->get_player_count() + 2) / 2);
		std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION))
			->set_position(time::TIME_ZERO, fvd->get_centre() + coord::phys3_delta{offset, 1, 0});
	}
	fvd->update_vision(state, time::TIME_ZERO);
}

} // namespace openage::gamestate::tests
