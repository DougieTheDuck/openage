// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <vector>

#include <nyan/nyan.h>

#include "curve/discrete.h"
#include "error/error.h"
#include "event/event_loop.h"
#include "event/eventhandler.h"
#include "gamestate/event/spawn_entity.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/internal/activity.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/build_tower.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/activity/xor_event_gate.h"
#include "gamestate/entity_factory.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/manager.h"
#include "gamestate/map.h"
#include "gamestate/player.h"
#include "gamestate/system/idle.h"
#include "gamestate/system/move.h"
#include "gamestate/terrain_chunk.h"
#include "gamestate/terrain_factory.h"
#include "log/log.h"
#include "pathfinding/path.h"
#include "pathfinding/definitions.h"
#include "pathfinding/pathfinder.h"

namespace openage::gamestate {

namespace {

/** Arrive at a construction site, then complete it at a later simulation time. */
class BuildProgressHandler : public openage::event::OnceEventHandler {
public:
	BuildProgressHandler() :
		OnceEventHandler{"game.fvd_build_progress"} {}

	void setup_event(const std::shared_ptr<openage::event::Event> &,
	                 const std::shared_ptr<openage::event::State> &) override {}

	void invoke(openage::event::EventLoop &,
	            const std::shared_ptr<openage::event::EventEntity> &,
	            const std::shared_ptr<openage::event::State> &state,
	            const time::time_t &time,
	            const param_map &params) override {
		auto game_state = std::dynamic_pointer_cast<GameState>(state);
		game_state->get_fvd()->advance_build(
			game_state,
			params.get<entity_id_t>("builder"),
			params.get<uint64_t>("order"),
			params.get<uint64_t>("route"),
			params.get<bool>("finish"),
			time);
	}

	time::time_t predict_invoke_time(
		const std::shared_ptr<openage::event::EventEntity> &,
		const std::shared_ptr<openage::event::State> &,
		const time::time_t &at) override {
		return at;
	}
};

/** Schedule Hunter-owned horses without assigning automatic movement. */
class SpawnHorseHandler : public openage::event::OnceEventHandler {
public:
	SpawnHorseHandler() :
		OnceEventHandler{"game.fvd_spawn_horse"} {}

	void setup_event(const std::shared_ptr<openage::event::Event> &,
	                 const std::shared_ptr<openage::event::State> &) override {}

	void invoke(openage::event::EventLoop &,
	            const std::shared_ptr<openage::event::EventEntity> &,
	            const std::shared_ptr<openage::event::State> &state,
	            const time::time_t &time,
	            const param_map &) override {
		auto game_state = std::dynamic_pointer_cast<GameState>(state);
		game_state->get_fvd()->spawn_hunter_horse(game_state, time);
	}

	time::time_t predict_invoke_time(
		const std::shared_ptr<openage::event::EventEntity> &,
		const std::shared_ptr<openage::event::State> &,
		const time::time_t &at) override {
		return at;
	}
};

/** Recalculate active routes at a low fixed frequency. */
class RefreshPathsHandler : public openage::event::RepeatEventHandler {
public:
	RefreshPathsHandler() :
		RepeatEventHandler{"game.fvd_refresh_paths"} {}

	void setup_event(const std::shared_ptr<openage::event::Event> &,
	                 const std::shared_ptr<openage::event::State> &) override {}

	void invoke(openage::event::EventLoop &,
	            const std::shared_ptr<openage::event::EventEntity> &,
	            const std::shared_ptr<openage::event::State> &state,
	            const time::time_t &time,
	            const param_map &) override {
		auto game_state = std::dynamic_pointer_cast<GameState>(state);
		game_state->get_fvd()->refresh_paths(game_state, time);
	}

