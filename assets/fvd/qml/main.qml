// Copyright 2026 the openage authors. See copying.md for legal info.

import QtQuick

Item {
	id: root

	property var hud: typeof fvdHud === "undefined" ? null : fvdHud
	readonly property bool hunter: hud !== null && hud.controlled === 1
	readonly property bool placing: hud !== null && hud.placing
	readonly property bool canBuild: hud !== null && hud.canBuild
	readonly property bool isTower: hud !== null && hud.isTower
	readonly property bool canAttack: hud !== null && hud.canAttack
	readonly property bool attacking: hud !== null && hud.attacking
	readonly property int hunterCountdown: hud === null ? 0 : hud.hunterCountdown
	readonly property var arrowStreaks: hud === null ? [] : hud.arrowStreaks

	// Screen-space display only. Simulation shot events determine impact
	// and damage; this Canvas never creates selectable world objects.
	Canvas {
		id: arrows
		anchors.fill: parent
		z: -1
		visible: root.arrowStreaks.length > 0
		onVisibleChanged: if (visible) requestPaint()
		Connections {
			target: root
			function onArrowStreaksChanged() {
				if (arrows.visible)
					arrows.requestPaint()
			}
		}
		onPaint: {
			var ctx = getContext("2d")
			ctx.clearRect(0, 0, width, height)
			var list = root.arrowStreaks
			for (var i = 0; i < list.length; ++i) {
				var shot = list[i]
				var dx = shot.x1 - shot.x0
				var dy = shot.y1 - shot.y0
				var len = Math.sqrt(dx * dx + dy * dy)
				if (len < 0.5) continue
				var nx = dx / len
				var ny = dy / len
				ctx.globalAlpha = shot.alpha
				ctx.strokeStyle = "#f9d7a1"
				ctx.lineWidth = 2
				ctx.lineCap = "round"
				ctx.beginPath()
				ctx.moveTo(shot.x0, shot.y0)
				ctx.lineTo(shot.x1, shot.y1)
				ctx.stroke()
				// Two tiny fletching strokes distinguish the arrow head.
				ctx.lineWidth = 1
				ctx.beginPath()
				ctx.moveTo(shot.x1 - nx * 5 - ny * 2, shot.y1 - ny * 5 + nx * 2)
				ctx.lineTo(shot.x1, shot.y1)
				ctx.lineTo(shot.x1 - nx * 5 + ny * 2, shot.y1 - ny * 5 - nx * 2)
				ctx.stroke()
			}
			ctx.globalAlpha = 1
		}
	}

	Rectangle {
		id: countdownBanner
		anchors.top: parent.top
		anchors.horizontalCenter: parent.horizontalCenter
		visible: root.hunterCountdown > 0
		width: 250
		height: visible ? 74 : 0
		color: "#ed182824"
		border.width: 1
		border.color: root.hunterCountdown <= 5 ? "#f4a861" : "#72918b"
		radius: 7

		Column {
			anchors.centerIn: parent
			spacing: 1

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: "HUNTER ARRIVES IN"
				color: "#e1e9e4"
				font.pixelSize: 12
				font.bold: true
			}

			Text {
				anchors.horizontalCenter: parent.horizontalCenter
				text: "00:" + ("0" + root.hunterCountdown).slice(-2)
				color: root.hunterCountdown <= 5 ? "#ffb877" : "#ffffff"
				font.pixelSize: 33
				font.bold: true
			}
		}
	}

	Rectangle {
		anchors.top: countdownBanner.bottom
		anchors.topMargin: root.hunterCountdown > 0 ? 6 : 0
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
			    : root.attacking ? "Attack-move: click map | Right-click or Esc cancels"
			    : root.canAttack
			    ? "Hunter: Right-click enemy to focus | Right-click ground to move | A: Attack-move"
			    : "Flat plains | 1: Hunter | 2-8: Builders | Drag to select | Right-click to move"
			horizontalAlignment: Text.AlignHCenter
		}
	}

	Rectangle {
		id: buildPanel
		anchors.left: parent.left
		anchors.bottom: parent.bottom
		width: 284
		height: 196
		radius: 6
		color: "#ee182824"
		border.width: 1
		border.color: "#72918b"

		Column {
			anchors.fill: parent
			anchors.margins: 12
			spacing: 8

			Text {
				color: "#ffffff"
				font.bold: true
				font.pixelSize: 16
				text: root.hud === null ? "FVD" : "Player " + root.hud.controlled
			      + (root.hunter ? " — Hunter" : " — Builder")
			}
			Text {
				color: "#ffffff"
				font.bold: true
				font.pixelSize: 14
				text: root.hud === null ? "No unit selected" : root.hud.selection
			}
			Text {
				visible: root.isTower || root.canBuild || root.canAttack
				color: "#d6e6db"
				font.pixelSize: 13
				text: "HP: " + (root.hud === null ? "—" : root.hud.health)
			}
			Text {
				visible: root.isTower
				color: "#d6e6db"
				font.pixelSize: 13
				text: "Damage: " + (root.hud === null ? "Not defined" : root.hud.damage)
			}
			Text {
				visible: root.isTower
				color: "#d6e6db"
				font.pixelSize: 13
				text: "Armour: " + (root.hud === null ? "Not defined" : root.hud.armor)
			}
			Text {
				color: "#d6e6db"
				font.pixelSize: 13
				text: "Tower kills: " + (root.hud === null ? 0 : root.hud.kills)
			}
			Rectangle {
				visible: root.canBuild || root.canAttack
				width: 252
				height: 32
				radius: 4
				color: root.placing || root.attacking ? "#ae6e30" : "#3e7862"
				border.width: 1
				border.color: "#b5d9c4"

				Text {
					anchors.centerIn: parent
					color: "#ffffff"
					font.bold: true
					font.pixelSize: 14
					text: root.placing || root.attacking ? "Cancel (Esc)" : root.canAttack ? "Attack-move (A)" : "Build Tower (B)"
				}

				MouseArea {
					anchors.fill: parent
					onClicked: {
						if (root.hud !== null)
							root.hud.action = root.placing || root.attacking ? "cancel" : root.canAttack ? "attack" : "build"
					}
				}
			}
		}
	}
}
