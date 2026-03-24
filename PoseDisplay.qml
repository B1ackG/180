import QtQuick 2.15

Rectangle {
    id: root
    width: 300
    height: 200
    color: "#0a0a1a" // 更深邃的背景
    radius: 8
    border.color: "#303060"
    border.width: 1
    clip: true

    // 数据输入 (6-DoF)
    property real xPos: typeof poseData !== 'undefined' ? poseData.x : 0
    property real yPos: typeof poseData !== 'undefined' ? poseData.y : 0
    property real zPos: typeof poseData !== 'undefined' ? poseData.z : 0
    property real roll: typeof poseData !== 'undefined' ? (poseData.roll * Math.PI / 180) : 0   // Rx: 横摇
    property real pitch: typeof poseData !== 'undefined' ? (poseData.pitch * Math.PI / 180) : 0 // Ry: 俯仰
    property real yaw: typeof poseData !== 'undefined' ? (poseData.yaw * Math.PI / 180) : 0     // Rz: 偏航

    // 绘制 3D 线框三棱锥
    Canvas {
        id: canvas3d
        anchors.fill: parent
        anchors.margins: 10
        renderTarget: Canvas.Image // 性能优化
        
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            
            var centerX = width / 2;
            var centerY = height / 2;
            
            // 物理参数定义 (锥体尺寸)
            var r = 40; // 底面半径
            var h = 60; // 锥体高度
            
            // 定义三棱锥顶点
            // 坐标轴定义：Z垂直(Up), Y前后(Front), X左右(Right)
            var vertices = [
                {x: 0,                   y: r,      z: -h/2}, // 0: 底面前顶点 (Y+)
                {x: -r * Math.sqrt(3)/2, y: -r/2,   z: -h/2}, // 1: 底面左后 (X-, Y-)
                {x: r * Math.sqrt(3)/2,  y: -r/2,   z: -h/2}, // 2: 底面右后 (X+, Y-)
                {x: 0,                   y: 0,      z: h/2}   // 3: 锥尖 (Z+)
            ];

            // 投影计算函数
            function project(v) {
                var x = v.x, y = v.y, z = v.z;
                var tmp;
                
                // --- 1. 应用旋转 ---
                // Rx (Roll - 绕Y轴)
                tmp = x * Math.cos(root.roll) + z * Math.sin(root.roll);
                z = -x * Math.sin(root.roll) + z * Math.cos(root.roll);
                x = tmp;

                // Ry (Pitch - 绕X轴)
                tmp = y * Math.cos(root.pitch) - z * Math.sin(root.pitch);
                z = y * Math.sin(root.pitch) + z * Math.cos(root.pitch);
                y = tmp;
                
                // Rz (Yaw - 绕Z轴)
                tmp = x * Math.cos(root.yaw) - y * Math.sin(root.yaw);
                y = x * Math.sin(root.yaw) + y * Math.cos(root.yaw);
                x = tmp;

                // --- 2. 应用位移 ---
                x += root.xPos * 0.1;           // X 左右
                y += root.yPos * 0.1;           // Y 前后
                z += root.zPos * 0.1;           // Z 垂直

                // --- 3. 透视投影 ---
                var viewDist = 300;
                var depth = viewDist - y; 
                var scale = viewDist / (depth > 10 ? depth : 10);
                
                return {
                    x: centerX + x * scale,
                    y: centerY - z * scale // 屏幕Y反向
                };
            }

            var p = vertices.map(project);

            // 绘图样式
            ctx.lineWidth = 2;
            ctx.lineJoin = "round";

            // 1. 绘制底面三角形
            ctx.strokeStyle = "#00f0ff";
            ctx.beginPath();
            ctx.moveTo(p[0].x, p[0].y);
            ctx.lineTo(p[1].x, p[1].y);
            ctx.lineTo(p[2].x, p[2].y);
            ctx.closePath();
            ctx.stroke();

            // 2. 绘制侧棱
            ctx.beginPath();
            for (var i = 0; i < 3; i++) {
                ctx.moveTo(p[i].x, p[i].y);
                ctx.lineTo(p[3].x, p[3].y);
            }
            ctx.stroke();
            
            // 3. 绘制前方标识
            ctx.strokeStyle = "#ff3333";
            ctx.beginPath();
            ctx.arc(p[0].x, p[0].y, 3, 0, 2 * Math.PI);
            ctx.stroke();
        }

        Connections {
            target: root
            function onRollChanged() { canvas3d.requestPaint(); }
            function onPitchChanged() { canvas3d.requestPaint(); }
            function onYawChanged() { canvas3d.requestPaint(); }
            function onXPosChanged() { canvas3d.requestPaint(); }
            function onYPosChanged() { canvas3d.requestPaint(); }
            function onZPosChanged() { canvas3d.requestPaint(); }
        }
    }

    // ==== 数据分布在四个角落 ====
    
    // 左上: X (左右)
    Text {
        anchors.left: parent.left; anchors.top: parent.top
        anchors.margins: 10
        text: "X: " + root.xPos.toFixed(1) + " mm\n[左右]"
        color: "#00ff00"; font.pixelSize: 10; font.family: "monospace"
    }

    // 右上: Rx (横摇)
    Text {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.margins: 10
        text: "Rx: " + (root.roll * 180 / Math.PI).toFixed(1) + "°\n[横摇]"
        color: "#ffaa00"; font.pixelSize: 10; font.family: "monospace"
        horizontalAlignment: Text.AlignRight
    }

    // 左下: Y (前后) & Z (垂直)
    Column {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.margins: 10; spacing: 4
        Text { text: "Y: " + root.yPos.toFixed(1) + " mm [前后]"; color: "#00ff00"; font.pixelSize: 10; font.family: "monospace" }
        Text { text: "Z: " + root.zPos.toFixed(1) + " mm [垂直]"; color: "#00ff00"; font.pixelSize: 10; font.family: "monospace" }
    }

    // 右下: Ry (俯仰) & Rz (偏航)
    Column {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 10; spacing: 4
        Text { width: parent.width; text: "Ry: " + (root.pitch * 180 / Math.PI).toFixed(1) + "° [俯仰]"; color: "#ffaa00"; font.pixelSize: 10; font.family: "monospace"; horizontalAlignment: Text.AlignRight }
        Text { width: parent.width; text: "Rz: " + (root.yaw * 180 / Math.PI).toFixed(1) + "° [偏航]"; color: "#ffaa00"; font.pixelSize: 10; font.family: "monospace"; horizontalAlignment: Text.AlignRight }
    }
}
