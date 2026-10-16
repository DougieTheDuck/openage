// Copyright 2026 the openage authors. See copying.md for legal info.

import QtQuick

Item {
	id: root

	// Injected by the presenter; the HUD is not loaded for non-FVD games.
	property var hud: typeof fvdHud === "undefined" ? null : fvdHud
	readonly property bool hunter: hud !== null && hud.controlled === 1
	readonly property bool placing: hud !== null && hud.placing
	readonly property bool canBuild: hud !== null && hud.canBuild

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
			text: root.placing
			    ? "Tower placement: click a map tile | Right-click or Esc to cancel"
			    : "Flat plains | 1: Hunter | 2-8: Builders | Drag to select | Right-click to move"
			horizontalAlignment: Text.AlignHCenter
		}
	}

	Rectangle {
		id: buildPanel
		anchors.left: parent.left
		anchors.bottom: parent.bottom
		width: 284
		height: 148
		radius: 6
		color: "#ee182824"
		border.width: 1
		border.color: "#72918b"

		Column {
			anchors.fill: parent
			anchors.margins: 12
			spacing: 7

			Text {
				color: "#ffffff"
				font.bold: true
				font.pixelSize: 16
				text: root.hud === null ? "FVD" : "Player " + root.hud.controlled
			      + (root.hunter ? " — Hunter" : " — Builder")
			}
			Text {
				color: "#d6e6db"
				font.pixelSize: 13
				text: root.hud === null ? "No unit selected" : root.hud.selection
			}
			Text {
				color: "#d6e6db"
				font.pixelSize: 13
				text: "Tower kills: " + (root.hud === null ? 0 : root.hud.kills)
			}
			Rectangle {
				width: 252
				height: 32
				radius: 4
				color: root.canBuild ? (root.placing ? "#ae6e30" : "#3e7862") : "#414a48"
				border.width: 1
				border.color: root.canBuild ? "#b5d9c4" : "#66706c"

				Text {
					anchors.centerIn: parent
					color: "#ffffff"
					font.bold: true
					font.pixelSize: 14
					text: root.placing ? "Cancel placement (Esc)" : "Build Tower (B)"
				}

				MouseArea {
					anchors.fill: parent
					enabled: root.canBuild
					onClicked: {
						if (root.hud !== null)
							root.hud.action = root.placing ? "cancel" : "build"
					}
				}
			}
		}
	}
}