	time::time_t predict_invoke_time(
		const std::shared_ptr<openage::event::EventEntity> &,
		const std::shared_ptr<openage::event::State> &,
		const time::time_t &at) override {
		return at + time::time_t::from_int(4);
	}
};

bool builder_alive(const std::shared_ptr<GameEntity> &entity,
                   const time::time_t &time) {
	if (not entity->has_component(component::component_t::LIVE)) {
		return false;
	}
	auto live = std::dynamic_pointer_cast<component::Live>(
		entity->get_component(component::component_t::LIVE));
	return live->get_attribute(time, "fvd.data.entities.Health")->get(time) > 0;
}

} // namespace

FVD::FVD(const std::shared_ptr<GameState> &state) {
	auto settings = state->get_db_view()->get_object("fvd.data.scenario.Scenario");
	auto size = settings.get_int("map_size");
	auto chunk = settings.get_int("chunk_size");
	auto players = settings.get_int("player_count");
	auto build_time = settings.get_float("construction_time");
	auto horse_delay = settings.get_int("horse_spawn_delay");
	auto horse_interval = settings.get_int("horse_spawn_interval");
	if (size < 16 or size > 512 or chunk < 4 or chunk > size
	    or chunk > MAX_CHUNK_WIDTH or chunk > MAX_CHUNK_HEIGHT
	    or size % chunk != 0 or players < 2 or players > 8
	    or build_time <= 0.0 or build_time > 300.0
	    or horse_delay < 0 or horse_delay > 600
	    or horse_interval < 1 or horse_interval > 60) {
		throw Error{ERR << "Invalid FVD settings: map 16..512, chunk 4.."
		                << MAX_CHUNK_WIDTH << ", players 2..8, construction >0..300 seconds"};
	}
	this->map_size = size;
	this->chunk_size = chunk;
	this->player_count = players;
	this->construction_time = time::time_t::from_double(build_time);
	this->horse_spawn_delay = time::time_t::from_int(horse_delay);
	this->horse_spawn_interval = time::time_t::from_int(horse_interval);
	this->hunter = settings.get_object("hunter").get_name();
	this->builders = settings.get_object("builders").get_name();
	this->builder = settings.get_object("builder").get_name();
	this->horse = settings.get_object("horse").get_name();
	this->foundation = settings.get_object("foundation").get_name();
	this->tower = settings.get_object("tower").get_name();
	this->plains = settings.get_object("terrain").get_name();

	auto tower_object = state->get_db_view()->get_object(this->tower);
	bool found_pathable = false;
	for (const auto &ability : tower_object.get_set("GameEntity.abilities")) {
		auto value = std::dynamic_pointer_cast<nyan::ObjectValue>(ability.get_ptr());
		auto object = state->get_db_view()->get_object(value->get_name());
		if (object.get_parents()[0] != "engine.ability.type.Pathable") {
			continue;
		}
		auto hitbox = object.get_object("Pathable.hitbox");
		this->tower_radius_ne = hitbox.get_float("Hitbox.radius_x");
		this->tower_radius_se = hitbox.get_float("Hitbox.radius_y");
		for (const auto &[path_type, cost] : object.get_dict("Pathable.path_costs")) {
			auto path_value = std::dynamic_pointer_cast<nyan::ObjectValue>(path_type.get_ptr());
			auto cost_value = std::dynamic_pointer_cast<nyan::Int>(cost.get_ptr());
			if (cost_value->get() < path::COST_MIN or cost_value->get() > path::COST_IMPASSABLE) {
				throw Error{ERR << "Tower path cost is outside the valid range"};
			}
			this->tower_path_costs.emplace(
				path_value->get_name(), static_cast<path::cost_t>(cost_value->get()));
		}
		found_pathable = true;
		break;
	}
	if (not found_pathable or this->tower_radius_ne <= 0.0 or this->tower_radius_se <= 0.0
	    or this->tower_path_costs.empty()) {
		throw Error{ERR << "FVD tower requires a positive Pathable hitbox and path costs"};
	}
}

void FVD::initialise(const std::shared_ptr<GameState> &state,
                     const std::shared_ptr<openage::event::EventLoop> &loop,
                     const std::shared_ptr<EntityFactory> &entities,
                     const std::shared_ptr<TerrainFactory> &terrain) {
	this->event_loop = loop;
	this->entity_factory = entities;
	this->horse_spawn_target = std::make_shared<event::Spawner>(loop);
	loop->add_event_handler(std::make_shared<SpawnHorseHandler>());
	loop->add_event_handler(std::make_shared<BuildProgressHandler>());
	loop->add_event_handler(std::make_shared<RefreshPathsHandler>());

	std::vector<std::shared_ptr<TerrainChunk>> chunks;
	for (size_t se = 0; se < this->map_size; se += this->chunk_size) {
		for (size_t ne = 0; ne < this->map_size; ne += this->chunk_size) {
			chunks.push_back(terrain->add_uniform_chunk(
				state, {this->chunk_size, this->chunk_size},
				{static_cast<coord::tile_t>(ne), static_cast<coord::tile_t>(se)}, this->plains));
		}
	}
	state->set_map(std::make_shared<Map>(
		state, terrain->add_terrain({this->map_size, this->map_size}, std::move(chunks))));

	// Reserve zero for neutral ownership; playable players are numbered from one.
	std::shared_ptr<GameEntity> refresh_target;
	for (size_t id = 0; id <= this->player_count; ++id) {
		auto setup = id == 0 ? "" : (id == 1 ? this->hunter : this->builders);
		state->add_player(entities->add_player(loop, state, setup));
		if (id >= 2) {
			auto centre = this->get_centre();
			auto offset = static_cast<int>(id) - static_cast<int>((this->player_count + 2) / 2);
			auto entity = entities->spawn_game_entity(
				loop, state, id, this->builder,
				centre + coord::phys3_delta{offset, 1, 0}, time::TIME_ZERO);
			if (not refresh_target) {
				refresh_target = entity;
			}
		}
	}
	loop->create_event("game.fvd_refresh_paths", refresh_target->get_manager(), state, time::TIME_ZERO);
}

bool FVD::tile_occupied(const std::shared_ptr<GameState> &state,
                        const coord::tile &tile,
                        const time::time_t &time,
                        std::optional<entity_id_t> ignore) const {
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (ignore.has_value() and id == *ignore) {
			continue;
		}
		if (entity->get_definition() == this->foundation) {
			continue;
		}
		auto position = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		if (position->get_positions().get(time).to_tile() == tile) {
			return true;
		}
	}
	return false;
}

