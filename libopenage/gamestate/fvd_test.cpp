// Copyright 2026 the openage authors. See copying.md for legal info.

#include <array>
#include <functional>
#include <vector>
#include <memory>

#include "assets/mod_manager.h"
#include "config.h"
#include "event/event_loop.h"
#include "gamestate/component/api/live.h"
#include "gamestate/component/api/move.h"
#include "gamestate/component/internal/commands/build_tower.h"
#include "gamestate/component/internal/commands/types.h"
#include "gamestate/component/internal/ownership.h"
#include "gamestate/component/internal/position.h"
#include "gamestate/entity_factory.h"
#include "gamestate/event/drag_select.h"
#include "gamestate/event/process_command.h"
#include "gamestate/event/send_command.h"
#include "gamestate/event/spawn_entity.h"
#include "gamestate/event/wait.h"
#include "gamestate/fvd.h"
#include "gamestate/game.h"
#include "gamestate/game_entity.h"
#include "gamestate/game_state.h"
#include "gamestate/map.h"
#include "gamestate/player.h"
#include "gamestate/system/idle.h"
#include "gamestate/system/move.h"
#include "gamestate/terrain.h"
#include "gamestate/terrain_chunk.h"
#include "gamestate/terrain_factory.h"
#include "gamestate/terrain_tile.h"
#include "pathfinding/cost_field.h"
#include "pathfinding/definitions.h"
#include "pathfinding/grid.h"
#include "pathfinding/pathfinder.h"
#include "pathfinding/sector.h"
#include "renderer/resources/parser/parse_sprite.h"
#include "renderer/resources/parser/parse_terrain.h"
#include "renderer/resources/texture_data.h"
#include "renderer/resources/texture_info.h"
#include "renderer/stages/world/render_entity.h"
#include "testing/testing.h"
#include "util/fslike/directory.h"
#include "util/path.h"

