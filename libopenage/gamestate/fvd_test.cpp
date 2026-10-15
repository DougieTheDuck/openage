// Copyright 2026 the openage authors. See copying.md for legal info.

#include <array>
#include <functional>
#include <memory>

#include "assets/mod_manager.h"
#include "config.h"
#include "event/event_loop.h"
#include "gamestate/component/api/live.h"
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
	auto tower_graphics = renderer::resources::parser::parse_sprite_file(graphics / "tower.sprite");
	TESTEQUALS(plains_graphics.get_texture_count(), 1);
	TESTEQUALS(builder_graphics.get_texture_count(), 1);
	TESTEQUALS(tower_graphics.get_texture_count(), 1);
	for (const auto &info : {plains_graphics.get_texture(0),
	                        builder_graphics.get_texture(0),
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
	TESTEQUALS(state->get_game_entities().size(), 7);

	// A conflicting reservation must not overwrite or cancel the first order.
	TESTEQUALS(place(3, {builders[3]}, {10, 10, 0}), false);
	TESTEQUALS(place(2, {builders[2]}, {10, 10, 0}), false);
	TESTEQUALS(fvd->get_build_order(builders[2])->get_id(), order->get_id());
	TESTEQUALS(fvd->get_build_order(builders[3]) == nullptr, true);
	TESTEQUALS(place(2, {builders[2]}, {12, 12, 0}), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{10, 10}), false);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{12, 12}), true);
	TESTEQUALS(fvd->get_build_order(builders[2])->get_id() != order->get_id(), true);

	// Other Builders may claim a tile released by the replaced order.
	TESTEQUALS(place(3, {builders[3]}, {10, 10, 0}), true);
	TESTEQUALS(fvd->get_build_order(builders[3])->get_owner(), player_id_t{3});
	TESTEQUALS(state->get_game_entities().size(), 7);

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

	TESTEQUALS(fvd->cancel_build(builders[2]), true);
	TESTEQUALS(fvd->is_tile_reserved(coord::tile{12, 12}), false);
	TESTEQUALS(fvd->cancel_build(builders[2]), false);
	TESTEQUALS(place(5, {builders[5]}, {12, 12, 0}), true);
	TESTEQUALS(fvd->cancel_build(builders[3]), true);
	TESTEQUALS(fvd->cancel_build(builders[5]), true);
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
}
