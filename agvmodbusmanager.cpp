// file name: agvmodbusmanager.cpp
#include "agvmodbusmanager.h"
#include "modbuswritegate.h"
#include "modbusthreadmanager.h"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <QMetaObject>

namespace {
template <typename Func>
bool resolveRequiredSymbol(QLibrary &library, const char *name, Func &target, QString &error)
{
    target = reinterpret_cast<Func>(library.resolve(name));
    if (!target) {
        error = QStringLiteral("动态库缺少符号: %1").arg(QString::fromLatin1(name));
        return false;
    }
    return true;
}

} // namespace

AGVModbusManager::AGVModbusManager(QObject *parent)
    : QObject(parent)
    , m_networkThread(nullptr)
    , m_host("192.168.1.88")
    , m_port(502)
    , m_autoReconnect(true)
    , m_reconnectInterval(5000)
    , m_reconnectTimer(nullptr)
    , m_pollTimer(nullptr)
    , m_pollInterval(200)  // 默认200ms
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &AGVModbusManager::pollRegisters);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &AGVModbusManager::tryReconnect);

    qDebug() << "AGV Modbus管理器已创建";
}

bool AGVModbusManager::startWorkerThread()
{
    if (m_networkThread && m_networkThread->isRunning()) {
        return true;
    }

    if (parent()) {
        qWarning() << "AGVModbusManager 有父对象，无法迁移到专用线程";
        return false;
    }

    m_networkThread = new QThread();
    moveToThread(m_networkThread);
    m_networkThread->start();
    qDebug() << "AGV Modbus管理器已迁移到专用线程:" << m_networkThread;
    return true;
}

void AGVModbusManager::stopWorkerThread()
{
    if (!m_networkThread) {
        return;
    }

    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { disconnectFromDevice(); }, Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(this, [this]() {
            if (QCoreApplication::instance()) {
                moveToThread(QCoreApplication::instance()->thread());
            }
        }, Qt::BlockingQueuedConnection);
    } else {
        disconnectFromDevice();
    }

    if (m_networkThread->isRunning()) {
        m_networkThread->quit();
        m_networkThread->wait();
    }

    delete m_networkThread;
    m_networkThread = nullptr;
}

AGVModbusManager::~AGVModbusManager()
{
    stopWorkerThread();
    unloadDynamicBackend();

    qDebug() << "AGV Modbus管理器已销毁";
}

bool AGVModbusManager::ensureDynamicBackendLoaded()
{
    if (m_dynamicBackendLoadAttempted) {
        return m_useDynamicBackend;
    }
    m_dynamicBackendLoadAttempted = true;

    // 兼容主链路变量名，同时支持 AGV 专用覆盖
    m_dynamicBackendPath = qEnvironmentVariable("AGV_MODBUS_BACKEND_LIB").trimmed();
    if (m_dynamicBackendPath.isEmpty()) {
        m_dynamicBackendPath = qEnvironmentVariable("MODBUS_BACKEND_LIB").trimmed();
    }
    if (m_dynamicBackendPath.isEmpty()) {
        return false;
    }

    m_dynamicBackendLibrary.setFileName(m_dynamicBackendPath);
    if (!m_dynamicBackendLibrary.load()) {
        m_lastDynamicBackendError = QStringLiteral("加载AGV动态库失败: %1")
                                        .arg(m_dynamicBackendLibrary.errorString());
        qWarning() << m_lastDynamicBackendError << "路径:" << m_dynamicBackendPath;
        return false;
    }

    if (!resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_create", m_backendCreate,
                               m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_destroy", m_backendDestroy,
                                  m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_connect", m_backendConnect,
                                  m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_disconnect", m_backendDisconnect,
                                  m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_is_connected", m_backendIsConnected,
                                  m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_read_holding_registers",
                                  m_backendReadHolding, m_lastDynamicBackendError)
        || !resolveRequiredSymbol(m_dynamicBackendLibrary, "modbus_backend_write_single_register",
                                  m_backendWriteSingle, m_lastDynamicBackendError)) {
        qWarning() << m_lastDynamicBackendError;
        unloadDynamicBackend();
        return false;
    }

    m_backendReadInput = reinterpret_cast<MbReadRegistersFn>(
        m_dynamicBackendLibrary.resolve("modbus_backend_read_input_registers"));
    m_backendWriteMultiple = reinterpret_cast<MbWriteMultipleFn>(
        m_dynamicBackendLibrary.resolve("modbus_backend_write_multiple_registers"));

    m_dynamicBackendHandle = m_backendCreate ? m_backendCreate() : nullptr;
    if (!m_dynamicBackendHandle) {
        m_lastDynamicBackendError = QStringLiteral("AGV动态库创建 backend 句柄失败");
        qWarning() << m_lastDynamicBackendError;
        unloadDynamicBackend();
        return false;
    }

    m_useDynamicBackend = true;
    qInfo() << "AGV Modbus 动态库后端已启用:" << m_dynamicBackendPath;
    return true;
}

