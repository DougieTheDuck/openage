// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#include "move.h"

#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Move::get_type() const {
	return component_t::MOVE;
}

void Move::set_active_route(const coord::phys3 &target, const time::time_t &arrival) {
	this->active_target = target;
	this->active_arrival = arrival;
}

void Move::clear_active_route() {
	this->active_target = std::nullopt;
	this->active_arrival = time::TIME_MIN;
}

const std::optional<coord::phys3> &Move::get_active_target() const {
	return this->active_target;
}

const time::time_t &Move::get_active_arrival() const {
	return this->active_arrival;
}

} // namespace openage::gamestate::component
