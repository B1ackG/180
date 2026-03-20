import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    visible: true
    width: 400
    height: 300
    title: "QML Test"
    color: "#0a0a20"

    Rectangle {
        anchors.centerIn: parent
        width: 200
        height: 100
        color: "cyan"
        radius: 10

        Text {
            anchors.centerIn: parent
            text: "QML IS WORKING"
            color: "black"
            font.bold: true
        }

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.3; duration: 1000 }
            NumberAnimation { from: 0.3; to: 1.0; duration: 1000 }
        }
    }
}
