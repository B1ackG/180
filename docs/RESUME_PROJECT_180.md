# 工业示教器 HMI 项目 — 简历素材

> 基于 Project 180 代码库整理，可直接复制到简历或面试准备。  
> 带 `[待填写]` 的字段请按你的实际情况修改。

---

## 个人信息（写入简历前请修改）

| 字段 | 你的填写 | 示例（仅供参考，勿直接照搬） |
|------|----------|------------------------------|
| 项目时间 | `[待填写]` | 2024.03 – 2025.06 |
| 公司名称 | `[待填写]` | 某工业机器人科技有限公司 |
| 你的角色 | `[待填写]` | 核心开发工程师 / 上位机负责人 |
| 团队规模 | `[待填写]` | 3 人（上位机 1 + 嵌入式 1 + 电气联调 1） |
| 量化成果 | `[待填写]` | 完成 RK356x 示教器量产部署；联调周期由 3 周缩短至 1.5 周；现场稳定运行 N 台 |

---

## 一、项目定位（标题 + 一句话）

**中文标题：** 工业机器人 + AGV 联合示教器 HMI 控制系统  
**英文标题：** Industrial Robot & AGV Teach-Pendant HMI Control System

**一句话：** 基于 Qt/C++ 的嵌入式工业示教器上位机，通过 Modbus TCP 同时控制机械臂主控与 AGV 底盘，部署于 RK356x aarch64 嵌入式 Linux 触摸屏，面向现场操作、点动/步进、安全联锁与操作审计。

---

## 二、项目背景与业务价值

| 维度 | 内容 |
|------|------|
| 行业领域 | 工业自动化 / 智能制造 / 移动机器人 |
| 产品形态 | 现场示教器（Teach Pendant）人机界面 |
| 控制对象 | 六自由度机械臂 + AGV 移动底盘（双 PLC 主控） |
| 部署环境 | RK356x aarch64、Buildroot、Wayland 触摸屏 |
| 用户角色 | 操作员、管理员（权限验证后可调功能开关） |

**业务价值：**
- 将机械臂与 AGV 的操控、状态监控、报警处理统一到一块嵌入式触摸屏，减少多终端切换
- 支持有线示教器 / 无线遥控双模式，满足现场调试与远程操作场景
- 操作记录可本地留存并 TCP 上报至 Windows 工控机，便于追溯与审计
- 启动自检（网络连通、矩阵键盘、使能按钮）降低现场部署故障率

---

## 三、技术栈（Skills 栏可引用）

**语言与框架：** C++17、Qt5（Widgets + QML/Quick 混合 UI）、Python（部署脚本）

**工业通信：** Modbus TCP（双设备）、libmodbus（QLibrary 动态加载）、TCP Socket（日志上报、连通性探测）

**嵌入式与硬件：** Linux Input（矩阵键盘）、自定义字符设备（使能按钮）、aarch64 交叉编译、Buildroot、Wayland

**工程化：** qmake、Doxygen、INI 配置、50+ 功能开关、SSH/SCP 远程部署

---

## 四、模板选择建议

| 场景 | 推荐模板 | 说明 |
|------|----------|------|
| 简历空间充足、应聘上位机/嵌入式岗位 | **模板 A（完整版）** | 6 条 bullet，突出架构与安全联锁 |
| 一页简历、项目较多需压缩 | **模板 B（精简版）** | 3 行概括，技术栈单独一行 |
| 外企或英文简历 | **模板 C（英文版）** | 与 A 对应，可直接粘贴 |

**默认推荐：模板 A** — 本项目技术深度足够，完整版更能体现工业 HMI 与 Modbus 联调经验。

---

## 五、简历正文（可直接粘贴）

### 模板 A — 完整版（推荐）

**工业机器人 + AGV 联合示教器 HMI 控制系统** | C++ / Qt / 嵌入式 Linux  
*`[待填写：时间]` | `[待填写：角色]`*

- 负责基于 Qt5（Widgets + QML）的工业示教器上位机开发，部署于 RK356x aarch64 嵌入式触摸屏，通过 Modbus TCP 同时控制六轴机械臂与 AGV 移动底盘
- 设计双通道 Modbus 通信架构（独立线程 + 动态 libmodbus 后端），实现状态轮询、多字节序解析（LREAL/REAL）、位/字变量分离及写互锁门控（寄存器 8192）
- 实现点动/步进/六自由度姿态控制，针对 PLC 扫描周期设计分阶段写入与定时重传机制，解决外部矩阵键触发可靠性问题
- 构建 10+ 类安全报警联锁（急停、倾角仪、限位、低电量等），支持有线/无线双控制模式切换与使能按钮安全输入
- 开发操作审计模块（JSON 记录 + TCP 上报工控机）与 50+ 项功能开关体系，支持现场模块化调试与降级
- 完成 aarch64 交叉编译、Buildroot 部署及 SSH 自动化发布脚本，编写 Modbus 寄存器对齐文档保障联调效率

