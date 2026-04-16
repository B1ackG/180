import QtQuick 2.15

Item {
    id: root
    width: 800
    height: 600
    property string currentCategory: "全部"

    // 计算滚动条位置和比例
    property real scrollPos: listView.visibleArea.yPosition
    property real scrollHeight: listView.visibleArea.heightRatio

    // 定义 Model
    ListModel {
        id: historyModel
    }

    // 公开接口供 C++ 调用
    function addRecord(time, page, control, op, oldVal, newVal, controlType) {
        historyModel.insert(0, {
            "time": time,
            "page": page,
            "control": control,
            "op": op,
            "oldVal": oldVal,
            "newVal": newVal,
            "controlType": controlType
        })
        if (historyModel.count > 500) historyModel.remove(500)
    }

    function clearRecords() {
        historyModel.clear()
    }

    function isAlarmRecord(controlType, op, page, control) {
        var t = (controlType || "") + " " + (op || "") + " " + (page || "") + " " + (control || "")
        t = t.toLowerCase()
        return t.indexOf("报警") >= 0 ||
               t.indexOf("急停") >= 0 ||
               t.indexOf("告警") >= 0 ||
               t.indexOf("超限") >= 0 ||
               t.indexOf("fault") >= 0 ||
               t.indexOf("error") >= 0 ||
               t.indexOf("warning") >= 0 ||
               t.indexOf("alarm") >= 0
    }

    function isControlRecord(controlType, op) {
        var ct = controlType || ""
        var opText = (op || "").toLowerCase()
        if (ct === "EnableButton" || ct === "MatrixKey") {
            return true
        }

        return opText.indexOf("external") >= 0 ||
               opText.indexOf("enable") >= 0 ||
               opText.indexOf("使能") >= 0
    }

    function matchesCategory(controlType, op, page, control) {
        if (currentCategory === "全部") {
            return true
        }

        if (currentCategory === "警报") {
            return isAlarmRecord(controlType, op, page, control)
        }

        if (currentCategory === "操控设备") {
            return !isAlarmRecord(controlType, op, page, control) && isControlRecord(controlType, op)
        }

        if (currentCategory === "常规运行") {
            return !isAlarmRecord(controlType, op, page, control) && !isControlRecord(controlType, op)
        }

        return true
    }

    function categoryButtonColor(category) {
        return currentCategory === category ? "#2E7DD8" : "#24456B"
    }

    function categoryButtonBorder(category) {
        return currentCategory === category ? "#89C3FF" : "#3F678D"
    }

    function categoryTextColor(category) {
        return currentCategory === category ? "#F7FBFF" : "#BFD9F2"
    }

    function hasVisibleRecords() {
        for (var i = 0; i < historyModel.count; ++i) {
            var r = historyModel.get(i)
            if (matchesCategory(r.controlType, r.op, r.page, r.control)) {
                return true
            }
        }
        return false
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

    Rectangle {
        id: categoryBar
        anchors.top: header.bottom
        anchors.topMargin: 8
        width: parent.width
        height: 36
        color: "transparent"

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 8
            spacing: 8

            Repeater {
                model: ["全部", "警报", "常规运行", "操控设备"]
                delegate: Rectangle {
                    width: modelData === "操控设备" ? 92 : 74
                    height: 28
                    radius: 14
                    color: root.categoryButtonColor(modelData)
                    border.width: 1
                    border.color: root.categoryButtonBorder(modelData)

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: root.categoryTextColor(modelData)
                        font.pixelSize: 12
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.currentCategory = modelData
                    }
                }
            }
        }
    }

    // 滚动列表 - 移除对 ScrollBar (QtQuick.Controls) 的直接依赖
    ListView {
        id: listView
        anchors.top: categoryBar.bottom
        anchors.topMargin: 6
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        model: historyModel
        clip: true
        spacing: 0

        delegate: Item {
            property bool rowVisible: root.matchesCategory(model.controlType, model.op, model.page, model.control)
            width: listView.width
            height: rowVisible ? 58 : 0
            visible: rowVisible

            // 背景层：外阴影感
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: 3
                color: "#1a5fb4"
                opacity: 0.1
                radius: 4
            }

            // 主体层
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 4
                anchors.bottomMargin: 4
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

        Text {
            anchors.centerIn: parent
            text: "当前分类下暂无记录"
            color: "#cfe6ff"
            opacity: 0.45
            font.pixelSize: 16
            visible: historyModel.count > 0 && !root.hasVisibleRecords()
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
