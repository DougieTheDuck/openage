// Copyright 2021-2025 the openage authors. See copying.md for legal info.

#include "live.h"

#include <optional>

#include "curve/container/iterator.h"
#include "curve/container/map_filter_iterator.h"
#include "curve/discrete.h"
#include "error/error.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Live::get_type() const {
	return component_t::LIVE;
}

void Live::add_attribute(const time::time_t &time,
                         const nyan::fqon_t &attribute,
                         std::shared_ptr<curve::Discrete<int64_t>> starting_values) {
	this->attribute_values.insert(time, attribute, starting_values);
}

void Live::set_attribute(const time::time_t &time,
                         const nyan::fqon_t &attribute,
                         int64_t value) {
	auto attribute_value = this->attribute_values.at(time, attribute);

	if (attribute_value) {
		(**attribute_value)->set_last(time, value);
	}
	else {
		throw Error{ERR << "Attribute '" << attribute
		                << "' does not exist at time " << time};
	}
}

const std::shared_ptr<curve::Discrete<int64_t>> &Live::get_attribute(
	const time::time_t &time,
	const nyan::fqon_t &attribute) const {
	auto attribute_value = this->attribute_values.at(time, attribute);
	if (not attribute_value) {
		throw Error{ERR << "Attribute '" << attribute
		                << "' does not exist at time " << time};
	}

	return **attribute_value;
}
} // namespace openage::gamestate::component
