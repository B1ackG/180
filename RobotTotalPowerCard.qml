import QtQuick 2.12

Item {
    id: root
    property real currentPower: 0
    property string title: "Power"
    property string unit: "W"
    property int maxSamples: 50
    property var samples: []
    property real maxDisplayPower: 1000

    implicitWidth: 240
    implicitHeight: 90

    function appendSample(v) {
        var next = Math.max(0, Number(v))
        var arr = samples.slice(0)
        arr.push(next)
        if (arr.length > maxSamples) {
            arr.shift()
        }
        samples = arr

        var localMax = 100
        for (var i = 0; i < arr.length; ++i) {
            if (arr[i] > localMax) {
                localMax = arr[i]
            }
        }
        maxDisplayPower = Math.max(100, localMax * 1.2)
        chart.requestPaint()
    }

    onCurrentPowerChanged: appendSample(currentPower)
    Component.onCompleted: appendSample(currentPower)

    Rectangle {
        id: card
        anchors.fill: parent
        radius: 18
        color: "#2A0E2F4D"
        border.color: "#6637B8FF"
        border.width: 1
        antialiasing: true

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: 17
            color: "transparent"
            border.width: 1
            border.color: "#2A9FE7FF"
        }
    }

    Text {
        id: valueText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 8
        text: Math.round(root.currentPower) + " " + root.unit
        color: "#EAF7FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: Math.min(Math.max(22, root.width * 0.14), 34)
        font.bold: true
        renderType: Text.NativeRendering
    }

    Canvas {
        id: chart
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        anchors.top: parent.verticalCenter
        anchors.topMargin: 2
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)

            if (w <= 1 || h <= 1) {
                return
            }

            var topPad = 6
            var bottomPad = 6
            var leftPad = 0
            var rightPad = 0
            var innerW = w - leftPad - rightPad
            var innerH = h - topPad - bottomPad

            ctx.strokeStyle = "#5A9FC4E8"
            ctx.lineWidth = 1
            ctx.setLineDash([4, 4])
            for (var g = 0; g < 4; ++g) {
                var gy = topPad + innerH * g / 3
                ctx.beginPath()
                ctx.moveTo(leftPad, gy)
                ctx.lineTo(leftPad + innerW, gy)
                ctx.stroke()
            }
            ctx.setLineDash([])

            if (!root.samples || root.samples.length < 1) {
                return
            }

            var pts = []
            var len = root.samples.length
            var span = Math.max(1, len - 1)
            for (var i = 0; i < len; ++i) {
                var x = leftPad + innerW * i / span
                var yNorm = Math.min(1, Math.max(0, root.samples[i] / root.maxDisplayPower))
                var y = topPad + innerH * (1 - yNorm)
                pts.push({x: x, y: y})
            }

            if (pts.length === 1) {
                pts.push({x: leftPad + innerW, y: pts[0].y})
            }

            var grad = ctx.createLinearGradient(0, topPad, 0, topPad + innerH)
            grad.addColorStop(0.0, "#6A47B3FF")
            grad.addColorStop(1.0, "#0847B3FF")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.moveTo(pts[0].x, topPad + innerH)
            for (var j = 0; j < pts.length; ++j) {
                ctx.lineTo(pts[j].x, pts[j].y)
            }
            ctx.lineTo(pts[pts.length - 1].x, topPad + innerH)
            ctx.closePath()
            ctx.fill()

            ctx.strokeStyle = "#53BEFF"
            ctx.lineWidth = 2
            ctx.beginPath()
            ctx.moveTo(pts[0].x, pts[0].y)
            for (var k = 1; k < pts.length; ++k) {
                ctx.lineTo(pts[k].x, pts[k].y)
            }
            ctx.stroke()
        }
    }
}
