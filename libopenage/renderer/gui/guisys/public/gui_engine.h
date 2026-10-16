// Copyright 2015-2022 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QString>

// QT_FORWARD_DECLARE_CLASS(QQuickWindow)

namespace qtgui {

class GuiQmlEngineImpl;
class GuiRenderer;

/**
 * Represents one QML execution environment.
 */
class GuiQmlEngine {
public:
	explicit GuiQmlEngine(std::shared_ptr<GuiRenderer> renderer);
	~GuiQmlEngine();

	/** Expose a QObject to QML without tying the GUI renderer to game logic. */
	void set_context_property(const QString &name, QObject *object);

private:
	friend class GuiQmlEngineImpl;
	std::unique_ptr<GuiQmlEngineImpl> impl;
};

} // namespace qtgui
