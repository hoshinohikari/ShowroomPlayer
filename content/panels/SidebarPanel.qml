import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ShowroomPlayer 1.0
import content

Rectangle {
    id: root
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1

    signal loginRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: {
                        if (ShowroomAuth.loggedIn)
                            return qsTr("Logged in successfully")
                        if (ShowroomAuth.busy)
                            return qsTr("Checking session...")
                        return qsTr("Guest")
                    }
                    color: ShowroomAuth.loggedIn ? Theme.liveSoft : Theme.textMuted
                    font.pixelSize: 11
                    font.weight: ShowroomAuth.loggedIn ? Font.Medium : Font.Normal
                }

                Label {
                    Layout.fillWidth: true
                    text: ShowroomAuth.loggedIn
                          ? (ShowroomAuth.displayName.length > 0
                             ? ShowroomAuth.displayName
                             : ShowroomAuth.accountId)
                          : qsTr("Not signed in")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible: ShowroomAuth.loggedIn && ShowroomAuth.displayName.length > 0
                    text: ShowroomAuth.accountId
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            AccentButton {
                visible: !ShowroomAuth.loggedIn
                text: ShowroomAuth.busy ? qsTr("Please wait...") : qsTr("Login")
                enabled: !ShowroomAuth.busy
                onClicked: root.loginRequested()
            }

            Button {
                visible: ShowroomAuth.loggedIn
                text: qsTr("Logout")
                enabled: !ShowroomAuth.busy
                onClicked: ShowroomAuth.logout()

                contentItem: Text {
                    text: parent.text
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: parent.hovered ? Theme.surfaceHover : Theme.input
                    border.color: Theme.border
                    border.width: 1
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: qsTr("Monitor")
                color: Theme.textPrimary
                font.pixelSize: 18
                font.weight: Font.Medium
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Track Showroom users and play when live")
                color: Theme.textSecondary
                font.pixelSize: 12
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            DarkTextField {
                id: usernameField
                Layout.fillWidth: true
                placeholderText: qsTr("Username")
                onAccepted: addUserButton.clicked()
            }

            AccentButton {
                id: addUserButton
                text: qsTr("Add")
                enabled: usernameField.text.trim().length > 0
                onClicked: {
                    ShowroomController.addUser(usernameField.text)
                    usernameField.clear()
                }
            }
        }

        ListView {
            id: userList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: ShowroomController.users

            delegate: ItemDelegate {
                id: userDelegate
                width: userList.width
                height: 48
                padding: 10
                highlighted: index === ShowroomController.selectedIndex

                background: Rectangle {
                    radius: Theme.radiusSm
                    color: userDelegate.highlighted ? Theme.selected
                                      : (userDelegate.hovered ? Theme.surfaceHover : "transparent")
                    border.width: userDelegate.highlighted ? 1 : 0
                    border.color: userDelegate.highlighted ? Theme.borderFocus : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 10

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: model.isLive ? Theme.live : Theme.textMuted
                    }

                    Label {
                        Layout.fillWidth: true
                        text: model.username
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }

                    Rectangle {
                        radius: 4
                        color: model.isLive ? "#33FF5C5C" : "#1AFFFFFF"
                        border.color: model.isLive ? Theme.liveSoft : Theme.border
                        border.width: 1
                        implicitWidth: liveBadge.implicitWidth + 12
                        implicitHeight: 22

                        Label {
                            id: liveBadge
                            anchors.centerIn: parent
                            text: model.isLive ? qsTr("LIVE") : qsTr("Offline")
                            color: model.isLive ? Theme.liveSoft : Theme.textMuted
                            font.pixelSize: 10
                            font.weight: Font.Medium
                        }
                    }

                    ToolButton {
                        text: "×"
                        padding: 4
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: ShowroomController.removeUser(index)

                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 16
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: 4
                            color: parent.hovered ? Theme.surfaceHover : "transparent"
                        }
                    }
                }

                onClicked: ShowroomController.selectUser(index)
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 24
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.textMuted
                font.pixelSize: 13
                text: ShowroomAuth.loggedIn
                      ? qsTr("Live followed rooms will appear here automatically")
                      : qsTr("Add a username to start monitoring")
                visible: userList.count === 0
            }
        }
    }
}
