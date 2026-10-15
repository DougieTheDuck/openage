// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd.h"

#include <nyan/nyan.h>

#include "curve/discrete.h"
#include "error/error.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/commands/build_tower.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/entity_factory.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/map.h"
#include "gamestate/player.h"
#include "gamestate/terrain_chunk.h"
#include "gamestate/terrain_factory.h"
#include "log/log.h"

namespace openage::gamestate {

FVD::FVD(const std::shared_ptr<GameState> &state) {
	auto settings = state->get_db_view()->get_object("fvd.data.scenario.Scenario");
	auto size = settings.get_int("map_size");
	auto chunk = settings.get_int("chunk_size");
	auto players = settings.get_int("player_count");
	if (size < 16 or size > 512 or chunk < 4 or chunk > size
	    or chunk > MAX_CHUNK_WIDTH or chunk > MAX_CHUNK_HEIGHT
	    or size % chunk != 0 or players < 2 or players > 8) {
		throw Error{ERR << "Invalid FVD settings: map size must be 16..512 and divisible "
		                  "by chunk size (4.." << MAX_CHUNK_WIDTH
		               << "); player count must be 2..8"};
	}
	this->map_size = size;
	this->chunk_size = chunk;
	this->player_count = players;
	this->hunter = settings.get_object("hunter").get_name();
	this->builders = settings.get_object("builders").get_name();
	this->builder = settings.get_object("builder").get_name();
	this->tower = settings.get_object("tower").get_name();
	this->plains = settings.get_object("terrain").get_name();
}

void FVD::initialise(const std::shared_ptr<GameState> &state,
                     const std::shared_ptr<openage::event::EventLoop> &loop,
                     const std::shared_ptr<EntityFactory> &entities,
                     const std::shared_ptr<TerrainFactory> &terrain) const {
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

bool FVD::request_build(const std::shared_ptr<GameState> &state,
                        player_id_t owner,
                        const std::vector<entity_id_t> &selected,
                        const coord::phys3 &position,
                        const time::time_t &time) {
	if (owner < 2 or owner > this->player_count) {
		log::log(WARN << "Only Builder players can place towers");
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
	if (not allowed) {
		log::log(WARN << "This civilisation cannot place towers");
		return false;
	}

	if (selected.size() != 1) {
		log::log(WARN << "Select exactly one Builder before ordering a tower");
		return false;
	}
	auto builder_id = selected.front();
	auto found = state->get_game_entities().find(builder_id);
	if (found == state->get_game_entities().end()) {
		log::log(WARN << "Builder does not exist");
		return false;
	}
	auto entity = found->second;
	if (entity->get_definition() != this->builder
	    or not entity->has_component(component::component_t::OWNERSHIP)
	    or not entity->has_component(component::component_t::LIVE)) {
		log::log(WARN << "The selected entity is not a living Builder");
		return false;
	}
	auto ownership = std::dynamic_pointer_cast<component::Ownership>(
		entity->get_component(component::component_t::OWNERSHIP));
	if (ownership->get_owners().get(time) != owner) {
		log::log(WARN << "Cannot order another player's Builder");
		return false;
	}
	auto live = std::dynamic_pointer_cast<component::Live>(
		entity->get_component(component::component_t::LIVE));
	if (live->get_attribute(time, "fvd.data.entities.Health")->get(time) <= 0) {
		log::log(WARN << "A dead Builder cannot construct");
		return false;
	}
	if (position.ne < 0 or position.se < 0
	    or position.ne >= this->map_size or position.se >= this->map_size
	    or position.up != 0) {
		log::log(WARN << "Tower placement must be on the plains map");
		return false;
	}
	auto tile = position.to_tile();
	if (this->is_tile_reserved(tile)) {
		log::log(WARN << "Tower placement tile is reserved");
		return false;
	}
	for (const auto &[id, candidate] : state->get_game_entities()) {
		auto pos = std::dynamic_pointer_cast<component::Position>(
			candidate->get_component(component::component_t::POSITION));
		if (pos->get_positions().get(time).to_tile() == tile) {
			log::log(WARN << "Tower placement tile is occupied");
			return false;
		}
	}

	// The validated FVD map is currently entirely flat, buildable plains.
	// A later terrain variant must also check the target's terrain type.
	this->cancel_build(builder_id);
	auto order = std::make_shared<component::command::BuildTowerCommand>(
		this->next_build_id++, owner, builder_id, tile);
	this->build_orders.emplace(builder_id, order);
	this->reserved_tiles.emplace(tile, builder_id);
	return true;
}

bool FVD::cancel_build(entity_id_t builder) {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end()) {
		return false;
	}

	auto reserved = this->reserved_tiles.find(it->second->get_target());
	if (reserved != this->reserved_tiles.end() and reserved->second == builder) {
		this->reserved_tiles.erase(reserved);
	}
	this->build_orders.erase(it);
	return true;
}

std::shared_ptr<component::command::BuildTowerCommand> FVD::get_build_order(
	entity_id_t builder) const {
	auto it = this->build_orders.find(builder);
	if (it == this->build_orders.end()) {
		return nullptr;
	}
	return it->second;
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

}