namespace openage::gamestate::tests {

void fvd_scenario() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto graphics = root / "assets" / "fvd" / "graphics";
	auto plains_graphics = renderer::resources::parser::parse_terrain_file(graphics / "grass.terrain");
	auto builder_graphics = renderer::resources::parser::parse_sprite_file(graphics / "builder.sprite");
	auto foundation_graphics = renderer::resources::parser::parse_sprite_file(graphics / "foundation.sprite");
	auto tower_graphics = renderer::resources::parser::parse_sprite_file(graphics / "tower.sprite");
	TESTEQUALS(plains_graphics.get_texture_count(), 1);
	TESTEQUALS(builder_graphics.get_texture_count(), 1);
	TESTEQUALS(foundation_graphics.get_texture_count(), 1);
	TESTEQUALS(tower_graphics.get_texture_count(), 1);
	for (const auto &info : {plains_graphics.get_texture(0),
	                        builder_graphics.get_texture(0),
	                        foundation_graphics.get_texture(0),
	                        tower_graphics.get_texture(0)}) {
		auto texture = renderer::resources::Texture2dData{info->get_image_path().value()};
		TESTEQUALS(info->get_cbits(), false);
		TESTEQUALS(texture.get_info().get_size().first, info->get_size().first);
		TESTEQUALS(texture.get_info().get_size().second, info->get_size().second);
	}
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();
	TESTEQUALS(fvd->get_player_count(), 8);
	TESTEQUALS(fvd->get_centre(), (coord::phys3{64, 64, 0}));
	TESTEQUALS(state->get_map()->get_size(), (util::Vector2s{128, 128}));
	TESTEQUALS(state->get_map()->get_terrain()->get_chunks().size(), 64);
	size_t tile_count = 0;
	for (const auto &chunk : state->get_map()->get_terrain()->get_chunks()) {
		for (const auto &tile : chunk->get_tiles()) {
			TESTEQUALS(tile.elevation, terrain_elevation_t::zero());
			TESTEQUALS(tile.terrain.get_name(), "fvd.data.terrain.Plains");
			if (not (root / "assets" / "converted" / tile.terrain_asset_path).is_file()) {
				TESTFAILMSG("Plains graphics cannot be resolved");
			}
			++tile_count;
		}
	}
	TESTEQUALS(tile_count, 128 * 128);
	TESTEQUALS(state->get_player(1)->get_setup(), "fvd.data.scenario.Hunter");
	TESTEQUALS(state->get_game_entities().size(), 7);
	std::array<size_t, 9> counts{};
	std::array<entity_id_t, 9> builders{};
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto owner = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP))->get_owners().get(time::TIME_ZERO);
		TESTEQUALS(entity->get_definition(), "fvd.data.entities.Builder");
		counts.at(owner)++;
		builders.at(owner) = id;
	}
	TESTEQUALS(counts[0], 0);
	TESTEQUALS(counts[1], 0);
	for (size_t id = 2; id <= 8; ++id) {
		TESTEQUALS(counts[id], 1);
		TESTEQUALS(state->get_player(id)->get_setup(), "fvd.data.scenario.Builders");
	}
	auto builder_entity = state->get_game_entity(builders[2]);
	auto builder_render = std::make_shared<renderer::world::RenderEntity>();
	builder_entity->set_render_entity(builder_render);
	system::Idle::idle(builder_entity, time::TIME_ZERO);
	{
		auto lock = builder_render->get_read_lock();
		auto position = std::dynamic_pointer_cast<component::Position>(
			builder_entity->get_component(component::component_t::POSITION));
		TESTEQUALS(builder_render->get_position().get(time::TIME_ZERO),
		           position->get_positions().get(time::TIME_ZERO).to_scene3());
	}
	TESTEQUALS(builder_render->has_health(), true);
	TESTEQUALS(builder_render->get_health_at(time::TIME_ZERO), 25);
	TESTEQUALS(builder_render->get_max_health(), 25);
	std::vector<entity_id_t> selection;
	event::ClickSelectHandler click_selector;
	auto click = [&](player_id_t controlled, const std::vector<entity_id_t> &candidates) {
		click_selector.invoke(*loop, nullptr, state, time::TIME_ZERO,
		                      {{"controlled", size_t{controlled}},
		                       {"entity_ids", candidates},
		                       {"select_cb", std::function<void(const std::vector<entity_id_t>)>{
		                                         [&](const std::vector<entity_id_t> ids) {
			                                         selection = ids;
		                                         }}}});
	};
	click(2, {builders[2]});
	TESTEQUALS(selection.size(), 1);
	TESTEQUALS(selection.front(), builders[2]);
	click(2, {builders[3]});
	TESTEQUALS(selection.empty(), true);
	click(2, {});
	TESTEQUALS(selection.empty(), true);

	event::DragSelectHandler selector;
	Eigen::Matrix4f selection_matrix = Eigen::Matrix4f::Identity();
	selection_matrix(0, 0) = 0.01f;
	selection_matrix(1, 2) = 0.01f;
	selector.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                {{"controlled", size_t{2}},
	                 {"drag_start", Eigen::Vector2f{-1, -1}},
	                 {"drag_end", Eigen::Vector2f{1, 1}},
	                 {"camera_matrix", selection_matrix},
	                 {"select_cb", std::function<void(const std::vector<entity_id_t>)>{
	                                   [&](const std::vector<entity_id_t> ids) {
		                                   selection = ids;
	                                   }}}});
	TESTEQUALS(selection.size(), 1);
	TESTEQUALS(selection.front(), builders[2]);

	auto place = [&](player_id_t owner, const std::vector<entity_id_t> &selected, coord::phys3 pos) {
		return fvd->request_build(state, owner, selected, pos, time::TIME_ZERO);
	};
	TESTEQUALS(place(1, {builders[2]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2], builders[3]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {builders[3]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {9999}, {10, 10, 0}), false);
	TESTEQUALS(place(9, {builders[2]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {-1, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {128, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {10, 128, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {10, 10, 1}), false);

	auto builder_pos = std::dynamic_pointer_cast<component::Position>(
		builder_entity->get_component(component::component_t::POSITION));
	TESTEQUALS(place(2, {builders[2]}, builder_pos->get_positions().get(time::TIME_ZERO)), false);
	TESTEQUALS(state->get_game_entities().size(), 7);
	TESTEQUALS(place(2, {builders[2]}, {10, 10, 0}), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{10, 10}), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{12, 12}), false);
	auto order = fvd->get_build_order(builders[2]);
	TESTEQUALS(order != nullptr, true);
	TESTEQUALS(order->get_type() == component::command::command_t::BUILD_TOWER, true);
	TESTEQUALS(order->get_owner(), player_id_t{2});
	TESTEQUALS(order->get_builder(), builders[2]);
	TESTEQUALS(order->get_target(), (coord::tile{10, 10}));
	TESTEQUALS(state->get_game_entities().size(), 8);
	entity_id_t first_foundation = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() != "fvd.data.entities.Foundation") {
			continue;
		}
		first_foundation = id;
		TESTEQUALS(entity->has_component(component::component_t::SELECTABLE), false);
		auto position = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		TESTEQUALS(position->get_positions().get(time::TIME_ZERO).to_tile(), (coord::tile{10, 10}));
		auto render = std::make_shared<renderer::world::RenderEntity>();
		entity->set_render_entity(render);
		system::Idle::idle(entity, time::TIME_ZERO);
		auto lock = render->get_read_lock();
		TESTEQUALS(render->get_animation_path().get(time::TIME_ZERO).ends_with("foundation.sprite"), true);
	}
	TESTEQUALS(first_foundation != 0, true);

	// A conflicting reservation must not overwrite or cancel the first order.
	TESTEQUALS(place(3, {builders[3]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {10, 10, 0}), false);
	TESTEQUALS(fvd->get_build_order(builders[2])->get_id(), order->get_id());
	TESTEQUALS(fvd->get_build_order(builders[3]) == nullptr, true);
	TESTEQUALS(place(2, {builders[2]}, {12, 12, 0}), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{10, 10}), false);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{12, 12}), true);
	TESTEQUALS(fvd->get_build_order(builders[2])->get_id() != order->get_id(), true);
	TESTEQUALS(state->get_game_entities().contains(first_foundation), false);

	// Other Builders may claim a tile released by the replaced order.
	TESTEQUALS(place(3, {builders[3]}, {10, 10, 0}), true);
	TESTEQUALS(fvd->get_build_order(builders[3])->get_owner(), player_id_t{3});
	TESTEQUALS(state->get_game_entities().size(), 9);

	// Dead Builders cannot initiate new construction.
	auto live = std::dynamic_pointer_cast<component::Live>(
		state->get_game_entity(builders[8])->get_component(component::component_t::LIVE));
	live->set_attribute(time::TIME_ZERO, "fvd.data.entities.Health", 0);
	TESTEQUALS(place(8, {builders[8]}, {127.5, 127.5, 0}), false);

	event::SendCommandHandler commander;
	commander.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                 {{"type", component::command::command_t::BUILD_TOWER},
	                  {"owner", player_id_t{4}},
	                  {"entity_ids", std::vector<entity_id_t>{builders[4]}},
	                  {"target", coord::phys3{20, 20, 0}}});
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{20, 20}), true);
	TESTEQUALS(fvd->get_build_order(builders[4])->get_owner(), player_id_t{4});
	// Spoofed MOVE must not cancel another player's pending build.
	commander.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                 {{"type", component::command::command_t::MOVE},
	                  {"owner", player_id_t{3}},
	                  {"entity_ids", std::vector<entity_id_t>{builders[4]}},
	                  {"target", coord::phys3{30, 30, 0}}});
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{20, 20}), true);
	commander.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                 {{"type", component::command::command_t::MOVE},
	                  {"owner", player_id_t{4}},
	                  {"entity_ids", std::vector<entity_id_t>{builders[4]}},
	                  {"target", coord::phys3{30, 30, 0}}});
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{20, 20}), false);
	TESTEQUALS(fvd->get_build_order(builders[4]) == nullptr, true);

	TESTEQUALS(fvd->cancel_build(state, builders[2], time::TIME_ZERO), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{12, 12}), false);
	TESTEQUALS(fvd->cancel_build(state, builders[2], time::TIME_ZERO), false);
	TESTEQUALS(place(5, {builders[5]}, {12, 12, 0}), true);
	TESTEQUALS(fvd->cancel_build(state, builders[3], time::TIME_ZERO), true);
	TESTEQUALS(fvd->cancel_build(state, builders[5], time::TIME_ZERO), true);
	TESTEQUALS(state->get_game_entities().size(), 7);

	event::SpawnEntityHandler spawner{loop, entities};
	TESTTHROWS(spawner.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                          {{"position", coord::phys3{10, 10, 0}},
	                           {"owner", player_id_t{2}},
	                           {"entity_ids", std::vector<entity_id_t>{builders[2]}}}));

	auto destination = coord::phys3{80, 80, 0};
	auto duration = system::Move::move_default(
		state->get_game_entity(builders[2]), state, destination, time::TIME_ZERO);
	TESTEQUALS(builder_pos->get_positions().get(duration), destination);
}

