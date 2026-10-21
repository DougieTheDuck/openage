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
#include "network/match_connection_state.h"

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
	case CommandKind::FOCUS_ATTACK:
		return gamestate::component::command::command_t::FOCUS_ATTACK;
	case CommandKind::PRIORITIZE_FOUNDATION:
		// Existing simulation handler prioritizes a foundation via MOVE
		// with target_entity, preserving the same ownership checks.
		return gamestate::component::command::command_t::MOVE;
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
		{"queue", command.intent.queue},
		{"result_cb", std::move(result_callback)},
	};
	if (command.intent.target_entity) {
		params.emplace("target_entity", *command.intent.target_entity);
	}
	if (not command.intent.build_line.empty()) {
		params.emplace("build_line", command.intent.build_line);
	}
	event_loop->submit_event(
		"game.send_command",
		commander,
		state,
		time,
		params);
}

void submit_authenticated_command(
	const AuthenticatedCommand &command,
	const MatchConnectionState &connections,
	const std::shared_ptr<::openage::event::EventLoop> &event_loop,
	const std::shared_ptr<gamestate::event::Commander> &commander,
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time,
	command_result_callback_t result_callback) {
	if (not result_callback) {
		throw Error{MSG(err) << "Network command intent requires a result callback"};
	}
	if (not connections.accepts_commands(command.identity)) {
		result_callback(false);
		return;
	}
	submit_authenticated_command(
		command, event_loop, commander, state, time,
		std::move(result_callback));
}

WorldState capture_fvd_presentation_state(
	const std::shared_ptr<gamestate::GameState> &state,
	const time::time_t &time) {
	auto fvd = state->get_fvd();
	if (not fvd) {
		throw Error{MSG(err) << "FVD world replication requires an FVD scenario"};
	}
	if (fvd->get_map_size() > std::numeric_limits<uint32_t>::max()) {
		throw Error{MSG(err) << "FVD map is too large for network replication"};
	}
	WorldState result{};
	result.simulation_time = time;
	result.map_size = static_cast<uint32_t>(fvd->get_map_size());

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
		result.entities.push_back({
			entity_id,
			ownership->get_owners().get(time),
			entity->get_definition(),
			position->get_positions().get(time),
		});
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
			construction.phase == gamestate::FVDConstructionPhase::PLANNED ? ConstructionPhase::PLANNED : ConstructionPhase::UNDER_CONSTRUCTION,
			construction.phase_end_time,
			construction.phase_start_time,
			construction.site_entity_id,
		});
	}
	return result;
}

WorldState capture_fvd_state(
	const std::shared_ptr<gamestate::GameState> &state,
	const MatchConnectionState &connections,
	const time::time_t &time,
	uint64_t base_revision,
	uint64_t world_revision,
	gamestate::player_id_t recipient) {
	auto result = capture_fvd_presentation_state(state, time);
	auto fvd = state->get_fvd();
	if (recipient == 0 or recipient > fvd->get_player_count()) {
		throw Error{MSG(err) << "FVD world replication requires a playable recipient"};
	}
	if (connections.get_player_count() != fvd->get_player_count()) {
		throw Error{MSG(err) << "FVD connection state does not match the playable player count"};
	}
	result.base_revision = base_revision;
	result.world_revision = world_revision;
	result.local_player_id = recipient;
	result.local_kill_count = fvd->get_kills(recipient);
	result.players.reserve(fvd->get_player_count());
	for (gamestate::player_id_t player_id = 1;
	     player_id <= fvd->get_player_count();
	     ++player_id) {
		result.players.push_back({
			player_id,
			player_id == 1 ? PlayerRole::HUNTER : PlayerRole::BUILDER,
			connections.is_connected(player_id),
		});
	}
	for (auto &entity_state : result.entities) {
		auto entity = state->get_game_entity(entity_state.entity_id);
		auto position = std::dynamic_pointer_cast<gamestate::component::Position>(
			entity->get_component(gamestate::component::component_t::POSITION));
		// FVD rotation uses instantaneous turn jumps on the simulation's
		// Segmented facing curve. Export the right side of each FUTURE jump
		// once; paired same-time curve keys must not be sent twice.
		entity_state.angle = position->get_angles().get(time);
		const auto &angles = position->get_angles().get_container();
		const auto angle_prior = angles.last(time);
		for (auto index = angle_prior + 1;
		     index < angles.size()
		     and entity_state.facing_keys.size() < MAX_ENTITY_FACING_KEYS;
		     ++index) {
			const auto &key = angles.get(index);
			if (index + 1 < angles.size()
			    and angles.get(index + 1).time() == key.time()) {
				continue; // left side of a jump
			}
			entity_state.facing_keys.push_back({key.time(), key.val()});
		}
		// Position curves are calculated on the simulation thread already:
		// copy only upcoming segments, never past history. Idle entities
		// naturally emit zero future keys. An update can replace this plan
		// when routing, collisions or player orders change the trajectory.
		const auto &positions = position->get_positions().get_container();
		const auto prior = positions.last(time);
		for (auto index = prior + 1;
		     index < positions.size()
		     and entity_state.motion_keys.size() < MAX_ENTITY_MOTION_KEYS;
		     ++index) {
			const auto &key = positions.get(index);
			entity_state.motion_keys.push_back({key.time(), key.val()});
		}
		auto health = entity_health(entity, time);
		if (health) {
			entity_state.health = health->first;
			entity_state.maximum_health = health->second;
		}
		if (entity_state.definition == "fvd.data.entities.Tower"
		    and entity_state.owner_id == recipient) {
			entity_state.tower_kills = fvd->get_tower_kills(entity_state.entity_id, recipient);
		}
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
