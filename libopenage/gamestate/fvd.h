// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
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
class Event;
class EventLoop;
class EventEntity;
}

namespace openage::gamestate {
class EntityFactory;
class GameEntity;
class GameState;
class TerrainFactory;

namespace component::command {
class BuildTowerCommand;
}

/** Construction phase exposed at the simulation/network boundary. */
enum class FVDConstructionPhase : uint16_t {
	PLANNED = 1,
	UNDER_CONSTRUCTION = 2,
};

/** Read-only description of one active construction order. */
struct FVDConstructionState {
	uint64_t order_id;
	player_id_t owner;
	entity_id_t builder_id;
	coord::tile target;
	FVDConstructionPhase phase;
	time::time_t phase_end_time;
	entity_id_t site_entity_id;
	/** Actual start of timed construction; planned orders have no progress. */
	time::time_t phase_start_time;

	/** Percentage of completed construction, clamped to 0..100. */
	int progress_percent(const time::time_t &now) const;
};

/** One footprint tile reserved by an active construction order. */
struct FVDTileReservation {
	coord::tile tile;
	entity_id_t builder_id;
};

/** Read-only description of one active Hunter combat order. */
struct FVDAttackOrderState {
	entity_id_t horse_id;
	coord::phys3 destination;
	std::optional<entity_id_t> target_id;
	bool reached_destination;
	bool focus_target;
};

/** Read-only timed projectile launch (Tower or Hunter). */
struct FVDArrowState {
	uint64_t arrow_id;
	player_id_t owner;
	entity_id_t target_id;
	coord::phys3 start;
	coord::phys3 destination;
	time::time_t launch_time;
	time::time_t impact_time;
};

/**
 * Flat plains scenario with one Hunter and the remaining players as Builders.
 */
class FVD {
public:
	/** Purely visual render snapshot; authoritative damage is separate. */
	struct ArrowTrail {
		coord::phys3 tail;
		coord::phys3 head;
		double progress;
		player_id_t owner;
		bool impact = false;
	};
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

	/** Validate ownership and the full tower footprint for placement preview. */
	bool can_place_tower(const std::shared_ptr<GameState> &state,
	                     player_id_t owner, const std::vector<entity_id_t> &selected,
	                     const coord::phys3 &position, const time::time_t &time,
	                     std::optional<entity_id_t> existing_foundation = std::nullopt) const;

	/** Append a construction site for a Builder, reserving the footprint. */
	bool queue_build(const std::shared_ptr<GameState> &state,
	                 player_id_t owner, const std::vector<entity_id_t> &selected,
	                 const coord::phys3 &position, const time::time_t &time);

	/** Resume an owned orphan foundation or prioritize a queued foundation. */
	bool prioritize_foundation(const std::shared_ptr<GameState> &state,
	                           player_id_t owner, const std::vector<entity_id_t> &selected,
	                           entity_id_t foundation_id, const time::time_t &time);

	/** Number of sites waiting after the Builder's currently active tower. */
	size_t get_queued_build_count(entity_id_t builder) const;

	/** Full grid footprint of a potential tower. */
	std::vector<coord::tile> get_tower_footprint(const coord::tile &site) const;

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

	/**
	 * Seconds remaining until the first Hunter spawn, rounded up.
	 * The deadline is shared read-only with the presenter; no countdown
	 * exists before match start or after the first spawn deadline.
	 */
	std::optional<int> get_hunter_spawn_countdown(const time::time_t &now) const;

	/** Queue the first Hunter spawn immediately and replace its delayed schedule. */
	void request_immediate_hunter_spawn(const std::shared_ptr<GameState> &state,
	                                    const time::time_t &time);

	/**
	 * Spawn an idle Hunter horse and schedule the next one.
	 *
	 * @param skip_initial_delay Cancel the pending first spawn and begin the wave now.
	 */
	void spawn_hunter_horse(const std::shared_ptr<GameState> &state,
	                       const time::time_t &time,
	                       bool skip_initial_delay = false);

	/** Validate a complete group of player-owned movable units. */
	bool can_command_units(const std::shared_ptr<GameState> &state,
	                       player_id_t owner, const std::vector<entity_id_t> &ids,
	                       const time::time_t &time) const;

