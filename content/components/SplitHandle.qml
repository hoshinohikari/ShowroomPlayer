import QtQuick
import QtQuick.Controls
import content

Rectangle {
    implicitWidth: orientation === Qt.Horizontal ? 6 : (parent ? parent.width : 6)
    implicitHeight: orientation === Qt.Vertical ? 6 : (parent ? parent.height : 6)
    color: SplitHandle.pressed ? Theme.accent
           : SplitHandle.hovered ? Theme.borderFocus
           : "transparent"

    property int orientation: Qt.Horizontal

    Rectangle {
        anchors.centerIn: parent
        width: orientation === Qt.Horizontal ? 2 : parent.width - 8
        height: orientation === Qt.Vertical ? 2 : parent.height - 8
        radius: 1
        color: SplitHandle.pressed ? Theme.accent
               : SplitHandle.hovered ? Theme.borderFocus
               : Theme.border
        opacity: SplitHandle.hovered || SplitHandle.pressed ? 1 : 0.6
    }
}