std::vector<coord::tile> FVD::get_tower_footprint(const coord::tile &site) const {
	auto ne_radius = static_cast<coord::tile_t>(std::ceil(this->tower_radius_ne - 0.5));
	auto se_radius = static_cast<coord::tile_t>(std::ceil(this->tower_radius_se - 0.5));
	std::vector<coord::tile> footprint;
	footprint.reserve((ne_radius * 2 + 1) * (se_radius * 2 + 1));
	for (auto se = site.se - se_radius; se <= site.se + se_radius; ++se) {
		for (auto ne = site.ne - ne_radius; ne <= site.ne + ne_radius; ++ne) {
			footprint.emplace_back(ne, se);
		}
	}
	return footprint;
}

void FVD::block_tower(const std::shared_ptr<GameState> &state,
                      entity_id_t tower_id,
                      const coord::tile &site,
                      bool under_construction,
                      const time::time_t &time) {
	if (this->tower_footprints.contains(tower_id)) [[unlikely]] {
		throw Error{ERR << "Tower " << tower_id << " already blocks a footprint"};
	}
	auto map = state->get_map();
	TowerFootprint blocked{{}, under_construction};
	std::unordered_set<nyan::fqon_t> changed_grids;
	for (const auto &tile : this->get_tower_footprint(site)) {
		for (const auto &[path_type, cost] : this->tower_path_costs) {
			auto previous = map->set_path_cost(path_type, tile, cost, time);
			blocked.cells.push_back(FootprintCell{tile, path_type, previous});
			changed_grids.insert(path_type);
		}
	}
	for (const auto &path_type : changed_grids) {
		map->rebuild_path_grid(path_type);
	}
	this->tower_footprints.emplace(tower_id, std::move(blocked));
}

