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
        anchors.fill: parent
        radius: 18
        color: "#1A5FB4"
        border.color: "#4FAFE8"
        border.width: 1
        antialiasing: true
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 16
        color: "transparent"
        border.width: 1
        border.color: "#2A9FE7AA"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        height: 1
        color: "#67C5F6"
        opacity: 0.45
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.topMargin: 8
        text: "机器人当前总功率"
        color: "#A6D8FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 12
        renderType: Text.NativeRendering
    }

    Text {
        id: valueText
        anchors.left: parent.left
        anchors.top: titleText.bottom
        anchors.leftMargin: 12
        anchors.topMargin: 0
        text: Math.round(root.currentPower) + " " + root.unit
        color: "#EAF7FF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 24
        font.bold: true
        renderType: Text.NativeRendering
    }

    Canvas {
        id: chart
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.top: valueText.bottom
        anchors.topMargin: 2
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            ctx.clearRect(0, 0, w, h)

            if (w <= 1 || h <= 1) {
                return
            }

            var topPad = 4
            var bottomPad = 2
            var leftPad = 0
            var rightPad = 0
            var innerW = w - leftPad - rightPad
            var innerH = h - topPad - bottomPad

            ctx.strokeStyle = "#4A95C9CC"
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
            grad.addColorStop(0.0, "#3D8FE08A")
            grad.addColorStop(1.0, "#0A2F5C14")
            ctx.fillStyle = grad
            ctx.beginPath()
            ctx.moveTo(pts[0].x, topPad + innerH)
            for (var j = 0; j < pts.length; ++j) {
                ctx.lineTo(pts[j].x, pts[j].y)
            }
            ctx.lineTo(pts[pts.length - 1].x, topPad + innerH)
            ctx.closePath()
            ctx.fill()

            ctx.strokeStyle = "#69D0FF"
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
