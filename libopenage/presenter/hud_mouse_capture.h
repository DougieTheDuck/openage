// Copyright 2026 the openage authors. See copying.md for legal info.

#pragma once

#include <QEvent>

namespace openage::presenter {

/**
 * Track mouse gestures begun over the FVD HUD.
 *
 * A panel click must not turn into a world selection/order if the pointer
 * leaves the panel before its button is released.
 */
class HudMouseCapture {
public:
	/** Whether this mouse button event must be withheld from world input. */
	bool consume(QEvent::Type type, Qt::MouseButton button, bool over_hud);

	/** Whether any HUD mouse gesture is currently active. */
	bool is_capturing() const;

private:
	unsigned int captured_buttons{0};
};

} // namespace openage::presenter
