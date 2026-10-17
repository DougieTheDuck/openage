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
	struct Result {
		bool consumed;
		bool activated_button;
	};

	/**
	 * Consume HUD gestures and activate a left-clicked action on release.
	 * Both press and release must occur within the same action button.
	 */
	Result process(QEvent::Type type, Qt::MouseButton button,
	               bool over_hud, bool over_action);

	/** Whether any HUD mouse gesture is currently active. */
	bool is_capturing() const;

private:
	unsigned int captured_buttons{0};
	bool action_button_pressed{false};
};

} // namespace openage::presenter
