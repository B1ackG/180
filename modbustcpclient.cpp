#include "modbustcpclient.h"
#include <QDebug>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcModbusTCPClient, "app.modbustcpclient")
#include <QHostAddress>
#include <algorithm>

ModbusTCPClient::ModbusTCPClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_networkThread(nullptr)
    , m_port(502)  // Modbus TCP默认端口
    , m_slaveId(1)
    , m_autoReconnect(false)
    , m_reconnectInterval(5000)
    , m_reconnectTimer(nullptr)
    , m_polling(false)
    , m_pollInterval(1000)  // 默认1秒轮询
    , m_pollTimer(nullptr)
    , m_transactionId(0)
{
    // 保持socket与当前对象在同一线程，避免跨线程访问
    m_socket = new QTcpSocket();

    // 连接信号槽
    connect(m_socket, &QTcpSocket::connected, this, &ModbusTCPClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ModbusTCPClient::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &ModbusTCPClient::onError);
    connect(m_socket, &QTcpSocket::readyRead, this, &ModbusTCPClient::onReadyRead);

    // 创建定时器（在正确的线程中）
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &ModbusTCPClient::pollRegisters);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ModbusTCPClient::tryReconnect);

}

ModbusTCPClient::~ModbusTCPClient()
{
    stopPolling();
    disconnectFromServer();

    if (m_networkThread && m_networkThread->isRunning()) {
        m_networkThread->quit();
        m_networkThread->wait();
    }
}

bool ModbusTCPClient::connectToServer(const QString &host, quint16 port, int slaveId)
{
    QMutexLocker locker(&m_mutex);

    m_host = host;
    m_port = port;
    m_slaveId = slaveId;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
    m_socket->connectToHost(host, port);

    return true;
}

void ModbusTCPClient::disconnectFromServer()
{
    QMutexLocker locker(&m_mutex);

    stopPolling();
    m_reconnectTimer->stop();
    m_connectedState = false;
    m_responseBuffer.clear();
    m_transactionAddressMap.clear();
    m_transactionMapMismatchLogged = false;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        m_socket->disconnectFromHost();
    }
}

bool ModbusTCPClient::isConnected() const
{
    // return m_socket->state() == QAbstractSocket::ConnectedState;
    return m_connectedState;
}

void ModbusTCPClient::onConnected()
{
    m_connectedState = true;
    m_responseBuffer.clear();
    m_transactionAddressMap.clear();
    m_transactionMapMismatchLogged = false;
    qCDebug(lcModbusTCPClient) << "Modbus TCP连接成功:" << m_host << ":" << m_port;
    emit connected();

    if (m_autoReconnect) {
        m_reconnectTimer->stop();
    }
}

void ModbusTCPClient::onDisconnected()
{
    m_connectedState = false;
    m_responseBuffer.clear();
    m_transactionAddressMap.clear();
    m_transactionMapMismatchLogged = false;
    qCDebug(lcModbusTCPClient) << "Modbus TCP连接断开";
    emit disconnected();

    if (m_autoReconnect && !m_host.isEmpty()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
}

void ModbusTCPClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        m_connectedState = false;
        m_responseBuffer.clear();
        m_transactionAddressMap.clear();
        m_transactionMapMismatchLogged = false;
    }
    QString errorStr = m_socket->errorString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool shouldReport = (errorStr != m_lastSocketError) || ((nowMs - m_lastSocketErrorMs) > 2000);
    if (shouldReport) {
        m_lastSocketError = errorStr;
        m_lastSocketErrorMs = nowMs;
        qWarning() << "Modbus TCP错误:" << errorStr;
        emit errorOccurred(errorStr);
    } else {
        qCDebug(lcModbusTCPClient) << "Modbus TCP错误(节流):" << errorStr;
    }
    
    // 如果是连接被拒绝或主机未找到等连接错误，且启用了自动重连，则重新启动重连定时器
    if (m_autoReconnect && !m_host.isEmpty()) {
        if (!m_reconnectTimer->isActive()) {
             qDebug() << "连接出错，" << m_reconnectInterval << "ms后尝试重连...";
             m_reconnectTimer->start(m_reconnectInterval);
        }
    }
    
}

