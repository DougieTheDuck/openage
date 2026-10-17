// Copyright 2026 the openage authors. See copying.md for legal info.

#include "hud_mouse_capture.h"

#include "testing/testing.h"

namespace openage::presenter::tests {

void hud_mouse_capture() {
	HudMouseCapture capture{};
	TESTEQUALS(capture.is_capturing(), false);

	// Clicking in the world still produces ordinary selection/orders.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, false), false);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, false), false);

	// Clicking Build Tower must never select terrain behind the button.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, true), true);
	TESTEQUALS(capture.is_capturing(), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, true), true);
	TESTEQUALS(capture.is_capturing(), false);

	// A press begun over the panel stays captured if released in the world.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, true), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, false), true);
	TESTEQUALS(capture.is_capturing(), false);
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, false), false);

	// A world drag released on the HUD must not select through the UI.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, false), false);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, true), true);

	// Cancel from the panel blocks right-click movement as well.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::RightButton, true), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::RightButton, false), true);
	TESTEQUALS(capture.is_capturing(), false);

	// Left and right gestures cannot accidentally uncapture one another.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, true), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::RightButton, true), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, false), true);
	TESTEQUALS(capture.is_capturing(), true);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::RightButton, false), true);
	TESTEQUALS(capture.is_capturing(), false);

	// The next normal world click remains fully functional.
	TESTEQUALS(capture.consume(QEvent::MouseButtonPress, Qt::LeftButton, false), false);
	TESTEQUALS(capture.consume(QEvent::MouseButtonRelease, Qt::LeftButton, false), false);
}

} // namespace openage::presenter::tests
