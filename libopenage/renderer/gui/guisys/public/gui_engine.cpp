// Copyright 2015-2022 the openage authors. See copying.md for legal info.

#include "renderer/gui/guisys/public/gui_engine.h"

#include "renderer/gui/guisys/private/gui_engine_impl.h"

#include <QQmlContext>
#include <QQmlEngine>

namespace qtgui {

GuiQmlEngine::GuiQmlEngine(std::shared_ptr<GuiRenderer> renderer) :
	impl{std::make_unique<GuiQmlEngineImpl>(renderer)} {
}

GuiQmlEngine::~GuiQmlEngine() = default;

void GuiQmlEngine::set_context_property(const QString &name, QObject *object) {
	this->impl->get_qml_engine()->rootContext()->setContextProperty(name, object);
}

} // namespace qtgui