void fvd_construction() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();

	std::array<entity_id_t, 9> builders{};
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		builders.at(ownership->get_owners().get(time::TIME_ZERO)) = id;
	}
	auto t = [](double seconds) {
		return time::time_t::from_double(seconds);
	};
	auto build = [&](player_id_t owner, const coord::phys3 &tile, double seconds) {
		return fvd->request_build(state, owner, {builders.at(owner)}, tile, t(seconds));
	};

	// Start a route that crosses the future tower tile. The refresh must
	// replace its remaining position curve once construction blocks the tile.
	event::SendCommandHandler route_commander;
	route_commander.invoke(*loop, nullptr, state, time::TIME_ZERO,
	                       {{"type", component::command::command_t::MOVE},
	                        {"owner", player_id_t{3}},
	                        {"entity_ids", std::vector<entity_id_t>{builders[3]}},
	                        {"target", coord::phys3{60.5, 67.5, 0}}});
	loop->reach_time(time::TIME_ZERO, state);
	auto routed_position = std::dynamic_pointer_cast<component::Position>(
		state->get_game_entity(builders[3])->get_component(component::component_t::POSITION));
	TESTEQUALS(routed_position->get_positions().get(t(0.5)).to_tile(), (coord::tile{61, 66}));

	// An adjacent Builder starts building at once. Its footprint becomes
	// impassable immediately while health rises over the configured duration.
	TESTEQUALS(build(2, {61, 66, 0}, 0), true);
	TESTEQUALS(fvd->is_tile_reserved({61, 66}), true);
	TESTEQUALS(state->get_game_entities().size(), 8);
	bool planned = false;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == "fvd.data.entities.Foundation") {
			planned = true;
		}
	}
	TESTEQUALS(planned, true);
	loop->reach_time(t(0.05), state);
	TESTEQUALS(state->get_game_entities().size(), 8);
	for (const auto &[id, entity] : state->get_game_entities()) {
		TESTNOTEQUALS(entity->get_definition(), "fvd.data.entities.Foundation");
	}
	auto land_grid = state->get_map()->get_pathfinder()->get_grid(
		state->get_map()->get_grid_id("fvd.data.terrain.Land"));
	auto sector_size = land_grid->get_sector_size();
	auto tower_sector = land_grid->get_sector(61 / sector_size, 66 / sector_size);
	auto tower_local = coord::tile{61, 66}
	                 - tower_sector->get_position().to_tile(sector_size);
	TESTEQUALS(tower_sector->get_cost_field()->get_cost(tower_local), path::COST_IMPASSABLE);
	entity_id_t first_tower = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() != "fvd.data.entities.Tower") {
			continue;
		}
		first_tower = id;
		auto tower_live = std::dynamic_pointer_cast<component::Live>(
			entity->get_component(component::component_t::LIVE));
		auto mid_health = tower_live->get_attribute(
			 t(0.05), "fvd.data.entities.Health")->get(t(0.05));
		TESTEQUALS(mid_health > 0 and mid_health < 100, true);
	}
	TESTEQUALS(first_tower != 0, true);
	fvd->refresh_paths(state, t(0.05));
	TESTEQUALS((routed_position->get_positions().get(t(0.5)).to_tile() != coord::tile{61, 66}), true);
	loop->reach_time(t(0.1), state);
	TESTEQUALS(state->get_game_entities().size(), 8);
	TESTEQUALS(fvd->is_tile_reserved({61, 66}), false);
	TESTEQUALS(fvd->get_build_order(builders[2]) == nullptr, true);
	bool completed = false;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == "fvd.data.entities.Tower") {
			auto pos = std::dynamic_pointer_cast<component::Position>(
				entity->get_component(component::component_t::POSITION));
			if (pos->get_positions().get(t(0.1)).to_tile() == coord::tile{61, 66}) {
				auto ownership = std::dynamic_pointer_cast<component::Ownership>(
					entity->get_component(component::component_t::OWNERSHIP));
				TESTEQUALS(ownership->get_owners().get(t(0.1)), player_id_t{2});
				auto tower_live = std::dynamic_pointer_cast<component::Live>(
					entity->get_component(component::component_t::LIVE));
				TESTEQUALS(tower_live->get_attribute(t(0.1), "fvd.data.entities.Health")->get(t(0.1)), 100);
				completed = true;
			}
		}
	}
	TESTEQUALS(completed, true);

	// Distant construction must travel first and then finish.
	TESTEQUALS(build(3, {75, 75, 0}, 4), true);
	auto moving = state->get_game_entity(builders[3]);
	auto position = std::dynamic_pointer_cast<component::Position>(
		moving->get_component(component::component_t::POSITION));
	auto start = position->get_positions().get(t(4));
	TESTEQUALS(position->get_positions().get(t(5)) != start, true);
	loop->reach_time(t(5), state);
	TESTEQUALS(state->get_game_entities().size(), 9);
	loop->reach_time(t(30), state);
	TESTEQUALS(fvd->is_tile_reserved({75, 75}), false);
	TESTEQUALS(state->get_game_entities().size(), 9);

	// Issuing a different command cancels the work, truncates the old
	// movement curve, and makes scheduled completion a no-op.
	TESTEQUALS(build(4, {90, 90, 0}, 31), true);
	event::SendCommandHandler commander;
	commander.invoke(*loop, nullptr, state, t(32),
	                 {{"type", component::command::command_t::MOVE},
	                  {"owner", player_id_t{4}},
	                  {"entity_ids", std::vector<entity_id_t>{builders[4]}},
	                  {"target", coord::phys3{70, 70, 0}}});
	TESTEQUALS(fvd->is_tile_reserved({90, 90}), false);
	TESTEQUALS(fvd->get_build_order(builders[4]) == nullptr, true);
	loop->reach_time(t(100), state);
	TESTEQUALS(state->get_game_entities().size(), 9);

	// A Builder who dies during travel does not complete construction.
	TESTEQUALS(build(5, {95, 95, 0}, 101), true);
	auto victim = state->get_game_entity(builders[5]);
	auto live = std::dynamic_pointer_cast<component::Live>(
		victim->get_component(component::component_t::LIVE));
	live->set_attribute(t(102), "fvd.data.entities.Health", 0);
	loop->reach_time(t(150), state);
	TESTEQUALS(fvd->is_tile_reserved({95, 95}), false);
	TESTEQUALS(fvd->get_build_order(builders[5]) == nullptr, true);
	TESTEQUALS(state->get_game_entities().size(), 9);

	// A newer order supersedes a previous construction. Its old event
	// cannot spawn a tower even once the previous deadline passes.
	TESTEQUALS(build(6, {83, 83, 0}, 151), true);
	auto original = fvd->get_build_order(builders[6])->get_id();
	TESTEQUALS(build(6, {84, 84, 0}, 152), true);
	TESTEQUALS(fvd->get_build_order(builders[6])->get_id() != original, true);
	TESTEQUALS(fvd->is_tile_reserved({83, 83}), false);
	loop->reach_time(t(210), state);
	TESTEQUALS(fvd->is_tile_reserved({84, 84}), false);

	TESTEQUALS(state->get_game_entities().size(), 10);

	// Fully surrounded sites have no reachable, vacant adjacent stand tile.
	const std::array<coord::tile_delta, 8> neighbors{{
		{-1, 0}, {1, 0}, {0, -1}, {0, 1},
		{-1, -1}, {-1, 1}, {1, -1}, {1, 1},
	}};
	for (const auto &delta : neighbors) {
		coord::tile tile{110 + delta.ne, 110 + delta.se};
		entities->spawn_game_entity(loop, state, 2, "fvd.data.entities.Tower",
		                           tile.to_phys3_center(), t(211));
	}
	TESTEQUALS(build(7, {110, 110, 0}, 211), false);
	TESTEQUALS(fvd->is_tile_reserved({110, 110}), false);
	TESTEQUALS(state->get_game_entities().size(), 18);

	// A dead Builder with a construction already underway cannot finish it.
	TESTEQUALS(build(7, {66, 66, 0}, 212), true);
	auto final_builder = state->get_game_entity(builders[7]);
	auto final_live = std::dynamic_pointer_cast<component::Live>(
		final_builder->get_component(component::component_t::LIVE));
	loop->reach_time(t(212.05), state);
	final_live->set_attribute(t(212.05), "fvd.data.entities.Health", 0);
	loop->reach_time(t(220), state);
	TESTEQUALS(fvd->is_tile_reserved({66, 66}), false);
	TESTEQUALS(state->get_game_entities().size(), 18);
	auto canceled_sector = land_grid->get_sector(66 / sector_size, 66 / sector_size);
	auto canceled_local = coord::tile{66, 66}
	                   - canceled_sector->get_position().to_tile(sector_size);
	TESTEQUALS(canceled_sector->get_cost_field()->get_cost(canceled_local), path::cost_t{1});

	// Destruction removes the entity and restores the terrain's original path cost.
	TESTEQUALS(fvd->destroy_tower(state, first_tower, t(221)), true);
	TESTEQUALS(state->get_game_entities().size(), 17);
	TESTEQUALS(tower_sector->get_cost_field()->get_cost(tower_local), path::cost_t{1});
	TESTEQUALS(fvd->destroy_tower(state, first_tower, t(222)), false);
}

