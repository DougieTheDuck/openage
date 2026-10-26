// Copyright 2022-2024 the openage authors. See copying.md for legal info.

#include "game_entity.h"

#include <algorithm>

#include "error/error.h"
#include "gamestate/api/ability.h"
#include "gamestate/api/animation.h"
#include "gamestate/api/property.h"
#include "gamestate/component/api/idle.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/base_component.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "log/message.h"
#include "renderer/stages/world/render_entity.h"

namespace openage::gamestate {

GameEntity::GameEntity(entity_id_t id) :
	id{id},
	components{},
	render_entity{nullptr} {
}

std::shared_ptr<GameEntity> GameEntity::copy(entity_id_t id) {
	auto copy = std::shared_ptr<GameEntity>(new GameEntity(*this));
	copy->set_id(id);

	return copy;
}

entity_id_t GameEntity::get_id() const {
	return this->id;
}

const std::string &GameEntity::get_definition() const {
	return this->definition;
}

void GameEntity::set_definition(const std::string &definition) {
	this->definition = definition;
}

void GameEntity::set_render_entity(const std::shared_ptr<renderer::world::RenderEntity> &entity) {
	// TODO: Transfer state from old render entity to new one?

	this->render_entity = entity;
}

const std::shared_ptr<renderer::world::RenderEntity> &GameEntity::get_render_entity() const {
	return this->render_entity;
}

void GameEntity::set_manager(const std::shared_ptr<GameEntityManager> &manager) {
	this->manager = manager;
}

const std::shared_ptr<GameEntityManager> &GameEntity::get_manager() const {
	return this->manager;
}

const std::shared_ptr<component::Component> &GameEntity::get_component(component::component_t type) {
	return this->components.at(type);
}

void GameEntity::add_component(const std::shared_ptr<component::Component> &component) {
	this->components.insert({component->get_type(), component});
}

bool GameEntity::has_component(component::component_t type) {
	return this->components.contains(type);
}

void GameEntity::render_update(const time::time_t &time,
                               const std::string &animation_path,
                               AnimationAction action) {
	this->render_update(time, std::vector<std::string>{animation_path}, action);
}

void GameEntity::render_update(const time::time_t &time,
                               const std::vector<std::string> &animation_paths,
                               AnimationAction action) {
	if (animation_paths.empty()) [[unlikely]] {
		throw Error{ERR << "Entity " << this->id << " has no animation variants."};
	}
	auto sorted_paths = animation_paths;
	std::sort(sorted_paths.begin(), sorted_paths.end());
	if (not this->animation_initialized or this->animation_action != action
	    or this->animation_paths != sorted_paths
	    // Each attack volley must restart its pose phase at the same
	    // authoritative time used for its 75%-cycle hit event.
	    or (action == AnimationAction::ATTACK and time > this->animation_started_at)) {
		this->animation_action = action;
		this->animation_started_at = time;
		this->animation_paths = sorted_paths;
		this->animation_initialized = true;
	}
	if (this->render_entity != nullptr) {
		if (this->has_component(component::component_t::OWNERSHIP)) {
			auto ownership = dynamic_pointer_cast<component::Ownership>(
				this->components.at(component::component_t::OWNERSHIP));
			const auto mode = this->has_component(component::component_t::MOVE)
			                    ? renderer::world::occlusion_mode::OUTLINED
			                    : renderer::world::occlusion_mode::OCCLUDER;
			this->render_entity->set_occlusion_mode(mode, ownership->get_owners().get(time));
		}
		const auto &pos = dynamic_pointer_cast<component::Position>(
							  this->components.at(component::component_t::POSITION))
		                      ->get_positions();
		const auto &angle = dynamic_pointer_cast<component::Position>(
								this->components.at(component::component_t::POSITION))
		                        ->get_angles();
		this->render_entity->update(
			this->id, pos, angle, sorted_paths, time, this->animation_started_at);

		if (this->has_component(component::component_t::LIVE)) {
			auto live = dynamic_pointer_cast<component::Live>(
				this->components.at(component::component_t::LIVE));
			auto attribute_settings = live->get_ability().get_set("Live.attributes");
			for (const auto &setting : attribute_settings) {
				auto setting_value = std::dynamic_pointer_cast<nyan::ObjectValue>(setting.get_ptr());
				auto setting_object = live->get_ability().get_view()->get_object(setting_value->get_name());
				auto attribute = setting_object.get_object("AttributeSetting.attribute");
				auto attribute_name = attribute.get_name();
				if (attribute_name.ends_with(".Health")) {
					auto max_health = setting_object.get_int("AttributeSetting.max_value");
					this->render_entity->update_health(
						*live->get_attribute(time, attribute_name), max_health, time);
					break;
				}
			}
		}
	}
}

AnimationAction GameEntity::get_animation_action() const {
	return this->animation_action;
}

time::time_t GameEntity::get_animation_started_at() const {
	return this->animation_started_at;
}

void GameEntity::deactivate_render(const time::time_t &time) {
	if (this->render_entity != nullptr) {
		this->render_entity->deactivate(time);
	}
}

void GameEntity::set_id(entity_id_t id) {
	this->id = id;
}

} // namespace openage::gamestate
