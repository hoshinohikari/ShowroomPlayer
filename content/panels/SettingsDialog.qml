import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ShowroomPlayer 1.0
import content

Dialog {
    id: settingsDialog
    title: qsTr("Settings")
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent.width - 48, 420)
    padding: 16

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    header: Label {
        text: settingsDialog.title
        color: Theme.textPrimary
        font.pixelSize: 16
        font.weight: Font.Medium
        padding: 16
    }

    contentItem: ColumnLayout {
        spacing: 14

        Label {
            Layout.fillWidth: true
            text: qsTr("Proxy")
            color: Theme.textPrimary
            font.pixelSize: 14
            font.weight: Font.Medium
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Route all network requests and HLS streams through the configured proxy. Takes effect immediately on Save.")
            color: Theme.textSecondary
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Switch {
                id: enableSwitch
                checked: ShowroomProxy.enabled
                onToggled: ShowroomProxy.enabled = checked

                contentItem: Label {
                    leftPadding: enableSwitch.indicator.width + enableSwitch.spacing
                    text: qsTr("Enable proxy")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter
                }

                indicator: Rectangle {
                    implicitWidth: 42
                    implicitHeight: 24
                    x: enableSwitch.leftPadding
                    y: parent.height / 2 - height / 2
                    radius: 12
                    color: enableSwitch.checked ? Theme.accent : Theme.input
                    border.color: Theme.border
                    border.width: 1

                    Rectangle {
                        x: enableSwitch.checked ? parent.width - width - 3 : 3
                        y: parent.height / 2 - height / 2
                        width: 18
                        height: 18
                        radius: 9
                        color: "#FFFFFF"
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            enabled: enableSwitch.checked
            opacity: enabled ? 1.0 : 0.5

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Type")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                ComboBox {
                    id: typeCombo
                    Layout.fillWidth: true
                    model: [
                        { key: qsTr("HTTP"), value: 0 },
                        { key: qsTr("SOCKS5"), value: 1 }
                    ]
                    textRole: "key"
                    valueRole: "value"
                    currentIndex: {
                        for (var i = 0; i < model.length; ++i) {
                            if (model[i].value === ShowroomProxy.type)
                                return i
                        }
                        return 0
                    }
                    onActivated: ShowroomProxy.type = currentValue

                    contentItem: Label {
                        leftPadding: 10
                        text: typeCombo.displayText
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: Theme.input
                        radius: Theme.radiusSm
                        border.color: typeCombo.activeFocus ? Theme.borderFocus : Theme.border
                        border.width: 1
                    }

                    delegate: ItemDelegate {
                        width: typeCombo.width
                        height: 36
                        contentItem: Label {
                            text: modelData.key
                            color: highlighted ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: 14
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: 10
                        }
                        highlighted: typeCombo.highlightedIndex === index
                        background: Rectangle {
                            color: highlighted ? Theme.surfaceHover : Theme.surface
                        }
                    }

                    popup: Popup {
                        y: typeCombo.height
                        width: typeCombo.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 1

                        background: Rectangle {
                            color: Theme.surface
                            radius: Theme.radiusSm
                            border.color: Theme.border
                            border.width: 1
                        }

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: typeCombo.popup.visible ? typeCombo.delegateModel : null
                            currentIndex: typeCombo.highlightedIndex
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Host")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                DarkTextField {
                    id: hostField
                    Layout.fillWidth: true
                    placeholderText: qsTr("127.0.0.1")
                    text: ShowroomProxy.host
                    selectByMouse: true
                    onEditingFinished: ShowroomProxy.host = text.trim()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Port")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                DarkTextField {
                    id: portField
                    Layout.fillWidth: true
                    placeholderText: qsTr("8080")
                    text: ShowroomProxy.port > 0 ? ShowroomProxy.port.toString() : ""
                    selectByMouse: true
                    validator: IntValidator { bottom: 0; top: 65535 }
                    onEditingFinished: {
                        const value = parseInt(text.trim(), 10)
                        ShowroomProxy.port = isNaN(value) ? 0 : value
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("User")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                DarkTextField {
                    id: userField
                    Layout.fillWidth: true
                    placeholderText: qsTr("(optional)")
                    text: ShowroomProxy.username
                    selectByMouse: true
                    onEditingFinished: ShowroomProxy.username = text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Pass")
                    color: Theme.textSecondary
                    font.pixelSize: 12
                    Layout.preferredWidth: 64
                }

                DarkTextField {
                    id: passField
                    Layout.fillWidth: true
                    placeholderText: qsTr("(optional)")
                    text: ShowroomProxy.password
                    echoMode: TextInput.Password
                    selectByMouse: true
                    onEditingFinished: ShowroomProxy.password = text
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: 11
            text: ShowroomProxy.dirty
                  ? qsTr("Unsaved changes")
                  : (ShowroomProxy.enabled
                     ? qsTr("Proxy active: %1").arg(
                           ShowroomProxy.host.length > 0
                           ? (ShowroomProxy.type === 1 ? "socks5://" : "http://")
                             + ShowroomProxy.host + ":" + ShowroomProxy.port
                           : qsTr("incomplete"))
                     : qsTr("Proxy disabled"))
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            AccentButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Save")
                enabled: ShowroomProxy.dirty
                onClicked: {
                    ShowroomProxy.host = hostField.text.trim()
                    const portValue = parseInt(portField.text.trim(), 10)
                    ShowroomProxy.port = isNaN(portValue) ? 0 : portValue
                    ShowroomProxy.username = userField.text
                    ShowroomProxy.password = passField.text
                    ShowroomProxy.enabled = enableSwitch.checked
                    ShowroomProxy.type = typeCombo.currentValue
                    ShowroomProxy.save()
                }
            }

            SecondaryButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Revert")
                enabled: ShowroomProxy.dirty
                onClicked: {
                    ShowroomProxy.revert()
                    enableSwitch.checked = ShowroomProxy.enabled
                    hostField.text = ShowroomProxy.host
                    portField.text = ShowroomProxy.port > 0 ? ShowroomProxy.port.toString() : ""
                    userField.text = ShowroomProxy.username
                    passField.text = ShowroomProxy.password
                    typeCombo.currentIndex = ShowroomProxy.type === 1 ? 1 : 0
                }
            }

            SecondaryButton {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: qsTr("Close")
                onClicked: settingsDialog.close()
            }
        }
    }

    onOpened: {
        enableSwitch.checked = ShowroomProxy.enabled
        hostField.text = ShowroomProxy.host
        portField.text = ShowroomProxy.port > 0 ? ShowroomProxy.port.toString() : ""
        userField.text = ShowroomProxy.username
        passField.text = ShowroomProxy.password
        typeCombo.currentIndex = ShowroomProxy.type === 1 ? 1 : 0
    }
}
