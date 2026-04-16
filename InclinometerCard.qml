import QtQuick 2.12

Item {
    id: root
    property real tiltValue: 0
    property string axisLabel: "X轴倾角"

    implicitWidth: 130
    implicitHeight: 91

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: "transparent"
        border.width: 1
        border.color: "#6637B8FF"
        antialiasing: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 13
            color: "transparent"
            border.width: 1
            border.color: "#2A9FE7FF"
        }
    }

    Text {
        id: angleValue
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        text: Number(root.tiltValue).toFixed(2) + "°"
        color: "#EAF7FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 25
        font.bold: true
        renderType: Text.NativeRendering
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        text: root.axisLabel
        color: "#A6D8FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 13
        renderType: Text.NativeRendering
    }
}
