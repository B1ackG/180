import QtQuick 2.12

Item {
    id: root
    property real weightValue: 0
    property bool dataValid: false
    property string title: "当前负载"
    property string unit: "KG"

    implicitWidth: 120
    implicitHeight: 80

    readonly property color accentColor: "#5CE1FF"

    Rectangle {
        anchors.fill: parent
        radius: 14
        border.width: 1
        border.color: "#4ABEEE84"
        antialiasing: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#E208223A" }
            GradientStop { position: 1.0; color: "#DA041222" }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 12
        color: "transparent"
        border.width: 1
        border.color: "#7AE0FF4A"
        antialiasing: true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 6
        height: 1
        color: "#6FE7FF"
        opacity: 0.55
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 12
        width: 3
        height: 12
        radius: 1.5
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.82)
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        height: 18
        text: root.title
        color: "#A8EAFF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 12
        font.bold: true
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
    }

    Rectangle {
        id: valueCell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleText.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 4
        anchors.bottomMargin: 8
        radius: 8
        border.width: 1
        border.color: "#5AB4DC46"
        antialiasing: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#78123A5C" }
            GradientStop { position: 1.0; color: "#5A0A243C" }
        }

        Row {
            anchors.centerIn: parent
            spacing: 3

            Text {
                id: valueNumber
                text: root.dataValid ? Math.round(root.weightValue) : "--"
                color: "#F2FBFF"
                font.family: "Consolas"
                font.pixelSize: 18
                font.bold: true
                renderType: Text.NativeRendering
            }

            Text {
                anchors.baseline: valueNumber.baseline
                text: root.unit
                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.86)
                font.family: "Noto Sans CJK SC"
                font.pixelSize: 12
                font.bold: true
                renderType: Text.NativeRendering
            }
        }
    }
}