void ModbusTCPClient::tryReconnect()
{
    if (m_autoReconnect && !m_host.isEmpty()) {
        qCDebug(lcModbusTCPClient) << "尝试重连Modbus TCP服务器...";
        connectToServer(m_host, m_port, m_slaveId);
    }
}

void ModbusTCPClient::setAutoReconnect(bool enable, int interval)
{
    m_autoReconnect = enable;
    m_reconnectInterval = interval;

    if (!enable) {
        m_reconnectTimer->stop();
    }
}

bool ModbusTCPClient::readHoldingRegisters(int startAddress, int count)
{
    return readRegisters(startAddress, count, 0x03);  // 使用0x03功能码
}
// 新增方法
bool ModbusTCPClient::readInputRegisters(int startAddress, int count)
{
    return readRegisters(startAddress, count, 0x04);  // 使用0x04功能码
}
// 通用读取方法
bool ModbusTCPClient::readRegisters(int startAddress, int count, quint8 functionCode)
{
    constexpr int kMaxPendingRequests = 256;

    if (!isConnected() || count <= 0 || count > 125) {
        return false;
    }

    if (m_transactionAddressMap.size() >= kMaxPendingRequests) {
        qWarning() << "Modbus待处理请求过多(" << m_transactionAddressMap.size()
                   << ")，丢弃本次读取请求，起始地址:" << startAddress;
        return false;
    }

    QByteArray request = createReadRequest(startAddress, count, functionCode);

    m_socket->write(request);

    return true;
}

bool ModbusTCPClient::writeSingleRegister(int address, quint16 value)
{
    if (!isConnected()) {
        return false;
    }

    QByteArray request = createWriteSingleRequest(address, value);

    m_socket->write(request);

    return true;
}

bool ModbusTCPClient::writeMultipleRegisters(int startAddress, const QVector<quint16> &values)
{
    if (!isConnected() || values.isEmpty() || values.size() > 123) {
        return false;
    }

    QByteArray request = createWriteMultipleRequest(startAddress, values);

    m_socket->write(request);

    return true;
}

QByteArray ModbusTCPClient::createReadRequest(int startAddress, int count, quint8 functionCode)
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
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x06));

    // 单元标识符 (1字节) - 从站ID
    request.append(static_cast<char>(m_slaveId & 0xFF));

    // 功能码 (1字节) - 使用参数传入的功能码
    request.append(static_cast<char>(functionCode));

    // 起始地址 (2字节)
    request.append(static_cast<char>((startAddress >> 8) & 0xFF));
    request.append(static_cast<char>(startAddress & 0xFF));

    // 寄存器数量 (2字节)
    request.append(static_cast<char>((count >> 8) & 0xFF));
    request.append(static_cast<char>(count & 0xFF));

    qCDebug(lcModbusTCPClient) << "发送读取请求 - 功能码: 0x" << QString::number(functionCode, 16).toUpper()
             << ", 起始地址:" << startAddress
             << ", 数量:" << count;

    return request;
}

QByteArray ModbusTCPClient::createWriteSingleRequest(int address, quint16 value)
{
    QByteArray request;

    // 事务标识符
    request.append(static_cast<char>((m_transactionId >> 8) & 0xFF));
    request.append(static_cast<char>(m_transactionId & 0xFF));
    m_transactionId++;

    // 协议标识符
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x00));

    // 长度
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x06));

    // 单元标识符
    request.append(static_cast<char>(m_slaveId & 0xFF));

    // 功能码 - 0x06 写单个寄存器
    request.append(static_cast<char>(0x06));

    // 地址
    request.append(static_cast<char>((address >> 8) & 0xFF));
    request.append(static_cast<char>(address & 0xFF));

    // 值
    request.append(static_cast<char>((value >> 8) & 0xFF));
    request.append(static_cast<char>(value & 0xFF));

    return request;
}

