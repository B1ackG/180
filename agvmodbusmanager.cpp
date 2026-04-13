// file name: agvmodbusmanager.cpp
#include "agvmodbusmanager.h"
#include "featureswitchmanager.h"
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <QMetaObject>

namespace {
bool isAgvReadLogEnabled()
{
    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    return featureSwitch && featureSwitch->isFeatureEnabled("modbus_agv", "modbus_agv.read_logs");
}

bool isAgvWriteLogEnabled()
{
    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    return featureSwitch && featureSwitch->isFeatureEnabled("modbus_agv", "modbus_agv.write_logs");
}
} // namespace

AGVModbusManager::AGVModbusManager(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_networkThread(nullptr)
    , m_host("192.168.1.88")
    , m_port(502)
    , m_autoReconnect(true)
    , m_reconnectInterval(5000)
    , m_reconnectTimer(nullptr)
    , m_pollTimer(nullptr)
    , m_pollInterval(200)  // 默认200ms
    , m_transactionId(0)
{
    // 注册元类型
    qRegisterMetaType<QAbstractSocket::SocketError>("QAbstractSocket::SocketError");
    // 保持socket与当前对象同线程，避免跨线程访问
    m_socket = new QTcpSocket(this);

    // 连接信号槽
    connect(m_socket, &QTcpSocket::connected, this, &AGVModbusManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &AGVModbusManager::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &AGVModbusManager::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &AGVModbusManager::onReadyRead);

    // 创建定时器
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

    qDebug() << "AGV Modbus管理器已销毁";
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

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    qDebug() << "AGV Modbus连接请求，主机:" << host << "端口:" << port;
    m_socket->connectToHost(host, port);

    return true;
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
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool AGVModbusManager::isConnected() const
{
    if (QThread::currentThread() != thread()) {
        bool connected = false;
        QMetaObject::invokeMethod(const_cast<AGVModbusManager *>(this), [this, &connected]() {
            connected = m_connectedState;
        }, Qt::BlockingQueuedConnection);
        return connected;
    }

    return m_connectedState;
}

void AGVModbusManager::onConnected()
{
    m_connectedState = true;
    m_lastSocketError.clear();
    m_disconnectedWriteWarnedAddresses.clear();
    qDebug() << "AGV Modbus连接成功:" << m_host << ":" << m_port;

    // 已禁用开机自动轮询
    // 启动轮询定时器以定期读取关键寄存器
    if (m_pollTimer) {
        m_pollTimer->start(m_pollInterval);
        qDebug() << "轮询定时器已启动，间隔:" << m_pollInterval << "ms";
    }

    // 停止重连定时器
    if (m_autoReconnect && m_reconnectTimer) {
        m_reconnectTimer->stop();
    }

    emit connected();
    emit updateStatusLabel("label_agv_connection", "已连接");
}

void AGVModbusManager::onDisconnected()
{
    m_connectedState = false;
    qDebug() << "AGV Modbus连接断开";

    // 停止轮询
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }

    // 启动重连
    if (m_autoReconnect && !m_host.isEmpty()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }

    emit disconnected();
    emit updateStatusLabel("label_agv_connection", "未连接");  // 修改标签名
}

void AGVModbusManager::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_connectedState = false;
    }
    QString errorStr = m_socket->errorString();
    const bool shouldReport = (errorStr != m_lastSocketError);
    if (shouldReport) {
        m_lastSocketError = errorStr;
        qWarning() << "AGV Modbus错误:" << errorStr;
    } else {
        qDebug() << "AGV Modbus错误(已抑制重复):" << errorStr;
    }

    // 如果发生连接错误且启用自动重连，则启动重连定时器
    if (m_autoReconnect && !m_host.isEmpty()) {
        if (m_reconnectTimer && !m_reconnectTimer->isActive()) {
            qDebug() << "连接出错，" << m_reconnectInterval << "ms后尝试重连...";
            m_reconnectTimer->start(m_reconnectInterval);
        }
    }

    if (shouldReport) {
        emit errorOccurred(errorStr);
        emit updateStatusLabel("label_agv_connection", "连接错误: " + errorStr);  // 修改标签名
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

    // 分组轮询，避免一次读取过大导致阻塞，同时保证所有AGV UI所需地址都能被覆盖。
    // 组0: 0    (OA/驻车相关控制位)
    // 组1: 2    (底盘切换模式值)
    // 组2: 50-51 (触边/避障/心跳位状态 + 驻车状态位)
    // 组3: 102-105 (电池1、电池2、速度、点动位移)
    // 组4: 110-113 (故障码1-4)
    // 组5: 114-117 (故障码5-8)
    // 组6: 100  (有线/无线控制模式)
    // 组7: 153-154 (运动速度/转向角度同步)
    static int pollGroup = 0;

    switch (pollGroup) {
    case 0:
        readMultipleRegisters(0, 1);
        break;
    case 1:
        readMultipleRegisters(2, 1);
        break;
    case 2:
        readMultipleRegisters(50, 2);
        break;
    case 3:
        readMultipleRegisters(102, 4);
        break;
    case 4:
        readMultipleRegisters(110, 4);
        break;
    case 5:
        readMultipleRegisters(114, 4);
        break;
    case 6:
        readMultipleRegisters(100, 1);
        break;
    case 7:
        readMultipleRegisters(153, 2);
        break;
    default:
        break;
    }

    pollGroup = (pollGroup + 1) % 8;
}






