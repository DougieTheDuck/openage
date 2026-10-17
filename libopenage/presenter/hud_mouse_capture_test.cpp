// Copyright 2026 the openage authors. See copying.md for legal info.

#include "hud_mouse_capture.h"

#include "testing/testing.h"

namespace openage::presenter::tests {

void hud_mouse_capture() {
	HudMouseCapture capture{};
	TESTEQUALS(capture.is_capturing(), false);
	auto click = [&](QEvent::Type type, Qt::MouseButton button,
	                 bool panel, bool action) {
		return capture.process(type, button, panel, action);
	};

	// Ordinary world clicks must remain usable.
	TESTEQUALS(click(QEvent::MouseButtonPress, Qt::LeftButton, false, false).consumed, false);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::LeftButton, false, false).consumed, false);

	// The Build Tower action activates on the matching release, only once.
	auto press = click(QEvent::MouseButtonPress, Qt::LeftButton, true, true);
	TESTEQUALS(press.consumed, true);
	TESTEQUALS(press.activated_button, false);
	TESTEQUALS(capture.is_capturing(), true);
	auto release = click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true);
	TESTEQUALS(release.consumed, true);
	TESTEQUALS(release.activated_button, true);
	TESTEQUALS(capture.is_capturing(), false);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true).activated_button, false);

	// Panel background and right-click cancel never activate the button.
	click(QEvent::MouseButtonPress, Qt::LeftButton, true, false);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true).activated_button, false);
	click(QEvent::MouseButtonPress, Qt::RightButton, true, true);
	auto right = click(QEvent::MouseButtonRelease, Qt::RightButton, true, true);
	TESTEQUALS(right.consumed, true);
	TESTEQUALS(right.activated_button, false);

	// Press on the action, drag outside, and release: no placement or order.
	click(QEvent::MouseButtonPress, Qt::LeftButton, true, true);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::LeftButton, false, false).consumed, true);
	TESTEQUALS(capture.is_capturing(), false);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true).activated_button, false);

	// A release inside the button without an action press cannot activate it.
	click(QEvent::MouseButtonPress, Qt::LeftButton, false, false);
	auto world_to_panel = click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true);
	TESTEQUALS(world_to_panel.consumed, true);
	TESTEQUALS(world_to_panel.activated_button, false);

	// Independent left/right capture does not steal or repeat the action.
	click(QEvent::MouseButtonPress, Qt::LeftButton, true, true);
	click(QEvent::MouseButtonPress, Qt::RightButton, true, false);
	TESTEQUALS(capture.is_capturing(), true);
	auto left = click(QEvent::MouseButtonRelease, Qt::LeftButton, true, true);
	TESTEQUALS(left.activated_button, true);
	TESTEQUALS(capture.is_capturing(), true);
	TESTEQUALS(click(QEvent::MouseButtonRelease, Qt::RightButton, false, false).consumed, true);
	TESTEQUALS(capture.is_capturing(), false);
}

} // namespace openage::presenter::tests
