import QtQuick
import QtQuick.Controls.Basic
import content

TextField {
    color: Theme.textPrimary
    placeholderTextColor: Theme.textMuted
    selectedTextColor: Theme.textPrimary
    selectionColor: Theme.accent
    padding: 10
    font.pixelSize: 14

    background: Rectangle {
        color: parent.enabled ? Theme.input : Theme.surface
        radius: Theme.radiusSm
        border.width: 1
        border.color: parent.activeFocus ? Theme.borderFocus : Theme.border
    }
}
