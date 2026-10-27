// Copyright 2026 the openage authors. See copying.md for legal info.

#include "hud_mouse_capture.h"

namespace openage::presenter {

HudMouseCapture::Result HudMouseCapture::process(
	QEvent::Type type, Qt::MouseButton button,
	bool over_hud, Action hovered_action) {
	const auto mask = static_cast<unsigned int>(button);
	const bool captured = (this->captured_buttons & mask) != 0;
	Action activated_action = Action::NONE;

	if (type == QEvent::MouseButtonPress or type == QEvent::MouseButtonDblClick) {
		if (over_hud) {
			this->captured_buttons |= mask;
		}
		if (button == Qt::LeftButton) {
			this->pressed_action = over_hud ? hovered_action : Action::NONE;
		}
	}
	else if (type == QEvent::MouseButtonRelease) {
		if (button == Qt::LeftButton) {
			if (captured and over_hud and this->pressed_action != Action::NONE
			    and hovered_action == this->pressed_action) {
				activated_action = hovered_action;
			}
			this->pressed_action = Action::NONE;
		}
		this->captured_buttons &= ~mask;
	}
	return {over_hud or captured, activated_action};
}

bool HudMouseCapture::is_capturing() const {
	return this->captured_buttons != 0;
}

} // namespace openage::presenter
