// Copyright 2015-2023 the openage authors. See copying.md for legal info.

#include <utility>

#include "gui_application.h"

#include "../private/gui_application_impl.h"

namespace qtgui {

GuiApplication::GuiApplication() :
	application{GuiApplicationImpl::get()} {
}

GuiApplication::GuiApplication(std::shared_ptr<GuiApplicationImpl> application) :
	application{application} {
}

GuiApplication::~GuiApplication() = default;

int GuiApplication::run_event_loop(const std::function<bool()> &on_frame) {
	return this->application->runEventLoop(on_frame);
}

void GuiApplication::process_events() {
	this->application->processEvents();
}

} // namespace qtgui