	/** Resolve a completed tower at its occupied tile, if one exists. */
	std::optional<entity_id_t> completed_tower_at(
		const std::shared_ptr<GameState> &state, const coord::tile &tile) const;

	/** Start Hunter AttackMove for every selected horse, or reject all. */
	bool issue_attack_move(const std::shared_ptr<GameState> &state,
	                       player_id_t owner, const std::vector<entity_id_t> &ids,
	                       const coord::phys3 &destination, const time::time_t &time);

	/** Focus a specific enemy entity, overriding automatic tower priority. */
	bool issue_focus_attack(const std::shared_ptr<GameState> &state,
	                        player_id_t owner, const std::vector<entity_id_t> &ids,
	                        entity_id_t enemy_id, const time::time_t &time);

	/** Remove the autonomous engagement portion when superseded by another order. */
	void cancel_attack_move(entity_id_t id);

	/** Authoritative combat simulation; called on a fixed simulation-time tick. */
	void combat_tick(const std::shared_ptr<GameState> &state, const time::time_t &time);

	/** Resolve one arrow with the firing tower's identity, even after its death. */
	void arrow_hit(const std::shared_ptr<GameState> &state, entity_id_t target,
	               player_id_t owner, entity_id_t firing_tower,
	               int64_t damage, const time::time_t &time);

	/** Apply a Hunter horse's pending strike at its animation's hit frame. */
	void horse_hit(const std::shared_ptr<GameState> &state,
	               entity_id_t horse, entity_id_t target,
	               int64_t damage, const time::time_t &time);

	/**
	 * Thread-safe snapshot of visible arrow trails at simulation time.
	 * Projectiles are never selectable or independently simulated clients.
	 */
	std::vector<ArrowTrail> get_visible_arrows(const time::time_t &now) const;

	/** Lifetime kills by all towers of this player, including destroyed towers. */
	uint64_t get_kills(player_id_t owner) const;

	/** Individual tower kills; only this tower's owning player may read them. */
	std::optional<uint64_t> get_tower_kills(entity_id_t tower, player_id_t owner) const;
	int64_t get_tower_damage() const;
	int64_t get_tower_armor() const;

	/** Get the common camera start location. */
	coord::phys3 get_centre() const;

	/** Get the number of playable players, excluding neutral player zero. */
	size_t get_player_count() const;

	/** Get the width and height of the square map in tiles. */
	size_t get_map_size() const;

	/** Capture active construction orders in stable order-ID order. */
	std::vector<FVDConstructionState> get_construction_state() const;

	/** Capture reserved footprint tiles in coordinate order. */
	std::vector<FVDTileReservation> get_reserved_tiles() const;

	/** Capture active Hunter orders in horse-ID order. */
	std::vector<FVDAttackOrderState> get_attack_order_state() const;

	/** Capture at most 2048 immutable launches, oldest undelivered first.
	 * Passing no cursor retains only the recent presentation window. */
	std::vector<FVDArrowState> get_arrow_state(
		const time::time_t &now,
		std::optional<uint64_t> delivered_through = std::nullopt) const;

	/** Enable backlog retention during a live network match. */
	void enable_projectile_replication();

	/** Only discard old launches once every live recipient has been sent them. */
	void prune_delivered_projectiles(const time::time_t &now,
	                                 uint64_t delivered_to_all);

private:
	/** A queued tower retains a concrete, pickable foundation entity. */
	struct QueuedBuildSite {
		coord::tile site;
		entity_id_t foundation_id;
	};

	/** Shared construction path and movement setup for new/queued sites. */
	bool start_build(const std::shared_ptr<GameState> &state,
	                 player_id_t owner, const std::vector<entity_id_t> &selected,
	                 const coord::phys3 &position, const time::time_t &time,
	                 std::optional<entity_id_t> existing_foundation);

	/** Find a reachable vacant adjacent tile with public Pathfinder API. */
	std::optional<coord::phys3> find_build_stand(
		const std::shared_ptr<GameState> &state,
		const std::shared_ptr<GameEntity> &builder,
		const coord::tile &site, const time::time_t &time) const;