void AGVModbusManager::unloadDynamicBackend()
{
    if (m_backendDisconnect && m_dynamicBackendHandle) {
        m_backendDisconnect(m_dynamicBackendHandle);
    }
    if (m_backendDestroy && m_dynamicBackendHandle) {
        m_backendDestroy(m_dynamicBackendHandle);
    }
    m_dynamicBackendHandle = nullptr;
    m_backendCreate = nullptr;
    m_backendDestroy = nullptr;
    m_backendConnect = nullptr;
    m_backendDisconnect = nullptr;
    m_backendIsConnected = nullptr;
    m_backendReadHolding = nullptr;
    m_backendReadInput = nullptr;
    m_backendWriteSingle = nullptr;
    m_backendWriteMultiple = nullptr;
    m_useDynamicBackend = false;

    if (m_dynamicBackendLibrary.isLoaded()) {
        m_dynamicBackendLibrary.unload();
    }
}

bool AGVModbusManager::connectToDevice(const QString &host, quint16 port)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, host, port, &ok]() {
            ok = connectToDevice(host, port);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    QMutexLocker locker(&m_mutex);

    m_host = host;
    m_port = port;

    if (!ensureDynamicBackendLoaded()) {
        const QString err = QStringLiteral("未加载AGV Modbus官方动态库，请设置MODBUS_BACKEND_LIB或AGV_MODBUS_BACKEND_LIB");
        m_lastSocketError = err;
        qWarning() << err;
        emit errorOccurred(err);
        emit updateStatusLabel("label_agv_connection", "连接错误: " + err);
        return false;
    }

    const int rc = m_backendConnect(m_dynamicBackendHandle,
                                    host.toUtf8().constData(),
                                    static_cast<int>(port),
                                    1);
    m_connectedState = (rc != 0);
    if (m_connectedState) {
        m_lastSocketError.clear();
        m_disconnectedWriteWarnedAddresses.clear();
        if (m_pollTimer) {
            m_pollTimer->start(m_pollInterval);
        }
        if (m_autoReconnect && m_reconnectTimer) {
            m_reconnectTimer->stop();
        }
        emit connected();
        emit updateStatusLabel("label_agv_connection", "已连接");
        qInfo() << "AGV Modbus动态库连接成功:" << host << ":" << port;
        return true;
    }
    const QString err = QStringLiteral("AGV动态库连接失败 host=%1 port=%2")
                            .arg(host)
                            .arg(port);
    m_lastSocketError = err;
    qWarning() << err;
    emit errorOccurred(err);
    emit updateStatusLabel("label_agv_connection", "连接错误: " + err);
    if (m_autoReconnect && m_reconnectTimer && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
    return false;
}

void AGVModbusManager::disconnectFromDevice()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { disconnectFromDevice(); }, Qt::BlockingQueuedConnection);
        return;
    }

    QMutexLocker locker(&m_mutex);

    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }

    m_connectedState = false;
    if (m_backendDisconnect && m_dynamicBackendHandle) {
        m_backendDisconnect(m_dynamicBackendHandle);
    }
    emit disconnected();
}

bool AGVModbusManager::isConnected() const
{
    if (QThread::currentThread() != thread()) {
        bool connected = false;
        QMetaObject::invokeMethod(const_cast<AGVModbusManager *>(this), [this, &connected]() {
            if (!m_connectedState) {
                connected = false;
                return;
            }
            if (m_backendIsConnected && m_dynamicBackendHandle) {
                connected = (m_backendIsConnected(m_dynamicBackendHandle) != 0);
            } else {
                connected = m_connectedState;
            }
        }, Qt::BlockingQueuedConnection);
        return connected;
    }

    if (!m_connectedState) {
        return false;
    }

    if (m_backendIsConnected && m_dynamicBackendHandle) {
        return m_backendIsConnected(m_dynamicBackendHandle) != 0;
    }
    return false;
}

