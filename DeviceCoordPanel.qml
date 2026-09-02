import QtQuick 2.12

Item {
    id: root
    property real coordX: 0
    property real coordY: 0
    property real coordZ: 0
    property real coordAr: 0

    implicitWidth: 560
    implicitHeight: 84

    function fmt(v) {
        if (typeof v !== "number" || !isFinite(v)) {
            return "—"
        }
        return v.toFixed(3)
    }

    // Panel shell — aligned with win7 DeviceCoordPanel
    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 14
        border.width: 1
        border.color: "#8fb4c8"
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
        color: "#5CE1FFD2"
    }

    Item {
        id: titleRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 22
        anchors.rightMargin: 14
        anchors.topMargin: 8
        height: 20

        Text {
            id: titleLabel
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "当前位姿"
            color: "#A8EAFF"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 12
            font.bold: true
            renderType: Text.NativeRendering
        }

        Rectangle {
            id: sourceBadge
            anchors.left: titleLabel.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(42, sourceText.implicitWidth + 16)
            height: 20
            radius: 6
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#5CE1FF" }
                GradientStop { position: 1.0; color: "#3BB8E8" }
            }

            Text {
                id: sourceText
                anchors.centerIn: parent
                text: "主控"
                color: "#0B2A3F"
                font.family: "Noto Sans CJK SC"
                font.pixelSize: 12
                font.bold: true
                renderType: Text.NativeRendering
            }
        }

        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "X / Y / Z / R"
            color: "#6FB8D8"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 12
            renderType: Text.NativeRendering
        }
    }

    Row {
        id: coordRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleRow.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 4
        anchors.bottomMargin: 8
        spacing: 6

        property real cellW: Math.max(0, (width - spacing * 3) / 4)

        Repeater {
            model: [
                { letter: "X", unit: "mm", accent: "#5CE1FF" },
                { letter: "Y", unit: "mm", accent: "#6FE7A8" },
                { letter: "Z", unit: "mm", accent: "#7AA8FF" },
                { letter: "R", unit: "°",   accent: "#FFC56E" }
            ]

            Item {
                width: coordRow.cellW
                height: coordRow.height

                readonly property real axisValue: {
                    switch (index) {
                    case 0: return root.coordX
                    case 1: return root.coordY
                    case 2: return root.coordZ
                    default: return root.coordAr
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    border.width: 1
                    border.color: "#5AB4DC46"
                    antialiasing: true
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#78123A5C" }
                        GradientStop { position: 1.0; color: "#5A0A243C" }
                    }
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 6

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 22
                        height: 22
                        radius: 6
                        color: Qt.rgba(
                            parseInt(modelData.accent.substr(1, 2), 16) / 255,
                            parseInt(modelData.accent.substr(3, 2), 16) / 255,
                            parseInt(modelData.accent.substr(5, 2), 16) / 255,
                            0.15)
                        border.width: 1
                        border.color: Qt.rgba(
                            parseInt(modelData.accent.substr(1, 2), 16) / 255,
                            parseInt(modelData.accent.substr(3, 2), 16) / 255,
                            parseInt(modelData.accent.substr(5, 2), 16) / 255,
                            0.63)

                        Text {
                            anchors.centerIn: parent
                            text: modelData.letter
                            color: modelData.accent
                            font.family: "Noto Sans CJK SC"
                            font.pixelSize: 12
                            font.bold: true
                            renderType: Text.NativeRendering
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 28
                        spacing: 0

                        Text {
                            width: parent.width
                            text: root.fmt(axisValue)
                            color: "#F2FBFF"
                            font.family: "Consolas"
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                            renderType: Text.NativeRendering
                        }

                        Text {
                            width: parent.width
                            text: modelData.unit
                            color: "#7EC8E8"
                            font.family: "Noto Sans CJK SC"
                            font.pixelSize: 12
                            renderType: Text.NativeRendering
                        }
                    }
                }
            }
        }
    }
}
