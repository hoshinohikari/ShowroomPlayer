import QtQuick
import QtQuick.Controls.Basic
import content

Button {
    padding: 10
    font.pixelSize: 14
    font.weight: Font.Medium

    contentItem: Text {
        text: parent.text
        font: parent.font
        color: parent.enabled ? "#FFFFFF" : Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: !parent.enabled ? Theme.surface
               : parent.pressed ? Theme.accentPressed
               : parent.hovered ? Theme.accentHover
               : Theme.accent
    }
}
