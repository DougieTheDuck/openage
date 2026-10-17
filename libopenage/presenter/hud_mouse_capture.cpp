// Copyright 2026 the openage authors. See copying.md for legal info.

#include "hud_mouse_capture.h"

namespace openage::presenter {

bool HudMouseCapture::consume(QEvent::Type type, Qt::MouseButton button, bool over_hud) {
	const auto mask = static_cast<unsigned int>(button);
	const bool captured = (this->captured_buttons & mask) != 0;

	if (type == QEvent::MouseButtonPress or type == QEvent::MouseButtonDblClick) {
		if (over_hud) {
			this->captured_buttons |= mask;
		}
	}
	else if (type == QEvent::MouseButtonRelease) {
		this->captured_buttons &= ~mask;
	}
	return over_hud or captured;
}

bool HudMouseCapture::is_capturing() const {
	return this->captured_buttons != 0;
}

} // namespace openage::presenter
