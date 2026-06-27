import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import ShowroomPlayer 1.0

ApplicationWindow {
    id: window
    width: Constants.width
    height: Constants.height
    minimumWidth: Constants.minimumWidth
    minimumHeight: Constants.minimumHeight
    visible: true
    title: qsTr("ShowroomPlayer")
    color: theme.window
    font.family: "Segoe UI"

    readonly property QtObject theme: QtObject {
        readonly property color window: "#0E1014"
        readonly property color surface: "#171A21"
        readonly property color surfaceHover: "#1F2430"
        readonly property color input: "#1F2430"
        readonly property color border: "#2E3544"
        readonly property color borderFocus: "#4C8DFF"
        readonly property color textPrimary: "#E8ECF4"
        readonly property color textSecondary: "#9AA3B5"
        readonly property color textMuted: "#5F6778"
        readonly property color accent: "#4C8DFF"
        readonly property color accentHover: "#6AA0FF"
        readonly property color accentPressed: "#3A74DB"
        readonly property color live: "#FF5C5C"
        readonly property color liveSoft: "#FF8A80"
        readonly property color selected: "#243148"
        readonly property color video: "#000000"
        readonly property color danger: "#C62828"
        readonly property int radius: 8
        readonly property int radiusSm: 6
    }

    Component.onCompleted: {
        ShowroomAuth.ensureSessionRestore()
        ShowroomController.wireToAuth(ShowroomAuth)
    }

    Connections {
        target: ShowroomAuth
        function onLoginFailed(message) {
            errorBanner.show(message)
        }
        function onLoginSucceeded(accountId) {
            loginDialog.close()
            loginPasswordField.clear()
        }
        function onSessionRestored(accountId) {
            errorBanner.show(qsTr("Logged in as %1").arg(accountId))
        }
    }

    Connections {
        target: ShowroomController
        function onPlayStream(url) {
            player.loadUrl(url)
        }
        function onErrorOccurred(message) {
            errorBanner.show(message)
        }
    }

    Dialog {
        id: loginDialog
        title: qsTr("Showroom Login")
        modal: true
        anchors.centerIn: parent
        width: Math.min(window.width - 48, 360)
        padding: 16

        background: Rectangle {
            color: theme.surface
            radius: theme.radius
            border.color: theme.border
            border.width: 1
        }

        header: Label {
            text: loginDialog.title
            color: theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.Medium
            padding: 16
        }

        contentItem: ColumnLayout {
            spacing: 12

            DarkTextField {
                id: loginAccountField
                Layout.fillWidth: true
                placeholderText: qsTr("Account ID")
                selectByMouse: true
                onAccepted: loginPasswordField.forceActiveFocus()
            }

            DarkTextField {
                id: loginPasswordField
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                selectByMouse: true
                onAccepted: loginSubmitButton.clicked()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                AccentButton {
                    id: loginSubmitButton
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: ShowroomAuth.busy ? qsTr("Logging in...") : qsTr("Login")
                    enabled: !ShowroomAuth.busy
                             && loginAccountField.text.trim().length > 0
                             && loginPasswordField.text.length > 0
                    onClicked: ShowroomAuth.login(loginAccountField.text, loginPasswordField.text)
                }

                SecondaryButton {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: qsTr("Cancel")
                    enabled: !ShowroomAuth.busy
                    onClicked: loginDialog.close()
                }
            }
        }
    }

    component DarkTextField: TextField {
        color: theme.textPrimary
        placeholderTextColor: theme.textMuted
        selectedTextColor: theme.textPrimary
        selectionColor: theme.accent
        padding: 10
        font.pixelSize: 14

        background: Rectangle {
            color: parent.enabled ? theme.input : theme.surface
            radius: theme.radiusSm
            border.width: 1
            border.color: parent.activeFocus ? theme.borderFocus : theme.border
        }
    }

    component AccentButton: Button {
        padding: 10
        font.pixelSize: 14
        font.weight: Font.Medium

        contentItem: Text {
            text: parent.text
            font: parent.font
            color: parent.enabled ? "#FFFFFF" : theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: theme.radiusSm
            color: !parent.enabled ? theme.surface
                   : parent.pressed ? theme.accentPressed
                   : parent.hovered ? theme.accentHover
                   : theme.accent
        }
    }

    component SecondaryButton: Button {
        padding: 10
        font.pixelSize: 14
        font.weight: Font.Medium

        contentItem: Text {
            text: parent.text
            font: parent.font
            color: parent.enabled ? theme.textSecondary : theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: theme.radiusSm
            color: !parent.enabled ? theme.surface
                   : parent.pressed ? theme.surfaceHover
                   : parent.hovered ? theme.surfaceHover
                   : theme.input
            border.color: theme.border
            border.width: 1
        }
    }

    component IconOverlayButton: Item {
        id: root
        width: 52
        height: 52
        property bool buttonHovered: false
        signal clicked()
        signal hoverChanged(bool hovered)

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: root.buttonHovered ? "#99000000" : "#66000000"
            border.color: root.buttonHovered ? "#AAFFFFFF" : "#66FFFFFF"
            border.width: 1
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
            onContainsMouseChanged: {
                root.buttonHovered = containsMouse
                root.hoverChanged(containsMouse)
            }
        }

        default property alias content: iconHost.data

        Item {
            id: iconHost
            anchors.centerIn: parent
            width: 24
            height: 24
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Rectangle {
            Layout.preferredWidth: Constants.sidebarWidth
            Layout.fillHeight: true
            radius: theme.radius
            color: theme.surface
            border.color: theme.border
            border.width: 1

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
                            color: ShowroomAuth.loggedIn ? theme.liveSoft : theme.textMuted
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
                            color: theme.textPrimary
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: ShowroomAuth.loggedIn && ShowroomAuth.displayName.length > 0
                            text: ShowroomAuth.accountId
                            color: theme.textSecondary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }

                    AccentButton {
                        visible: !ShowroomAuth.loggedIn
                        text: ShowroomAuth.busy ? qsTr("Please wait...") : qsTr("Login")
                        enabled: !ShowroomAuth.busy
                        onClicked: loginDialog.open()
                    }

                    Button {
                        visible: ShowroomAuth.loggedIn
                        text: qsTr("Logout")
                        enabled: !ShowroomAuth.busy
                        onClicked: ShowroomAuth.logout()

                        contentItem: Text {
                            text: parent.text
                            color: theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        background: Rectangle {
                            radius: theme.radiusSm
                            color: parent.hovered ? theme.surfaceHover : theme.input
                            border.color: theme.border
                            border.width: 1
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: qsTr("Monitor")
                        color: theme.textPrimary
                        font.pixelSize: 18
                        font.weight: Font.Medium
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("Track Showroom users and play when live")
                        color: theme.textSecondary
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
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
                            radius: theme.radiusSm
                            color: userDelegate.highlighted ? theme.selected
                                              : (userDelegate.hovered ? theme.surfaceHover : "transparent")
                            border.width: userDelegate.highlighted ? 1 : 0
                            border.color: userDelegate.highlighted ? theme.borderFocus : "transparent"
                        }

                        contentItem: RowLayout {
                            spacing: 10

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: model.isLive ? theme.live : theme.textMuted
                            }

                            Label {
                                Layout.fillWidth: true
                                text: model.username
                                color: theme.textPrimary
                                font.pixelSize: 14
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            Rectangle {
                                radius: 4
                                color: model.isLive ? "#33FF5C5C" : "#1AFFFFFF"
                                border.color: model.isLive ? theme.liveSoft : theme.border
                                border.width: 1
                                implicitWidth: liveBadge.implicitWidth + 12
                                implicitHeight: 22

                                Label {
                                    id: liveBadge
                                    anchors.centerIn: parent
                                    text: model.isLive ? qsTr("LIVE") : qsTr("Offline")
                                    color: model.isLive ? theme.liveSoft : theme.textMuted
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
                                    color: parent.hovered ? theme.textPrimary : theme.textSecondary
                                    font.pixelSize: 16
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }

                                background: Rectangle {
                                    radius: 4
                                    color: parent.hovered ? theme.surfaceHover : "transparent"
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
                        color: theme.textMuted
                        font.pixelSize: 13
                        text: ShowroomAuth.loggedIn
                              ? qsTr("Live followed rooms will appear here automatically")
                              : qsTr("Add a username to start monitoring")
                        visible: userList.count === 0
                    }
                }
            }
        }

        Rectangle {
            id: videoFrame
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theme.video
            radius: theme.radius
            clip: true
            border.color: theme.border
            border.width: 1

            property bool controlsVisible: false

            function showControls() {
                hideControlsTimer.stop()
                controlsVisible = true
            }

            function scheduleHideControls() {
                hideControlsTimer.restart()
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
                    errorBanner.show(message)
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: !player.playing
                color: theme.video
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
                        color: theme.textSecondary
                        font.pixelSize: 16
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: ShowroomController.selectedIndex >= 0
                              ? qsTr("Click to resume")
                              : qsTr("Select a live user from the monitor list")
                        color: theme.textMuted
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
                                color: theme.live
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
                radius: theme.radiusSm
                color: "#E6C62828"
                border.color: theme.danger
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

        Rectangle {
            id: chatPanel
            Layout.preferredWidth: Constants.chatPanelWidth
            Layout.fillHeight: true
            radius: theme.radius
            color: theme.surface
            border.color: theme.border
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Live chat")
                        color: theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.Medium
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: ShowroomController.liveChat.count > 0 ? theme.live : theme.textMuted
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
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

                    // true while the view is pinned to the bottom; disabled when user scrolls up
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
                                return theme.accent
                            if (messageKind === "system")
                                return theme.textMuted
                            return theme.liveSoft
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
                            color: theme.textPrimary
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
                        color: theme.textMuted
                        font.pixelSize: 13
                        visible: chatList.count === 0
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Gifts")
                        color: theme.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.Medium
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: ShowroomController.liveGifts.count > 0 ? "#FFB74D" : theme.textMuted
                    }
                }

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
                    Layout.fillWidth: true
                    Layout.preferredHeight: Constants.giftPanelHeight
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
                        height: giftAccountText.implicitHeight + giftBodyText.implicitHeight + 1

                        Text {
                            id: giftAccountText
                            width: parent.width
                            visible: model.account.length > 0
                            text: model.account
                            color: "#FFB74D"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            maximumLineCount: 1
                            elide: Text.ElideRight
                        }

                        Text {
                            id: giftBodyText
                            width: parent.width
                            y: giftAccountText.visible ? giftAccountText.implicitHeight + 1 : 0
                            text: model.text
                            color: theme.textPrimary
                            font.pixelSize: 12
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
                        color: theme.textMuted
                        font.pixelSize: 12
                        visible: giftList.count === 0
                    }
                }
            }
        }
    }
}