**技术栈：** C++17、Qt5、QML、Modbus TCP、libmodbus、Linux Input、多线程、嵌入式 Linux、交叉编译

---

### 模板 B — 精简版

**工业示教器 HMI（机械臂 + AGV）** | Qt / C++ / Modbus TCP  
*`[待填写：时间]` | `[待填写：角色]`*

- 嵌入式触摸屏示教器，双 Modbus TCP 控制机械臂与 AGV，部署于 RK356x Linux
- 多线程通信、寄存器映射对齐、PLC 时序写入优化、安全联锁与操作审计
- 技术：C++17、Qt Widgets/QML、libmodbus、aarch64 交叉编译

---

### 模板 C — 英文版

**Industrial Robot & AGV Teach-Pendant HMI** | C++ / Qt / Embedded Linux  
*`[Date]` | `[Role]`*

- Developed an embedded Qt5 HMI teach pendant on RK356x (aarch64/Buildroot) controlling a 6-DOF robot arm and AGV chassis over dual Modbus TCP channels
- Architected multi-threaded Modbus communication with dynamic libmodbus backend, register mirroring, endian-aware encoding, and multi-pendant write interlock (register 8192)
- Implemented jog/step/6-DOF pose control with staged register writes and retry timers to match PLC scan cycles
- Built safety interlock system (e-stop, inclinometer, limits, comm faults) and operation audit trail with TCP log forwarding
- Delivered cross-compilation toolchain, deployment automation, and register-map alignment documentation

**Tech:** C++17, Qt5, QML, Modbus TCP, libmodbus, multithreading, embedded Linux, aarch64 cross-compile

---

## 六、个人贡献（已勾选推荐 5 条）

根据代码结构与模块划分，若你参与核心上位机开发，建议在简历中优先保留以下 **5 条**（可按实际删减）：

- [x] **双 Modbus 通信模块设计与线程模型** — `ModbusThreadManager`、`AGVModbusManager`、`modbus_backend_c.cpp`
- [x] **机械臂点动/步进/六轴控制逻辑与外部矩阵键映射** — `mainwindow.cpp` 运动控制、矩阵键 `MatrixKeyThreadManager`
- [x] **安全报警联锁与示教器写互锁（8192）** — `ModbusWriteGate`、报警弹窗体系
- [x] **AGV 状态轮询、位变量读改写、转向/避障控制** — `agvmodbusmanager.cpp`、影子寄存器读改写
- [x] **aarch64 交叉编译、部署脚本与联调文档** — `deploy_and_run.py`、`docs/MODBUS_*`

**备选（若分工侧重 UI 或运维）：**
- [ ] QML 仪表盘与自定义 Qt 控件（`TechSpeedGauge.qml`、`DeviceCoordPanel.qml` 等）
- [ ] 操作记录、TCP 上报与 mapping 脱敏（`operationrecorder.cpp`、`mappingconfig.cpp`）
- [ ] 功能开关体系与启动自检流程（`featureswitchmanager.cpp`、`main.cpp`）

### 贡献 bullets 合并写法（可替换模板 A 中对应条目）

若希望更突出个人分工，可将模板 A 第 2、3、4 条替换为：

- 设计并实现双通道 Modbus 线程架构与 `libmodbus_backend.so` 动态加载方案，主控与 AGV 独立轮询，支持 LREAL/REAL 多字节序与寄存器 8192 写互锁
- 负责机械臂点动/步进及六轴外部矩阵键控制，采用分阶段寄存器写入 + 35/90/150ms 重传，解决 PLC 采样窗口竞态导致的触发失败
- 实现 AGV 位变量影子寄存器读改写、9 组地址轮询调度及 10+ 类安全报警联锁（急停、倾角、限位、通讯故障等）

---

## 七、技术难点（面试 STAR 素材）

### 1. 双 PLC 寄存器地图对齐

- **S（情境）：** CSV 规格书、现场 PLC、C++ 代码三方寄存器地址存在历史偏差（如矩阵键 124→526、全局速度 5001→5000）。
- **T（任务）：** 统一双设备映射，保障机械臂与 AGV 联调一致。
- **A（行动）：** 编写 `MODBUS_REGISTER_DECISIONS.md`；实现 `%MX100.x`→Modbus 50.x 偏移；LREAL GHEFCDAB / REAL CDAB 字节序对齐。
- **R（结果）：** 联调争议有文档基准，减少反复对表时间。（量化：`[待填写：如联调周期缩短 X%]`）

