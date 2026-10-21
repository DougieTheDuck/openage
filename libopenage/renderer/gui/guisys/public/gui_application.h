// Copyright 2015-2022 the openage authors. See copying.md for legal info.

#pragma once

#include <functional>
#include <memory>

namespace qtgui {

class GuiApplicationImpl;

/**
 * Houses gui logic event queue.
 */
class GuiApplication {
public:
	GuiApplication();
	GuiApplication(std::shared_ptr<GuiApplicationImpl> application);
	~GuiApplication();

	/**
	 * Run the Qt event loop and invoke a callback for each frame.
	 *
	 * @param on_frame Frame callback. Return false to exit the event loop.
	 * @return Qt event loop exit code.
	 */
	int run_event_loop(const std::function<bool()> &on_frame);

	void process_events();

private:
	std::shared_ptr<GuiApplicationImpl> application;
};

} // namespace qtgui