QByteArray AGVModbusManager::createReadRequest(int startAddress, int count)
{
    QByteArray request;

    // 事务标识符 (2字节)
    request.append(static_cast<char>((m_transactionId >> 8) & 0xFF));
    request.append(static_cast<char>(m_transactionId & 0xFF));

    // 记录事务ID到起始地址的映射
    m_transactionAddressMap[m_transactionId] = startAddress;
    m_transactionId++;

    // 协议标识符 (2字节) - Modbus = 0
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x00));

    // 长度 (2字节) - 后面字节数
    int length = 6;  // 单元标识符1 + 功能码1 + 起始地址2 + 寄存器数量2
    request.append(static_cast<char>((length >> 8) & 0xFF));
    request.append(static_cast<char>(length & 0xFF));

    // 单元标识符 (1字节) - 从站ID
    request.append(static_cast<char>(1));  // 默认从站ID为1

    // 功能码 (1字节) - 0x03 读保持寄存器
    request.append(static_cast<char>(0x03));

    // 起始地址 (2字节)
    request.append(static_cast<char>((startAddress >> 8) & 0xFF));
    request.append(static_cast<char>(startAddress & 0xFF));

    // 寄存器数量 (2字节)
    request.append(static_cast<char>((count >> 8) & 0xFF));
    request.append(static_cast<char>(count & 0xFF));

    return request;
}

void AGVModbusManager::readMultipleRegisters(int startAddress, int count)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, startAddress, count]() {
            readMultipleRegisters(startAddress, count);
        }, Qt::QueuedConnection);
        return;
    }

    if (!isConnected() || count <= 0 || count > 125) {
        return;
    }

    // 清理过期的请求记录
    QDateTime now = QDateTime::currentDateTime();
    QMutableMapIterator<quint16, QDateTime> it(m_requestTimestamps);
    while (it.hasNext()) {
        it.next();
        if (it.value().msecsTo(now) > REQUEST_TIMEOUT) {
            qDebug() << "清理超时请求，事务ID:" << it.key();
            m_transactionAddressMap.remove(it.key());
            it.remove();
        }
    }

    // 限制在途请求数量，避免网络异常时无限堆积
    constexpr int kMaxPendingRequests = 32;
    if (m_transactionAddressMap.size() >= kMaxPendingRequests) {
        qWarning() << "AGV在途请求过多，跳过本次轮询发送。pending=" << m_transactionAddressMap.size();
        return;
    }

    QByteArray request = createReadRequest(startAddress, count);

    // 记录请求时间
    quint16 requestId = m_transactionId - 1;  // 注意：createReadRequest中已经增加了m_transactionId
    m_requestTimestamps[requestId] = QDateTime::currentDateTime();

    if (isAgvReadLogEnabled()) {
        qInfo().noquote() << QString("[AGV Modbus TX] ReqID:%1 FC:0x03 Addr:%2 Count:%3 Hex:%4")
                                 .arg(requestId)
                                 .arg(startAddress)
                                 .arg(count)
                                 .arg(QString::fromLatin1(request.toHex(' ')));
    }

    m_socket->write(request);
}

void AGVModbusManager::onReadyRead()
{
    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    m_responseBuffer.append(chunk);
    parseResponse(m_responseBuffer);
}

