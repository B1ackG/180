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

    // 触边状态（内环）
    property bool touchFront: false
    property bool touchBack: false
    property bool touchLeft: false
    property bool touchRight: false

    // 避障状态（外环）：0=无, 1=减速(黄), 2=停止(红)
    property int avoidFrontState: 0
    property int avoidBackState: 0
    property int avoidLeftState: 0
    property int avoidRightState: 0

    property string statusText: "正常"

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

            // 外环：避障状态（减速黄、停止红）
            var avoidRingR = r + 18
            var avoidRingW = 8
            function drawAvoidSector(state, a1Deg, a2Deg) {
                if (state <= 0) return
                ctx.beginPath()
                ctx.lineWidth = avoidRingW
                ctx.strokeStyle = state === 2 ? "#ff3b30" : "#ffd60a"
                ctx.arc(cx, cy, avoidRingR, a1Deg * Math.PI / 180, a2Deg * Math.PI / 180, false)
                ctx.stroke()
            }

            // 内环：触边状态（红）
            var touchRingR = r + 8
            var touchRingW = 6
            function drawTouchSector(active, a1Deg, a2Deg) {
                if (!active) return
                ctx.beginPath()
                ctx.lineWidth = touchRingW
                ctx.strokeStyle = "#ff3b30"
                ctx.arc(cx, cy, touchRingR, a1Deg * Math.PI / 180, a2Deg * Math.PI / 180, false)
                ctx.stroke()
            }

            // 前/后/左/右
            drawAvoidSector(root.avoidFrontState, 225, 315)
            drawAvoidSector(root.avoidBackState, 45, 135)
            drawAvoidSector(root.avoidLeftState, 135, 225)
            drawAvoidSector(root.avoidRightState, -45, 45)

            drawTouchSector(root.touchFront, 225, 315)
            drawTouchSector(root.touchBack, 45, 135)
            drawTouchSector(root.touchLeft, 135, 225)
            drawTouchSector(root.touchRight, -45, 45)
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
            spacing: 1

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.statusText
                color: root.statusText === "正常" ? "#9fc3ff" : "#ffb4b4"
                font.bold: true
                font.pixelSize: parent.parent.width * 0.14
            }

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
    onTouchFrontChanged: gaugeCanvas.requestPaint()
    onTouchBackChanged: gaugeCanvas.requestPaint()
    onTouchLeftChanged: gaugeCanvas.requestPaint()
    onTouchRightChanged: gaugeCanvas.requestPaint()
    onAvoidFrontStateChanged: gaugeCanvas.requestPaint()
    onAvoidBackStateChanged: gaugeCanvas.requestPaint()
    onAvoidLeftStateChanged: gaugeCanvas.requestPaint()
    onAvoidRightStateChanged: gaugeCanvas.requestPaint()
    Component.onCompleted: gaugeCanvas.requestPaint()
}