void AGVModbusManager::handleCommunicationFailure(const QString &reason)
{
    bool shouldEmitDisconnected = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_connectedState) {
            m_connectedState = false;
            if (m_pollTimer && m_pollTimer->isActive()) {
                m_pollTimer->stop();
            }
            if (m_backendDisconnect && m_dynamicBackendHandle) {
                m_backendDisconnect(m_dynamicBackendHandle);
            }
            shouldEmitDisconnected = true;
        }
    }

    if (shouldEmitDisconnected) {
        emit disconnected();
    }

    m_lastSocketError = reason;
    emit errorOccurred(reason);
    emit updateStatusLabel("label_agv_connection", "连接中断，自动重连中...");

    if (m_autoReconnect && m_reconnectTimer && !m_reconnectTimer->isActive() && !m_host.isEmpty()) {
        qWarning() << "AGV 通信中断，启动自动重连，原因:" << reason;
        m_reconnectTimer->start(m_reconnectInterval);
    }
}

void AGVModbusManager::tryReconnect()
{
    if (m_autoReconnect && !m_host.isEmpty()) {
        qDebug() << "尝试重连AGV Modbus服务器...";
        connectToDevice(m_host, m_port);
    }
}

void AGVModbusManager::setPollInterval(int ms)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, ms]() { setPollInterval(ms); }, Qt::QueuedConnection);
        return;
    }

    m_pollInterval = ms;
    if (m_pollTimer->isActive()) {
        m_pollTimer->setInterval(ms);
    }
}

void AGVModbusManager::setAutoReconnect(bool enable, int interval)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enable, interval]() {
            setAutoReconnect(enable, interval);
        }, Qt::QueuedConnection);
        return;
    }

    m_autoReconnect = enable;
    m_reconnectInterval = interval;

    if (!enable) {
        m_reconnectTimer->stop();
    }
}

void AGVModbusManager::pollRegisters()
{
    if (!isConnected()) {
        return;
    }

    // 对齐 180_win7 轮询策略（适配本项目阻塞式 libmodbus）：
    // - 安全关键位（50-51）每个 tick 都读，保证最快刷新；
    // - 其余地址合并为连续区段批量读取并轮转，全量刷新约 4 个 tick。
    readMultipleRegisters(50, 2);

    static int pollGroup = 0;
    switch (pollGroup) {
    case 0:
        readMultipleRegisters(0, 1);
        break;
    case 1:
        readMultipleRegisters(100, 6);
        break;
    case 2:
        readMultipleRegisters(110, 8);
        break;
    case 3:
        readMultipleRegisters(151, 6); // 151-156：倾角/速度转向同步/转向模式/充电状态
        break;
    default:
        break;
    }

    pollGroup = (pollGroup + 1) % 4;
}

void AGVModbusManager::readMultipleRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readMultipleRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return;
    }

    if (!m_readsEnabled || !isConnected() || count <= 0 || count > 125) {
        return;
    }

    MbReadRegistersFn readFn = m_backendReadHolding;
    if (!readFn || !m_dynamicBackendHandle) {
        qWarning() << "AGV动态库读失败: 缺少读取函数";
        return;
    }

    QVector<quint16> values(count);
    const int readCount = readFn(m_dynamicBackendHandle,
                                 startAddress,
                                 count,
                                 values.data(),
                                 values.size());
    if (readCount <= 0) {
        qWarning() << "AGV动态库读失败 地址:" << startAddress << "数量:" << count;
        const QString reason = QStringLiteral("AGV动态库读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        handleCommunicationFailure(reason);
        return;
    }

    const int actualCount = qMin(readCount, count);
    bool faultCodesChanged = false;
    for (int i = 0; i < actualCount; ++i) {
        const int address = startAddress + i;
        const quint16 value = values.at(i);

        // 与主设备链路保持一致：轮询读到相同值时不重复驱动 UI、QML 和日志。
        if (m_registerValues.contains(address) && m_registerValues.value(address) == value) {
            continue;
        }

        emit registerValueChanged(address, value);

        if (address >= 50 && address <= 51) {
            processBitVariables(address, value);
        } else if (address >= 102 && address <= 117) {
            if (address == 102) {
                processBitVariables(address, value);
                processWordVariables(address, value);
            } else if (address == 103) {
                processWordVariables(address, value);
            } else {
                processWordVariables(address, value);
            }
        } else if (address == 156) {
            // 电池1充电状态（1=充电中）
            processWordVariables(address, value);
        }

        m_registerValues[address] = value;
        if (address >= 110 && address <= 117) {
            faultCodesChanged = true;
        }
    }

    // 故障寄存器按块读取；整块处理完仅重建一次列表，避免一次轮询触发
    // 多轮 clear/add 和跨线程 QListWidgetItem 分配。
    if (faultCodesChanged) {
        updateFaultCodesDisplay();
    }
}