void FVD::unblock_tower(const std::shared_ptr<GameState> &state,
                        entity_id_t tower_id,
                        const time::time_t &time) {
	auto found = this->tower_footprints.find(tower_id);
	if (found == this->tower_footprints.end()) {
		return;
	}
	auto map = state->get_map();
	std::unordered_set<nyan::fqon_t> changed_grids;
	for (const auto &cell : found->second.cells) {
		map->set_path_cost(cell.path_type, cell.tile, cell.previous_cost, time);
		changed_grids.insert(cell.path_type);
	}
	for (const auto &path_type : changed_grids) {
		map->rebuild_path_grid(path_type);
	}
	this->tower_footprints.erase(found);
}

bool FVD::request_build(const std::shared_ptr<GameState> &state,
                        player_id_t owner,
                        const std::vector<entity_id_t> &selected,
                        const coord::phys3 &position,
                        const time::time_t &time) {
	if (owner < 2 or owner > this->player_count) {
		log::log(WARN << "Only Builder players can construct towers");
		return false;
	}
	auto civilisation = state->get_player(owner)->get_db_view()->get_object(
		state->get_player(owner)->get_setup());
	bool allowed = false;
	for (const auto &entry : civilisation.get_set("Civilisation.buildables")) {
		if (std::dynamic_pointer_cast<nyan::ObjectValue>(entry.get_ptr())->get_name() == this->tower) {
			allowed = true;
		}
	}
	if (not allowed or selected.size() != 1) {
		return false;
	}
	auto builder_id = selected.front();
	auto found = state->get_game_entities().find(builder_id);
	if (found == state->get_game_entities().end()) {
		return false;
	}
	auto entity = found->second;
	if (entity->get_definition() != this->builder
	    or not entity->has_component(component::component_t::OWNERSHIP)
	    or not entity->has_component(component::component_t::MOVE)
	    or not builder_alive(entity, time)) {
		return false;
	}
	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	if (ownership->get_owners().get(time) != owner) {
		return false;
	}
	if (position.ne < 0 or position.se < 0
	    or position.ne >= this->map_size or position.se >= this->map_size
	    or position.up != 0) {
		return false;
	}
	auto site = position.to_tile();
	for (const auto &tile : this->get_tower_footprint(site)) {
		if (tile.ne < 0 or tile.se < 0
		    or tile.ne >= this->map_size or tile.se >= this->map_size
		    or this->is_tile_reserved(tile) or this->tile_occupied(state, tile, time)) {
			return false;
		}
	}

	auto current_position = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION))->get_positions().get(time);
	auto origin = current_position.to_tile();
	auto move = std::dynamic_pointer_cast<component::Move>(
		entity->get_component(component::component_t::MOVE));
	auto grid_name = move->get_ability().get<nyan::ObjectValue>("Move.path_type")->get_name();
	auto map = state->get_map();
	auto grid = map->get_grid_id(grid_name);
	auto pathfinder = map->get_pathfinder();

	const std::array<coord::tile_delta, 8> offsets{{
		{-1, 0}, {1, 0}, {0, -1}, {0, 1},
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
	}};
	std::vector<coord::tile> candidates;
	for (const auto &offset : offsets) {
		coord::tile candidate{site.ne + offset.ne, site.se + offset.se};
		if (candidate.ne < 0 or candidate.se < 0
		    or candidate.ne >= this->map_size or candidate.se >= this->map_size
		    or this->is_tile_reserved(candidate)
		    or this->tile_occupied(state, candidate, time, builder_id)) {
			continue;
		}
		candidates.push_back(candidate);
	}
	std::stable_sort(candidates.begin(), candidates.end(),
	                 [&](const coord::tile &a, const coord::tile &b) {
		                 auto da = (a.ne - origin.ne) * (a.ne - origin.ne)
		                         + (a.se - origin.se) * (a.se - origin.se);
		                 auto db = (b.ne - origin.ne) * (b.ne - origin.ne)
		                         + (b.se - origin.se) * (b.se - origin.se);
		                 return da < db;
	                 });

	std::optional<coord::phys3> stand;
	for (const auto &candidate : candidates) {
		if (candidate == origin) {
			stand = current_position;
			break;
		}
		auto route = pathfinder->get_path(path::PathRequest{grid, origin, candidate, time});
		if (route.status == path::PathResult::FOUND and not route.waypoints.empty()) {
			stand = candidate.to_phys3_center();
			break;
		}
	}
	if (not stand.has_value()) {
		log::log(WARN << "No reachable adjacent construction tile");
		return false;
	}

	// Old movement commands must not restart while the build order is active.
	this->cancel_build(state, builder_id, time);
	auto activity = std::dynamic_pointer_cast<component::Activity>(
		entity->get_component(component::component_t::ACTIVITY));
	auto commands = std::dynamic_pointer_cast<component::CommandQueue>(
		entity->get_component(component::component_t::COMMANDQUEUE));
	activity->cancel_events(time);
	while (not commands->get_queue().empty(time)) {
		commands->pop_command(time);
	}
	activity->init(time);
	entity->get_manager()->run_activity_system(time);

	auto loop = this->event_loop.lock();
	auto factory = this->entity_factory.lock();
	if (not loop or not factory) [[unlikely]] {
		throw Error{ERR << "FVD construction services no longer exist"};
	}

	auto order = std::make_shared<component::command::BuildTowerCommand>(
		this->next_build_id++, owner, builder_id, site);
	for (const auto &tile : this->get_tower_footprint(site)) {
		this->reserved_tiles.emplace(tile, builder_id);
	}
	auto travel = *stand == current_position
	            ? time::TIME_ZERO
	            : system::Move::move_default(entity, state, *stand, time);
	auto arrival = time + travel;
	auto foundation_entity = factory->spawn_game_entity(
		loop, state, owner, this->foundation, site.to_phys3_center(), time);
	this->build_orders.emplace(
		builder_id,
		PendingBuild{order, *stand, arrival, 0, foundation_entity->get_id(), std::nullopt});
	loop->create_event("game.fvd_build_progress", entity->get_manager(), state, arrival,
	                   {{"builder", builder_id}, {"order", order->get_id()},
	                    {"route", uint64_t{0}}, {"finish", false}});
	return true;
}

