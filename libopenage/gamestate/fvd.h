// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "coord/phys.h"
#include "gamestate/types.h"
#include "time/time.h"

namespace openage::event {
class EventLoop;
}

namespace openage::gamestate {
class EntityFactory;
class GameState;
class TerrainFactory;

/**
 * Flat plains scenario with one Hunter and the remaining players as Builders.
 */
class FVD {
public:
	/**
	 * Read and validate the scenario's nyan settings.
	 */
	explicit FVD(const std::shared_ptr<GameState> &state);

	/**
	 * Create terrain, players and starting Builders.
	 */
	void initialise(const std::shared_ptr<GameState> &state,
	                const std::shared_ptr<openage::event::EventLoop> &loop,
	                const std::shared_ptr<EntityFactory> &entities,
	                const std::shared_ptr<TerrainFactory> &terrain) const;

	/**
	 * Place a tower if the owner, selected Builder and site are valid.
	 *
	 * Invalid player actions are rejected with a diagnostic.
	 * @return Whether a tower was placed.
	 */
	bool place_tower(const std::shared_ptr<GameState> &state,
	                 const std::shared_ptr<openage::event::EventLoop> &loop,
	                 const std::shared_ptr<EntityFactory> &entities,
	                 player_id_t owner,
	                 const std::vector<entity_id_t> &selected,
	                 const coord::phys3 &position,
	                 const time::time_t &time) const;

	/**
	 * Get the common camera start location.
	 */
	coord::phys3 get_centre() const;

	/**
	 * Get the number of playable players, excluding neutral player zero.
	 */
	size_t get_player_count() const;

private:
	size_t map_size;
	size_t chunk_size;
	size_t player_count;
	std::string hunter;
	std::string builders;
	std::string builder;
	std::string tower;
	std::string plains;
};
}
