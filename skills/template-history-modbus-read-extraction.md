# 模板提取清单：仅保留历史记录 + Modbus读取UI

## 目标
从当前工程裁剪出一个“可复用模板”，只保留：
- 历史记录页面与记录能力
- Modbus 读取链路与读值展示 UI

## A. 必留文件（核心）

### 1) 入口与主窗口骨架
- `main.cpp`
- `mainwindow.h`
- `mainwindow.cpp`
- `mainwindow_lifecycle.cpp`
- `mainwindow.ui`
- `res.qrc`

### 2) 历史记录模块
- `operationrecorder.h`
- `operationrecorder.cpp`
- `mappingconfig.h`
- `mappingconfig.cpp`
- `HistoryList.qml`

### 3) Modbus 读取主链路（主设备）
- `modbustcpclient.h`
- `modbustcpclient.cpp`
- `modbusthreadmanager.h`
- `modbusthreadmanager.cpp`
- `maindevicemodbusapi.h`
- `maindevicemodbusapi.cpp`
- `mainmodbuspoller.h`
- `mainmodbuspoller.cpp`
- `mainmodbusstatus.h`
- `mainmodbusstatus.cpp`
- `mainmodbusconnector.h`
- `mainmodbusconnector.cpp`
- `modbusvariables.h`
- `modbusvariables.cpp`
- `mainmodbuslabelmapper.h`
- `mainmodbuslabelmapper.cpp`

## B. 视图控件（按你最终UI保留）
- 如果保留自定义控件，需要同步保留其文件：
  - `techsliderlabel.*`
  - `techslideredit.*`
  - `techarcgauge.*`
  - `techspeedgauge.*`
  - `batterywidget.*`
  - 以及它们在 `.ui` 中对应的使用项

## C. 可选保留（建议）
- `featureswitchmanager.*`（保留开关能力，便于模板裁剪）
- `featureswitchwidget.*`（如果你要继续保留运行期配置页）

## D. 建议删除（若目标是“精简模板”）
- 与矩阵键/硬件按键相关：
  - `matrixkeymonitor.*`
  - `matrixkeythreadmanager.*`
  - `enablebuttonworker.*`
- 与当前业务动作强绑定但非“读UI”必须：
  - `agvmodbusmanager.*`（如果新模板不需要AGV专项逻辑）
  - `speedmodeselector.*`
  - `steeringmodeselector.*`
  - `poseprovider.*`（若不保留对应QML）
  - `animationmanager.*`（按需）

## E. 代码层推荐拆分（进一步降耦）
1. `HistoryModule`：仅管记录、筛选、导出、QML展示。
2. `ModbusReadModule`：仅管连接、轮询、解析、读值发布。
3. `MainWindow`：仅保留页面切换与模块装配，不直接写业务细节。

## F. 提取顺序（避免一次性大爆炸）
1. 先保留全部核心文件，确保能编译跑通。
2. 删除明显无关模块（输入设备、动作控制）。
3. 逐步替换 `MainWindow` 中与目标无关的初始化。
4. 最后收敛 `.pro` 文件中的 `SOURCES/HEADERS`。

## 验收标准
- 能连接到 Modbus 设备并周期读取
- 关键读值可在 UI 上显示
- 历史记录可新增、筛选、导出
- 工程可独立编译并运行