void fvd_hunter_spawning() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();
	auto at = [](double seconds) {
		return time::time_t::from_double(seconds);
	};
	auto count_horses = [&]() {
		size_t horses = 0;
		for (const auto &[id, entity] : state->get_game_entities()) {
			if (entity->get_definition() == "fvd.data.entities.Horse") {
				++horses;
			}
		}
		return horses;
	};

	// Preparing the map does not start the Hunter wave.
	loop->reach_time(at(0), state);
	TESTEQUALS(count_horses(), size_t{0});
	fvd->start_hunter_wave(state, at(0));
	fvd->start_hunter_wave(state, at(0));

	loop->reach_time(at(19.999), state);
	TESTEQUALS(count_horses(), size_t{0});
	loop->reach_time(at(20), state);
	TESTEQUALS(count_horses(), size_t{1});
	loop->reach_time(at(20.999), state);
	TESTEQUALS(count_horses(), size_t{1});
	loop->reach_time(at(21), state);
	TESTEQUALS(count_horses(), size_t{2});
	loop->reach_time(at(22), state);
	TESTEQUALS(count_horses(), size_t{3});

	// Horses never receive an automatic Move order. The only displacement
	// comes from nearby bodies making room for subsequent spawns.
	loop->reach_time(at(23), state);
	TESTEQUALS(count_horses(), size_t{4});
	loop->reach_time(at(23.4), state);
	std::vector<coord::phys3> horse_positions;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() != "fvd.data.entities.Horse") {
			continue;
		}
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		auto position = std::dynamic_pointer_cast<component::Position>(
			entity->get_component(component::component_t::POSITION));
		auto move = std::dynamic_pointer_cast<component::Move>(
			entity->get_component(component::component_t::MOVE));
		TESTEQUALS(ownership->get_owners().get(at(23.4)), player_id_t{1});
		TESTEQUALS(entity->has_component(component::component_t::SELECTABLE), true);
		TESTEQUALS(move->get_active_target().has_value(), false);
		auto at_end = position->get_positions().get(at(23.4));
		auto displacement = at_end - fvd->get_centre();
		TESTEQUALS(displacement.length() < 5.0, true);
		horse_positions.push_back(at_end);
	}
	TESTEQUALS(horse_positions.size(), size_t{4});
	for (size_t i = 0; i < horse_positions.size(); ++i) {
		for (size_t j = i + 1; j < horse_positions.size(); ++j) {
			auto delta = horse_positions[i] - horse_positions[j];
			// 0.38 + 0.38 radii, with a little numerical tolerance.
			TESTEQUALS(delta.length() > 0.68, true);
		}
	}
}