bool AGVModbusManager::readHoldingRegistersSync(int startAddress, int count, QVector<quint16> &values)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, count, &values, &ok]() {
            ok = readHoldingRegistersSync(startAddress, count, values);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    values.clear();
    if (!isConnected() || count <= 0 || count > 125) {
        return false;
    }

    MbReadRegistersFn readFn = m_backendReadHolding;
    if (!readFn || !m_dynamicBackendHandle) {
        qWarning() << "AGV动态库读失败: 缺少读取函数";
        return false;
    }

    values.resize(count);
    const int readCount = readFn(m_dynamicBackendHandle,
                                 startAddress,
                                 count,
                                 values.data(),
                                 values.size());
    if (readCount <= 0) {
        qWarning() << "AGV动态库同步读失败 地址:" << startAddress << "数量:" << count;
        const QString reason = QStringLiteral("AGV动态库同步读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        handleCommunicationFailure(reason);
        values.clear();
        return false;
    }

    const int actualCount = qMin(readCount, count);
    values.resize(actualCount);
    for (int i = 0; i < actualCount; ++i) {
        m_registerValues[startAddress + i] = values.at(i);
    }
    return true;
}


void AGVModbusManager::processBitVariables(int address, quint16 value)
{
    // 处理位变量（每个位对应一个BOOL变量）
    quint16 oldValue = m_registerValues.contains(address) ? m_registerValues[address] : 0;

    // 如果值没有变化，直接返回
    if (oldValue == value) {
        return;
    }

    // 更新寄存器值
    m_registerValues[address] = value;

    for (int bitPos = 0; bitPos < 16; bitPos++) {
        bool bitValue = (value >> bitPos) & 0x01;
        bool oldBitValue = (oldValue >> bitPos) & 0x01;

        // 检查位状态是否变化
        if (oldBitValue != bitValue) {
            m_bitStates[address][bitPos] = bitValue;

            // 发射位变量变化信号
            emit bitVariableChanged(address, bitPos, bitValue);

            // 处理特定位变量
            QString bitName = getBitVariableName(address, bitPos);
            if (!bitName.isEmpty()) {
                // 心跳位不需要显示
                if (bitName == "heartbeat") {
                    continue;
                }

                // 故障相关的位由updateFaultsDisplay统一处理
                if (address == 102 && (bitPos == 0 || bitPos == 1 || bitPos == 2)) {
                    continue;
                }

                // 其他位变量更新对应的Label
                QString displayText;
                if (bitValue) {
                    if (bitName == "label_back_slow") displayText = "后避障减速触发";
                    else if (bitName == "label_front_stop") displayText = "前避障停止触发";
                    else if (bitName == "label_front_touch") displayText = "前触边触发";
                    else if (bitName == "label_front_slow") displayText = "前避障减速触发";
                    else if (bitName == "label_back_stop") displayText = "后避障停止触发";
                    else if (bitName == "label_back_touch") displayText = "后触边触发";
                    else if (bitName == "label_left_touch") displayText = "左触边触发";
                    else if (bitName == "label_right_touch") displayText = "右触边触发";
                    else displayText = "触发";
                } else {
                    displayText = "无动作";
                }

                // 确保Label名称有正确的格式
                QString labelName = bitName;
                if (!labelName.startsWith("label_")) {
                    labelName = "label_" + bitName;
                }

                qDebug() << "  更新Label:" << labelName << "->" << displayText;
                emit updateStatusLabel(labelName, displayText);

                // 特别处理特定的位变量
                if (bitName == "back_touch") {
                    qDebug() << "  后触边状态变化:" << (bitValue ? "触发" : "无动作");
                }

                if (bitName == "jog_running") {
                    qDebug() << "  点动运行状态变化:" << (bitValue ? "运行中" : "停止");
                }
            }

            // 特别处理心跳位
            if (address == 50 && bitPos == 0) {
                if (bitValue && !m_lastHeartbeatState) {
                    m_lastHeartbeatTime = QDateTime::currentDateTime();
                    qDebug() << "  收到心跳信号，时间:" << m_lastHeartbeatTime.toString("hh:mm:ss.zzz");
                    emit heartbeatReceived();
                }
                m_lastHeartbeatState = bitValue;
            }
        }
    }

    // 更新故障显示（处理地址52的故障位）
    if (address == 102) {
        qDebug() << "  更新故障显示";
        updateFaultsDisplay();
    }

    qDebug() << "=== 位变量处理完成 ===";
}

void AGVModbusManager::processWordVariables(int address, quint16 value)
{
    qDebug() << "=== 开始处理字变量 ===";
    qDebug() << "【进入processWordVariables】地址:" << address << "值:" << value;

    QString varName = getWordVariableName(address);
    qDebug() << "获取变量名 - 地址:" << address << "->" << varName;

    if (varName.isEmpty()) {
        qDebug() << "变量名为空，跳过处理";
        qDebug() << "=== 字变量处理完成 ===";
        return;
    }

    if (varName == "battery1") {
        // 电池1电量 (0-100%)
        int batteryPercent = qMin(value, static_cast<quint16>(100));
        qDebug() << "电池1电量:" << batteryPercent << "%";
        qDebug() << "发出updateProgressBar信号: progressBar_battery1, " << batteryPercent;
        emit updateProgressBar("progressBar_battery1", batteryPercent);
        emit updateStatusLabel("label_battery1_text", QString("%1%").arg(batteryPercent));
        emit wordVariableChanged(address, value); // 兜底链路：允许 UI 直接基于寄存器更新
    }
    else if (varName == "battery2") {
        // 电池2电量 (0-100%)
        int batteryPercent = qMin(value, static_cast<quint16>(100));
        qDebug() << "电池2电量:" << batteryPercent << "%";
        emit updateStatusLabel("label_battery2_text", QString("%1%").arg(batteryPercent));
        emit wordVariableChanged(address, value); // 兜底链路：允许 UI 直接基于寄存器更新
    }
    else if (varName == "speed") {
        // 行驶速度 (mm/s) - 更新到wordVariableChanged信号，由MainWindow处理
        qDebug() << "行驶速度:" << value << "mm/s";
        // 这里不需要单独处理，wordVariableChanged信号会触发MainWindow的处理
        emit wordVariableChanged(address, value);
    }
    else if (varName == "jog_displacement") {
        // 点动位移
        qDebug() << "点动位移:" << value << "mm";
        emit updateStatusLabel("label_jog_displacement", QString("%1 mm").arg(value));
        emit wordVariableChanged(address, value);
    }
    else if (varName == "battery1_charging") {
        // 电池1充电状态（1=充电中，其他值=非充电）
        qDebug() << "电池1充电状态:" << value;
        emit wordVariableChanged(address, value);
    }
    else if (varName.startsWith("fault_code_")) {
        // 故障代码处理
        QString faultName = varName.mid(11);  // 去掉"fault_code_"前缀
        QString displayText = QString("%1: 代码 %2").arg(faultName).arg(value);

        if (value != 0) {
            // 有故障代码
            m_faultCodes[address] = value;
            qDebug() << "故障代码" << faultName << ":" << value;
            qDebug() << "发现故障: " << displayText;
        } else {
            // 故障代码为0，表示无故障
            m_faultCodes.remove(address);
            qDebug() << "故障代码" << faultName << "已清除";
        }

        emit wordVariableChanged(address, value);
    }
    else {
        qDebug() << "未知的字变量类型:" << varName;
        emit wordVariableChanged(address, value);
    }

    qDebug() << "=== 字变量处理完成 ===";
}


// 在 agvmodbusmanager.cpp 中
QString AGVModbusManager::getBitVariableName(int address, int bitPos) const
{
    // 映射位变量地址和位置到变量名（对应UI中的label对象名）
    static QMap<int, QMap<int, QString>> bitMap = {
        {50, {
                 {0, "heartbeat"},                // 心跳（不显示）
                 {1, "label_front_touch"},        // 前触边
                 {2, "label_back_touch"},         // 后触边
                 {3, "label_left_touch"},         // 左触边
                 {4, "label_right_touch"},        // 右触边
                 {5, "label_front_slow"},         // 前避障减速
                 {6, "label_front_stop"},         // 前避障停止
                 {7, "label_back_slow"},          // 后避障减速
                 {8, "label_back_stop"},          // 后避障停止
                 {9, "label_jog_running"},        // 点动运行中
                 {10, "steering_reset"},          // 转向轮回正
                 {11, "lateral_steering_ready"},  // 横移模式转向轮到位
                 {12, "rotate_steering_ready"},   // 原地旋转模式转向轮到位
             }},
        {51, {
                 // 地址51的位变量（根据新地址码表，这里可能没有位变量）
             }},
        {102, {  // 注意：这里是102，不是52
                  {0, "low_battery"},              // 低电报警（合并显示）
                  {1, "comm_fault"},               // 通讯故障（合并显示）
                  {2, "drive_fault"},              // 驱动故障（合并显示）
              }}
    };

    if (bitMap.contains(address) && bitMap[address].contains(bitPos)) {
        return bitMap[address][bitPos];
    }

    return "";
}

QString AGVModbusManager::getWordVariableName(int address) const
{
    // 映射字变量地址到变量名
    static QMap<int, QString> wordMap = {
        {102, "battery1"},                   // 电池1电量（注意：同时是位变量）
        {103, "battery2"},                   // 电池2电量
        {104, "speed"},                      // 行驶速度
        {105, "jog_displacement"},           // 点动位移
        {156, "battery1_charging"},          // 电池1充电状态（1=充电中）
        {110, "fault_code_1"},               // 转向1故障代码
        {111, "fault_code_2"},               // 转向2故障代码
        {112, "fault_code_3"},               // 转向3故障代码
        {113, "fault_code_4"},               // 转向4故障代码
        {114, "fault_code_5"},               // 行走1故障代码
        {115, "fault_code_6"},               // 行走2故障代码
        {116, "fault_code_7"},               // 行走3故障代码
        {117, "fault_code_8"},               // 行走4故障代码
    };

    QString name = wordMap.value(address, "");
    qDebug() << "【getWordVariableName】地址" << address << "->" << name;
    return name;
}


void AGVModbusManager::updateFaultsDisplay()
{
    // 获取故障状态 - 注意：现在故障位在地址102
    bool lowBattery = m_bitStates.value(102).value(0, false);
    bool commFault = m_bitStates.value(102).value(1, false);
    bool driveFault = m_bitStates.value(102).value(2, false);

    // 更新故障状态
    m_faultStatus.lowBattery = lowBattery;
    m_faultStatus.commFault = commFault;
    m_faultStatus.driveFault = driveFault;

    // 构建显示文本
    QStringList faults;
    if (lowBattery) faults << "低电报警";
    if (commFault) faults << "通讯故障";
    if (driveFault) faults << "驱动故障";

    QString faultText;
    if (faults.isEmpty()) {
        faultText = "无故障";
    } else {
        faultText = "当前故障: " + faults.join(", ");
    }

    // 发送到UI
    emit updateFaultsLabel(faultText);

    // 调试输出
    if (!faults.isEmpty()) {
        qDebug() << "故障状态更新: " << faultText;
    }
}






void AGVModbusManager::updateFaultCodesDisplay()
{
    // 清空故障列表
    emit clearFaultCodes();

    // 如果没有故障代码
    if (m_faultCodes.isEmpty()) {
        emit addFaultCodeToList("无故障代码");
        qDebug() << "无故障代码";
        return;
    }

    // 添加所有非零故障代码
    for (auto it = m_faultCodes.begin(); it != m_faultCodes.end(); ++it) {
        int address = it.key();
        quint16 code = it.value();

        if (code != 0) {
            QString varName = getWordVariableName(address);
            if (varName.startsWith("fault_code_")) {
                QString faultName = varName.mid(11);  // 去掉"fault_code_"前缀
                QString displayText = QString("%1: 代码 %2")
                                          .arg(faultName)
                                          .arg(code);
                emit addFaultCodeToList(displayText);
                qDebug() << "添加故障代码到列表: " << displayText;
            }
        }
    }
}

// 添加写入函数
bool AGVModbusManager::writeSingleRegister(int address, quint16 value)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, address, value, &ok]() {
            ok = writeSingleRegister(address, value);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    // 如果全局禁用了写操作，则直接阻止并返回失败（用于故障排查）
    if (!m_writesEnabled) {
        qWarning() << "AGV 写操作已被禁用，忽略写入请求 地址:" << address << "值:" << value;
        return false;
    }

    if (!isConnected()) {
        if (!m_disconnectedWriteWarnedAddresses.contains(address)) {
            qWarning() << "AGV Modbus未连接，无法写入地址" << address;
            m_disconnectedWriteWarnedAddresses.insert(address);
        }
        return false;
    }

    m_disconnectedWriteWarnedAddresses.remove(address);

    if (!ModbusWriteGate::allowAgvWrite(ModbusThreadManager::instance())) {
        qWarning() << ModbusWriteGate::deniedReason() << "(AGV地址" << address << ")";
        emit teachingWriteGateDenied();
        emit writeCompleted(address, value, false);
        return false;
    }

    if (!m_backendWriteSingle || !m_dynamicBackendHandle) {
        qWarning() << "AGV动态库写失败: 未找到单写函数";
        emit writeCompleted(address, value, false);
        return false;
    }

    const bool ok = m_backendWriteSingle(m_dynamicBackendHandle, address, value) != 0;
    emit writeCompleted(address, value, ok);
    if (!ok) {
        qWarning() << "AGV动态库写失败 地址:" << address << "值:" << value;
        const QString reason = QStringLiteral("AGV动态库写入失败 address=%1").arg(address);
        handleCommunicationFailure(reason);
    }
    return ok;
}

