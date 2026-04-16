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
        color: "#1A5FB4"
        border.width: 1
        border.color: "#4FAFE8"
        antialiasing: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 12
        color: "transparent"
        border.width: 1
        border.color: "#2A9FE7AA"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        height: 1
        color: "#55B9EE"
        opacity: 0.5
    }

    Text {
        id: angleValue
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 12
        text: Number(root.tiltValue).toFixed(2) + "°"
        color: "#EAF7FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 28
        font.bold: true
        renderType: Text.NativeRendering
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        text: root.axisLabel
        color: "#A8DAFF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 14
        font.bold: true
        renderType: Text.NativeRendering
    }
}