bool AGVModbusManager::parseResponse(QByteArray &data)
{
    bool parsedAny = false;
    while (data.size() >= 7) {
        quint16 length = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
        int frameLength = 6 + length;

        if (frameLength < 9) {
            qWarning() << "非法AGV Modbus帧长度:" << frameLength;
            data.clear();
            emit errorOccurred("非法AGV Modbus帧长度");
            return false;
        }

        if (data.size() < frameLength) {
            break;
        }

        if (!parseSingleResponseFrame(data)) {
            qWarning() << "解析AGV Modbus响应帧失败";
            return false;
        }
        parsedAny = true;
    }
    return parsedAny;
}

bool AGVModbusManager::parseSingleResponseFrame(QByteArray &data)
{
    if (data.size() < 9) {
        return false;
    }

    // 提取事务ID
    quint16 transactionId = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);

    // 提取长度字段（偏移4-5字节）
    quint16 length = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);

    // 计算完整帧长度
    int frameLength = 6 + length;  // MBAP头6字节 + 长度字段的值

    if (data.size() < frameLength) {
        return false;
    }

    // 提取完整的帧
    QByteArray frame = data.left(frameLength);
    data.remove(0, frameLength);  // 移除已处理的数据

    // 解析这个帧
    return processSingleResponseFrame(frame, transactionId);
}

