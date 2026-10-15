// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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
	/** Read and validate the scenario's nyan settings. */
	explicit FVD(const std::shared_ptr<GameState> &state);

	/** Create terrain, players and starting Builders. */
	void initialise(const std::shared_ptr<GameState> &state,
	                const std::shared_ptr<openage::event::EventLoop> &loop,
	                const std::shared_ptr<EntityFactory> &entities,
	                const std::shared_ptr<TerrainFactory> &terrain);

	/**
	 * Validate a Builder construction request, route to a reachable adjacent
	 * stand tile and schedule construction completion in simulation time.
	 *
	 * @return Whether the order was accepted.
	 */
	bool request_build(const std::shared_ptr<GameState> &state,
	                   player_id_t owner,
	                   const std::vector<entity_id_t> &selected,
	                   const coord::phys3 &position,
	                   const time::time_t &time);

	/**
	 * Stop an outstanding construction order and release its site reservation.
	 *
	 * @return Whether an order was canceled.
	 */
	bool cancel_build(const std::shared_ptr<GameState> &state,
	                  entity_id_t builder,
	                  const time::time_t &time);

	/** Get an unfinished construction order, or nullptr if absent. */
	std::shared_ptr<component::command::BuildTowerCommand> get_build_order(
		entity_id_t builder) const;

	/** Whether an unfinished construction order reserves this tile. */
	bool is_tile_reserved(const coord::tile &tile) const;

	/** Execute an arrival or completion event for a matching order ID. */
	void advance_build(const std::shared_ptr<GameState> &state,
	                   entity_id_t builder,
	                   uint64_t order_id,
	                   bool finish,
	                   const time::time_t &time);

	/** Get the common camera start location. */
	coord::phys3 get_centre() const;

	/** Get the number of playable players, excluding neutral player zero. */
	size_t get_player_count() const;

private:
	struct PendingBuild {
		std::shared_ptr<component::command::BuildTowerCommand> order;
		coord::phys3 stand;
		time::time_t arrival;
	};

	/** Check occupancy, optionally excluding the constructing Builder. */
	bool tile_occupied(const std::shared_ptr<GameState> &state,
	                   const coord::tile &tile,
	                   const time::time_t &time,
	                   std::optional<entity_id_t> ignore = std::nullopt) const;

	size_t map_size;
	size_t chunk_size;
	size_t player_count;
	int64_t construction_time;
	std::string hunter;
	std::string builders;
	std::string builder;
	std::string tower;
	std::string plains;

	std::weak_ptr<openage::event::EventLoop> event_loop;
	std::weak_ptr<EntityFactory> entity_factory;
	uint64_t next_build_id = 1;
	std::unordered_map<entity_id_t, PendingBuild> build_orders;
	std::unordered_map<coord::tile, entity_id_t> reserved_tiles;
};
}
