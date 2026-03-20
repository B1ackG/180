import QtQuick 2.15

Item {
    id: root
    width: 60
    height: 30
    
    property real batteryLevel: 80 // 0 - 100
    property bool isCharging: false
    
    // 基础颜色定义
    readonly property color bodyColor: "#a9d4ff"
    readonly property color fluidColor: {
        if (batteryLevel < 20) return "#ff4d4d"; // 红色
        if (batteryLevel < 40) return "#ffa500"; // 橙色
        return "#00f0ff"; // 科技青
    }

    // 电池外壳
    Rectangle {
        id: body
        anchors.fill: parent
        anchors.rightMargin: 4
        color: "transparent"
        border.color: bodyColor
        border.width: 2
        radius: 3
        
        // 内部填充（电量）
        Rectangle {
            id: fillRect
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 3
            width: (parent.width - 6) * (batteryLevel / 100)
            color: fluidColor
            radius: 1
            
            // 呼吸动画（充电中）
            SequentialAnimation on opacity {
                running: isCharging
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.3; duration: 800 }
                NumberAnimation { from: 0.3; to: 1.0; duration: 800 }
            }
        }
    }
    
    // 电池正极（小突起）
    Rectangle {
        width: 4
        height: parent.height * 0.4
        anchors.left: body.right
        anchors.verticalCenter: parent.verticalCenter
        color: bodyColor
        radius: 1
    }
    
    // 电量文字显示
    Text {
        anchors.centerIn: body
        text: Math.round(batteryLevel) + "%"
        color: batteryLevel > 50 ? "#000" : "#fff"
        font.pixelSize: 10
        font.bold: true
        visible: parent.width > 40 // 宽度太小时不显示文字
    }
}
