// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_bridge.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "error/error.h"
#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/event/send_command.h"
#include "gamestate/fvd.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"

namespace openage::network {

namespace {

gamestate::component::command::command_t simulation_command(CommandKind kind) {
	switch (kind) {
	case CommandKind::MOVE:
		return gamestate::component::command::command_t::MOVE;
	case CommandKind::ATTACK_MOVE:
		return gamestate::component::command::command_t::ATTACK_MOVE;
	case CommandKind::BUILD_TOWER:
		return gamestate::component::command::command_t::BUILD_TOWER;
	default:
		throw Error{MSG(err) << "Unknown authenticated FVD command kind "
		                     << static_cast<uint16_t>(kind)};
	}
}

std::optional<std::pair<int64_t, int64_t>> entity_health(
	const std::shared_ptr<gamestate::GameEntity> &entity,
	const time::time_t &time) {
	if (not entity->has_component(gamestate::component::component_t::LIVE)) {
		return std::nullopt;
	}
	auto live = std::dynamic_pointer_cast<gamestate::component::Live>(
		entity->get_component(gamestate::component::component_t::LIVE));
	if (not live) {
		throw Error{MSG(err) << "Game entity " << entity->get_id()
		                     << " has an invalid Live component"};
	}
	for (const auto &setting : live->get_ability().get_set("Live.attributes")) {
		auto setting_value = std::dynamic_pointer_cast<nyan::ObjectValue>(setting.get_ptr());
		auto setting_object = live->get_ability().get_view()->get_object(
			setting_value->get_name());
		auto attribute_name = setting_object.get_object(
			"AttributeSetting.attribute").get_name();
		if (attribute_name.ends_with(".Health")) {
			return std::pair{
				live->get_attribute(time, attribute_name)->get(time),
				setting_object.get_int("AttributeSetting.max_value")};
		}
	}
	throw Error{MSG(err) << "Live game entity " << entity->get_id()
	                     << " has no Health attribute"};
}

} // namespace

void submit_authenticated_command(
	const AuthenticatedCommand &command,
	const std::shared_ptr<::openage::event::EventLoop> &event_loop,
	const std::shared_ptr<gamestate::event::Commander> &commander,
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	command_result_callback_t result_callback) {
	if (not result_callback) {
		throw Error{MSG(err) << "Network command intent requires a result callback"};
	}

	event::EventHandler::param_map::map_t params{
		{"type", simulation_command(command.intent.kind)},
		{"target", command.intent.target},
		{"owner", command.identity.player_id},
		{"entity_ids", command.intent.entity_ids},
		{"result_cb", std::move(result_callback)},
	};
	event_loop->submit_event(
		"game.send_command",
		commander,
		state,
		time,
		params);
}

WorldState capture_fvd_state(
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	uint64_t base_revision,
	uint64_t world_revision,
	gamestate::player_id_t recipient) {
	auto fvd = state->get_fvd();
	if (not fvd) {
		throw Error{MSG(err) << "FVD world replication requires an FVD scenario"};
	}
	if (fvd->get_map_size() > std::numeric_limits<uint32_t>::max()) {
		throw Error{MSG(err) << "FVD map is too large for network replication"};
	}
	if (recipient == 0 or recipient > fvd->get_player_count()) {
		throw Error{MSG(err) << "FVD world replication requires a playable recipient"};
	}

	WorldState result{};
	result.base_revision = base_revision;
	result.world_revision = world_revision;
	result.simulation_time = time;
	result.map_size = static_cast<uint32_t>(fvd->get_map_size());
	result.local_player_id = recipient;
	result.local_kill_count = fvd->get_kills(recipient);
	result.players.reserve(fvd->get_player_count());
	for (gamestate::player_id_t player_id = 1;
	     player_id <= fvd->get_player_count(); ++player_id) {
		result.players.push_back({
			player_id,
			player_id == 1 ? PlayerRole::HUNTER : PlayerRole::BUILDER,
		});
	}

	std::vector<gamestate::entity_id_t> entity_ids{};
	entity_ids.reserve(state->get_game_entities().size());
	for (const auto &[entity_id, entity] : state->get_game_entities()) {
		(void)entity;
		entity_ids.push_back(entity_id);
	}
	std::sort(entity_ids.begin(), entity_ids.end());
	result.entities.reserve(entity_ids.size());
	for (auto entity_id : entity_ids) {
		auto entity = state->get_game_entity(entity_id);
		if (not entity->has_component(gamestate::component::component_t::POSITION)
		    or not entity->has_component(gamestate::component::component_t::OWNERSHIP)) {
			throw Error{MSG(err) << "Game entity " << entity_id
			                     << " lacks replicated FVD components"};
		}
		auto position = std::dynamic_pointer_cast<gamestate::component::Position>(
			entity->get_component(gamestate::component::component_t::POSITION));
		auto ownership = std::dynamic_pointer_cast<gamestate::component::Ownership>(
			entity->get_component(gamestate::component::component_t::OWNERSHIP));
		if (not position or not ownership) {
			throw Error{MSG(err) << "Game entity " << entity_id
			                     << " has invalid replicated FVD components"};
		}
		auto health = entity_health(entity, time);
		EntityState entity_state{
			entity_id,
			ownership->get_owners().get(time),
			entity->get_definition(),
			position->get_positions().get(time),
		};
		if (health) {
			entity_state.health = health->first;
			entity_state.maximum_health = health->second;
		}
		result.entities.push_back(std::move(entity_state));
	}
	for (const auto &reservation : fvd->get_reserved_tiles()) {
		result.reservations.push_back({reservation.tile, reservation.builder_id});
	}
	for (const auto &construction : fvd->get_construction_state()) {
		result.constructions.push_back({
			construction.order_id,
			construction.owner,
			construction.builder_id,
			construction.target,
			construction.phase == gamestate::FVDConstructionPhase::PLANNED
				? ConstructionPhase::PLANNED
				: ConstructionPhase::UNDER_CONSTRUCTION,
			construction.phase_end_time,
			construction.site_entity_id,
		});
	}
	for (const auto &order : fvd->get_attack_order_state()) {
		result.attack_orders.push_back({
			order.horse_id,
			order.destination,
			order.target_id,
			order.reached_destination,
			order.focus_target,
		});
	}
	for (const auto &arrow : fvd->get_arrow_state(time)) {
		result.arrows.push_back({
			arrow.arrow_id,
			arrow.owner,
			arrow.target_id,
			arrow.start,
			arrow.destination,
			arrow.launch_time,
			arrow.impact_time,
		});
	}
	return result;
}

} // namespace openage::network
