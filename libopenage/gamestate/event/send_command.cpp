// Copyright 2023-2023 the openage authors. See copying.md for legal info.

#include "send_command.h"

#include <vector>

#include "coord/phys.h"
#include "error/error.h"
#include "gamestate/component/internal/command_queue.h"
#include "gamestate/component/internal/commands/idle.h"
#include "gamestate/component/internal/commands/move.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/types.h"
#include "gamestate/fvd.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/types.h"


namespace openage::gamestate {
namespace component {
class CommandQueue;

namespace command {
class IdleCommand;
class MoveCommand;
} // namespace command
} // namespace component

namespace event {

Commander::Commander(const std::shared_ptr<openage::event::EventLoop> &loop) :
	openage::event::EventEntity{loop} {
}

size_t Commander::id() const {
	return 0;
}

std::string Commander::idstr() const {
	return "command_target";
}


SendCommandHandler::SendCommandHandler() :
	openage::event::OnceEventHandler{"game.send_command"} {
}

void SendCommandHandler::setup_event(const std::shared_ptr<openage::event::Event> & /* event */,
                                     const std::shared_ptr<openage::event::State> & /* state */) {
	// TODO
}

void SendCommandHandler::invoke(openage::event::EventLoop & /* loop */,
                                const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                const std::shared_ptr<openage::event::State> &state,
                                const time::time_t &time,
                                const param_map &params) {
	auto gstate = std::dynamic_pointer_cast<openage::gamestate::GameState>(state);

	auto command_type = params.get("type", component::command::command_t::NONE);
	std::vector<gamestate::entity_id_t> ids = params.get("entity_ids",
	                                                     std::vector<gamestate::entity_id_t>{});
	auto fvd = gstate->get_fvd();
	if (fvd) {
		if (not params.check_type<player_id_t>("owner")) {
			throw Error{ERR << "FVD commands require an authenticated owner"};
		}
		auto owner = params.get<player_id_t>("owner");
		if (command_type == component::command::command_t::BUILD_TOWER) {
			if (not params.check_type<coord::phys3>("target")) {
				throw Error{ERR << "BuildTower requires a target position"};
			}
			fvd->request_build(gstate, owner, ids, params.get<coord::phys3>("target"), time);
			return;
		}
	}

	for (auto id : ids) {
		auto found = gstate->get_game_entities().find(id);
		if (fvd and found == gstate->get_game_entities().end()) {
			continue;
		}
		auto entity = fvd ? found->second : gstate->get_game_entity(id);
		if (fvd) {
			if (not entity->has_component(component::component_t::OWNERSHIP)) {
				continue;
			}
			auto ownership = std::dynamic_pointer_cast<component::Ownership>(
				entity->get_component(component::component_t::OWNERSHIP));
			if (ownership->get_owners().get(time) != params.get<player_id_t>("owner")) {
				continue;
			}
			if (command_type != component::command::command_t::MOVE
			    and command_type != component::command::command_t::IDLE) {
				continue;
			}
			if (command_type == component::command::command_t::MOVE
			    and not entity->has_component(component::component_t::MOVE)) {
				continue;
			}
			fvd->cancel_build(gstate, id, time);
		}
		auto command_queue = std::dynamic_pointer_cast<component::CommandQueue>(
			entity->get_component(component::component_t::COMMANDQUEUE));

		switch (command_type) {
		case component::command::command_t::IDLE:
			command_queue->add_command(time, std::make_shared<component::command::IdleCommand>());
			break;
		case component::command::command_t::MOVE:
			command_queue->add_command(
				time,
				std::make_shared<component::command::MoveCommand>(
					params.get("target",
			                   coord::phys3{0, 0, 0})));
			break;
		default:
			break;
		}
	}
}

time::time_t SendCommandHandler::predict_invoke_time(const std::shared_ptr<openage::event::EventEntity> & /* target */,
                                                     const std::shared_ptr<openage::event::State> & /* state */,
                                                     const time::time_t &at) {
	return at;
}

} // namespace event
} // namespace openage::gamestate
