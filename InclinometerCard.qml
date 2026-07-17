import QtQuick 2.12

Item {
    id: root
    property real tiltValue: 0
    property string axisLabel: "X轴倾角"
    property string thresholdText: ""
    property real tiltVisualRange: 15.0

    readonly property color accentColor: {
        if (axisLabel.indexOf("Y") === 0 || axisLabel.indexOf("Y轴") >= 0)
            return "#6FE7A8"
        return "#5CE1FF"
    }

    readonly property string axisLetter: {
        if (axisLabel.indexOf("Y") === 0 || axisLabel.indexOf("Y轴") >= 0)
            return "Y"
        if (axisLabel.indexOf("X") === 0 || axisLabel.indexOf("X轴") >= 0)
            return "X"
        return axisLabel.length > 0 ? axisLabel.charAt(0).toUpperCase() : "?"
    }

    implicitWidth: 140
    implicitHeight: 100

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
        anchors.topMargin: 8
        height: 1
        color: "#6FE7FF"
        opacity: 0.55
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.topMargin: 14
        width: 3
        height: 12
        radius: 1.5
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.82)
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 20
        anchors.topMargin: 10
        anchors.right: badge.left
        anchors.rightMargin: 4
        height: 20
        text: root.axisLabel
        color: "#A8EAFF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 11
        font.bold: true
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
    }

    Rectangle {
        id: badge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 12
        anchors.topMargin: 12
        width: 18
        height: 18
        radius: 5
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.16)
        border.width: 1
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.67)

        Text {
            anchors.centerIn: parent
            text: root.axisLetter
            color: root.accentColor
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 10
            font.bold: true
            renderType: Text.NativeRendering
        }
    }

    Rectangle {
        id: valueCell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 34
        height: Math.max(28, parent.height - 58 - (root.thresholdText.length > 0 ? 12 : 0))
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
            spacing: 2

            Text {
                id: valueNumber
                text: Number(root.tiltValue).toFixed(2)
                color: "#F2FBFF"
                font.family: "Noto Sans CJK SC"
                font.pixelSize: 22
                font.bold: true
                renderType: Text.NativeRendering
            }

            Text {
                anchors.baseline: valueNumber.baseline
                text: "°"
                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.86)
                font.family: "Noto Sans CJK SC"
                font.pixelSize: 12
                font.bold: true
                renderType: Text.NativeRendering
            }
        }
    }

    Text {
        id: thresholdLabel
        visible: root.thresholdText.length > 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: valueCell.bottom
        anchors.topMargin: 1
        text: root.thresholdText
        color: "#6FB8D8"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 9
        renderType: Text.NativeRendering
    }

    // Bubble level ±tiltVisualRange
    Item {
        id: levelBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        height: 8

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: "#A0081C30"
            border.width: 1
            border.color: "#5AB4DC50"
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            width: 1
            color: "#6FE7FF"
            opacity: 0.55
        }

        Rectangle {
            id: bubble
            width: 6.4
            height: 6.4
            radius: 3.2
            color: root.accentColor
            anchors.verticalCenter: parent.verticalCenter
            x: {
                var ratio = Math.max(-1.0, Math.min(1.0, root.tiltValue / root.tiltVisualRange))
                var travel = (levelBar.width * 0.5) - (width * 0.5) - 2.0
                return levelBar.width * 0.5 + ratio * travel - width * 0.5
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 1.5
                height: parent.height * 1.5
                radius: width * 0.5
                z: -1
                color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.25)
            }
        }
    }
}
