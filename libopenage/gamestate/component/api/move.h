// Copyright 2021-2024 the openage authors. See copying.md for legal info.

#pragma once

#include <optional>

#include "coord/phys.h"
#include "gamestate/component/api_component.h"
#include "gamestate/component/types.h"
#include "time/time.h"


namespace openage::gamestate::component {

class Move final : public APIComponent {
public:
	using APIComponent::APIComponent;

	component_t get_type() const override;

	/** Record the destination and expected arrival of the current route. */
	void set_active_route(const coord::phys3 &target, const time::time_t &arrival);

	/** Clear the current route when movement stops. */
	void clear_active_route();

	/** Get the current route destination, if the entity is moving. */
	const std::optional<coord::phys3> &get_active_target() const;

	/** Get the expected arrival time of the current route. */
	const time::time_t &get_active_arrival() const;

private:
	std::optional<coord::phys3> active_target;
	time::time_t active_arrival = time::TIME_MIN;
};

} // namespace openage::gamestate::component