void fvd_horse_spawn_blocked() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();
	auto t = [](double seconds) {
		return time::time_t::from_double(seconds);
	};
	entity_id_t builder = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (ownership->get_owners().get(time::TIME_ZERO) == player_id_t{5}) {
			builder = id;
			break;
		}
	}
	TESTEQUALS(builder != 0, true);
	// Construct a path-blocking tower directly on the future spawn site.
	TESTEQUALS(fvd->request_build(state, 5, {builder}, fvd->get_centre(), t(0)), true);
	loop->reach_time(t(1), state);
	TESTEQUALS(fvd->is_tile_reserved({64, 64}), false);

	fvd->start_hunter_wave(state, t(0));
	loop->reach_time(t(20), state);
	std::shared_ptr<GameEntity> horse;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == "fvd.data.entities.Horse") {
			horse = entity;
		}
	}
	TESTEQUALS(horse != nullptr, true);
	loop->reach_time(t(20.4), state);
	auto position = std::dynamic_pointer_cast<component::Position>(
		horse->get_component(component::component_t::POSITION));
	// A blocked centre never vetoes the scheduled spawn: contact shoves
	// the new horse into nearby accessible terrain instead.
	TESTEQUALS(position->get_positions().get(t(20.4)) != fvd->get_centre(), true);
}