QByteArray ModbusTCPClient::createWriteMultipleRequest(int startAddress, const QVector<quint16> &values)
{
    QByteArray request;
    int byteCount = values.size() * 2;

    // 事务标识符
    request.append(static_cast<char>((m_transactionId >> 8) & 0xFF));
    request.append(static_cast<char>(m_transactionId & 0xFF));
    m_transactionId++;

    // 协议标识符
    request.append(static_cast<char>(0x00));
    request.append(static_cast<char>(0x00));

    // 长度 - 后面字节数
    int length = 7 + byteCount;
    request.append(static_cast<char>((length >> 8) & 0xFF));
    request.append(static_cast<char>(length & 0xFF));

    // 单元标识符
    request.append(static_cast<char>(m_slaveId & 0xFF));

    // 功能码 - 0x10 写多个寄存器
    request.append(static_cast<char>(0x10));

    // 起始地址
    request.append(static_cast<char>((startAddress >> 8) & 0xFF));
    request.append(static_cast<char>(startAddress & 0xFF));

    // 寄存器数量
    int regCount = values.size();
    request.append(static_cast<char>((regCount >> 8) & 0xFF));
    request.append(static_cast<char>(regCount & 0xFF));

    // 字节数
    request.append(static_cast<char>(byteCount & 0xFF));

    // 值
    for (quint16 value : values) {
        request.append(static_cast<char>((value >> 8) & 0xFF));
        request.append(static_cast<char>(value & 0xFF));
    }

    return request;
}

void ModbusTCPClient::onReadyRead()
{
    constexpr quint16 kMinMbapLength = 3;    // UnitId(1) + Function(1) + Data(>=1)
    constexpr quint16 kMaxMbapLength = 260;  // 保守上限，避免异常长度卡住解析

    const QByteArray chunk = m_socket->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    emit dataReceived(chunk);
    m_responseBuffer.append(chunk);

    bool parsedAnyFrame = false;

    while (m_responseBuffer.size() >= 7) {
        const quint16 protocolId = (static_cast<quint8>(m_responseBuffer[2]) << 8)
                                   | static_cast<quint8>(m_responseBuffer[3]);
        quint16 length = (static_cast<quint8>(m_responseBuffer[4]) << 8)
                         | static_cast<quint8>(m_responseBuffer[5]);

        if (protocolId != 0 || length < kMinMbapLength || length > kMaxMbapLength) {
            m_responseBuffer.remove(0, 1);
            continue;
        }

        int frameLength = 6 + length;

        if (frameLength < 9) {
            m_responseBuffer.remove(0, 1);
            continue;
        }

        if (m_responseBuffer.size() < frameLength) {
            break;
        }

        QByteArray frame = m_responseBuffer.left(frameLength);
        m_responseBuffer.remove(0, frameLength);

        if (parseResponse(frame)) {
            parsedAnyFrame = true;
        }
    }

    if (!parsedAnyFrame && m_responseBuffer.size() > 8192) {
        qWarning() << "Modbus响应缓冲区异常增长，执行保护性清理";
        m_responseBuffer.clear();
    }

    while (m_transactionAddressMap.size() > 2048) {
        m_transactionAddressMap.remove(m_transactionAddressMap.firstKey());
    }
}
bool ModbusTCPClient::parseResponse(const QByteArray &data)
{
    if (data.size() < 9) {
        qCDebug(lcModbusTCPClient) << "数据不足，无法解析";
        return false;
    }

    // 提取事务ID
    quint16 transactionId = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);

    // 提取长度字段
    quint16 length = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);

    // 计算完整帧长度
    int frameLength = 6 + length;

    if (data.size() < frameLength) {
        qCDebug(lcModbusTCPClient) << "数据不足，等待更多数据";
        return false;
    }

    // 提取PDU（从第7字节开始）
    QByteArray pdu = data.mid(7);

    if (pdu.size() < 1) {
        qCDebug(lcModbusTCPClient) << "PDU数据不足";
        return false;
    }

    quint8 functionCode = static_cast<quint8>(pdu[0]);
    qCDebug(lcModbusTCPClient) << "响应功能码: 0x" << QString::number(functionCode, 16).toUpper();

    // 检查异常响应
    if (functionCode & 0x80) {
        if (pdu.size() < 2) {
            emit errorOccurred("Modbus异常响应长度非法");
            return false;
        }
        quint8 errorCode = static_cast<quint8>(pdu[1]);
        QString errorMsg = QString("Modbus异常: 错误码 0x%1").arg(errorCode, 2, 16, QChar('0'));
        qWarning() << errorMsg;
        emit errorOccurred(errorMsg);
        return false;
    }

    // 处理功能码 0x03, 0x04, 0x06, 0x10 响应
    if (functionCode == 0x03 || functionCode == 0x04 || functionCode == 0x06 || functionCode == 0x10) {
        
        // 对于写操作响应(0x06, 0x10)，我们只记录日志，不需要解析数据
        if (functionCode == 0x06 || functionCode == 0x10) {
            // qCDebug(lcModbusTCPClient) << "收到写操作响应，功能码:" << QString::number(functionCode, 16);
            return true;
        }

        if (pdu.size() >= 3) {
            quint8 byteCount = static_cast<quint8>(pdu[1]);
            qCDebug(lcModbusTCPClient) << "字节数:" << byteCount << "寄存器数量:" << byteCount / 2;

            // 通过事务ID获取起始地址
            int startAddress = -1;
            if (m_transactionAddressMap.contains(transactionId)) {
                startAddress = m_transactionAddressMap.take(transactionId);
                m_transactionMapMismatchLogged = false;
                qCDebug(lcModbusTCPClient) << "批量读取响应 - 起始地址:" << startAddress;
            } else {
                // 容错处理：如果Map中只有一个元素，尝试使用之（兼容不规范设备）
                if (m_transactionAddressMap.size() == 1) {
                    transactionId = m_transactionAddressMap.keys().first();
                    startAddress = m_transactionAddressMap.take(transactionId);
                    if (!m_transactionMapMismatchLogged) {
                        qWarning() << "未找到事务ID" << transactionId << "对应的地址映射，启用容错模式";
                    }
                    m_transactionMapMismatchLogged = false;
                } else {
                     if (!m_transactionMapMismatchLogged) {
                         qWarning() << "未找到事务ID" << transactionId << "对应的地址映射";
                         qWarning() << "当前Map中有" << m_transactionAddressMap.size() << "个待处理请求，无法匹配";
                     }
                     m_transactionMapMismatchLogged = true;
                }
            }

            if (startAddress != -1 && byteCount > 0) {
                for (int i = 0; i < byteCount / 2; i++) {
                    int bytePos = 2 + i * 2;
                    if (bytePos + 1 >= pdu.size()) break;

                    quint16 value = (static_cast<quint8>(pdu[bytePos]) << 8) |
                                    static_cast<quint8>(pdu[bytePos + 1]);

                    int address = startAddress + i;

                    qCDebug(lcModbusTCPClient) << "寄存器地址:" << address
                             << "值:" << value
                             << "(0x" << QString::number(value, 16).toUpper() << ")";

                    // 更新寄存器值
                    updateRegisterValue(address, value);
                }
                return true;
            }
        }
    } else {
        qWarning() << "不支持的功能码: 0x" << QString::number(functionCode, 16).toUpper();
    }

    return false;
}

