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
        color: "#EFFFFFFF"
        border.width: 1
        border.color: "#66FFFFFF"
        antialiasing: true
    }

    Text {
        id: angleValue
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 10
        text: Number(root.tiltValue).toFixed(2) + "°"
        color: "#111111"
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
        color: "#5F6B76"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 13
        renderType: Text.NativeRendering
    }
}
