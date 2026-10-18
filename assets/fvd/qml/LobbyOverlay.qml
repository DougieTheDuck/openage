// Copyright 2026 the openage authors. See copying.md for legal info.

import QtQuick

Item {
    id: menu
    anchors.fill: parent
    property var lobby: typeof fvdLobby === "undefined" ? null : fvdLobby
    visible: lobby !== null && lobby.enabled
    z: 100

    Rectangle {
        anchors.fill: parent
        color: "#f0121e1a"
    }

    Rectangle {
        id: panel
        objectName: "fvdLobbyPanel"
        width: Math.min(parent.width - 32, 540)
        height: Math.min(parent.height - 32, settings.implicitHeight + 46)
        anchors.centerIn: parent
        clip: true
        radius: 12
        color: "#f421302b"
        border.width: 1
        border.color: "#72918b"

        Flickable {
            anchors.fill: parent
            anchors.margins: 23
            contentWidth: width
            contentHeight: settings.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            clip: true

            Column {
                id: settings
                width: parent.width
                spacing: 9

                Text {
                    width: parent.width
                    text: "FAST VILLAGER DEFENSE"
                    horizontalAlignment: Text.AlignHCenter
                    color: "#f4ebca"
                    font.pixelSize: 22
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: "MULTIPLAYER LOBBY"
                    horizontalAlignment: Text.AlignHCenter
                    color: "#9ccab3"
                    font.pixelSize: 12
                    font.letterSpacing: 2
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: "Host a public game or refresh the relay room list and join by room ID. Enter an access token only when the relay runs in private-alpha mode."
                    color: "#bdd0c5"
                    font.pixelSize: 12
                }

                Item { width: 1; height: 3 }

                Text { text: "Player name"; color: "#dfe9e3"; font.pixelSize: 12 }
                Rectangle {
                    width: parent.width; height: 36; color: "#17221e"; radius: 4
                    border.color: "#688478"; border.width: 1
                    TextInput {
                        anchors.fill: parent; anchors.margins: 9
                        color: "white"; selectionColor: "#5c997f"
                        font.pixelSize: 14; text: "Player"
                        onTextEdited: if (menu.lobby) menu.lobby.name = text
                    }
                }

                Text { text: "Relay address and port"; color: "#dfe9e3"; font.pixelSize: 12 }
                Row {
                    width: parent.width; spacing: 8
                    Rectangle {
                        width: parent.width * 0.69; height: 36; color: "#17221e"; radius: 4
                        border.color: "#688478"; border.width: 1
                        TextInput {
                            anchors.fill: parent; anchors.margins: 9
                            text: "127.0.0.1"; color: "white"; font.pixelSize: 14
                            onTextEdited: if (menu.lobby) menu.lobby.address = text
                        }
                    }
                    Rectangle {
                        width: parent.width * 0.31 - 8; height: 36
                        color: "#17221e"; radius: 4; border.color: "#688478"; border.width: 1
                        TextInput {
                            anchors.fill: parent; anchors.margins: 9
                            text: "32456"; color: "white"; font.pixelSize: 14
                            inputMethodHints: Qt.ImhDigitsOnly
                            onTextEdited: if (menu.lobby) menu.lobby.port = text
                        }
                    }
                }

                Text { text: "Game name"; color: "#dfe9e3"; font.pixelSize: 12 }
                Rectangle {
                    width: parent.width; height: 36
                    color: "#17221e"; radius: 4; border.color: "#688478"; border.width: 1
                    TextInput {
                        anchors.fill: parent; anchors.margins: 9
                        text: "FVD Game"; color: "white"; font.pixelSize: 14
                        onTextEdited: if (menu.lobby) menu.lobby.roomName = text
                    }
                }

                Text { text: "Room ID to join"; color: "#dfe9e3"; font.pixelSize: 12 }
                Rectangle {
                    width: parent.width; height: 36
                    color: "#17221e"; radius: 4; border.color: "#688478"; border.width: 1
                    TextInput {
                        anchors.fill: parent; anchors.margins: 9
                        text: menu.lobby ? menu.lobby.roomId : ""
                        color: "white"; font.pixelSize: 14
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextEdited: if (menu.lobby) menu.lobby.roomId = text
                    }
                }

                Text { text: "Private-alpha player token (optional)"; color: "#dfe9e3"; font.pixelSize: 12 }
                Rectangle {
                    width: parent.width; height: 36
                    color: "#17221e"; radius: 4; border.color: "#688478"; border.width: 1
                    TextInput {
                        anchors.fill: parent; anchors.margins: 9
                        text: ""; color: "white"; font.pixelSize: 13
                        echoMode: TextInput.Password
                        onTextEdited: if (menu.lobby) menu.lobby.token = text
                    }
                }

                Row {
                    width: parent.width; spacing: 8
                    Rectangle {
                        objectName: "fvdLobbyHostButton"
                        width: (settings.width - 8) / 2; height: 41; radius: 5
                        color: "#426c5a"; border.color: "#9bcdb8"
                        Text {
                            anchors.centerIn: parent
                            text: "HOST GAME"; color: "white"; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                    }
                    Rectangle {
                        objectName: "fvdLobbyRefreshButton"
                        width: (settings.width - 8) / 2; height: 41; radius: 5
                        color: "#4b5f69"; border.color: "#9bbdd1"
                        Text {
                            anchors.centerIn: parent
                            text: "REFRESH GAMES"; color: "white"; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                    }
                }

                Rectangle {
                    objectName: "fvdLobbyJoinButton"
                    width: parent.width; height: 41; radius: 5
                    color: "#365168"; border.color: "#9bbdd1"
                    Text {
                        anchors.centerIn: parent
                        text: "JOIN ROOM"; color: "white"; font.bold: true
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                }

                Rectangle {
                    width: parent.width
                    height: 96
                    radius: 5
                    color: "#17221e"
                    border.color: "#546d60"
                    clip: true
                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 5
                        Text {
                            color: "#bbd8c8"
                            text: "AVAILABLE GAMES"
                            font.bold: true
                            font.pixelSize: 11
                        }
                        Text {
                            width: parent.width
                            color: "#f4deb3"
                            font.pixelSize: 12
                            textFormat: Text.PlainText
                            text: menu.lobby && menu.lobby.rooms.length > 0
                                  ? menu.lobby.rooms : "Refresh to look for waiting games."
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 125
                    radius: 5
                    color: "#17221e"
                    border.color: "#546d60"
                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7
                        Text {
                            width: parent.width
                            color: "#bbd8c8"
                            text: "STATUS - " + (menu.lobby ? menu.lobby.phase.toUpperCase() : "MENU")
                            font.bold: true
                            font.pixelSize: 11
                        }
                        Text {
                            width: parent.width
                            color: "#ecf3ed"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            text: menu.lobby ? menu.lobby.status : ""
                        }
                        Text {
                            width: parent.width
                            color: "#f4deb3"
                            font.pixelSize: 12
                            textFormat: Text.PlainText
                            text: menu.lobby ? menu.lobby.participants : ""
                        }
                    }
                }

                Row {
                    width: parent.width; spacing: 10
                    Rectangle {
                        id: startControl
                        objectName: "fvdLobbyStartButton"
                        width: (settings.width - 10) / 2; height: 40; radius: 5
                        color: menu.lobby && menu.lobby.canStart ? "#527e59" : "#384842"
                        border.color: "#799883"
                        Text {
                            anchors.centerIn: parent
                            text: "DRAW ROLES"; color: "white"; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                    }
                    Rectangle {
                        objectName: "fvdLobbyBackButton"
                        width: (settings.width - 10) / 2; height: 40; radius: 5
                        color: "#594b3d"; border.color: "#a69a79"
                        Text {
                            anchors.centerIn: parent
                            text: "DISCONNECT"; color: "white"; font.bold: true
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor }
                    }
                }

                Text {
                    width: parent.width
                    color: "#b7b7ae"; font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Local developer play: run game --solo\nRun relay with no token options for public games, or add one --player-token per alpha player."
                }
            }
        }
    }
}
