import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ShowroomPlayer 1.0
import content

Dialog {
    id: loginDialog
    title: qsTr("Showroom Login")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 48, 360)
    padding: 16

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    header: Label {
        text: loginDialog.title
        color: Theme.textPrimary
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

    function clearPassword() {
        loginPasswordField.clear()
    }
}
