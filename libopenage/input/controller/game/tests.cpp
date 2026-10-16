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
}

} // namespace openage::input::game::tests