bool FVD::cancel_build(const std::shared_ptr<GameState> &state,
                       entity_id_t builder,
                       const time::time_t &time) {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end()) {
		return false;
	}
	for (const auto &tile : this->get_tower_footprint(it->second.order->get_target())) {
		auto reserved = this->reserved_tiles.find(tile);
		if (reserved != this->reserved_tiles.end() and reserved->second == builder) {
			this->reserved_tiles.erase(reserved);
		}
	}
	if (state->get_game_entities().contains(it->second.foundation_id)) {
		state->remove_game_entity(it->second.foundation_id, time);
	}
	if (it->second.tower_id.has_value()) {
		this->destroy_tower(state, *it->second.tower_id, time);
	}
	this->build_orders.erase(it);
	auto found = state->get_game_entities().find(builder);
	if (found != state->get_game_entities().end()) {
		auto entity = found->second;
		auto position = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto current = position->get_positions().get(time);
		position->set_position(time, current);
		if (entity->has_component(component::component_t::IDLE)) {
			system::Idle::idle(entity, time);
		}
	}
	return true;
}

void FVD::advance_build(const std::shared_ptr<GameState> &state,
                        entity_id_t builder,
                        uint64_t order_id,
                        uint64_t route_revision,
                        bool finish,
                        const time::time_t &time) {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end()
	    or it->second.order->get_id() != order_id
	    or it->second.route_revision != route_revision) {
		return;
	}
	auto found = state->get_game_entities().find(builder);
	if (found == state->get_game_entities().end()) {
		this->cancel_build(state, builder, time);
		return;
	}
	auto entity = found->second;
	auto order = it->second.order;
	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	auto position = std::dynamic_pointer_cast<component::Position>(
		entity->get_component(component::component_t::POSITION));
	auto tile = order->get_target();
	auto current = position->get_positions().get(time);
	auto placed_tower = it->second.tower_id;
	if (not builder_alive(entity, time)
	    or ownership->get_owners().get(time) != order->get_owner()
	    or current != it->second.stand
	    or (not placed_tower.has_value()
	        and this->tile_occupied(state, current.to_tile(), time, builder))
	    or not this->is_tile_reserved(tile)
	    or this->reserved_tiles.at(tile) != builder
	    or (not placed_tower.has_value() and this->tile_occupied(state, tile, time))) {
		this->cancel_build(state, builder, time);
		return;
	}
	if (not finish) {
		auto loop = this->event_loop.lock();
		auto factory = this->entity_factory.lock();
		if (not loop or not factory) [[unlikely]] {
			throw Error{ERR << "FVD construction services no longer exist"};
		}
		if (not state->get_game_entities().contains(it->second.foundation_id)) [[unlikely]] {
			throw Error{ERR << "Pending construction has no foundation entity"};
		}
		state->remove_game_entity(it->second.foundation_id, time);
		auto tower_entity = factory->spawn_game_entity(
			loop, state, order->get_owner(), this->tower, tile.to_phys3_center(), time);
		if (not tower_entity->has_component(component::component_t::LIVE)) [[unlikely]] {
			throw Error{ERR << "FVD tower requires a Live ability"};
		}
		auto live = std::dynamic_pointer_cast<component::Live>(
			tower_entity->get_component(component::component_t::LIVE));
		auto health = nyan::fqon_t{"fvd.data.entities.Health"};
		int64_t max_health = 0;
		for (const auto &setting : live->get_ability().get_set("Live.attributes")) {
			auto value = std::dynamic_pointer_cast<nyan::ObjectValue>(setting.get_ptr());
			auto object = live->get_ability().get_view()->get_object(value->get_name());
			if (object.get_object("AttributeSetting.attribute").get_name() == health) {
				max_health = object.get_int("AttributeSetting.max_value");
				break;
			}
		}
		if (max_health <= 0) [[unlikely]] {
			throw Error{ERR << "FVD tower requires a positive maximum health"};
		}
		live->set_attribute_progress(time, time + this->construction_time,
		                             health, 0, max_health);
		system::Idle::idle(tower_entity, time);
		this->block_tower(state, tower_entity->get_id(), tile, true, time);
		it->second.tower_id = tower_entity->get_id();
		system::Idle::idle(entity, time);
		loop->create_event(
			"game.fvd_build_progress", entity->get_manager(), state,
			time + this->construction_time,
			{{"builder", builder}, {"order", order_id},
			 {"route", route_revision}, {"finish", true}});
		return;
	}

	if (not placed_tower.has_value() or not this->tower_footprints.contains(*placed_tower)) [[unlikely]] {
		throw Error{ERR << "Completed construction has no tower footprint"};
	}
	this->tower_footprints.at(*placed_tower).under_construction = false;
	for (const auto &footprint_tile : this->get_tower_footprint(tile)) {
		this->reserved_tiles.erase(footprint_tile);
	}
	this->build_orders.erase(it);
	system::Idle::idle(entity, time);
}