### 2. 多线程 Modbus 与 UI 解耦

- **问题：** Modbus 不可阻塞 UI，但业务需同步读寄存器。
- **方案：** 双 `QThread` + `BlockingQueuedConnection`；AGV 9 组地址轮询（200ms/组）。
- **结果：** UI 与总线 I/O 分离，状态刷新约 200ms 级。

### 3. PLC 扫描周期下的可靠写入

- **问题：** 步进需写 500→502–505→514 序列，值未落稳即触发导致运动异常。
- **方案：** 分阶段写入 + 序列号防重入 + `QTimer::singleShot` 重传（35/90/150ms）。
- **结果：** 外部矩阵键点动/步进触发可靠性显著提升。

### 4. 多示教器写互锁（8192）

- **问题：** 多台示教器同时写同一 PLC 有冲突风险。
- **方案：** `ModbusWriteGate` 统一拦截主控与 AGV 写路径，校验 8192 设备 ID。
- **结果：** 多终端并发场景下 PLC 写入安全可控。

### 5. AGV 位寄存器读改写

- **问题：** 同一字寄存器多位控制，单 bit 写易覆盖其他位。
- **方案：** `m_agvRegisterShadow` 影子寄存器，合并后单次写入。
- **结果：** 避免位覆盖导致的误动作。

### 6. 嵌入式交叉编译与动态后端

- **问题：** Qt 应用与 libmodbus 需分别交叉编译，运行时依赖 `MODBUS_BACKEND_LIB`。
- **方案：** C ABI 封装 + QLibrary 动态加载；独立部署文档与脚本。
- **结果：** 适配 RK356x Buildroot，现场可单独升级协议栈 `.so`。

### 7. 复杂安全联锁

- **问题：** 十余类报警源需实时响应且互不干扰。
- **方案：** 位变量边沿检测 + 弹窗状态机 + 有线/无线模式门控 + 功能开关降级。
- **结果：** 覆盖急停、倾角、限位、通讯等 10+ 安全场景。

---

## 八、面试追问 — 参考答案

### Q1：为什么用 QLibrary 动态加载 libmodbus，而不是静态链接或 Qt SerialBus？

**答：**
1. **编译链解耦：** Qt 交叉编译套件与 libmodbus 可分开构建，避免在裁剪版 Qt SDK 上强依赖 `qtserialbus` 模块。
2. **现场可维护：** 协议栈升级只需替换 `libmodbus_backend.so`，无需重编整个 HMI 应用。
3. **双设备独立路径：** 主控与 AGV 可通过 `MODBUS_BACKEND_LIB` / `AGV_MODBUS_BACKEND_LIB` 指向不同后端实例（或同库不同连接）。
4. **避免双栈混用：** 工程明确不混用 Qt Serial Bus 与 libmodbus 两套 TCP 实现，降低行为不一致风险。

参考：`docs/MODBUS_DYNAMIC_BACKEND_DEPLOY.md` 第 1 节。

---

### Q2：BlockingQueuedConnection 会不会卡 UI？如何权衡？

**答：**
- **会阻塞：** UI 线程发起 `BlockingQueuedConnection` 调用时，会等待 Modbus 线程完成读写才返回，网络超时或 PLC 无响应时可能造成界面卡顿。
- **为何仍用：** 部分业务逻辑需要「写完再读确认」的同步语义，代码路径简单、避免竞态。
- **权衡与优化：**
  - 高频轮询、状态刷新走 **信号槽异步回调**（`AGVModbusManager` 的 `variableUpdated` 等）。
  - 用户操作触发的单次写入可接受短暂阻塞；长时间操作应加超时与进度提示。
  - 启动自检在 Splash 阶段阻塞用户可接受。
- **改进方向（可主动提及）：** 关键路径改为 `QueuedConnection` + `QFuture` 或状态机，减少 UI 线程同步等待。

---

### Q3：寄存器 8192 互锁如何防止绕过？

**答：**
1. **统一入口：** 所有主控写操作经 `ModbusThreadManager::writeSingleRegister` / `writeMultipleRegisters`；AGV 写经 `AGVModbusManager::writeSingleRegister`。
2. **门控逻辑：** `ModbusWriteGate::allowWrite()` 在每次写前检查当前 8192 值是否等于 `config.ini` 中的 `teaching_write_device_id`。
3. **豁免寄存器：** 8192、8193、8194 本身可写（用于互锁协商），其余寄存器未获锁则拒绝写入。
4. **UI 同步：** 定时轮询 8192 并更新示教器按钮状态，操作员可感知当前是否拥有写权限。