void fvd_hunter_combat() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();
	auto at = [](double seconds) { return time::time_t::from_double(seconds); };

	entity_id_t builder = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto owner = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (owner->get_owners().get(time::TIME_ZERO) == player_id_t{5}) {
			builder = id;
			break;
		}
	}
	TESTEQUALS(builder != 0, true);
	TESTEQUALS(fvd->request_build(state, 5, {builder}, {68, 69, 0}, at(0)), true);
	loop->reach_time(at(15), state);
	entity_id_t tower = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == "fvd.data.entities.Tower") {
			tower = id;
		}
	}
	TESTEQUALS(tower != 0, true);
	auto tower_entity = state->get_game_entity(tower);
	auto tower_health = std::dynamic_pointer_cast<component::Live>(
		tower_entity->get_component(component::component_t::LIVE));
	TESTEQUALS(tower_health->get_attribute(at(15), "fvd.data.entities.Health")->get(at(15)), int64_t{100});
	TESTEQUALS(fvd->get_kills(5), uint64_t{0});

	// The host validates an entire Hunter group; mixed player ownership
	// rejects the command without moving any of the horses.
	auto one = entities->spawn_game_entity(loop, state, 1, "fvd.data.entities.Horse",
	                                      {71, 69, 0}, at(15));
	auto two = entities->spawn_game_entity(loop, state, 1, "fvd.data.entities.Horse",
	                                      {72, 69, 0}, at(15));
	TESTEQUALS(fvd->issue_attack_move(state, 2, {one->get_id()}, {73, 69, 0}, at(15)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {one->get_id(), builder}, {73, 69, 0}, at(15)), false);
	TESTEQUALS(fvd->can_command_units(state, 1, {one->get_id(), two->get_id()}, at(15)), true);
	TESTEQUALS(fvd->can_command_units(state, 5, {one->get_id()}, at(15)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {one->get_id(), two->get_id()},
	                                  {70, 69, 0}, at(15)), true);
	auto horse_pos = std::dynamic_pointer_cast<component::Position>(
		one->get_component(component::component_t::POSITION));
	TESTEQUALS(horse_pos->get_positions().get(at(15.25)).ne != coord::phys_t::from_int(71), true);

	// Completed towers automatically fire at Hunter units.
	fvd->combat_tick(state, at(15));
	loop->reach_time(at(15.2), state);
	auto horse_live = std::dynamic_pointer_cast<component::Live>(
		one->get_component(component::component_t::LIVE));
	TESTEQUALS(horse_live->get_attribute(at(15.2), "fvd.data.entities.Health")->get(at(15.2)) < 25, true);

	// An in-flight shot resolves once; extra hits against a corpse cannot
	// award another kill even if several towers fire simultaneously.
	for (int i = 0; i < 15 and state->get_game_entities().contains(one->get_id()); ++i) {
		const auto when = at(16 + i);
		fvd->combat_tick(state, when);
		loop->reach_time(when + at(0.2), state);
	}
	TESTEQUALS(state->get_game_entities().contains(one->get_id()), false);
	auto kills = fvd->get_kills(5);
	TESTEQUALS(kills >= 1, true);
	fvd->arrow_hit(state, one->get_id(), 5, 100, at(32));
	TESTEQUALS(fvd->get_kills(5), kills);
	TESTEQUALS(fvd->get_kills(4), uint64_t{0});

	// A separate horse in melee range damages a tower, with armour
	// reducing but not preventing all damage.
	auto melee = entities->spawn_game_entity(loop, state, 1, "fvd.data.entities.Horse",
	                                        {67, 69, 0}, at(33));
	TESTEQUALS(fvd->issue_attack_move(state, 1, {melee->get_id()},
	                                  {67, 70, 0}, at(33)), true);
	fvd->combat_tick(state, at(33));
	TESTEQUALS(tower_health->get_attribute(at(33), "fvd.data.entities.Health")->get(at(33)) < 100, true);
}

