import QtQuick

Item {
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