void FVD::refresh_paths(const std::shared_ptr<GameState> &state,
                        const time::time_t &time) {
	std::vector<entity_id_t> destroyed;
	for (const auto &[tower_id, footprint] : this->tower_footprints) {
		if (footprint.under_construction) {
			continue;
		}
		auto found = state->get_game_entities().find(tower_id);
		if (found == state->get_game_entities().end()) {
			destroyed.push_back(tower_id);
			continue;
		}
		auto tower_entity = found->second;
		if (tower_entity->has_component(component::component_t::LIVE)) {
			auto live = std::dynamic_pointer_cast<component::Live>(
				tower_entity->get_component(component::component_t::LIVE));
			if (live->get_attribute(time, "fvd.data.entities.Health")->get(time) <= 0) {
				destroyed.push_back(tower_id);
			}
		}
	}
	for (auto tower_id : destroyed) {
		this->destroy_tower(state, tower_id, time);
	}

	auto loop = this->event_loop.lock();
	if (not loop) [[unlikely]] {
		throw Error{ERR << "FVD construction event loop no longer exists"};
	}
	for (auto &[builder_id, build] : this->build_orders) {
		if (build.tower_id.has_value()) {
			continue;
		}
		auto found = state->get_game_entities().find(builder_id);
		if (found == state->get_game_entities().end() or not builder_alive(found->second, time)) {
			continue;
		}
		auto entity = found->second;
		auto travel = system::Move::move_default(entity, state, build.stand, time);
		build.arrival = time + travel;
		build.route_revision += 1;
		loop->create_event(
			"game.fvd_build_progress", entity->get_manager(), state, build.arrival,
			{{"builder", builder_id}, {"order", build.order->get_id()},
			 {"route", build.route_revision}, {"finish", false}});
	}

	for (const auto &[entity_id, entity] : state->get_game_entities()) {
		if (this->build_orders.contains(entity_id)
		    or not entity->has_component(component::component_t::MOVE)) {
			continue;
		}
		auto move = std::dynamic_pointer_cast<component::Move>(
			entity->get_component(component::component_t::MOVE));
		if (not move->get_active_target().has_value()
		    or move->get_active_arrival() <= time) {
			continue;
		}
		auto activity_component = std::dynamic_pointer_cast<component::Activity>(
			entity->get_component(component::component_t::ACTIVITY));
		auto node = activity_component->get_node(time);
		if (node->get_type() != activity::node_t::XOR_EVENT_GATE) {
			continue;
		}
		auto target = *move->get_active_target();
		auto duration = system::Move::move_default(entity, state, target, time);
		activity_component->cancel_events(time);
		auto gate = std::static_pointer_cast<activity::XorEventGate>(node);
		for (const auto &[next_id, primer] : gate->get_primers()) {
			activity_component->add_event(primer(time + duration, entity, loop, state, next_id));
		}
	}
}

