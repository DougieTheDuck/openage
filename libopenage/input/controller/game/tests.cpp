// Copyright 2026 the openage authors. See copying.md for legal info.

#include "controller.h"

#include "testing/testing.h"

namespace openage::input::game::tests {

void tower_placement_mode() {
	Controller controller{{1, 2, 3}, 2};

	TESTEQUALS(controller.begin_tower_placement(), false);
	TESTEQUALS(controller.is_tower_placement_active(), false);

	controller.set_selected({42}, SelectionKind::TOWER);
	TESTEQUALS(controller.begin_tower_placement(), false);
	TESTEQUALS(controller.get_selection_details().kind == SelectionKind::TOWER, true);
	TESTEQUALS(controller.is_tower_placement_active(), false);

	controller.set_selected({42}, SelectionKind::BUILDER);
	TESTEQUALS(controller.begin_tower_placement(), true);
	TESTEQUALS(controller.is_tower_placement_active(), true);
	controller.cancel_tower_placement();
	TESTEQUALS(controller.is_tower_placement_active(), false);

	controller.begin_tower_placement();
	controller.set_selected({55}, SelectionKind::TOWER);
	TESTEQUALS(controller.get_selection_details().kind == SelectionKind::TOWER, true);
	TESTEQUALS(controller.is_tower_placement_active(), false);
	TESTEQUALS(controller.begin_tower_placement(), false);
	controller.set_selected({42}, SelectionKind::BUILDER);

	controller.begin_tower_placement();
	controller.set_selected({});
	TESTEQUALS(controller.is_tower_placement_active(), false);

	controller.set_selected({42}, SelectionKind::BUILDER);
	controller.begin_tower_placement();
	controller.set_control(1);
	TESTEQUALS(controller.is_tower_placement_active(), false);
	TESTEQUALS(controller.begin_tower_placement(), false);

	controller.set_control(2);
	controller.set_selected({42, 43}, SelectionKind::BUILDER);
	TESTEQUALS(controller.begin_tower_placement(), false);
	TESTEQUALS(controller.get_selection_details().kind == SelectionKind::NONE, true);
	controller.set_selected({43}, SelectionKind::TOWER);
	TESTEQUALS(controller.begin_tower_placement(), false);

	controller.set_control(1);
	controller.set_selected({101}, SelectionKind::HORSE);
	TESTEQUALS(controller.begin_attack_move(), true);
	TESTEQUALS(controller.is_attack_move_active(), true);
	controller.cancel_attack_move();
	TESTEQUALS(controller.is_attack_move_active(), false);
	controller.set_selected({101, 102, 103}, SelectionKind::GROUP_HORSES);
	TESTEQUALS(controller.begin_attack_move(), true);
	controller.set_control(2);
	TESTEQUALS(controller.is_attack_move_active(), false);
	TESTEQUALS(controller.begin_attack_move(), false);

	controller.set_selected({42}, SelectionKind::BUILDER);
	bool deferred_ran = false;
	auto stale_generation = controller.begin_selection_update();
	controller.defer_until_selection_applied([&]() { deferred_ran = true; }) or TESTFAIL;
	auto current_generation = controller.begin_selection_update();
	TESTEQUALS(controller.set_selected_if_current(
		stale_generation, {101}, SelectionKind::HORSE), false);
	TESTEQUALS(controller.get_selected().front(), gamestate::entity_id_t{42});
	TESTEQUALS(deferred_ran, false);
	controller.defer_until_selection_applied([&]() {
		deferred_ran = controller.get_selected().size() == 2;
	}) or TESTFAIL;
	TESTEQUALS(controller.set_selected_if_current(
		current_generation, {101, 102}, SelectionKind::GROUP_HORSES), true);
	TESTEQUALS(deferred_ran, true);
	TESTEQUALS(controller.get_selected().size(), size_t{2});

	auto invalidated_generation = controller.begin_selection_update();
	controller.defer_until_selection_applied([&]() { deferred_ran = false; }) or TESTFAIL;
	controller.set_control(1);
	TESTEQUALS(controller.set_selected_if_current(
		invalidated_generation, {101}, SelectionKind::HORSE), false);
	TESTEQUALS(deferred_ran, true);
}

} // namespace openage::input::game::tests