void fvd_hunter_attack_move_regressions() {
	auto root = util::Path{std::make_shared<util::fslike::Directory>(config::buildsystem_sourcefile_dir)};
	auto mods = std::make_shared<assets::ModManager>(root / "assets" / "converted");
	mods->register_modpack(root / "assets" / "converted" / "engine" / "modpack.toml");
	mods->register_modpack(root / "assets" / "fvd" / "modpack.toml");
	mods->activate_modpacks({"engine", "fvd"});
	auto loop = std::make_shared<openage::event::EventLoop>();
	loop->add_event_handler(std::make_shared<event::ProcessCommandHandler>());
	loop->add_event_handler(std::make_shared<event::WaitHandler>());
	auto entities = std::make_shared<EntityFactory>();
	auto terrain = std::make_shared<TerrainFactory>();
	Game game{loop, mods, entities, terrain};
	auto state = game.get_state();
	auto fvd = state->get_fvd();
	auto at = [](double seconds) { return time::time_t::from_double(seconds); };

	entity_id_t builder = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		auto ownership = std::dynamic_pointer_cast<component::Ownership>(
			entity->get_component(component::component_t::OWNERSHIP));
		if (ownership->get_owners().get(time::TIME_ZERO) == player_id_t{5}) {
			builder = id;
			break;
		}
	}
	TESTEQUALS(builder != 0, true);
	TESTEQUALS(fvd->request_build(state, 5, {builder}, {68, 69, 0}, at(0)), true);
	loop->reach_time(at(15), state);
	entity_id_t tower = 0;
	for (const auto &[id, entity] : state->get_game_entities()) {
		if (entity->get_definition() == "fvd.data.entities.Tower") {
			tower = id;
			break;
		}
	}
	TESTEQUALS(tower != 0, true);
	auto tower_entity = state->get_game_entity(tower);
	auto health = std::dynamic_pointer_cast<component::Live>(
		tower_entity->get_component(component::component_t::LIVE));

	// Notice a tower within three tiles and approach it instead of
	// following an attack-move path that leads away from the tower.
	auto approaching = entities->spawn_game_entity(
		loop, state, 1, "fvd.data.entities.Horse", {71, 69, 0}, at(15));
	TESTEQUALS(fvd->issue_attack_move(state, 1, {approaching->get_id()},
	                                  {78, 69, 0}, at(15)), true);
	auto approach_position = std::dynamic_pointer_cast<component::Position>(
		approaching->get_component(component::component_t::POSITION));
	auto tower_position = std::dynamic_pointer_cast<component::Position>(
		tower_entity->get_component(component::component_t::POSITION));
	auto tower_at = tower_position->get_positions().get(at(15));
	auto before = approach_position->get_positions().get(at(15)) - tower_at;
	fvd->combat_tick(state, at(15));
	auto after = approach_position->get_positions().get(at(15.2)) - tower_at;
	TESTEQUALS(after.length() < before.length(), true);
	fvd->combat_tick(state, at(16));
	TESTEQUALS(health->get_attribute(at(16), "fvd.data.entities.Health")->get(at(16)) < 100, true);

	// Reaching the destination doesn't remove tower awareness.
	auto finished = entities->spawn_game_entity(
		loop, state, 1, "fvd.data.entities.Horse", {74, 69, 0}, at(20));
	TESTEQUALS(fvd->issue_attack_move(state, 1, {finished->get_id()},
	                                  {71, 69, 0}, at(20)), true);
	fvd->combat_tick(state, at(20));
	auto finished_position = std::dynamic_pointer_cast<component::Position>(
		finished->get_component(component::component_t::POSITION));
	auto at_destination = finished_position->get_positions().get(at(21.2)) - tower_at;
	fvd->combat_tick(state, at(21.2));
	auto after_destination = finished_position->get_positions().get(at(21.5)) - tower_at;
	TESTEQUALS(after_destination.length() < at_destination.length(), true);

	// A horse killed between selection and command is pruned, rather
	// than invalidating all surviving Hunter horses in that selection.
	auto dead = entities->spawn_game_entity(
		loop, state, 1, "fvd.data.entities.Horse", {95, 95, 0}, at(30));
	auto survivor = entities->spawn_game_entity(
		loop, state, 1, "fvd.data.entities.Horse", {96, 95, 0}, at(30));
	fvd->arrow_hit(state, dead->get_id(), 5, 100, at(30));
	TESTEQUALS(state->get_game_entities().contains(dead->get_id()), false);
	TESTEQUALS(fvd->can_command_units(
		state, 1, {dead->get_id(), survivor->get_id()}, at(30)), true);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {dead->get_id(), survivor->get_id()},
	                                  {97, 95, 0}, at(30)), true);
	auto surviving_move = std::dynamic_pointer_cast<component::Move>(
		survivor->get_component(component::component_t::MOVE));
	TESTEQUALS(surviving_move->get_active_target().has_value(), true);
	TESTEQUALS(fvd->can_command_units(state, 1, {dead->get_id()}, at(30)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {dead->get_id()},
	                                  {98, 95, 0}, at(30)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {survivor->get_id(), builder},
	                                  {98, 95, 0}, at(30)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {survivor->get_id(), entity_id_t{999999}},
	                                  {98, 95, 0}, at(30)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 1, {survivor->get_id(), survivor->get_id()},
	                                  {98, 95, 0}, at(30)), false);
	TESTEQUALS(fvd->issue_attack_move(state, 2, {survivor->get_id()},
	                                  {98, 95, 0}, at(30)), false);

	// Both Hunter movement orders go through the same stale-selection
	// validation at the authoritative command handler.
	event::SendCommandHandler commander;
	bool accepted = false;
	commander.invoke(*loop, nullptr, state, at(30),
	                 {{"type", component::command::command_t::MOVE},
	                  {"owner", player_id_t{1}},
	                  {"entity_ids", std::vector<entity_id_t>{dead->get_id(), survivor->get_id()}},
	                  {"target", coord::phys3{98, 95, 0}},
	                  {"result_cb", std::function<void(bool)>{[&accepted](bool result) {
		                  accepted = result;
	                  }}}});
	TESTEQUALS(accepted, true);
}
}
