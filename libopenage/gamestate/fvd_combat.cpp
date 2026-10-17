// Copyright 2026 the openage authors. See copying.md for legal info.

#include "fvd.h"

#include <algorithm>
#include <array>
#include <optional>
#include <cmath>
#include <limits>
#include <vector>

#include "event/event_loop.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/internal/activity.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/entity_factory.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/map.h"
#include "gamestate/manager.h"
#include "gamestate/system/idle.h"
#include "gamestate/system/move.h"
#include "pathfinding/pathfinder.h"
#include "renderer/stages/world/render_entity.h"

namespace openage::gamestate {

namespace {

constexpr const char *HEALTH = "fvd.data.entities.Health";

/** Use stable ordering for simultaneous combat and lethal-hit decisions. */
std::vector<entity_id_t> entities_by_definition(
	const std::shared_ptr<GameState> &state, const std::string &definition) {
	std::vector<entity_id_t> ids;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == definition) {
			ids.push_back(id);
		}
	}
	std::sort(ids.begin(), ids.end());
	return ids;
}

bool alive(const std::shared_ptr<GameEntity> &entity, const time::time_t &time) {
	if (not entity->has_component(component::component_t::LIVE)) {
		return false;
	}
	auto live = std::dynamic_pointer_cast<component::Live>(
		entity->get_component(component::component_t::LIVE));
	return live->get_attribute(time, HEALTH)->get(time) > 0;
}

double distance_squared(const std::shared_ptr<GameEntity> &first,
                        const std::shared_ptr<GameEntity> &second,
                        const time::time_t &time) {
	auto first_position = std::dynamic_pointer_cast<component::Position>(
		first->get_component(component::component_t::POSITION))->get_positions().get(time);
	auto second_position = std::dynamic_pointer_cast<component::Position>(
		second->get_component(component::component_t::POSITION))->get_positions().get(time);
	auto delta = first_position - second_position;
	double x = delta.ne.to_double();
	double y = delta.se.to_double();
	return x * x + y * y;
}


/** Stop a previous route at the authoritative time of an engagement. */
void halt_for_engagement(const std::shared_ptr<GameEntity> &horse,
                         const time::time_t &time) {
	auto position = std::dynamic_pointer_cast<component::Position>(
		horse->get_component(component::component_t::POSITION));
	position->set_position(time, position->get_positions().get(time));
	auto activity = std::dynamic_pointer_cast<component::Activity>(
		horse->get_component(component::component_t::ACTIVITY));
	activity->cancel_events(time);
	system::Idle::idle(horse, time);
}

/**
 * Find an accessible tile in melee range of a target unit or tower.
 * Destinations are ranked deterministically by travel proximity.
 */
std::optional<coord::phys3> melee_approach(
	const std::shared_ptr<GameState> &state,
	const std::shared_ptr<GameEntity> &horse,
	const std::shared_ptr<GameEntity> &target,
	const time::time_t &time, size_t map_size) {
	const auto target_position = std::dynamic_pointer_cast<component::Position>(
		target->get_component(component::component_t::POSITION))->get_positions().get(time);
	const auto horse_position = std::dynamic_pointer_cast<component::Position>(
		horse->get_component(component::component_t::POSITION))->get_positions().get(time);
	const auto site = target_position.to_tile();
	const std::array<coord::tile_delta, 8> offsets{{
		{-1, 0}, {1, 0}, {0, -1}, {0, 1},
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
	}};
	const auto map = state->get_map();
	const auto grid = map->get_grid_id("fvd.data.terrain.Land");
	const auto pathfinder = map->get_pathfinder();
	std::optional<coord::phys3> best;
	double shortest = std::numeric_limits<double>::infinity();
	for (const auto &offset : offsets) {
		coord::tile tile{site.ne + offset.ne, site.se + offset.se};
		if (tile.ne < 0 or tile.se < 0
		    or tile.ne >= static_cast<coord::tile_t>(map_size)
		    or tile.se >= static_cast<coord::tile_t>(map_size)) {
			continue;
		}
		auto destination = tile.to_phys3_center();
		auto to_target = destination - target_position;
		if (to_target.length() > 1.45) {
			continue;
		}
		auto to_horse = destination - horse_position;
		auto proximity = to_horse.length();
		if (proximity >= shortest) {
			continue;
		}
		auto route = pathfinder->get_path(path::PathRequest{
			grid,
			horse_position.to_tile(),
			tile,
			time});
		if (route.status != path::PathResult::FOUND) {
			continue;
		}
		best = destination;
		shortest = proximity;
	}
	return best;
}

} // namespace

uint64_t FVD::get_kills(player_id_t owner) const {
	return owner > 0 and owner < this->player_kills.size()
	         ? this->player_kills[owner].load() : 0;
}

int64_t FVD::get_tower_damage() const {
	return this->tower_damage;
}

