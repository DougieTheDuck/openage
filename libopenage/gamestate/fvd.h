// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "coord/phys.h"
#include "coord/tile.h"
#include "gamestate/types.h"
#include "time/time.h"

namespace openage::event {
class EventLoop;
}

namespace openage::gamestate {
class EntityFactory;
class GameState;
class TerrainFactory;

namespace component::command {
class BuildTowerCommand;
}

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
	 * Validate a Builder construction request and reserve its tile.
	 *
	 * Exactly one selected, living Builder owned by the requesting player
	 * must be specified. The order remains pending until the construction
	 * activity consumes or cancels it.
	 *
	 * @return Whether a construction order was accepted.
	 */
	bool request_build(const std::shared_ptr<GameState> &state,
	                   player_id_t owner,
	                   const std::vector<entity_id_t> &selected,
	                   const coord::phys3 &position,
	                   const time::time_t &time);

	/**
	 * Cancel an outstanding Builder construction order and release its site.
	 *
	 * @return Whether an order was canceled.
	 */
	bool cancel_build(entity_id_t builder);

	/** Get an outstanding order, or nullptr if there is none. */
	std::shared_ptr<component::command::BuildTowerCommand> get_build_order(
		entity_id_t builder) const;

	/** Whether an incomplete construction order reserves this tile. */
	bool is_tile_reserved(const coord::tile &tile) const;

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

	uint64_t next_build_id = 1;
	std::unordered_map<entity_id_t, std::shared_ptr<component::command::BuildTowerCommand>> build_orders;
	std::unordered_map<coord::tile, entity_id_t> reserved_tiles;
};
}
