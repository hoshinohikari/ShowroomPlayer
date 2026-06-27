import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import ShowroomPlayer 1.0
import content

Rectangle {
    id: videoFrame
    color: Theme.video
    radius: Theme.radius
    clip: true
    border.color: Theme.border
    border.width: 1

    property bool controlsVisible: false
    property alias player: player

    function showControls() {
        hideControlsTimer.stop()
        controlsVisible = true
    }

    function scheduleHideControls() {
        hideControlsTimer.restart()
    }

    function showError(message) {
        errorBanner.show(message)
    }

    Timer {
        id: hideControlsTimer
        interval: 400
        onTriggered: videoFrame.controlsVisible = false
    }

    MpvVideoItem {
        id: player
        anchors.fill: parent
        anchors.margins: 1

        onPlaybackError: function(message) {
            videoFrame.showError(message)
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !player.playing
        color: Theme.video
        z: 0

        MouseArea {
            anchors.fill: parent
            enabled: ShowroomController.selectedIndex >= 0
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: ShowroomController.selectUser(ShowroomController.selectedIndex)
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 8

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: ShowroomController.selectedIndex >= 0
                      ? qsTr("Playback stopped")
                      : qsTr("No stream selected")
                color: Theme.textSecondary
                font.pixelSize: 16
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: ShowroomController.selectedIndex >= 0
                      ? qsTr("Click to resume")
                      : qsTr("Select a live user from the monitor list")
                color: Theme.textMuted
                font.pixelSize: 13
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: 1
        onContainsMouseChanged: {
            if (containsMouse)
                videoFrame.showControls()
            else if (!playPauseButton.buttonHovered && !goLiveButton.buttonHovered
                     && !exitButton.buttonHovered)
                videoFrame.scheduleHideControls()
        }
    }

    IconOverlayButton {
        id: exitButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        z: 3
        visible: player.playing
        onClicked: player.stopPlayback()
        onHoverChanged: function(hovered) {
            if (hovered)
                videoFrame.showControls()
            else
                videoFrame.scheduleHideControls()
        }

        Item {
            anchors.centerIn: parent
            width: 20
            height: 20

            Rectangle {
                width: 14
                height: 2
                radius: 1
                color: "#FFFFFF"
                anchors.centerIn: parent
                rotation: 45
            }

            Rectangle {
                width: 14
                height: 2
                radius: 1
                color: "#FFFFFF"
                anchors.centerIn: parent
                rotation: -45
            }
        }
    }

    Item {
        id: controlsOverlay
        anchors.fill: parent
        z: 2
        opacity: videoFrame.controlsVisible ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 20

            IconOverlayButton {
                id: playPauseButton
                visible: player.playing
                onClicked: player.togglePause()
                onHoverChanged: function(hovered) {
                    if (hovered)
                        videoFrame.showControls()
                    else
                        videoFrame.scheduleHideControls()
                }

                Item {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    visible: player.paused

                    Shape {
                        anchors.fill: parent
                        ShapePath {
                            fillColor: "#FFFFFF"
                            strokeWidth: 0
                            startX: 4
                            startY: 2
                            PathLine { x: 20; y: 11 }
                            PathLine { x: 4; y: 20 }
                            PathLine { x: 4; y: 2 }
                        }
                    }
                }

                Item {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    visible: !player.paused

                    Rectangle {
                        width: 6
                        height: 18
                        radius: 1
                        color: "#FFFFFF"
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 6
                        height: 18
                        radius: 1
                        color: "#FFFFFF"
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            IconOverlayButton {
                id: goLiveButton
                visible: player.playing && !player.atLiveEdge
                onClicked: player.catchUpToLive()
                onHoverChanged: function(hovered) {
                    if (hovered)
                        videoFrame.showControls()
                    else
                        videoFrame.scheduleHideControls()
                }

                Item {
                    anchors.centerIn: parent
                    width: 22
                    height: 22

                    Rectangle {
                        width: 4
                        height: 16
                        radius: 1
                        color: "#FFFFFF"
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Shape {
                        width: 14
                        height: 16
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter

                        ShapePath {
                            fillColor: "#FFFFFF"
                            strokeWidth: 0
                            startX: 0
                            startY: 0
                            PathLine { x: 14; y: 8 }
                            PathLine { x: 0; y: 16 }
                            PathLine { x: 0; y: 0 }
                        }
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.live
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: -2
                        anchors.topMargin: -2
                        border.color: "#FFFFFF"
                        border.width: 1
                    }
                }
            }
        }
    }

    Rectangle {
        id: errorBanner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        height: 40
        radius: Theme.radiusSm
        color: "#E6C62828"
        border.color: Theme.danger
        border.width: 1
        opacity: 0
        visible: opacity > 0
        z: 3

        function show(message) {
            bannerText.text = message
            hideTimer.stop()
            hideAnim.stop()
            showAnim.start()
            hideTimer.start()
        }

        NumberAnimation {
            id: showAnim
            target: errorBanner
            property: "opacity"
            to: 1
            duration: 200
        }

        Timer {
            id: hideTimer
            interval: 4000
            onTriggered: hideAnim.start()
        }

        NumberAnimation {
            id: hideAnim
            target: errorBanner
            property: "opacity"
            to: 0
            duration: 400
        }

        Label {
            id: bannerText
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            color: "#FFFFFF"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideMiddle
        }
    }
}
