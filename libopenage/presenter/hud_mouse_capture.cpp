// Copyright 2026 the openage authors. See copying.md for legal info.

#include "hud_mouse_capture.h"

namespace openage::presenter {

HudMouseCapture::Result HudMouseCapture::process(
	QEvent::Type type, Qt::MouseButton button,
	bool over_hud, bool over_action) {
	const auto mask = static_cast<unsigned int>(button);
	const bool captured = (this->captured_buttons & mask) != 0;
	bool activated_button = false;

	if (type == QEvent::MouseButtonPress or type == QEvent::MouseButtonDblClick) {
		if (over_hud) {
			this->captured_buttons |= mask;
		}
		if (button == Qt::LeftButton) {
			this->action_button_pressed = over_hud and over_action;
		}
	}
	else if (type == QEvent::MouseButtonRelease) {
		if (button == Qt::LeftButton) {
			activated_button = captured and this->action_button_pressed
			                   and over_hud and over_action;
			this->action_button_pressed = false;
		}
		this->captured_buttons &= ~mask;
	}
	return {over_hud or captured, activated_button};
}

bool HudMouseCapture::is_capturing() const {
	return this->captured_buttons != 0;
}

} // namespace openage::presenter
