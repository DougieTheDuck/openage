// Copyright 2026 the openage authors. See copying.md for legal info.

import QtQuick

Item {
	Rectangle {
		anchors.top: parent.top
		anchors.horizontalCenter: parent.horizontalCenter
		width: help.width + 32
		height: help.height + 20
		color: "#d0203020"
		radius: 6

		Text {
			id: help
			anchors.centerIn: parent
			color: "#ffffff"
			font.pixelSize: 14
			text: "Flat plains | 1: Hunter | 2-8: Builders\nDrag to select | Right-click to move | Ctrl-left-click to place a tower"
			horizontalAlignment: Text.AlignHCenter
		}
	}
}