int64_t FVD::get_tower_armor() const {
	return this->tower_armor;
}

bool FVD::apply_damage(const std::shared_ptr<GameState> &state,
                       entity_id_t target, int64_t damage,
                       player_id_t owner, bool tower_shot,
                       const time::time_t &time) {
	auto found = state->get_game_entities().find(target);
	if (found == state->get_game_entities().end() or not alive(found->second, time)) {
		return false;
	}
	auto entity = found->second;
	const auto &definition = entity->get_definition();
	if (tower_shot) {
		if (owner < 2 or owner > this->player_count or definition != this->horse) {
			return false;
		}
	}
	else if (owner != 1 or (definition != this->tower and definition != this->builder)) {
		return false;
	}
	const int64_t armour = definition == this->tower ? this->tower_armor : 0;
	const int64_t actual_damage = std::max<int64_t>(1, damage - armour);
	auto live = std::dynamic_pointer_cast<component::Live>(
		entity->get_component(component::component_t::LIVE));
	const int64_t hp = live->get_attribute(time, HEALTH)->get(time);
	const int64_t new_hp = std::max<int64_t>(0, hp - actual_damage);
	live->set_attribute(time, HEALTH, new_hp);

	if (auto render = entity->get_render_entity()) {
		const int64_t max_health = definition == this->tower ? 100 : 25;
		render->update_health(*live->get_attribute(time, HEALTH), max_health, time);
	}
	if (new_hp > 0) {
		return false;
	}

	if (tower_shot) {
		this->player_kills.at(owner).fetch_add(1);
	}
	if (definition == this->tower) {
		this->tower_next_shot.erase(target);
		this->destroy_tower(state, target, time);
	}
	else {
		this->attack_orders.erase(target);
		if (definition == this->horse) {
			this->dead_horses.insert(target);
		}
		if (definition == this->builder) {
			this->cancel_build(state, target, time);
		}
		state->remove_game_entity(target, time);
	}
	return true;
}

void FVD::arrow_hit(const std::shared_ptr<GameState> &state,
                    entity_id_t target, player_id_t owner,
                    int64_t damage, const time::time_t &time) {
	// Flight survives destruction of the firing tower, but an already
	// deceased target cannot grant another kill.
	this->apply_damage(state, target, damage, owner, true, time);
}