bool FVD::destroy_tower(const std::shared_ptr<GameState> &state,
                        entity_id_t tower_id,
                        const time::time_t &time) {
	if (not this->tower_footprints.contains(tower_id)) {
		return false;
	}
	this->unblock_tower(state, tower_id, time);
	if (state->get_game_entities().contains(tower_id)) {
		state->remove_game_entity(tower_id, time);
	}
	return true;
}

std::shared_ptr<component::command::BuildTowerCommand> FVD::get_build_order(
	entity_id_t builder) const {
	auto it = this->build_orders.find(builder);
	return it == this->build_orders.end() ? nullptr : it->second.order;
}

bool FVD::is_tile_reserved(const coord::tile &tile) const {
	return this->reserved_tiles.contains(tile);
}

void FVD::start_hunter_wave(const std::shared_ptr<GameState> &state,
                            const time::time_t &match_start) {
	if (this->hunter_wave_started) {
		return;
	}
	auto loop = this->event_loop.lock();
	if (not loop or not this->horse_spawn_target) {
		throw Error{ERR << "Cannot start the Hunter wave before FVD initialization"};
	}
	this->hunter_wave_started = true;
	loop->create_event("game.fvd_spawn_horse", this->horse_spawn_target, state,
	                   match_start + this->horse_spawn_delay);
}

void FVD::spawn_hunter_horse(const std::shared_ptr<GameState> &state,
                            const time::time_t &time) {
	if (not this->hunter_wave_started) {
		return;
	}
	auto loop = this->event_loop.lock();
	auto factory = this->entity_factory.lock();
	if (not loop or not factory or not this->horse_spawn_target) {
		throw Error{ERR << "Cannot spawn a Hunter horse without FVD services"};
	}
	factory->spawn_game_entity(loop, state, 1, this->horse, this->get_centre(), time);
	loop->create_event("game.fvd_spawn_horse", this->horse_spawn_target, state,
	                   time + this->horse_spawn_interval);
}

coord::phys3 FVD::get_centre() const {
	auto centre = coord::phys_t::from_int(this->map_size / 2);
	return {centre, centre, 0};
}

size_t FVD::get_player_count() const {
	return this->player_count;
}

} // namespace openage::gamestate
