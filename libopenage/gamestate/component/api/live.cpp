// Copyright 2021-2025 the openage authors. See copying.md for legal info.

#include "live.h"

#include <optional>

#include "curve/container/iterator.h"
#include "curve/container/map_filter_iterator.h"
#include "curve/segmented.h"
#include "error/error.h"
#include "gamestate/component/types.h"


namespace openage::gamestate::component {

component_t Live::get_type() const {
	return component_t::LIVE;
}

void Live::add_attribute(const time::time_t &time,
                         const nyan::fqon_t &attribute,
                         std::shared_ptr<curve::Segmented<int64_t>> starting_values) {
	this->attribute_values.insert(time, attribute, starting_values);
}

void Live::set_attribute(const time::time_t &time,
                         const nyan::fqon_t &attribute,
                         int64_t value) {
	auto attribute_value = this->attribute_values.at(time, attribute);

	if (attribute_value) {
		auto &curve = **attribute_value;
		curve->set_last_jump(time, curve->get(time), value);
	}
	else {
		throw Error{ERR << "Attribute '" << attribute
		                << "' does not exist at time " << time};
	}
}

void Live::set_attribute_progress(const time::time_t &start_time,
                                  const time::time_t &end_time,
                                  const nyan::fqon_t &attribute,
                                  int64_t start_value,
                                  int64_t end_value) {
	if (end_time <= start_time) [[unlikely]] {
		throw Error{ERR << "Attribute progress must end after it starts"};
	}
	auto attribute_value = this->attribute_values.at(start_time, attribute);
	if (not attribute_value) [[unlikely]] {
		throw Error{ERR << "Attribute '" << attribute
		                << "' does not exist at time " << start_time};
	}

	auto &curve = **attribute_value;
	curve->set_last(start_time, start_value);
	curve->set_insert(end_time, end_value);
}

const std::shared_ptr<curve::Segmented<int64_t>> &Live::get_attribute(
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
