// Copyright 2022-2024 the openage authors. See copying.md for legal info.

#include "render_entity.h"

#include <functional>
#include <mutex>

#include "renderer/definitions.h"


namespace openage::renderer::world {

RenderEntity::RenderEntity(bool pickable) :
	renderer::RenderEntity{},
	ref_id{0},
	position{nullptr, 0, "", nullptr, SCENE_ORIGIN},
	angle{nullptr, 0, "", nullptr, 0},
	animation_path{nullptr, 0},
	health_available{false},
	active{true},
	pickable{pickable},
	health{nullptr, 0, "", nullptr, 0},
	max_health{0} {
}

void RenderEntity::update(const uint32_t ref_id,
                          const curve::Continuous<coord::phys3> &position,
                          const curve::Segmented<coord::phys_angle_t> &angle,
                          const std::string animation_path,
                          const time::time_t time,
                          std::optional<time::time_t> animation_started_at) {
	std::unique_lock lock{this->mutex};

	this->ref_id = ref_id;
	std::function<coord::scene3(const coord::phys3 &)> to_scene3 = [](const coord::phys3 &pos) {
		return pos.to_scene3();
	};
	this->position.sync(position,
	                    std::function<coord::scene3(const coord::phys3 &)>([](const coord::phys3 &pos) {
							return pos.to_scene3();
						}),
	                    this->last_update);
	this->angle.sync(angle, this->last_update);
	const auto start_at = animation_started_at.value_or(time);
	const auto current = this->animation_path.get_time(time);
	if (current.second != animation_path or current.first != start_at) {
		this->animation_path.set_last(start_at, animation_path);
	}
	this->changed = true;
	this->last_update = time;
}

void RenderEntity::update_facing(
	coord::phys_angle_t sampled, time::time_t time,
	const std::vector<std::pair<time::time_t, coord::phys_angle_t>> &future_keys) {
	std::unique_lock lock{this->mutex};
	curve::Segmented<coord::phys_angle_t> new_angles{nullptr, 0, "", nullptr, sampled};
	new_angles.set_last(time, sampled);
	auto last_angle = sampled;
	auto last_time = time;
	for (const auto &[at, angle] : future_keys) {
		if (at <= last_time) {
			throw Error{MSG(err) << "Replicated facing keys must be strictly future-ordered"};
		}
		new_angles.set_insert_jump(at, last_angle, angle);
		last_angle = angle;
		last_time = at;
	}
	this->angle.sync(new_angles);
	this->changed = true;
	this->last_update = time;
}

void RenderEntity::update_health(const curve::Segmented<int64_t> &health,
                                 int64_t max_health,
                                 const time::time_t time) {
	std::unique_lock lock{this->mutex};

	this->health.sync(health);
	this->max_health = max_health;
	this->health_available = true;
	this->changed = true;
	this->last_update = time;
}

void RenderEntity::update(const uint32_t ref_id,
                          const coord::phys3 position,
                          const std::string animation_path,
                          const time::time_t time) {
	std::unique_lock lock{this->mutex};

	this->ref_id = ref_id;
	this->position.set_last(time, position.to_scene3());
	this->animation_path.set_last(time, animation_path);
	this->changed = true;
	this->last_update = time;
}

void RenderEntity::update_sample(uint32_t ref_id,
                                 const coord::phys3 &position,
                                 const std::string &animation_path,
                                 time::time_t time,
                                 std::optional<time::time_t> interpolate_from,
                                 const std::vector<std::pair<time::time_t, coord::phys3>> &future_keys,
                                 std::optional<time::time_t> animation_started_at) {
	std::unique_lock lock{this->mutex};
	if (interpolate_from and *interpolate_from > time) {
		throw Error{MSG(err) << "Position sample precedes presentation time"};
	}

	const auto start = interpolate_from ? this->position.get(*interpolate_from) : position.to_scene3();
	curve::Continuous<coord::scene3> samples{nullptr, 0, "", nullptr, start};
	if (interpolate_from) {
		samples.set_last(*interpolate_from, start);
		for (const auto &frame : this->position.get_container()) {
			if (frame.time() > *interpolate_from and frame.time() < time) {
				samples.set_last(frame.time(), frame.val());
			}
		}
	}
	samples.set_last(time, position.to_scene3());
	auto previous_time = time;
	for (const auto &[at, next_position] : future_keys) {
		if (at <= previous_time) {
			throw Error{MSG(err) << "Replicated movement curve is not strictly future-ordered"};
		}
		samples.set_last(at, next_position.to_scene3());
		previous_time = at;
	}
	this->position.sync(samples);
	const auto start_at = animation_started_at.value_or(interpolate_from.value_or(time));
	const auto current_animation = this->animation_path.get_time(time);
	if (not interpolate_from or current_animation.second != animation_path
	    or (animation_started_at and current_animation.first != *animation_started_at)) {
		// Match the authoritative phase even if the first guest snapshot
		// arrives mid-loop. Clock-only deltas must not restart the animation.
		this->animation_path.set_last(start_at, animation_path);
	}
	this->ref_id = ref_id;
	this->changed = true;
	this->last_update = time;
}

uint32_t RenderEntity::get_id() {
	std::shared_lock lock{this->mutex};

	return this->ref_id;
}

const curve::Continuous<coord::scene3> &RenderEntity::get_position() {
	return this->position;
}

const curve::Segmented<coord::phys_angle_t> &RenderEntity::get_angle() {
	return this->angle;
}

const curve::Discrete<std::string> &RenderEntity::get_animation_path() {
	return this->animation_path;
}

bool RenderEntity::has_health() {
	std::shared_lock lock{this->mutex};

	return this->health_available;
}

void RenderEntity::deactivate(const time::time_t time) {
	std::unique_lock lock{this->mutex};

	this->active = false;
	this->changed = true;
	this->last_update = time;
}

bool RenderEntity::is_active() {
	std::shared_lock lock{this->mutex};

	return this->active;
}

bool RenderEntity::is_pickable() const {
	return this->pickable;
}

coord::scene3 RenderEntity::get_position_at(const time::time_t &time) {
	std::shared_lock lock{this->mutex};

	return this->position.get(time);
}

int64_t RenderEntity::get_health_at(const time::time_t &time) {
	std::shared_lock lock{this->mutex};

	return this->health.get(time);
}

int64_t RenderEntity::get_max_health() {
	std::shared_lock lock{this->mutex};

	return this->max_health;
}

} // namespace openage::renderer::world