void FVD::combat_tick(const std::shared_ptr<GameState> &state,
                      const time::time_t &time) {
	const auto horses = entities_by_definition(state, this->horse);
	const auto builders = entities_by_definition(state, this->builder);
	const auto towers = entities_by_definition(state, this->tower);

	// AttackMove searches three tiles for towers and Builders, even after
	// arrival. Towers take priority; horses approach before striking.
	for (auto id : horses) {
		auto order_it = this->attack_orders.find(id);
		if (order_it == this->attack_orders.end()) {
			continue;
		}
		auto found = state->get_game_entities().find(id);
		if (found == state->get_game_entities().end() or not alive(found->second, time)) {
			this->attack_orders.erase(order_it);
			continue;
		}
		auto horse = found->second;
		auto &order = order_it->second;
		auto move = std::dynamic_pointer_cast<component::Move>(
			horse->get_component(component::component_t::MOVE));
		auto position = std::dynamic_pointer_cast<component::Position>(
			horse->get_component(component::component_t::POSITION));
		auto current = position->get_positions().get(time);
		auto remaining = current - order.destination;
		if (remaining.length() < 0.15 and not order.target) {
			order.reached_destination = true;
		}

		auto eligible = [&](entity_id_t candidate) {
			auto other = state->get_game_entities().find(candidate);
			if (other == state->get_game_entities().end() or not alive(other->second, time)) {
				return false;
			}
			if (other->second->get_definition() == this->tower) {
				auto footprint = this->tower_footprints.find(candidate);
				return footprint != this->tower_footprints.end()
				    and not footprint->second.under_construction;
			}
			return other->second->get_definition() == this->builder;
		};
		// A tower entering the three-tile alert area takes precedence over
		// a Builder already targeted by the same AttackMove.
		if (order.target and eligible(*order.target)
		    and state->get_game_entity(*order.target)->get_definition() == this->builder) {
			for (auto tower_id : towers) {
				if (not eligible(tower_id)) {
					continue;
				}
				auto tower_entity = state->get_game_entity(tower_id);
				auto distance = distance_squared(horse, tower_entity, time);
				if (distance <= 3.0 * 3.0
				    and (distance <= 1.45 * 1.45
				         or melee_approach(state, horse, tower_entity,
				                           time, this->map_size).has_value())) {
					order.target.reset();
					break;
				}
			}
		}
		if (order.target and not eligible(*order.target)) {
			// A destroyed target must not leave an orphaned approach route.
			if (move->get_active_target() and move->get_active_arrival() > time) {
				halt_for_engagement(horse, time);
			}
			order.target.reset();
		}
		if (not order.target) {
			// A tower within three tiles takes priority over a nearby
			// Builder, even if the original move destination was reached.
			entity_id_t nearest = 0;
			double closest = 3.0 * 3.0;
			for (auto tower_id : towers) {
				if (not eligible(tower_id)) {
					continue;
				}
				auto tower_entity = state->get_game_entity(tower_id);
				auto distance = distance_squared(horse, tower_entity, time);
				if (distance <= closest) {
					if (distance <= 1.45 * 1.45
					    or melee_approach(state, horse, tower_entity,
					                      time, this->map_size).has_value()) {
						nearest = tower_id;
						closest = distance;
					}
				}
			}
			if (nearest == 0) {
				// Builders have the same three-tile alert radius, but only
				// become automatic targets when no tower takes precedence.
				closest = 3.0 * 3.0;
				for (auto enemy : builders) {
					if (not eligible(enemy)) {
						continue;
					}
					auto builder_entity = state->get_game_entity(enemy);
					auto distance = distance_squared(horse, builder_entity, time);
					if (distance <= closest
					    and (distance <= 1.45 * 1.45
					         or melee_approach(state, horse, builder_entity,
					                           time, this->map_size).has_value())) {
						closest = distance;
						nearest = enemy;
					}
				}
			}
			if (nearest != 0) {
				order.target = nearest;
				halt_for_engagement(horse, time);
			}
		}
		if (order.target) {
			const auto target = *order.target;
			auto enemy = state->get_game_entities().find(target);
			if (enemy == state->get_game_entities().end()) {
				order.target.reset();
				continue;
			}
			auto distance = distance_squared(horse, enemy->second, time);
			if (distance > 1.45 * 1.45) {
				// Both Builders and towers are pursued to melee range.
				// A moving Builder may require another approach after
				// the current route reaches its last-known position.
				if (not move->get_active_target()
				    or move->get_active_arrival() <= time) {
					auto approach = melee_approach(
						state, horse, enemy->second, time, this->map_size);
					if (approach) {
						system::Move::move_default(horse, state, *approach, time);
					}
					else {
						order.target.reset();
					}
				}
				continue;
			}

			// A horse may enter melee range before its approach curve ends.
			if (move->get_active_target() and move->get_active_arrival() > time) {
				halt_for_engagement(horse, time);
			}
			if (time >= order.next_attack) {
				this->apply_damage(state, target, this->horse_damage, 1, false, time);
				order.next_attack = time + this->horse_cooldown;
				if (not state->get_game_entities().contains(target)) {
					order.target.reset();
				}
			}
			continue;
		}

		// After reaching the destination, remain alert to enemies within
		// three tiles until a human issues another command.
		if (order.reached_destination) {
			if (move->get_active_target()) {
				system::Idle::idle(horse, time);
			}
			continue;
		}
		if (remaining.length() < 0.15) {
			order.reached_destination = true;
			system::Idle::idle(horse, time);
		}
		else if (not move->get_active_target()
		         or move->get_active_arrival() <= time) {
			system::Move::move_default(horse, state, order.destination, time);
		}
	}

	// Completed towers fire automatically, with a deterministic target order.
	for (auto tower_id : towers) {
		auto tower_it = state->get_game_entities().find(tower_id);
		if (tower_it == state->get_game_entities().end()
		    or not alive(tower_it->second, time)) {
			continue;
		}
		auto footprint = this->tower_footprints.find(tower_id);
		if (footprint == this->tower_footprints.end() or footprint->second.under_construction) {
			continue;
		}
		auto ready = this->tower_next_shot.find(tower_id);
		if (ready != this->tower_next_shot.end() and time < ready->second) {
			continue;
		}
		entity_id_t nearest = 0;
		double closest = 7.0 * 7.0;
		for (auto horse_id : horses) {
			auto horse_it = state->get_game_entities().find(horse_id);
			if (horse_it == state->get_game_entities().end()
			    or not alive(horse_it->second, time)) {
				continue;
			}
			const double d = distance_squared(tower_it->second, horse_it->second, time);
			if (d < closest) {
				closest = d;
				nearest = horse_id;
			}
		}
		if (nearest == 0) {
			continue;
		}
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			tower_it->second->get_component(component::component_t::OWNERSHIP));
		const auto owner = ownership->get_owners().get(time);
		auto loop = this->event_loop.lock();
		loop->create_event("game.fvd_arrow_hit", this->horse_spawn_target, state,
		                   time + this->arrow_travel,
		                   {{"target", nearest}, {"owner", owner}, {"damage", this->tower_damage}});
		this->tower_next_shot.insert_or_assign(tower_id, time + this->tower_cooldown);
	}
}

} // namespace openage::gamestate