参考：`modbuswritegate.cpp`、`mainwindow.cpp` 中 8192 同步定时器。

---

### Q4：步进写入为什么要分阶段 + 重传？

**答：**
- PLC 以固定扫描周期（如 10–50ms）读取保持寄存器；上位机若在 **502–505（LREAL 步进值）尚未被 PLC 采样落稳** 时就写 **514（触发位）**，PLC 可能用旧值或零值执行运动。
- **分阶段：** 先写目标轴（500）→ 步进值（502–505）→ 延迟后再写触发（514）。
- **重传：** `QTimer::singleShot(35, 90, 150ms)` 多次写触发，覆盖不同 PLC 扫描相位。
- **防重入：** `m_robotExternalWriteSeq` 序列号丢弃过期定时器回调，避免按键连按导致乱序。

参考：`skills/six-axis-step-control-write-reliability.md`。

---

### Q5：AGV 轮询 9 组地址的设计权衡？

**答：**
- **约束：** 单 Modbus TCP 连接、请求数量与报文长度受限；一次性读全表可能造成超时或阻塞其他写操作。
- **策略：** 将 AGV 寄存器分为 9 组，定时器每 200ms 轮询一组，充电状态（156）等低频数据单独 5s 槽位。
- **权衡：**
  - 优点：带宽平滑、单组失败不影响其他组、实现简单。
  - 缺点：全量状态刷新约 1.8s，安全关键位（如触摸边沿）非每周期更新。
- **补救：** 故障/急停类位在 UI 层做边沿检测；若需更快可为核心安全组提高优先级或缩短周期。

参考：`agvmodbusmanager.cpp` 轮询分组逻辑。

---

### 附加追问（建议了解）

| 问题 | 要点 |
|------|------|
| LREAL 字节序为何用 GHEFCDAB？ | 与现场 PLC 定义一致，错序会导致步进距离数量级错误 |
| 有线/无线模式如何互锁？ | 写 AGV 前检查控制模式寄存器，无线模式下禁止示教器写底盘 |
| 操作记录为何要做 mapping 脱敏？ | 操作员看中文业务描述，不应暴露寄存器号等技术细节 |
| 功能开关有何用？ | 现场调试可关闭 Modbus/报警/记录等模块，定位问题不需重编译 |

---

## 九、架构简图（面试白板可用）

```
示教器 HMI (Qt)
├── UI: Widgets + QML
├── MainWindow (业务中枢)
├── ModbusThreadManager → Robot PLC (192.168.1.13:502)
├── AGVModbusManager    → AGV PLC   (192.168.1.88:502)
├── ModbusWriteGate (8192 互锁)
├── MatrixKey / EnableButton (Linux 输入设备)
└── OperationRecorder → TCP → 工控机
         ↓
   libmodbus_backend.so (QLibrary 动态加载)
```

---

## 十、代码规模与关键文件

| 指标 | 数值 |
|------|------|
| `mainwindow.cpp` | ~12,300 行 |
| Modbus 相关模块 | ~3,500+ 行 |
| 源文件（.cpp/.h/.qml） | ~170 个 |
| 功能开关 | 10 大模块 + 40+ 子功能 |

| 主题 | 文件 |
|------|------|
| 程序入口与自检 | `main.cpp` |
| 核心业务 | `mainwindow.cpp` |
| 寄存器对齐 | `docs/MODBUS_REGISTER_DECISIONS.md` |
| 部署指南 | `docs/MODBUS_DYNAMIC_BACKEND_DEPLOY.md` |
| 写互锁 | `modbuswritegate.cpp` |
| AGV 管理 | `agvmodbusmanager.cpp` |
| 机械臂 Modbus | `modbusthreadmanager.cpp` |
| 时序写入 | `skills/six-axis-step-control-write-reliability.md` |

---

## 十一、使用 checklist

- [ ] 填写第一节「个人信息」表格
- [ ] 根据简历篇幅选择模板 A 或 B（外企用 C）
- [ ] 从第六节确认 5 条贡献是否与你的分工一致，删改备选
- [ ] 将量化成果写入模板 A 的 bullet 或单独一行
- [ ] 面试前通读第八节 5 个追问 + 第七节 STAR 素材

---

*文档生成自 Project 180 代码库分析。最后更新：2026-07-13*