void ModbusTCPClient::parseSingleResponse(const QByteArray &response, quint16 transactionId)
{
    if (response.size() < 9) {
        return;
    }

    // 跳过MBAP头（7字节）
    QByteArray pdu = response.mid(7);

    if (pdu.size() >= 1) {
        quint8 functionCode = static_cast<quint8>(pdu[0]);
        qDebug() << "功能码: 0x" << QString::number(functionCode, 16).toUpper();

        // 检查是否是异常响应
        if (functionCode & 0x80) {
            // 异常响应
            quint8 errorCode = static_cast<quint8>(pdu[1]);
            QString errorMsg = QString("Modbus异常响应: 错误码 0x%1").arg(errorCode, 2, 16, QChar('0'));
            qWarning() << errorMsg;
            emit errorOccurred(errorMsg);
            return;
        }

        // 处理功能码 0x04（读输入寄存器）和 0x03（读保持寄存器）的响应
        if (functionCode == 0x04 || functionCode == 0x03) {
            if (pdu.size() >= 3) {
                quint8 byteCount = static_cast<quint8>(pdu[1]);
                qDebug() << "读寄存器响应(" << QString::number(functionCode, 16) 
                         << ") - 字节数:" << byteCount
                         << "寄存器数量:" << byteCount / 2;
                qDebug() << "数据部分(HEX):" << pdu.mid(2, byteCount).toHex().toUpper();

                // 通过事务ID映射找到对应的起始地址
                if (m_transactionAddressMap.contains(transactionId)) {
                    int startAddress = m_transactionAddressMap.take(transactionId);
                    m_transactionMapMismatchLogged = false;
                    int registerCount = byteCount / 2;  // 每个寄存器2字节

                    qDebug() << "批量读取 - 起始地址:" << startAddress
                             << "寄存器数量:" << registerCount;

                    // 解析所有寄存器值
                    for (int i = 0; i < registerCount; i++) {
                        int bytePos = 2 + i * 2;
                        if (bytePos + 1 >= pdu.size()) break;

                        quint16 value = (static_cast<quint8>(pdu[bytePos]) << 8) |
                                        static_cast<quint8>(pdu[bytePos + 1]);

                        int address = startAddress + i;

                        // qDebug() << "更新寄存器地址" << address
                        //          << "(&MB" << (address + 1) << ")"
                        //          << "的值为" << value
                        //          << "(0x" << QString::number(value, 16).toUpper() << ")";

                        // 更新寄存器值
                        updateRegisterValue(address, value);
                    }
                } else {
                    if (!m_transactionMapMismatchLogged) {
                        qWarning() << "未找到事务ID" << transactionId << "对应的地址映射";
                    }
                    m_transactionMapMismatchLogged = true;
                }
            }
        }
    }
}
void ModbusTCPClient::addRegisterToPoll(int address, const QString &name)
{
    QMutexLocker locker(&m_mutex);

    if (!m_pollList.contains(address)) {
        m_pollList.append(address);
    }

    if (!name.isEmpty()) {
        m_registerNames[address] = name;
    }
}

