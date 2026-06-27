import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import ShowroomPlayer 1.0
import content

ApplicationWindow {
    id: window
    width: Constants.width
    height: Constants.height
    minimumWidth: Constants.minimumWidth
    minimumHeight: Constants.minimumHeight
    visible: true
    title: qsTr("ShowroomPlayer")
    color: Theme.window
    font.family: "Segoe UI"

    Component.onCompleted: {
        ShowroomAuth.ensureSessionRestore()
        ShowroomController.wireToAuth(ShowroomAuth)
    }

    LoginDialog {
        id: loginDialog
    }

    Connections {
        target: ShowroomAuth
        function onLoginFailed(message) {
            videoPanel.showError(message)
        }
        function onLoginSucceeded(accountId) {
            loginDialog.close()
            loginDialog.clearPassword()
        }
        function onSessionRestored(accountId) {
            videoPanel.showError(qsTr("Logged in as %1").arg(accountId))
        }
    }

    Connections {
        target: ShowroomController
        function onPlayStream(url) {
            videoPanel.player.loadUrl(url)
        }
        function onErrorOccurred(message) {
            videoPanel.showError(message)
        }
    }

    SplitView {
        id: mainSplit
        anchors.fill: parent
        anchors.margins: 16
        orientation: Qt.Horizontal
        spacing: 0

        handle: SplitHandle {
            orientation: mainSplit.orientation
        }

        SidebarPanel {
            id: sidebarPanel
            SplitView.preferredWidth: Constants.sidebarWidth
            SplitView.minimumWidth: 200
            SplitView.maximumWidth: window.width * 0.45

            onLoginRequested: loginDialog.open()
        }

        VideoPanel {
            id: videoPanel
            SplitView.fillWidth: true
            SplitView.minimumWidth: 360
        }

        ChatPanel {
            id: chatPanel
            SplitView.preferredWidth: Constants.chatPanelWidth
            SplitView.minimumWidth: 220
            SplitView.maximumWidth: window.width * 0.5
        }
    }
}
