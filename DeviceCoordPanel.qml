import QtQuick 2.12

Item {
    id: root
    property real coordX: 0
    property real coordY: 0
    property real coordZ: 0
    property real coordAr: 0

    implicitWidth: 460
    implicitHeight: 70

    function fmt(v) {
        if (typeof v !== "number" || !isFinite(v)) {
            return "—"
        }
        return v.toFixed(3)
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: "#1A5FB4"
        border.color: "#4FAFE8"
        border.width: 1
        antialiasing: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 8
        color: "transparent"
        border.width: 1
        border.color: "#2A9FE7AA"
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 8
        anchors.topMargin: 4
        text: "当前位姿 (主控)"
        color: "#A6D8FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 12
        font.bold: true
        renderType: Text.NativeRendering
    }

    Row {
        id: coordRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleText.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.bottomMargin: 4
        anchors.topMargin: 1
        spacing: 4

        property real cellW: Math.max(0, (width - spacing * 3) / 4)

        Item {
            width: coordRow.cellW
            height: coordRow.height
            Column {
                anchors.centerIn: parent
                width: parent.width
                spacing: 1
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "X"
                    color: "#A8DAFF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmt(root.coordX)
                    color: "#EAF7FF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 17
                    font.bold: true
                    renderType: Text.NativeRendering
                    elide: Text.ElideRight
                }
            }
        }
        Item {
            width: coordRow.cellW
            height: coordRow.height
            Column {
                anchors.centerIn: parent
                width: parent.width
                spacing: 1
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "Y"
                    color: "#A8DAFF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmt(root.coordY)
                    color: "#EAF7FF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 17
                    font.bold: true
                    renderType: Text.NativeRendering
                    elide: Text.ElideRight
                }
            }
        }
        Item {
            width: coordRow.cellW
            height: coordRow.height
            Column {
                anchors.centerIn: parent
                width: parent.width
                spacing: 1
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "Z"
                    color: "#A8DAFF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmt(root.coordZ)
                    color: "#EAF7FF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 17
                    font.bold: true
                    renderType: Text.NativeRendering
                    elide: Text.ElideRight
                }
            }
        }
        Item {
            width: coordRow.cellW
            height: coordRow.height
            Column {
                anchors.centerIn: parent
                width: parent.width
                spacing: 1
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: "R"
                    color: "#A8DAFF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Text.NativeRendering
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: fmt(root.coordAr)
                    color: "#EAF7FF"
                    font.family: "Noto Sans CJK SC"
                    font.pixelSize: 17
                    font.bold: true
                    renderType: Text.NativeRendering
                    elide: Text.ElideRight
                }
            }
        }
    }
}