void ModbusTCPClient::removeRegisterFromPoll(int address)
{
    QMutexLocker locker(&m_mutex);
    m_pollList.removeAll(address);
    m_registerNames.remove(address);
}

void ModbusTCPClient::clearPollList()
{
    QMutexLocker locker(&m_mutex);
    m_pollList.clear();
    m_registerNames.clear();
}

void ModbusTCPClient::setPollInterval(int ms)
{
    m_pollInterval = ms;
    if (m_pollTimer->isActive()) {
        m_pollTimer->setInterval(ms);
    }
}

void ModbusTCPClient::startPolling()
{
    if (!m_polling && isConnected()) {
        m_polling = true;
        m_pollTimer->start(m_pollInterval);
    }
}

void ModbusTCPClient::stopPolling()
{
    m_polling = false;
    m_pollTimer->stop();
}

void ModbusTCPClient::pollRegisters()
{
    QList<int> pollListSnapshot;
    {
        QMutexLocker locker(&m_mutex);
        pollListSnapshot = m_pollList;
    }

    if (!isConnected() || pollListSnapshot.isEmpty()) {
        return;
    }

    std::sort(pollListSnapshot.begin(), pollListSnapshot.end());
    pollListSnapshot.erase(std::unique(pollListSnapshot.begin(), pollListSnapshot.end()), pollListSnapshot.end());

    constexpr int kMaxRegistersPerRequest = 120;
    int rangeStart = pollListSnapshot.first();
    int previousAddress = rangeStart;

    auto flushRange = [this](int start, int end) {
        int count = end - start + 1;
        if (count > 0) {
            readHoldingRegisters(start, count);
        }
    };

    for (int i = 1; i < pollListSnapshot.size(); ++i) {
        const int currentAddress = pollListSnapshot.at(i);
        const bool isContinuous = (currentAddress == previousAddress + 1);
        const bool exceedMaxCount = (currentAddress - rangeStart + 1) > kMaxRegistersPerRequest;

        if (!isContinuous || exceedMaxCount) {
            flushRange(rangeStart, previousAddress);
            rangeStart = currentAddress;
        }

        previousAddress = currentAddress;
    }

    flushRange(rangeStart, previousAddress);
}

void ModbusTCPClient::updateRegisterValue(int address, quint16 value)
{
    bool changed = true;
    QString registerName;

    {
        QMutexLocker locker(&m_mutex);

        auto it = m_registers.find(address);
        if (it != m_registers.end() && it.value().value == value) {
            changed = false;
        } else {
            ModbusRegister &reg = m_registers[address];
            reg.address = address;
            reg.value = value;
        }

        if (!changed) {
            return;
        }

        registerName = m_registerNames.value(address);
    }

    emit registerValueChanged(address, value);
    if (!registerName.isEmpty()) {
        emit registerValueChangedNamed(registerName, value);
    }
}

