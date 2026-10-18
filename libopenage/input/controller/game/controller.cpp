// Copyright 2021-2023 the openage authors. See copying.md for legal info.

#include "controller.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include <QGuiApplication>
#include <QStyleHints>

#include "log/log.h"

#include "event/event_loop.h"
#include "event/evententity.h"
#include "event/state.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/event/send_command.h"
#include "gamestate/event/spawn_entity.h"
#include "gamestate/fvd.h"
#include "gamestate/game.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/simulation.h"
#include "input/controller/game/binding_context.h"
#include "time/clock.h"
#include "time/time_loop.h"
#include "util/stall_profiler.h"

#include "coord/phys.h"
#include "renderer/camera/camera.h"
#include "renderer/stages/world/render_entity.h"


namespace openage::input::game {

Controller::Controller(const std::unordered_set<size_t> &controlled_factions,
                       size_t active_faction_id) :
	controlled_factions{controlled_factions},
	active_faction_id{active_faction_id} {}

void Controller::set_control(size_t faction_id) {
	std::unique_lock lock{this->mutex};

	if (this->controlled_factions.find(faction_id) != this->controlled_factions.end()) {
		this->active_faction_id = faction_id;
		this->selected.clear();
		this->selected_kind = SelectionKind::NONE;
		this->selected_render_entity.reset();
		this->selection_generation += 1;
		this->selection_pending = false;
		this->deferred_selection_action = {};
		this->placing_tower = false;
		this->attacking = false;
	}
}

void Controller::lock_to_online_player(size_t faction_id) {
	std::unique_lock lock{this->mutex};
	this->controlled_factions = {faction_id};
	this->active_faction_id = faction_id;
	this->selected.clear();
	this->selected_kind = SelectionKind::NONE;
	this->selected_render_entity.reset();
	++this->selection_generation;
	this->selection_pending = false;
	this->deferred_selection_action = {};
	this->placing_tower = false;
	this->attacking = false;
}

void Controller::reset_online_player_slots(size_t player_count) {
	std::unique_lock lock{this->mutex};
	this->controlled_factions.clear();
	for (size_t id = 1; id <= player_count; ++id) {
		this->controlled_factions.insert(id);
	}
	this->active_faction_id = 2;
	this->selected.clear();
	this->selected_kind = SelectionKind::NONE;
	this->selected_render_entity.reset();
	++this->selection_generation;
	this->selection_pending = false;
	this->deferred_selection_action = {};
	this->placing_tower = false;
	this->attacking = false;
}

size_t Controller::get_controlled() const {
	std::unique_lock lock{this->mutex};

	return this->active_faction_id;
}

std::vector<gamestate::entity_id_t> Controller::get_selected() const {
	std::unique_lock lock{this->mutex};

	return this->selected;
}

void Controller::set_selected(
	const std::vector<gamestate::entity_id_t> ids,
	SelectionKind kind,
	const std::shared_ptr<renderer::world::RenderEntity> &render_entity) {
	std::unique_lock lock{this->mutex};

	log::log(DBG << "Selected " << ids.size() << " entities");

	this->selected = ids;
	this->selected_kind = (ids.size() == 1 or kind == SelectionKind::GROUP_HORSES)
	                         ? kind : SelectionKind::NONE;
	this->selected_render_entity = ids.size() == 1 ? render_entity : nullptr;
	this->selection_generation += 1;
	this->selection_pending = false;
	this->deferred_selection_action = {};
	this->placing_tower = false;
	this->attacking = false;
}

uint64_t Controller::begin_selection_update() {
	std::unique_lock lock{this->mutex};

	this->selection_generation += 1;
	this->selection_pending = true;
	this->deferred_selection_action = {};
	return this->selection_generation;
}

bool Controller::set_selected_if_current(
	uint64_t generation,
	const std::vector<gamestate::entity_id_t> ids,
	SelectionKind kind,
	const std::shared_ptr<renderer::world::RenderEntity> &render_entity) {
	std::function<void()> deferred;
	{
		std::unique_lock lock{this->mutex};
		if (not this->selection_pending or generation != this->selection_generation) {
			return false;
		}

		log::log(DBG << "Selected " << ids.size() << " entities");
		this->selected = ids;
		this->selected_kind = (ids.size() == 1 or kind == SelectionKind::GROUP_HORSES)
		                         ? kind : SelectionKind::NONE;
		this->selected_render_entity = ids.size() == 1 ? render_entity : nullptr;
		this->selection_pending = false;
		this->placing_tower = false;
		this->attacking = false;
		deferred = std::move(this->deferred_selection_action);
	}

	if (deferred) {
		deferred();
	}
	return true;
}

bool Controller::defer_until_selection_applied(std::function<void()> action) {
	std::unique_lock lock{this->mutex};
	if (not this->selection_pending) {
		return false;
	}
	this->deferred_selection_action = std::move(action);
	return true;
}

SelectionDetails Controller::get_selection_details() const {
	std::unique_lock lock{this->mutex};
	return {this->selected, this->selected_kind, this->selected_render_entity};
}

bool Controller::begin_tower_placement() {
	std::unique_lock lock{this->mutex};
	if (this->active_faction_id < 2 or this->selected.size() != 1
	    or this->selected_kind != SelectionKind::BUILDER) {
		return false;
	}
	this->attacking = false;
	this->placing_tower = true;
	return true;
}

void Controller::cancel_tower_placement() {
	std::unique_lock lock{this->mutex};
	this->placing_tower = false;
}

bool Controller::is_tower_placement_active() const {
	std::unique_lock lock{this->mutex};
	return this->placing_tower;
}

bool Controller::begin_attack_move() {
	std::unique_lock lock{this->mutex};
	if (this->active_faction_id != 1 or this->selected.empty()
	    or (this->selected_kind != SelectionKind::HORSE
	        and this->selected_kind != SelectionKind::GROUP_HORSES)) {
		return false;
	}
	this->placing_tower = false;
	this->attacking = true;
	return true;
}

void Controller::cancel_attack_move() {
	std::unique_lock lock{this->mutex};
	this->attacking = false;
}

bool Controller::is_attack_move_active() const {
	std::unique_lock lock{this->mutex};
	return this->attacking;
}

bool Controller::process(const event_arguments &ev_args, const std::shared_ptr<BindingContext> &ctx) {
	util::StallProfiler profile{"input.game_controller.process"};
	util::StallProfiler lock_profile{"input.game_controller.lock_wait"};
	std::unique_lock lock{this->mutex};
	lock_profile.finish();

	if (not ctx->is_bound(ev_args.e)) {
		return false;
	}

	// TODO: check if action is allowed
	auto bind = ctx->lookup(ev_args.e);
	bind.transform(ev_args, this->shared_from_this());

	return true;
}

void Controller::set_drag_select_start(const coord::input &start) {
	std::unique_lock lock{this->mutex};

	log::log(DBG << "Drag select start at " << start);
	this->drag_select_start = start;
}

const coord::input Controller::get_drag_select_start() const {
	std::unique_lock lock{this->mutex};

	if (not this->drag_select_start.has_value()) {
		throw Error{ERR << "Drag select start not set."};
	}

	return this->drag_select_start.value();
}

void Controller::reset_drag_select() {
	std::unique_lock lock{this->mutex};

	log::log(DBG << "Drag select start reset");
	this->drag_select_start = std::nullopt;
}

namespace {

/** Resolve the selected entity type on the simulation thread. */
void apply_selection(const std::shared_ptr<Controller> &controller,
                     const std::shared_ptr<gamestate::GameState> &state,
                     uint64_t generation,
                     const std::vector<gamestate::entity_id_t> &ids) {
	if (not state->get_fvd()) {
		controller->set_selected_if_current(generation, ids);
		return;
	}
	if (ids.size() > 1) {
		bool all_horses = std::all_of(ids.begin(), ids.end(), [&](auto id) {
			auto found = state->get_game_entities().find(id);
			return found != state->get_game_entities().end()
			    and found->second->get_definition() == "fvd.data.entities.Horse";
		});
		controller->set_selected_if_current(
			generation, ids,
			all_horses ? SelectionKind::GROUP_HORSES : SelectionKind::NONE);
		return;
	}
	if (ids.empty()) {
		controller->set_selected_if_current(generation, {});
		return;
	}
	auto found = state->get_game_entities().find(ids.front());
	if (found == state->get_game_entities().end()) {
		controller->set_selected_if_current(generation, {});
		return;
	}
	auto definition = found->second->get_definition();
	auto kind = SelectionKind::OTHER;
	if (definition == "fvd.data.entities.Builder") {
		kind = SelectionKind::BUILDER;
	}
	else if (definition == "fvd.data.entities.Horse") {
		kind = SelectionKind::HORSE;
	}
	else if (definition == "fvd.data.entities.Tower") {
		kind = SelectionKind::TOWER;
	}
	controller->set_selected_if_current(
		generation, ids, kind, found->second->get_render_entity());
}

/** Resolve guest selections exclusively from the host's replicated world. */
void apply_guest_selection(
	const std::shared_ptr<Controller> &controller,
	const network::WorldState &world,
	uint64_t generation,
	const std::vector<gamestate::entity_id_t> &ids) {
	std::vector<gamestate::entity_id_t> valid;
	SelectionKind kind = SelectionKind::NONE;
	for (auto id : ids) {
		auto found = std::find_if(world.entities.begin(), world.entities.end(),
			[id](const network::EntityState &entity) { return entity.entity_id == id; });
		if (found == world.entities.end() or found->owner_id != world.local_player_id) {
			continue;
		}
		if (found->definition == "fvd.data.entities.Horse") {
			if (kind != SelectionKind::NONE and kind != SelectionKind::HORSE) continue;
			kind = SelectionKind::HORSE;
		}
		else if (found->definition == "fvd.data.entities.Builder") {
			if (not valid.empty()) continue;
			kind = SelectionKind::BUILDER;
		}
		else if (found->definition == "fvd.data.entities.Tower") {
			if (not valid.empty()) continue;
			kind = SelectionKind::TOWER;
		}
		else {
			continue;
		}
		valid.push_back(id);
		if (kind != SelectionKind::HORSE) break;
	}
	if (kind == SelectionKind::HORSE and valid.size() > 1) {
		kind = SelectionKind::GROUP_HORSES;
	}
	controller->set_selected_if_current(generation, valid, kind);
}

} // namespace

void setup_defaults(const std::shared_ptr<BindingContext> &ctx,
                    const std::shared_ptr<time::TimeLoop> &time_loop,
                    const std::shared_ptr<openage::gamestate::GameSimulation> &simulation,
                    const std::shared_ptr<renderer::camera::Camera> &camera,
	                const std::function<void(
		                const coord::input &,
		                const std::function<void(std::optional<gamestate::entity_id_t>)> &)> &pick_entity,
                    const std::function<bool(network::CommandIntent)> &send_guest_command,
                    const std::function<std::optional<network::WorldState>()> &guest_world,
                    bool online_lobby) {
	binding_func_t create_entity_event{[simulation, time_loop, camera, send_guest_command, guest_world](const event_arguments &args,
	                                       const std::shared_ptr<Controller> controller) {
		auto mouse_pos = args.mouse.to_phys3(camera);
		auto state = simulation->get_game()->get_state();
		const bool append = (args.e.mod_code & Qt::ShiftModifier) != 0;
		if (state->get_fvd()) {
			auto selection = controller->get_selection_details();
			if (selection.kind != SelectionKind::BUILDER) {
				return;
			}
			const bool remote = guest_world and guest_world().has_value();
			if (remote) {
				network::CommandIntent intent{
					0, network::CommandKind::BUILD_TOWER, selection.ids, mouse_pos};
				intent.queue = append;
				if (send_guest_command and send_guest_command(std::move(intent))) {
					if (not append) controller->cancel_tower_placement();
				}
				return;
			}
			if (not state->get_fvd()->can_place_tower(
			        state, static_cast<gamestate::player_id_t>(controller->get_controlled()),
			        selection.ids, mouse_pos, time_loop->get_clock()->get_time())) {
				return; // Stay in placement mode so an invalid click is correctable.
			}
			if (not append) {
				controller->cancel_tower_placement();
			}
			event::EventHandler::param_map::map_t params{
				{"type", gamestate::component::command::command_t::BUILD_TOWER},
				{"target", mouse_pos},
				{"queue", append},
				{"owner", static_cast<gamestate::player_id_t>(controller->get_controlled())},
				{"entity_ids", controller->get_selected()},
			};
			simulation->get_event_loop()->submit_event(
				"game.send_command", simulation->get_commander(), state,
				time_loop->get_clock()->get_time(), params);
			return;
		}

		event::EventHandler::param_map::map_t params{
			{"position", mouse_pos},
			{"owner", controller->get_controlled()},
			{"entity_ids", controller->get_selected()},
		};
		simulation->get_event_loop()->submit_event(
			"game.spawn_entity", simulation->get_spawner(), state,
			time_loop->get_clock()->get_time(), params);
	}};

	binding_func_t attack_move_event{[simulation, time_loop, camera, send_guest_command, guest_world](
		const event_arguments &args, const std::shared_ptr<Controller> controller) {
		if (not controller->is_attack_move_active()) {
			return;
		}
		controller->cancel_attack_move();
		if (guest_world and guest_world().has_value()) {
			if (send_guest_command) {
				send_guest_command(network::CommandIntent{
					0, network::CommandKind::ATTACK_MOVE,
					controller->get_selected(), args.mouse.to_phys3(camera)});
			}
			return;
		}
		event::EventHandler::param_map::map_t params{
			{"type", gamestate::component::command::command_t::ATTACK_MOVE},
			{"target", args.mouse.to_phys3(camera)},
			{"owner", static_cast<gamestate::player_id_t>(controller->get_controlled())},
			{"entity_ids", controller->get_selected()},
		};
		simulation->get_event_loop()->submit_event(
			"game.send_command", simulation->get_commander(),
			simulation->get_game()->get_state(),
			time_loop->get_clock()->get_time(), params);
	}};

	auto fvd = simulation->get_game()->get_state()->get_fvd();
	if (fvd) {
		binding_func_t start_placement{[](const event_arguments &,
		                                 const std::shared_ptr<Controller> controller) {
			controller->begin_tower_placement();
		}};
		binding_func_t cancel_placement{[](const event_arguments &,
		                                  const std::shared_ptr<Controller> controller) {
			controller->cancel_tower_placement();
			controller->cancel_attack_move();
		}};
		binding_func_t start_attack_move{[](const event_arguments &,
		                                    const std::shared_ptr<Controller> controller) {
			controller->begin_attack_move();
		}};
		ctx->bind(Event{event_class::KEYBOARD, Qt::Key_A,
		                Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
		          binding_action{forward_action_t::CLEAR, start_attack_move});
		ctx->bind(Event{event_class::KEYBOARD, Qt::Key_B,
		                Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
		          binding_action{forward_action_t::CLEAR, start_placement});
		ctx->bind(Event{event_class::KEYBOARD, Qt::Key_Escape,
		                Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
		          binding_action{forward_action_t::CLEAR, cancel_placement});

		if (not online_lobby) for (size_t id = 1; id <= fvd->get_player_count(); ++id) {
			binding_func_t switch_player{[id, camera, fvd](const event_arguments &,
			                                               const std::shared_ptr<Controller> controller) {
				controller->set_control(id);
				camera->look_at_coord(fvd->get_centre().to_scene3());
				log::log(INFO << "Controlling player " << id << (id == 1 ? " (Hunter)" : " (Builders)"));
			}};
			ctx->bind(Event{event_class::KEYBOARD, static_cast<int>(Qt::Key_0 + id),
			                Qt::KeyboardModifier::NoModifier, QEvent::KeyRelease},
			          binding_action{forward_action_t::CLEAR, switch_player});
		}
	}

	auto submit_order = [simulation, time_loop, camera, pick_entity, send_guest_command, guest_world](
		                    const event_arguments &args,
		                    const std::shared_ptr<Controller> controller) {
		if (controller->is_tower_placement_active()) {
			controller->cancel_tower_placement();
			return;
		}
		if (controller->is_attack_move_active()) {
			controller->cancel_attack_move();
			return;
		}
		auto mouse_pos = args.mouse.to_phys3(camera);
		auto selection = controller->get_selection_details();
		auto owner = static_cast<gamestate::player_id_t>(controller->get_controlled());
		if (guest_world) {
			auto replica = guest_world();
			if (replica) {
				if (not send_guest_command or selection.ids.empty()) return;
				const bool horses = owner == 1
					&& (selection.kind == SelectionKind::HORSE
					    or selection.kind == SelectionKind::GROUP_HORSES);
				const bool builder = owner >= 2 and selection.kind == SelectionKind::BUILDER;
				auto order = [send_guest_command, selection, mouse_pos,
				              replica = std::move(*replica), owner, horses, builder](
					std::optional<gamestate::entity_id_t> hit) {
					network::CommandIntent intent{
						0, network::CommandKind::MOVE, selection.ids, mouse_pos};
					if (hit) {
						auto found = std::find_if(
							replica.entities.begin(), replica.entities.end(),
							[hit](const network::EntityState &entry) {
								return entry.entity_id == *hit;
							});
						if (found != replica.entities.end()) {
							if (horses and found->owner_id != owner
							    and found->definition == "fvd.data.entities.Builder") {
								intent.kind = network::CommandKind::FOCUS_ATTACK;
								intent.target_entity = *hit;
							}
							else if (builder and found->owner_id == owner
							         and found->definition == "fvd.data.entities.Foundation") {
								intent.kind = network::CommandKind::PRIORITIZE_FOUNDATION;
								intent.target_entity = *hit;
							}
						}
					}
					send_guest_command(std::move(intent));
				};
				if (horses or builder) {
					pick_entity(args.mouse, std::move(order));
				}
				else {
					order(std::nullopt);
				}
				return;
			}
		}

		// The render-thread pick reports only an entity ID. Target
		// ownership, type, and life are checked on the simulation thread.
		const bool hunter_selection = owner == 1 and not selection.ids.empty()
			and (selection.kind == SelectionKind::HORSE
			     or selection.kind == SelectionKind::GROUP_HORSES);
		const bool builder_selection = owner >= 2 and selection.ids.size() == 1
			and selection.kind == SelectionKind::BUILDER;
		auto send_order = [simulation, time_loop, mouse_pos, owner, ids = selection.ids,
		                   hunter_selection, builder_selection](
			std::optional<gamestate::entity_id_t> picked) {
			event::EventHandler::param_map::map_t params{
				{"type", picked and hunter_selection
				           ? gamestate::component::command::command_t::FOCUS_ATTACK
				           : gamestate::component::command::command_t::MOVE},
				{"target", mouse_pos},
				{"owner", owner},
				{"entity_ids", ids},
			};
			if (picked and (hunter_selection or builder_selection)) {
				params.emplace("target_entity", *picked);
			}
			simulation->get_event_loop()->submit_event(
				"game.send_command", simulation->get_commander(),
				simulation->get_game()->get_state(),
				time_loop->get_clock()->get_time(), params);
		};
		if (hunter_selection or builder_selection) {
			pick_entity(args.mouse, send_order);
		}
		else {
			send_order(std::nullopt);
		}
	};
	binding_func_t move_entity{[submit_order](const event_arguments &args,
	                                            const std::shared_ptr<Controller> controller) {
		if (controller->defer_until_selection_applied(
			    [submit_order, args, controller]() { submit_order(args, controller); })) {
			return;
		}
		submit_order(args, controller);
	}};

	binding_action move_entity_action{forward_action_t::SEND, move_entity};
	Event ev_mouse_rmb{
		event_class::MOUSE_BUTTON,
		Qt::MouseButton::RightButton,
		Qt::KeyboardModifier::NoModifier,
		QEvent::MouseButtonRelease};

	ctx->bind(ev_mouse_rmb, move_entity_action);

	binding_func_t init_drag_selection{[](const event_arguments &args,
	                                       const std::shared_ptr<Controller> controller) {
		controller->set_drag_select_start(args.mouse);
	}};

	binding_action init_drag_selection_action{forward_action_t::CLEAR, init_drag_selection};
	Event ev_mouse_lmb_press{
		event_class::MOUSE_BUTTON,
		Qt::MouseButton::LeftButton,
		Qt::KeyboardModifier::NoModifier,
		QEvent::MouseButtonPress};

	ctx->bind(ev_mouse_lmb_press, init_drag_selection_action);

	binding_func_t drag_selection{
		[simulation, time_loop, camera, pick_entity, create_entity_event, attack_move_event, guest_world](const event_arguments &args,
	        const std::shared_ptr<Controller> controller) {
			if (controller->is_tower_placement_active()) {
				controller->reset_drag_select();
				create_entity_event(args, controller);
				return;
			}
			// Shift-click queues a tower even after a normal click exited
			// placement mode; no second press of B is required.
			if ((args.e.mod_code & Qt::ShiftModifier) != 0
			    and controller->get_selection_details().kind == SelectionKind::BUILDER
			    and controller->begin_tower_placement()) {
				controller->reset_drag_select();
				create_entity_event(args, controller);
				return;
			}
			if (controller->is_attack_move_active()) {
				controller->reset_drag_select();
				attack_move_event(args, controller);
				return;
			}
			auto selection_generation = controller->begin_selection_update();
			auto drag_start = controller->get_drag_select_start();
			auto drag_delta = args.mouse - drag_start;
			auto drag_threshold = QGuiApplication::styleHints()->startDragDistance();
			if (guest_world) {
				auto replica = guest_world();
				if (replica) {
					if (std::abs(drag_delta.x) < drag_threshold
					    and std::abs(drag_delta.y) < drag_threshold) {
						pick_entity(args.mouse,
							[controller, selection_generation, replica = std::move(*replica)](
								std::optional<gamestate::entity_id_t> id) {
								apply_guest_selection(controller, replica, selection_generation,
									id ? std::vector<gamestate::entity_id_t>{*id}
									   : std::vector<gamestate::entity_id_t>{});
							});
					}
					else {
						// Drag-select host-owned Horse groups using authoritative
						// positions, without querying the paused guest simulation.
						Eigen::Matrix4f matrix =
							camera->get_projection_matrix() * camera->get_view_matrix();
						const auto viewport = camera->get_viewport_size();
						std::vector<gamestate::entity_id_t> picked;
						for (const auto &entity : replica->entities) {
							if (entity.owner_id != replica->local_player_id
							    or entity.definition != "fvd.data.entities.Horse") continue;
							auto scene = entity.position.to_scene3().to_world_space();
							Eigen::Vector4f clip = matrix * Eigen::Vector4f{
								scene.x(), scene.y(), scene.z(), 1.0f};
							if (clip.w() <= 0) continue;
							double x = (clip.x() / clip.w() * 0.5 + 0.5) * viewport[0];
							double y = (0.5 - clip.y() / clip.w() * 0.5) * viewport[1];
							if (x >= std::min(drag_start.x, args.mouse.x)
							    and x <= std::max(drag_start.x, args.mouse.x)
							    and y >= std::min(drag_start.y, args.mouse.y)
							    and y <= std::max(drag_start.y, args.mouse.y)) {
								picked.push_back(entity.entity_id);
							}
						}
						apply_guest_selection(controller, *replica, selection_generation, picked);
					}
					controller->reset_drag_select();
					return;
				}
			}
			if (std::abs(drag_delta.x) < drag_threshold
			    and std::abs(drag_delta.y) < drag_threshold) {
				auto controlled = controller->get_controlled();
				pick_entity(
					args.mouse,
					[simulation, time_loop, controller, controlled, selection_generation](
						std::optional<gamestate::entity_id_t> id) {
						std::vector<gamestate::entity_id_t> picked;
						if (id.has_value()) {
							picked.push_back(*id);
						}
						event::EventHandler::param_map::map_t params{
							{"controlled", controlled},
							{"entity_ids", picked},
							{"select_cb",
							 std::function<void(const std::vector<gamestate::entity_id_t>)>{
								 [controller, simulation, selection_generation](const std::vector<gamestate::entity_id_t> ids) {
									 apply_selection(controller, simulation->get_game()->get_state(),
									                 selection_generation, ids);
								 }}},
						};

						simulation->get_event_loop()->submit_event(
							"game.click_select",
							simulation->get_commander(),
							simulation->get_game()->get_state(),
							time_loop->get_clock()->get_time(),
							params);
					});
				controller->reset_drag_select();
				return;
			}

			Eigen::Matrix4f cam_matrix = camera->get_projection_matrix() * camera->get_view_matrix();
			event::EventHandler::param_map::map_t params{
				{"controlled", controller->get_controlled()},
				{"drag_start", drag_start.to_viewport(camera).to_ndc_space(camera)},
				{"drag_end", args.mouse.to_viewport(camera).to_ndc_space(camera)},
				{"camera_matrix", cam_matrix},
				{"select_cb",
		         std::function<void(const std::vector<gamestate::entity_id_t> ids)>{
					 [controller, simulation, selection_generation](
						 const std::vector<gamestate::entity_id_t> ids) {
						 apply_selection(controller, simulation->get_game()->get_state(),
						                 selection_generation, ids);
					 }}},
			};

			simulation->get_event_loop()->submit_event(
				"game.drag_select",
				simulation->get_commander(),
				simulation->get_game()->get_state(),
				time_loop->get_clock()->get_time(),
				params);

			// Reset drag selection start
			controller->reset_drag_select();
		}};

	binding_action drag_selection_action{forward_action_t::CLEAR, drag_selection};
	Event ev_mouse_lmb_release{
		event_class::MOUSE_BUTTON,
		Qt::MouseButton::LeftButton,
		Qt::KeyboardModifier::NoModifier,
		QEvent::MouseButtonRelease};

	ctx->bind(ev_mouse_lmb_release, drag_selection_action);
	// Shift+left release goes through the same placement handler, which
	// appends a site and deliberately stays in placement mode.
	ctx->bind(Event{event_class::MOUSE_BUTTON, Qt::MouseButton::LeftButton,
	                Qt::KeyboardModifier::ShiftModifier, QEvent::MouseButtonRelease},
	          drag_selection_action);
}


} // namespace openage::input::game
