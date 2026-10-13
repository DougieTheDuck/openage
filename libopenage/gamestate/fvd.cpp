// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd.h"

#include <nyan/nyan.h>

#include "error/error.h"
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

bool FVD::place_tower(const std::shared_ptr<GameState> &state,
                      const std::shared_ptr<openage::event::EventLoop> &loop,
                      const std::shared_ptr<EntityFactory> &entities,
                      player_id_t owner,
                      const std::vector<entity_id_t> &selected,
                      const coord::phys3 &position,
                      const time::time_t &time) const {
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

	bool has_builder = false;
	for (auto id : selected) {
		auto found = state->get_game_entities().find(id);
		if (found == state->get_game_entities().end()) {
			continue;
		}
		auto entity = found->second;
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (entity->get_definition() == this->builder and ownership->get_owners().get(time) == owner) {
			has_builder = true;
		}
	}
	if (not has_builder) {
		log::log(WARN << "Select one of your Builders before placing a tower");
		return false;
	}
	if (position.ne < 0 or position.se < 0
	    or position.ne >= this->map_size or position.se >= this->map_size
	    or position.up != 0) {
		log::log(WARN << "Tower placement must be on the plains map");
		return false;
	}
	auto tile = position.to_tile();
	auto site = tile.to_phys3_center();
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto pos = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		if (pos->get_positions().get(time).to_tile() == tile) {
			log::log(WARN << "Tower placement tile is occupied");
			return false;
		}
	}
	entities->spawn_game_entity(loop, state, owner, this->tower, site, time);
	return true;
}

coord::phys3 FVD::get_centre() const {
	auto centre = coord::phys_t::from_int(this->map_size / 2);
	return {centre, centre, 0};
}

size_t FVD::get_player_count() const {
	return this->player_count;
}

}
