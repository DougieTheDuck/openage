// Copyright 2026 the openage authors. See copying.md for legal info.

#include "build_tower.h"

namespace openage::gamestate::component::command {

BuildTowerCommand::BuildTowerCommand(uint64_t id,
                                     player_id_t owner,
                                     entity_id_t builder,
                                     const coord::tile &target) :
	id{id},
	owner{owner},
	builder{builder},
	target{target} {}

command_t BuildTowerCommand::get_type() const {
	return command_t::BUILD_TOWER;
}

uint64_t BuildTowerCommand::get_id() const {
	return this->id;
}

player_id_t BuildTowerCommand::get_owner() const {
	return this->owner;
}

entity_id_t BuildTowerCommand::get_builder() const {
	return this->builder;
}

const coord::tile &BuildTowerCommand::get_target() const {
	return this->target;
}

} // namespace openage::gamestate::component::command