bool AGVModbusManager::writeMultipleRegisters(int startAddress, const QVector<quint16> &values)
{
    if (QThread::currentThread() != thread()) {
        bool ok = false;
        QMetaObject::invokeMethod(this, [this, startAddress, values, &ok]() {
            ok = writeMultipleRegisters(startAddress, values);
        }, Qt::BlockingQueuedConnection);
        return ok;
    }

    if (!m_writesEnabled) {
        qWarning() << "AGV 写操作已被禁用，忽略批量写入 起始地址:" << startAddress << "字数:" << values.size();
        return false;
    }

    if (values.isEmpty()) {
        qWarning() << "AGV 批量写入拒绝: 空数据";
        return false;
    }

    if (!isConnected()) {
        qWarning() << "AGV Modbus未连接，无法批量写入起始地址" << startAddress;
        return false;
    }

    if (!ModbusWriteGate::allowAgvWrite(ModbusThreadManager::instance())) {
        qWarning() << ModbusWriteGate::deniedReason() << "(AGV批量写起始" << startAddress << ")";
        emit teachingWriteGateDenied();
        return false;
    }

    if (m_backendWriteMultiple && m_dynamicBackendHandle) {
        const int rc = m_backendWriteMultiple(m_dynamicBackendHandle,
                                              startAddress,
                                              values.constData(),
                                              static_cast<int>(values.size()));
        if (rc) {
            return true;
        }
        qWarning() << "AGV 动态库批量写失败，尝试按单寄存器依次写入 起始:" << startAddress;
    }

    for (int i = 0; i < values.size(); ++i) {
        if (!writeSingleRegister(startAddress + i, values.at(i))) {
            qWarning() << "AGV 批量写退化失败 地址:" << (startAddress + i);
            return false;
        }
    }
    return true;
}

void AGVModbusManager::setWritesEnabled(bool enabled)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enabled]() { setWritesEnabled(enabled); }, Qt::QueuedConnection);
        return;
    }
    m_writesEnabled = enabled;
}

bool AGVModbusManager::writesEnabled() const
{
    if (QThread::currentThread() != thread()) {
        bool enabled = false;
        QMetaObject::invokeMethod(const_cast<AGVModbusManager *>(this), [this, &enabled]() {
            enabled = m_writesEnabled;
        }, Qt::BlockingQueuedConnection);
        return enabled;
    }
    return m_writesEnabled;
}

void AGVModbusManager::setReadsEnabled(bool enabled)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, enabled]() { setReadsEnabled(enabled); }, Qt::QueuedConnection);
        return;
    }
    m_readsEnabled = enabled;
}

bool AGVModbusManager::readsEnabled() const
{
    if (QThread::currentThread() != thread()) {
        bool enabled = false;
        QMetaObject::invokeMethod(const_cast<AGVModbusManager *>(this), [this, &enabled]() {
            enabled = m_readsEnabled;
        }, Qt::BlockingQueuedConnection);
        return enabled;
    }
    return m_readsEnabled;
}