bool AGVModbusManager::processSingleResponseFrame(const QByteArray &frame, quint16 transactionId)
{

    // 跳过MBAP头（7字节）
    QByteArray pdu = frame.mid(7);

    if (pdu.size() < 1) {
        qWarning() << "PDU数据不足";
        return false;
    }

    quint8 functionCode = static_cast<quint8>(pdu[0]);

    const bool isReadFrame = (functionCode == 0x03 || functionCode == 0x04);
    const bool isWriteFrame = (functionCode == 0x05 || functionCode == 0x06 || functionCode == 0x10);
    if ((isReadFrame && isAgvReadLogEnabled()) || (isWriteFrame && isAgvWriteLogEnabled())) {
        qInfo().noquote() << QString("[AGV Modbus RX %1] ReqID:%2 FC:0x%3 Len:%4 Hex:%5")
                                 .arg(isWriteFrame ? "WRITE" : "READ")
                                 .arg(transactionId)
                                 .arg(functionCode, 2, 16, QChar('0'))
                                 .arg(frame.size())
                                 .arg(QString::fromLatin1(frame.toHex(' ')));
    }

    // 检查异常响应
    if (functionCode & 0x80) {
        quint8 errorCode = static_cast<quint8>(pdu[1]);
        QString errorMsg = QString("Modbus异常: 错误码 0x%1").arg(errorCode, 2, 16, QChar('0'));
        qWarning() << errorMsg;
        m_transactionAddressMap.remove(transactionId);
        m_requestTimestamps.remove(transactionId);
        emit errorOccurred(errorMsg);
        return false;
    }

    // 处理功能码 0x03（读保持寄存器）响应
    if (functionCode == 0x03) {
        if (!m_transactionAddressMap.contains(transactionId)) {
            // 请求已超时清理或收到过期/乱序响应，直接忽略该帧，避免错误映射到地址0。
            m_requestTimestamps.remove(transactionId);
            qWarning() << "收到无映射事务ID的读响应，已忽略 transactionId=" << transactionId;
            return true;
        }

        int startAddress = m_transactionAddressMap.take(transactionId);
        m_requestTimestamps.remove(transactionId);

        // 字节数
        quint8 byteCount = static_cast<quint8>(pdu[1]);

        // 寄存器数量
        int registerCount = byteCount / 2;

        for (int i = 0; i < registerCount; i++) {
            int dataIndex = 2 + i * 2;
            if (dataIndex + 1 >= pdu.size()) {
                qWarning() << "数据不足，无法解析寄存器";
                break;
            }

            quint16 value = (static_cast<quint8>(pdu[dataIndex]) << 8) |
                            static_cast<quint8>(pdu[dataIndex + 1]);

            int address = startAddress + i;

            // 发射寄存器值变化信号
            emit registerValueChanged(address, value);

                // 根据地址范围处理数据
            if (address >= 50 && address <= 51) {
                // 位变量区域（50-51）
                processBitVariables(address, value);
            } else if (address >= 102 && address <= 117) {
                // 这个区域既有位变量又有字变量
                if (address == 102) {
                    // 地址102：先处理位变量（故障状态）
                    processBitVariables(address, value);
                    // 然后处理字变量（电池1电量）
                    processWordVariables(address, value);
                } else if (address == 103) {
                    // 地址103：电池2电量
                    processWordVariables(address, value);
                } else {
                    // 其他地址：直接处理字变量
                    processWordVariables(address, value);
                }
            }
        }
        return true;
    }
    // 添加功能码 0x06（写单个寄存器）响应处理
    else if (functionCode == 0x06) {
        m_transactionAddressMap.remove(transactionId);
        m_requestTimestamps.remove(transactionId);

        if (pdu.size() >= 5) {
            // 提取写入的地址
            int writtenAddress = (static_cast<quint8>(pdu[1]) << 8) |
                                 static_cast<quint8>(pdu[2]);

            // 提取写入的值
            quint16 writtenValue = (static_cast<quint8>(pdu[3]) << 8) |
                                   static_cast<quint8>(pdu[4]);

            if (isAgvWriteLogEnabled()) {
                qInfo().noquote() << QString("[AGV Modbus ACK] ReqID:%1 FC:0x06 Addr:%2 Value:%3")
                                         .arg(transactionId)
                                         .arg(writtenAddress)
                                         .arg(writtenValue);
            }

            // 发出写入完成信号
            emit writeCompleted(writtenAddress, writtenValue, true);

            return true;
        }
    }
    else {
        qWarning() << "不支持的功能码: 0x" << QString::number(functionCode, 16).toUpper();
    }

    return false;
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
    else if (varName.startsWith("fault_code_")) {
        // 故障代码处理
        QString faultName = varName.mid(11);  // 去掉"fault_code_"前缀
        QString displayText = QString("%1: 代码 %2").arg(faultName).arg(value);

        if (value != 0) {
            // 有故障代码
            m_faultCodes[address] = value;
            qDebug() << "故障代码" << faultName << ":" << value;
            qDebug() << "发出addFaultCodeToList信号:" << displayText;

            // 发射信号到UI
            emit addFaultCodeToList(displayText);

            // 记录到日志
            qDebug() << "发现故障: " << displayText;
        } else {
            // 故障代码为0，表示无故障
            m_faultCodes.remove(address);
            qDebug() << "故障代码" << faultName << "已清除";
        }

        // 更新故障代码显示
        updateFaultCodesDisplay();
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

    QByteArray request = createWriteRequest(address, value);

    // 记录请求时间
    quint16 requestId = m_transactionId - 1;
    m_requestTimestamps[requestId] = QDateTime::currentDateTime();

    if (isAgvWriteLogEnabled()) {
        qInfo().noquote() << QString("[AGV Modbus TX] ReqID:%1 FC:0x06 Addr:%2 Value:%3 Hex:%4")
                                 .arg(requestId)
                                 .arg(address)
                                 .arg(value)
                                 .arg(QString::fromLatin1(request.toHex(' ')));
    }

    m_socket->write(request);

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

QByteArray AGVModbusManager::createWriteRequest(int address, quint16 value)
{
    QByteArray request;

    // 事务标识符 (2字节)
    request.append(static_cast<char>((m_transactionId >> 8) & 0xFF));
    request.append(static_cast<char>(m_transactionId & 0xFF));

    // 记录事务ID
    m_transactionAddressMap[m_transactionId] = address;
    m_transactionId++;

    // 协议标识符 (2字节) - Modbus = 0
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x00));

    // 长度 (2字节) - 后面字节数
    int length = 6;  // 单元标识符1 + 功能码1 + 地址2 + 值2
    request.append(static_cast<char>((length >> 8) & 0xFF));
    request.append(static_cast<char>(length & 0xFF));

    // 单元标识符 (1字节) - 从站ID
    request.append(static_cast<char>(1));  // 默认从站ID为1

    // 功能码 (1字节) - 0x06 写单个寄存器
    request.append(static_cast<char>(0x06));

    // 寄存器地址 (2字节)
    request.append(static_cast<char>((address >> 8) & 0xFF));
    request.append(static_cast<char>(address & 0xFF));

    // 寄存器值 (2字节)
    request.append(static_cast<char>((value >> 8) & 0xFF));
    request.append(static_cast<char>(value & 0xFF));

    return request;
}
