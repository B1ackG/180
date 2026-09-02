import QtQuick 2.12

Item {
    id: root
    property real currentPower: 0
    property string title: "总功率"
    property string unit: "W"
    property bool showCardBackground: true
    property int maxSamples: 50
    property var samples: []
    property real maxDisplayPower: 1000
    property real peakPower: 0

    implicitWidth: 280
    implicitHeight: 100

    function appendSample(v) {
        var next = Math.max(0, Number(v))
        var arr = samples
        if (!arr) {
            arr = []
        }
        arr.push(next)
        if (arr.length > maxSamples) {
            arr.shift()
        }
        if (samples !== arr) {
            samples = arr
        }

        var localMax = 100
        var peak = 0
        for (var i = 0; i < arr.length; ++i) {
            if (arr[i] > localMax) {
                localMax = arr[i]
            }
            if (arr[i] > peak) {
                peak = arr[i]
            }
        }
        maxDisplayPower = Math.max(100, localMax * 1.2)
        peakPower = peak
        chart.requestPaint()
    }

    onCurrentPowerChanged: appendSample(currentPower)
    Component.onCompleted: appendSample(currentPower)

    Rectangle {
        visible: root.showCardBackground
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
        visible: root.showCardBackground
        anchors.fill: parent
        anchors.margins: 2
        radius: 12
        color: "transparent"
        border.width: 1
        border.color: "#7AE0FF4A"
        antialiasing: true
    }

    Rectangle {
        visible: root.showCardBackground
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
        visible: root.showCardBackground
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 12
        anchors.topMargin: 14
        width: 3
        height: 12
        radius: 1.5
        color: "#5CE1FFD2"
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: root.showCardBackground ? 20 : 12
        anchors.topMargin: 10
        text: root.title
        color: "#A8EAFF"
        font.family: "Noto Sans CJK SC"
        font.pixelSize: 12
        font.bold: true
        renderType: Text.NativeRendering
    }

    Rectangle {
        id: liveBadge
        visible: root.showCardBackground
        anchors.left: titleText.right
        anchors.verticalCenter: titleText.verticalCenter
        anchors.leftMargin: 8
        width: 36
        height: 16
        radius: 5
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#5CE1FF" }
            GradientStop { position: 1.0; color: "#3BB8E8" }
        }

        Text {
            anchors.centerIn: parent
            text: "实时"
            color: "#0B2A3F"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 12
            font.bold: true
            renderType: Text.NativeRendering
        }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 14
        anchors.topMargin: 10
        spacing: 4

        Text {
            id: powerNumber
            text: Math.round(root.currentPower)
            color: "#F2FBFF"
            font.family: "Consolas"
            font.pixelSize: 18
            font.bold: true
            renderType: Text.NativeRendering
        }

        Text {
            anchors.baseline: powerNumber.baseline
            text: root.unit
            color: "#7EC8E8"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 12
            font.bold: true
            renderType: Text.NativeRendering
        }
    }

    Rectangle {
        id: chartCell
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.top: titleText.bottom
        anchors.leftMargin: root.showCardBackground ? 10 : 12
        anchors.rightMargin: root.showCardBackground ? 10 : 12
        anchors.bottomMargin: 10
        anchors.topMargin: 8
        radius: 8
        border.width: root.showCardBackground ? 1 : 0
        border.color: "#5AB4DC46"
        color: root.showCardBackground ? "#78123A5C" : "transparent"
        antialiasing: true

        Text {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 8
            anchors.topMargin: 2
            text: "峰值 " + Math.round(root.peakPower)
            color: "#6FB8D8"
            font.family: "Noto Sans CJK SC"
            font.pixelSize: 12
            renderType: Text.NativeRendering
        }

        Canvas {
            id: chart
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 16
            anchors.bottomMargin: 6
            antialiasing: true
            renderTarget: Canvas.Image

            onPaint: {
                var ctx = getContext("2d")
                var w = width
                var h = height
                ctx.clearRect(0, 0, w, h)

                if (w <= 1 || h <= 1) {
                    return
                }

                ctx.strokeStyle = "#5A9ABE5A"
                ctx.lineWidth = 1
                ctx.setLineDash([4, 4])
                for (var g = 0; g < 3; ++g) {
                    var gy = h * g / 2
                    ctx.beginPath()
                    ctx.moveTo(0, gy)
                    ctx.lineTo(w, gy)
                    ctx.stroke()
                }
                ctx.setLineDash([])

                if (!root.samples || root.samples.length < 1) {
                    ctx.fillStyle = "#6FB8D8"
                    ctx.font = "11px 'Noto Sans CJK SC'"
                    ctx.textAlign = "center"
                    ctx.textBaseline = "middle"
                    ctx.fillText("等待功率数据…", w / 2, h / 2)
                    return
                }

                var pts = []
                var len = root.samples.length
                var span = Math.max(1, len - 1)
                for (var i = 0; i < len; ++i) {
                    var x = w * i / span
                    var yNorm = Math.min(1, Math.max(0, root.samples[i] / root.maxDisplayPower))
                    var y = h * (1 - yNorm)
                    pts.push({x: x, y: y})
                }

                if (pts.length === 1) {
                    pts.push({x: w, y: pts[0].y})
                }

                var grad = ctx.createLinearGradient(0, 0, 0, h)
                grad.addColorStop(0.0, "#00B0E86E")
                grad.addColorStop(1.0, "#005C8C0C")
                ctx.fillStyle = grad
                ctx.beginPath()
                ctx.moveTo(pts[0].x, h)
                for (var j = 0; j < pts.length; ++j) {
                    ctx.lineTo(pts[j].x, pts[j].y)
                }
                ctx.lineTo(pts[pts.length - 1].x, h)
                ctx.closePath()
                ctx.fill()

                // Glow underlay
                ctx.strokeStyle = "#6FE7FF46"
                ctx.lineWidth = 4
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(pts[0].x, pts[0].y)
                for (var k = 1; k < pts.length; ++k) {
                    ctx.lineTo(pts[k].x, pts[k].y)
                }
                ctx.stroke()

                ctx.strokeStyle = "#6FE7FF"
                ctx.lineWidth = 2
                ctx.beginPath()
                ctx.moveTo(pts[0].x, pts[0].y)
                for (var m = 1; m < pts.length; ++m) {
                    ctx.lineTo(pts[m].x, pts[m].y)
                }
                ctx.stroke()

                // Latest-point glow
                var tip = pts[pts.length - 1]
                var tipGrad = ctx.createRadialGradient(tip.x, tip.y, 0, tip.x, tip.y, 8)
                tipGrad.addColorStop(0.0, "#6FE7FFC8")
                tipGrad.addColorStop(1.0, "#6FE7FF00")
                ctx.fillStyle = tipGrad
                ctx.beginPath()
                ctx.arc(tip.x, tip.y, 7, 0, Math.PI * 2)
                ctx.fill()

                ctx.fillStyle = "#F2FBFF"
                ctx.beginPath()
                ctx.arc(tip.x, tip.y, 2.6, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }
}
