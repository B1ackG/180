import QtQuick 2.15

Item {
    id: root
    width: 300
    height: 300

    property real minValue: 0
    property real maxValue: 900
    property real currentValue: 0
    property string unit: "mm/s"
    property string title: "行驶速度"

    property bool obstacleFront: false
    property bool obstacleBack: false
    property bool obstacleLeft: false
    property bool obstacleRight: false

    readonly property real safeRange: Math.max(1, maxValue - minValue)
    readonly property real clampedValue: Math.max(minValue, Math.min(maxValue, currentValue))
    readonly property real ratio: (clampedValue - minValue) / safeRange

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "#0b1324"
        border.width: 2
        border.color: "#1d3557"
    }

    Canvas {
        id: gaugeCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            var cx = width / 2
            var cy = height / 2
            var r = Math.min(width, height) * 0.42
            var start = -210 * Math.PI / 180
            var sweep = 240 * Math.PI / 180
            var end = start + sweep
            var valueEnd = start + sweep * root.ratio

            // 背景弧
            ctx.beginPath()
            ctx.lineWidth = 14
            ctx.strokeStyle = "#223554"
            ctx.arc(cx, cy, r, start, end, false)
            ctx.stroke()

            // 数值弧
            var grad = ctx.createLinearGradient(cx - r, cy, cx + r, cy)
            grad.addColorStop(0.0, "#00d4ff")
            grad.addColorStop(1.0, "#6a5cff")
            ctx.beginPath()
            ctx.lineWidth = 12
            ctx.strokeStyle = grad
            ctx.arc(cx, cy, r, start, valueEnd, false)
            ctx.stroke()

            // 障碍物环段
            var ringR = r + 16
            var ringW = 8
            function drawSector(active, a1Deg, a2Deg) {
                if (!active) return
                ctx.beginPath()
                ctx.lineWidth = ringW
                ctx.strokeStyle = "#ff3b30"
                ctx.arc(cx, cy, ringR, a1Deg * Math.PI / 180, a2Deg * Math.PI / 180, false)
                ctx.stroke()
            }
            // 前/后/左/右
            drawSector(root.obstacleFront, 225, 315)
            drawSector(root.obstacleBack, 45, 135)
            drawSector(root.obstacleLeft, 135, 225)
            drawSector(root.obstacleRight, -45, 45)
        }
    }

    Rectangle {
        width: parent.width * 0.45
        height: width
        anchors.centerIn: parent
        radius: width / 2
        color: "#15233d"
        border.width: 2
        border.color: "#2b4c7a"

        Column {
            anchors.centerIn: parent
            spacing: 2

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: Math.round(root.clampedValue)
                color: "#e8f1ff"
                font.bold: true
                font.pixelSize: parent.parent.width * 0.32
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.unit
                color: "#9fc3ff"
                font.pixelSize: parent.parent.width * 0.12
                font.bold: true
            }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.08
        text: root.title
        color: "#8fd3ff"
        font.bold: true
        font.pixelSize: parent.width * 0.08
    }

    onCurrentValueChanged: gaugeCanvas.requestPaint()
    onObstacleFrontChanged: gaugeCanvas.requestPaint()
    onObstacleBackChanged: gaugeCanvas.requestPaint()
    onObstacleLeftChanged: gaugeCanvas.requestPaint()
    onObstacleRightChanged: gaugeCanvas.requestPaint()
    Component.onCompleted: gaugeCanvas.requestPaint()
}
