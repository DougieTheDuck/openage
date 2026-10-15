// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd.h"

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <nyan/nyan.h>

#include "curve/discrete.h"
#include "error/error.h"
#include "event/event_loop.h"
#include "event/eventhandler.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/internal/activity.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/build_tower.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
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
	auto build_time = settings.get_int("construction_time");
	if (size < 16 or size > 512 or chunk < 4 or chunk > size
	    or chunk > MAX_CHUNK_WIDTH or chunk > MAX_CHUNK_HEIGHT
	    or size % chunk != 0 or players < 2 or players > 8
	    or build_time < 1 or build_time > 300) {
		throw Error{ERR << "Invalid FVD settings: map 16..512, chunk 4.."
		                << MAX_CHUNK_WIDTH << ", players 2..8, construction 1..300 seconds"};
	}
	this->map_size = size;
	this->chunk_size = chunk;
	this->player_count = players;
	this->construction_time = build_time;
	this->hunter = settings.get_object("hunter").get_name();
	this->builders = settings.get_object("builders").get_name();
	this->builder = settings.get_object("builder").get_name();
	this->tower = settings.get_object("tower").get_name();
	this->plains = settings.get_object("terrain").get_name();
}

void FVD::initialise(const std::shared_ptr<GameState> &state,
                     const std::shared_ptr<openage::event::EventLoop> &loop,
                     const std::shared_ptr<EntityFactory> &entities,
                     const std::shared_ptr<TerrainFactory> &terrain) {
	this->event_loop = loop;
	this->entity_factory = entities;
	loop->add_event_handler(std::make_shared<BuildProgressHandler>());

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
	for (size_t id = 0; id <= this->player_count; ++id) {
		auto setup = id == 0 ? "" : (id == 1 ? this->hunter : this->builders);
		state->add_player(entities->add_player(loop, state, setup));
		if (id >= 2) {
			auto centre = this->get_centre();
			auto offset = static_cast<int>(id) - static_cast<int>((this->player_count + 2) / 2);
			entities->spawn_game_entity(loop, state, id, this->builder,
			                           centre + coord::phys3_delta{offset, 1, 0}, time::TIME_ZERO);
		}
	}
}

bool FVD::tile_occupied(const std::shared_ptr<GameState> &state,
                        const coord::tile &tile,
                        const time::time_t &time,
                        std::optional<entity_id_t> ignore) const {
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (ignore.has_value() and id == *ignore) {
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
	if (this->is_tile_reserved(site) or this->tile_occupied(state, site, time)) {
		return false;
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

	auto order = std::make_shared<component::command::BuildTowerCommand>(
		this->next_build_id++, owner, builder_id, site);
	this->reserved_tiles.emplace(site, builder_id);
	auto travel = *stand == current_position
	            ? time::TIME_ZERO
	            : system::Move::move_default(entity, state, *stand, time);
	auto arrival = time + travel;
	this->build_orders.emplace(builder_id, PendingBuild{order, *stand, arrival});

	auto loop = this->event_loop.lock();
	if (not loop) {
		throw Error{ERR << "FVD construction event loop no longer exists"};
	}
	loop->create_event("game.fvd_build_progress", entity->get_manager(), state, arrival,
	                   {{"builder", builder_id}, {"order", order->get_id()}, {"finish", false}});
	return true;
}

bool FVD::cancel_build(const std::shared_ptr<GameState> &state,
                       entity_id_t builder,
                       const time::time_t &time) {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end()) {
		return false;
	}
	auto reserved = this->reserved_tiles.find(it->second.order->get_target());
	if (reserved != this->reserved_tiles.end() and reserved->second == builder) {
		this->reserved_tiles.erase(reserved);
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
                        bool finish,
                        const time::time_t &time) {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end() or it->second.order->get_id() != order_id) {
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
	if (not builder_alive(entity, time)
	    or ownership->get_owners().get(time) != order->get_owner()
	    or current != it->second.stand
	    or this->tile_occupied(state, current.to_tile(), time, builder)
	    or not this->is_tile_reserved(tile)
	    or this->reserved_tiles.at(tile) != builder
	    or this->tile_occupied(state, tile, time)) {
		this->cancel_build(state, builder, time);
		return;
	}
	if (not finish) {
		auto loop = this->event_loop.lock();
		loop->create_event(
			"game.fvd_build_progress", entity->get_manager(), state,
			time + time::time_t::from_int(this->construction_time),
			{{"builder", builder}, {"order", order_id}, {"finish", true}});
		return;
	}

	auto loop = this->event_loop.lock();
	auto factory = this->entity_factory.lock();
	if (not loop or not factory) {
		throw Error{ERR << "FVD construction services no longer exist"};
	}
	factory->spawn_game_entity(loop, state, order->get_owner(), this->tower,
	                           tile.to_phys3_center(), time);
	this->reserved_tiles.erase(tile);
	this->build_orders.erase(it);
	system::Idle::idle(entity, time);
}

std::shared_ptr<component::command::BuildTowerCommand> FVD::get_build_order(
	entity_id_t builder) const {
	auto it = this->build_orders.find(builder);
	return it == this->build_orders.end() ? nullptr : it->second.order;
}

bool FVD::is_tile_reserved(const coord::tile &tile) const {
	return this->reserved_tiles.contains(tile);
}

coord::phys3 FVD::get_centre() const {
	auto centre = coord::phys_t::from_int(this->map_size / 2);
	return {centre, centre, 0};
}

size_t FVD::get_player_count() const {
	return this->player_count;
}

} // namespace openage::gamestate
