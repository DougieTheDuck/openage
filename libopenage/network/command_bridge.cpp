// Copyright 2026 the openage authors. See copying.md for legal info.

#include "command_bridge.h"

#include <utility>

#include "error/error.h"
#include "event/eventhandler.h"
#include "event/event_loop.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/component/types.h"
#include "gamestate/event/send_command.h"
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

WorldState capture_positions(
	const std::shared_ptr<gamestate::GameState> &state,
	const std::vector<gamestate::entity_id_t> &entity_ids,
	const time::time_t &time,
	uint64_t base_revision,
	uint64_t world_revision) {
	WorldState result{};
	result.base_revision = base_revision;
	result.world_revision = world_revision;
	result.simulation_time = time;
	result.positions.reserve(entity_ids.size());
	for (auto entity_id : entity_ids) {
		auto entity = state->get_game_entity(entity_id);
		if (not entity->has_component(gamestate::component::component_t::POSITION)) {
			throw Error{MSG(err) << "Game entity " << entity_id
			                     << " has no position component for network replication"};
		}
		auto position = std::dynamic_pointer_cast<gamestate::component::Position>(
			entity->get_component(gamestate::component::component_t::POSITION));
		if (not position) {
			throw Error{MSG(err) << "Game entity " << entity_id
			                     << " has an invalid position component for network replication"};
		}
		result.positions.push_back({entity_id, position->get_positions().get(time)});
	}
	return result;
}

} // namespace openage::network
