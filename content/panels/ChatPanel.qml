import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ShowroomPlayer 1.0
import content

Rectangle {
    id: chatPanel
    radius: Theme.radius
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    clip: true

    SplitView {
        id: chatSplit
        anchors.fill: parent
        anchors.margins: 12
        orientation: Qt.Vertical
        spacing: 0

        handle: SplitHandle {
            orientation: chatSplit.orientation
        }

        Item {
            id: chatSection
            SplitView.fillHeight: true
            SplitView.minimumHeight: 120

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Live chat")
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: ShowroomController.liveChat.count > 0 ? Theme.live : Theme.textMuted
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }

                Timer {
                    id: chatScrollTimer
                    interval: 50
                    repeat: false
                    onTriggered: {
                        if (chatList.count > 0 && chatList.autoScrollEnabled)
                            chatList.positionViewAtEnd()
                    }
                }

                ListView {
                    id: chatList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    clip: true
                    model: ShowroomController.liveChat
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    cacheBuffer: height * 2

                    property bool autoScrollEnabled: true

                    function syncAutoScrollFromPosition() {
                        autoScrollEnabled = atYEnd
                    }

                    onCountChanged: {
                        if (count === 0)
                            autoScrollEnabled = true
                    }

                    onMovementEnded: syncAutoScrollFromPosition()

                    onFlickingChanged: {
                        if (!flicking && !moving)
                            syncAutoScrollFromPosition()
                    }

                    WheelHandler {
                        blocking: false
                        onWheel: (event) => {
                            if (event.angleDelta.y > 0)
                                chatList.autoScrollEnabled = false
                            else if (event.angleDelta.y < 0)
                                Qt.callLater(chatList.syncAutoScrollFromPosition)
                        }
                    }

                    Connections {
                        target: ShowroomController.liveChat
                        function onMessagesFlushed() {
                            if (chatList.autoScrollEnabled)
                                chatScrollTimer.restart()
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: chatScrollBar
                        policy: ScrollBar.AsNeeded

                        onPressedChanged: {
                            if (!pressed)
                                chatList.syncAutoScrollFromPosition()
                        }
                    }

                    delegate: Item {
                        id: chatRow
                        width: chatList.width
                        height: accountText.visible
                              ? accountText.implicitHeight + bodyText.implicitHeight + 2
                              : bodyText.implicitHeight

                        readonly property string messageKind: model.kind
                        readonly property color accentColor: {
                            if (messageKind === "telop")
                                return Theme.accent
                            if (messageKind === "system")
                                return Theme.textMuted
                            return Theme.liveSoft
                        }

                        Text {
                            id: accountText
                            width: parent.width
                            visible: model.account.length > 0
                            text: model.account
                            color: chatRow.accentColor
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        Text {
                            id: bodyText
                            width: parent.width
                            y: accountText.visible ? accountText.implicitHeight + 2 : 0
                            text: model.text
                            color: Theme.textPrimary
                            font.pixelSize: 13
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Comments appear here while a live room is playing")
                        color: Theme.textMuted
                        font.pixelSize: 13
                        visible: chatList.count === 0
                    }
                }
            }
        }

        Item {
            id: giftSection
            SplitView.preferredHeight: Constants.giftPanelHeight
            SplitView.minimumHeight: 100

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: ShowroomController.liveGifts.eventActive
                              ? qsTr("Gifts · Event x2.5")
                              : qsTr("Gifts")
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }

                    CheckBox {
                        id: forceEventCheck
                        text: qsTr("Event")
                        font.pixelSize: 11
                        checked: ShowroomController.liveGifts.forceEventActive
                        onToggled: ShowroomController.liveGifts.forceEventActive = checked
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: ShowroomController.liveGifts.count > 0 ? "#FFB74D" : Theme.textMuted
                    }
                }

                SplitView {
                    id: giftSplit
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    orientation: Qt.Vertical
                    spacing: 0

                    handle: SplitHandle {
                        orientation: giftSplit.orientation
                    }

                    Rectangle {
                        SplitView.preferredHeight: Constants.giftRankHeight
                        SplitView.minimumHeight: 48
                        radius: 8
                        color: Theme.window
                        border.color: Theme.border
                        border.width: 1
                        clip: true

                        ListView {
                            id: giftRankList
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 4
                            clip: true
                            model: ShowroomController.liveGifts.contributors
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: RowLayout {
                                width: giftRankList.width
                                spacing: 6

                                Label {
                                    Layout.preferredWidth: 28
                                    text: "#" + model.rank
                                    color: model.rank <= 3 ? "#FFB74D" : Theme.textMuted
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: model.account
                                    color: Theme.textPrimary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: model.totalPt + " pt"
                                    color: "#FFB74D"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 12
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: qsTr("Contributor ranking")
                                color: Theme.textMuted
                                font.pixelSize: 11
                                visible: giftRankList.count === 0
                            }
                        }
                    }

                    Item {
                        SplitView.fillHeight: true
                        SplitView.minimumHeight: 48

                        Timer {
                            id: giftScrollTimer
                            interval: 50
                            repeat: false
                            onTriggered: {
                                if (giftList.count > 0 && giftList.autoScrollEnabled)
                                    giftList.positionViewAtEnd()
                            }
                        }

                        ListView {
                            id: giftList
                            anchors.fill: parent
                            spacing: 4
                            clip: true
                            model: ShowroomController.liveGifts
                            boundsBehavior: Flickable.StopAtBounds
                            reuseItems: true
                            cacheBuffer: height

                            property bool autoScrollEnabled: true

                            function syncAutoScrollFromPosition() {
                                autoScrollEnabled = atYEnd
                            }

                            onCountChanged: {
                                if (count === 0)
                                    autoScrollEnabled = true
                            }

                            onMovementEnded: syncAutoScrollFromPosition()

                            onFlickingChanged: {
                                if (!flicking && !moving)
                                    syncAutoScrollFromPosition()
                            }

                            WheelHandler {
                                blocking: false
                                onWheel: (event) => {
                                    if (event.angleDelta.y > 0)
                                        giftList.autoScrollEnabled = false
                                    else if (event.angleDelta.y < 0)
                                        Qt.callLater(giftList.syncAutoScrollFromPosition)
                                }
                            }

                            Connections {
                                target: ShowroomController.liveGifts
                                function onMessagesFlushed() {
                                    if (giftList.autoScrollEnabled)
                                        giftScrollTimer.restart()
                                }
                            }

                            ScrollBar.vertical: ScrollBar {
                                id: giftScrollBar
                                policy: ScrollBar.AsNeeded

                                onPressedChanged: {
                                    if (!pressed)
                                        giftList.syncAutoScrollFromPosition()
                                }
                            }

                            delegate: Item {
                                width: giftList.width
                                height: giftAccountText.implicitHeight + giftBodyText.implicitHeight + giftMetaText.implicitHeight + 2

                                Text {
                                    id: giftAccountText
                                    width: parent.width
                                    visible: model.account.length > 0
                                    text: model.rank > 0
                                          ? qsTr("%1  #%2").arg(model.account).arg(model.rank)
                                          : model.account
                                    color: "#FFB74D"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                Text {
                                    id: giftBodyText
                                    width: parent.width
                                    y: giftAccountText.implicitHeight + 1
                                    text: model.text
                                    color: Theme.textPrimary
                                    font.pixelSize: 12
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                Text {
                                    id: giftMetaText
                                    width: parent.width
                                    y: giftAccountText.implicitHeight + giftBodyText.implicitHeight + 2
                                    visible: model.totalPt > 0
                                    text: qsTr("Session total: %1 pt").arg(model.totalPt)
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 16
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: qsTr("Gifts appear here")
                                color: Theme.textMuted
                                font.pixelSize: 12
                                visible: giftList.count === 0
                            }
                        }
                    }
                }
            }
        }
    }
}
