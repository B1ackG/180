# Modbus 寄存器对齐决策

本文档记录 `docs/180.csv` 与上位机代码（`modbusregistermap.h`）的对齐结论，作为联调与后续变更的基准。

## 设备划分

| 设备 | 默认地址 | 代码入口 |
|------|----------|----------|
| AGV 底盘 | `192.168.1.88:502` | `AGVModbusManager`, `writeToAGVDevice` |
| Robot 主控 | `192.168.1.13:502` | `ModbusThreadManager`, `writeToMainDevice` |

CSV 中 **AGV Tab 曾重复列出主控写寄存器（500~615 等）**，已自 AGV 段移除；这些条目仅保留在 **Robot Tab**。

## AGV 关键决策

| 主题 | CSV / 现场 | 代码常量 |
|------|------------|----------|
| 控制模式写 | 原 CSV 误标为 ChoiceAxis@500 | `AgvReg::ControlModeWrite` (500)，读 `AgvReg::ControlModeRead` (100) |
| 故障 BOOL | 51.bit0~2 低电/通讯/驱动 | 仅解析 51，不再从 102 取位 |
| 电池电量 | 102 整字 0~100% | `AgvReg::Battery1`，102 不做位变量 |
| 故障码 | 原 CSV 第 55 行笔误 | `AgvReg::FaultCodeStart` = **110**，共 8 字 (110~117) |
| 速度/角度命令 | CSV 未单独列写地址 | 写 3/4，读 153/154 |
| 转向模式命令 | CSV 未单独列写地址 | 写 2，读 155 |
| 步进位移 | 读 105 | 写 5 |
| 驻车/避障等 | — | 写 `AgvReg::ControlCommandBits` (0) 各位 |

## Robot 主控关键决策

| 主题 | CSV | 代码 |
|------|-----|------|
| 轴选择 | 500 ChoiceAxis 1~7 | 步进目标轴索引 1~5 写入 500 |
| 点动/步进模式 | 501 1=点动 2=数字化 | 首页写 501；六自由度页写 600 |
| 步进位置 | 502 LREAL (502~505) | 仅 `writeStepValueDoubleToMainDevice` 写 502~505 |
| 矩阵键运动 | 526 JogXYZ | 原误写 124，已改为 526；124 只读 |
| 全局速度 | 5000 AxisSetVelocity | 滑块写 5000（原误写 5001） |

## PLC 地址与 %MX 偏移

AGV 直连 Modbus 地址与 PLC 符号对照（状态字）：

- `%MX100.x` → Modbus **50**.x
- `%MX101.x` → Modbus **51**.x
- `%MW102` 及以后 → 同号 Modbus 字地址

`modbusvariables.cpp` 已按 AGV 直连地址（50/51/102…）维护上位机镜像表。
