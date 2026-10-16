// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nyan/nyan.h>

#include "coord/phys.h"
#include "coord/tile.h"
#include "gamestate/types.h"
#include "pathfinding/types.h"
#include "time/time.h"

namespace openage::event {
class EventLoop;
class EventEntity;
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
	                   uint64_t route_revision,
	                   bool finish,
	                   const time::time_t &time);

	/** Recalculate current routes and release footprints of destroyed towers. */
	void refresh_paths(const std::shared_ptr<GameState> &state,
	                   const time::time_t &time);

	/** Destroy a tower and restore the path costs under its footprint. */
	bool destroy_tower(const std::shared_ptr<GameState> &state,
	                   entity_id_t tower_id,
	                   const time::time_t &time);

	/**
	 * Start the local match's Hunter spawn schedule at match simulation time.
	 * Safe to call more than once; no horses spawn while waiting to start.
	 */
	void start_hunter_wave(const std::shared_ptr<GameState> &state,
	                       const time::time_t &match_start);

	/** Spawn an idle Hunter horse and schedule the next one. */
	void spawn_hunter_horse(const std::shared_ptr<GameState> &state,
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
		uint64_t route_revision;
		entity_id_t foundation_id;
		std::optional<entity_id_t> tower_id;
	};

	struct FootprintCell {
		coord::tile tile;
		nyan::fqon_t path_type;
		path::cost_t previous_cost;
	};

	struct TowerFootprint {
		std::vector<FootprintCell> cells;
		bool under_construction;
	};

	/** Check occupancy, optionally excluding the constructing Builder. */
	bool tile_occupied(const std::shared_ptr<GameState> &state,
	                   const coord::tile &tile,
	                   const time::time_t &time,
	                   std::optional<entity_id_t> ignore = std::nullopt) const;

	/** Separate overlapping idle horses after a spawn without interrupting orders. */
	void relax_horse_crowd(const std::shared_ptr<GameState> &state,
	                      entity_id_t newest_horse,
	                      const time::time_t &time);

	/** Get all tiles intersected by the configured tower hitbox. */
	std::vector<coord::tile> get_tower_footprint(const coord::tile &site) const;

	/** Apply or restore a tower footprint in the map path grids. */
	void block_tower(const std::shared_ptr<GameState> &state,
	                 entity_id_t tower_id,
	                 const coord::tile &site,
	                 bool under_construction,
	                 const time::time_t &time);
	void unblock_tower(const std::shared_ptr<GameState> &state,
	                   entity_id_t tower_id,
	                   const time::time_t &time);

	size_t map_size;
	size_t chunk_size;
	size_t player_count;
	time::time_t construction_time;
	time::time_t horse_spawn_delay;
	time::time_t horse_spawn_interval;
	double horse_collision_radius;
	time::time_t horse_shove_duration;
	std::string hunter;
	std::string builders;
	std::string builder;
	std::string horse;
	std::string foundation;
	std::string tower;
	std::string plains;
	double tower_radius_ne;
	double tower_radius_se;
	std::unordered_map<nyan::fqon_t, path::cost_t> tower_path_costs;

	std::weak_ptr<openage::event::EventLoop> event_loop;
	std::weak_ptr<EntityFactory> entity_factory;
	std::shared_ptr<openage::event::EventEntity> horse_spawn_target;
	bool hunter_wave_started = false;
	uint64_t next_build_id = 1;
	std::unordered_map<entity_id_t, PendingBuild> build_orders;
	std::unordered_map<coord::tile, entity_id_t> reserved_tiles;
	std::unordered_map<entity_id_t, TowerFootprint> tower_footprints;
};
}