	/** Abandon queued orders and release their reservations, retaining foundations. */
	void clear_queued_builds(const std::shared_ptr<GameState> &state,
	                        entity_id_t builder, const time::time_t &time);

	struct AttackOrder {
		coord::phys3 destination;
		std::optional<entity_id_t> target;
		time::time_t next_attack;
		bool reached_destination = false;
		bool focus_target = false;
		std::optional<time::time_t> pending_hit{};
	};

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

	/**
	 * Validate a Hunter selection while ignoring horses confirmed dead by
	 * this simulation. Unknown IDs or foreign entities still invalidate it.
	 */
	std::optional<std::vector<entity_id_t>> resolve_command_units(
		const std::shared_ptr<GameState> &state, player_id_t owner,
		const std::vector<entity_id_t> &ids, const time::time_t &time) const;

	/** Apply damage once and remove dead units. */
	bool apply_damage(const std::shared_ptr<GameState> &state, entity_id_t target,
	                  int64_t damage, player_id_t owner, bool tower_shot,
	                  const time::time_t &time);

	/** Check occupancy, optionally excluding the constructing Builder. */
	bool tile_occupied(const std::shared_ptr<GameState> &state,
	                   const coord::tile &tile,
	                   const time::time_t &time,
	                   std::optional<entity_id_t> ignore = std::nullopt) const;

	/** Separate overlapping idle horses after a spawn without interrupting orders. */
	void relax_horse_crowd(const std::shared_ptr<GameState> &state,
	                      entity_id_t newest_horse,
	                      const time::time_t &time);

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
	int64_t tower_damage;
	int64_t tower_armor;
	int64_t horse_damage;
	time::time_t arrow_travel;
	time::time_t tower_cooldown;
	time::time_t horse_cooldown;
	time::time_t horse_hit_delay;
	time::time_t horse_shove_duration;
	std::string hunter;
	std::string builders;
	std::string builder;
	std::string horse;
	std::string horse_attack_animation;
	std::string foundation;
	std::string tower;
	std::string plains;
	double tower_radius_ne;
	double tower_radius_se;
	std::unordered_map<nyan::fqon_t, path::cost_t> tower_path_costs;

	std::weak_ptr<openage::event::EventLoop> event_loop;
	std::weak_ptr<EntityFactory> entity_factory;
	std::shared_ptr<openage::event::EventEntity> horse_spawn_target;
	std::shared_ptr<openage::event::Event> next_hunter_spawn_event;
	bool hunter_wave_started = false;
	/** Protect the match-relative first-spawn deadline read by the presenter. */
	mutable std::mutex hunter_spawn_time_mutex;
	std::optional<time::time_t> first_hunter_spawn_time;
	/** Visual event history only; all damage remains authoritative. */
	struct ArrowVisual {
		uint64_t arrow_id;
		player_id_t owner;
		entity_id_t target_id;
		coord::phys3 start;
		coord::phys3 destination;
		time::time_t launch_time;
		time::time_t impact_time;
	};
	mutable std::mutex arrow_visuals_mutex;
	std::vector<ArrowVisual> arrow_visuals;
	bool projectile_replication_enabled = false;
	uint64_t next_arrow_id = 1;
	uint64_t next_build_id = 1;
	std::unordered_map<entity_id_t, AttackOrder> attack_orders;
	std::unordered_set<entity_id_t> dead_horses;
	std::unordered_map<entity_id_t, time::time_t> tower_next_shot;
	std::array<std::atomic<uint64_t>, 9> player_kills{};
	struct TowerKillCount {
		player_id_t owner;
		uint64_t kills;
	};
	/** Keep retired tower records so shots in flight can still score. */
	mutable std::mutex tower_kills_mutex;
	std::unordered_map<entity_id_t, TowerKillCount> tower_kills{};
	std::unordered_map<entity_id_t, PendingBuild> build_orders;
	std::unordered_map<entity_id_t, std::deque<QueuedBuildSite>> queued_builds;
	std::unordered_map<coord::tile, entity_id_t> reserved_tiles;
	std::unordered_map<entity_id_t, TowerFootprint> tower_footprints;
};
}
