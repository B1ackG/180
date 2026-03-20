import QtQuick 2.15

Item {
    id: root
    width: 800
    height: 600

    // 计算滚动条位置和比例
    property real scrollPos: listView.visibleArea.yPosition
    property real scrollHeight: listView.visibleArea.heightRatio

    // 定义 Model
    ListModel {
        id: historyModel
    }

    // 公开接口供 C++ 调用
    function addRecord(time, page, control, op, oldVal, newVal) {
        historyModel.insert(0, {
            "time": time,
            "page": page,
            "control": control,
            "op": op,
            "oldVal": oldVal,
            "newVal": newVal
        })
        if (historyModel.count > 500) historyModel.remove(500)
    }

    function clearRecords() {
        historyModel.clear()
    }

    // 渐变标题背景
    Rectangle {
        id: header
        width: parent.width
        height: 40
        color: "transparent"
        
        Rectangle {
            anchors.fill: parent
            color: "#1a5fb4"
            opacity: 0.2
            radius: 4
        }
        
        Row {
            anchors.fill: parent
            anchors.leftMargin: 15
            spacing: 0
            
            Text { width: parent.width * 0.15; text: "时间"; color: "#a9d4ff"; font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
            Text { width: parent.width * 0.20; text: "页面"; color: "#a9d4ff"; font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
            Text { width: parent.width * 0.25; text: "控件"; color: "#a9d4ff"; font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
            Text { width: parent.width * 0.40; text: "操作详情"; color: "#a9d4ff"; font.bold: true; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
        }
    }

    // 滚动列表 - 移除对 ScrollBar (QtQuick.Controls) 的直接依赖
    ListView {
        id: listView
        anchors.top: header.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        model: historyModel
        clip: true
        spacing: 6

        delegate: Item {
            width: listView.width
            height: 52

            // 背景层：外阴影感
            Rectangle {
                anchors.fill: parent
                color: "#1a5fb4"
                opacity: 0.1
                radius: 4
            }

            // 主体层
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                color: index % 2 === 0 ? "#254a8a" : "#1e3c78"
                opacity: 0.6  // 提升透明度，让颜色更亮
                radius: 4
                border.width: 1
                border.color: index % 2 === 0 ? "#50a9d4ff" : "#30a9d4ff"

                // 左侧发光条装饰
                Rectangle {
                    width: 3
                    height: parent.height * 0.6
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#00f0ff"
                    visible: index === 0 // 最新一条加亮
                }

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    spacing: 0

                    Text {
                        width: parent.width * 0.15; height: parent.height
                        text: model.time; color: "#00f0ff"; verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12; font.family: "Monospace"
                    }
                    Text {
                        width: parent.width * 0.20; height: parent.height
                        text: model.page; color: "#ffffff"; verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                    }
                    Text {
                        width: parent.width * 0.25; height: parent.height
                        text: model.control; color: "#ffffff"; verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13; font.bold: true
                        elide: Text.ElideRight
                    }
                    
                    Column {
                        width: parent.width * 0.40; height: parent.height
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        
                        Text {
                            text: model.op; color: "#ff8888"; font.pixelSize: 11; font.italic: true
                        }
                        Text {
                            text: qsTr("%1 → %2").arg(model.oldVal).arg(model.newVal)
                            color: "#00ff88"; font.pixelSize: 12; font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: parent.opacity = 0.9
                    onExited: parent.opacity = 0.6
                }
            }
        }

        Text {
            anchors.centerIn: parent
            text: "暂无操作记录"
            color: "#ffffff"
            opacity: 0.3
            font.pixelSize: 18
            visible: historyModel.count === 0
        }
    }

    // 自实现简易滚动条，不依赖 QtQuick.Controls 模块
    Rectangle {
        id: customScrollBar
        anchors.right: listView.right
        anchors.rightMargin: 2
        y: header.height + 5 + scrollPos * listView.height
        width: 6
        height: scrollHeight * listView.height
        color: "#1a5fb4"
        opacity: 0.5
        radius: 3
        visible: scrollHeight < 1.0
    }
}
