#include "modbustcpclient.h"
#include "modbus_backend_c.h"
#include <QDebug>
#include <QLoggingCategory>
Q_LOGGING_CATEGORY(lcModbusTCPClient, "app.modbustcpclient")
#include <algorithm>

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

bool registerRangeValid(int start, int count)
{
    return start >= 0 && count > 0 && start <= 65535 && count - 1 <= 65535 - start;
}

} // namespace

ModbusTCPClient::ModbusTCPClient(QObject *parent)
    : IModbusClient(parent)
    , m_networkThread(nullptr)
    , m_port(502)  // Modbus TCP默认端口
    , m_slaveId(1)
    , m_autoReconnect(false)
    , m_reconnectInterval(5000)
    , m_reconnectTimer(nullptr)
    , m_polling(false)
    , m_pollInterval(1000)  // 默认1秒轮询
    , m_pollTimer(nullptr)
{
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
    unloadBackend();

    if (m_networkThread && m_networkThread->isRunning()) {
        m_networkThread->quit();
        m_networkThread->wait();
    }
}

bool ModbusTCPClient::ensureBackendLoaded()
{
    if (m_backendLoadAttempted) {
        return m_backendReady;
    }
    m_backendLoadAttempted = true;
    m_backendMode = processModbusBackendMode();

    const bool bound = (m_backendMode == ModbusBackendMode::Static)
                           ? bindStaticBackend()
                           : loadDynamicBackend();
    if (!bound) {
        return false;
    }

    m_dynamicBackendHandle = m_backendCreate ? m_backendCreate() : nullptr;
    if (!m_dynamicBackendHandle) {
        m_lastDynamicBackendError = (m_backendMode == ModbusBackendMode::Static)
                                        ? QStringLiteral("静态链接后端初始化失败")
                                        : QStringLiteral("动态库创建 backend 句柄失败");
        qWarning() << m_lastDynamicBackendError;
        unloadBackend();
        return false;
    }

    m_backendReady = true;
    if (m_backendMode == ModbusBackendMode::Static) {
        qInfo() << "Modbus 静态链接后端已启用";
    } else {
        qInfo() << "Modbus 动态库后端已启用:" << m_dynamicBackendPath;
    }
    return true;
}

bool ModbusTCPClient::loadDynamicBackend()
{
    m_dynamicBackendPath = qEnvironmentVariable("MODBUS_BACKEND_LIB").trimmed();
    if (m_dynamicBackendPath.isEmpty()) {
        m_lastDynamicBackendError = QStringLiteral("未设置MODBUS_BACKEND_LIB");
        return false;
    }

    m_dynamicBackendLibrary.setFileName(m_dynamicBackendPath);
    if (!m_dynamicBackendLibrary.load()) {
        m_lastDynamicBackendError = QStringLiteral("加载动态库失败: %1")
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
        unloadBackend();
        return false;
    }

    m_backendReadInput = reinterpret_cast<MbReadRegistersFn>(
        m_dynamicBackendLibrary.resolve("modbus_backend_read_input_registers"));
    m_backendWriteMultiple = reinterpret_cast<MbWriteMultipleFn>(
        m_dynamicBackendLibrary.resolve("modbus_backend_write_multiple_registers"));
    return true;
}

bool ModbusTCPClient::bindStaticBackend()
{
    m_backendCreate = &modbus_backend_create;
    m_backendDestroy = &modbus_backend_destroy;
    m_backendConnect = &modbus_backend_connect;
    m_backendDisconnect = &modbus_backend_disconnect;
    m_backendIsConnected = &modbus_backend_is_connected;
    m_backendReadHolding = &modbus_backend_read_holding_registers;
    m_backendReadInput = &modbus_backend_read_input_registers;
    m_backendWriteSingle = &modbus_backend_write_single_register;
    m_backendWriteMultiple = &modbus_backend_write_multiple_registers;
    if (!m_backendCreate || !m_backendDestroy || !m_backendConnect || !m_backendDisconnect
        || !m_backendIsConnected || !m_backendReadHolding || !m_backendWriteSingle) {
        m_lastDynamicBackendError = QStringLiteral("静态链接后端缺少必需接口");
        return false;
    }
    return true;
}

void ModbusTCPClient::unloadBackend()
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
    m_backendReady = false;

    if (m_backendMode == ModbusBackendMode::Dynamic && m_dynamicBackendLibrary.isLoaded()) {
        m_dynamicBackendLibrary.unload();
    }
}

bool ModbusTCPClient::connectToServer(const QString &host, quint16 port, int slaveId)
{
    QMutexLocker locker(&m_mutex);

    m_host = host;
    m_port = port;
    m_slaveId = slaveId;

    if (!ensureBackendLoaded()) {
        QString err;
        if (m_backendMode == ModbusBackendMode::Static) {
            err = QStringLiteral("未初始化Modbus静态链接后端");
        } else {
            err = QStringLiteral("未加载Modbus官方动态库，请设置MODBUS_BACKEND_LIB");
        }
        if (!m_lastDynamicBackendError.isEmpty()) {
            err += QStringLiteral("：") + m_lastDynamicBackendError;
        }
        qWarning() << err;
        emit errorOccurred(err);
        return false;
    }

    const int rc = m_backendConnect(m_dynamicBackendHandle,
                                    host.toUtf8().constData(),
                                    static_cast<int>(port),
                                    slaveId);
    m_connectedState = (rc != 0);
    ++m_sessionGeneration;
    if (m_connectedState) {
        emit connected();
        if (m_autoReconnect) {
            m_reconnectTimer->stop();
        }
        qInfo() << "[Modbus连接] 动态库后端连接成功"
                << host << ":" << port << "SlaveID:" << slaveId;
        return true;
    }
    const QString err = QStringLiteral("动态库后端连接失败 host=%1 port=%2 slave=%3")
                            .arg(host)
                            .arg(port)
                            .arg(slaveId);
    qWarning() << err;
    emit errorOccurred(err);
    if (m_autoReconnect && !m_host.isEmpty() && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
    return false;
}

void ModbusTCPClient::disconnectFromServer()
{
    QMutexLocker locker(&m_mutex);

    stopPolling();
    m_reconnectTimer->stop();
    m_connectedState = false;
    ++m_sessionGeneration;

    if (m_backendDisconnect && m_dynamicBackendHandle) {
        m_backendDisconnect(m_dynamicBackendHandle);
    }
    emit disconnected();
}

bool ModbusTCPClient::isConnected() const
{
    if (!m_connectedState) {
        return false;
    }
    if (m_backendIsConnected && m_dynamicBackendHandle) {
        return m_backendIsConnected(m_dynamicBackendHandle) != 0;
    }
    return false;
}

void ModbusTCPClient::handleCommunicationFailure(const QString &reason)
{
    bool shouldEmitDisconnected = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_connectedState) {
            m_connectedState = false;
            ++m_sessionGeneration;
            if (m_backendDisconnect && m_dynamicBackendHandle) {
                m_backendDisconnect(m_dynamicBackendHandle);
            }
            shouldEmitDisconnected = true;
        }
    }

    if (shouldEmitDisconnected) {
        emit disconnected();
    }

    emit errorOccurred(reason);

    if (m_autoReconnect && !m_host.isEmpty() && !m_reconnectTimer->isActive()) {
        qWarning() << "[Modbus通信中断] 启动自动重连，原因:" << reason;
        m_reconnectTimer->start(m_reconnectInterval);
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
    if (!registerRangeValid(startAddress, count) || count > 125 || !isConnected()) {
        return false;
    }
    m_activeReadSession = m_sessionGeneration;

    MbReadRegistersFn readFn = nullptr;
    if (functionCode == 0x03) {
        readFn = m_backendReadHolding;
    } else if (functionCode == 0x04) {
        readFn = m_backendReadInput ? m_backendReadInput : m_backendReadHolding;
    }
    if (!readFn || !m_dynamicBackendHandle) {
        qWarning() << "[Modbus动态库读失败] 未找到读取函数";
        return false;
    }

    QVector<quint16> values(count);
    const int readCount = readFn(m_dynamicBackendHandle,
                                 startAddress,
                                 count,
                                 values.data(),
                                 values.size());
    if (m_activeReadSession != m_sessionGeneration) {
        return false;
    }
    if (readCount <= 0) {
        const QString reason = QStringLiteral("动态库读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        qWarning() << "[Modbus动态库读失败] 地址:" << startAddress << "数量:" << count;
        handleCommunicationFailure(reason);
        return false;
    }

    const int actualCount = qMin(readCount, count);
    for (int i = 0; i < actualCount; ++i) {
        updateRegisterValue(startAddress + i, values.at(i));
    }
    return true;
}

bool ModbusTCPClient::writeSingleRegister(int address, quint16 value)
{
    if (!registerRangeValid(address, 1)) {
        return false;
    }
    if (!isConnected()) {
        qWarning() << "[Modbus写失败] 未连接到服务器";
        return false;
    }

    if (!m_backendWriteSingle || !m_dynamicBackendHandle) {
        qWarning() << "[Modbus动态库写失败] 未找到单写函数";
        return false;
    }
    const bool ok = m_backendWriteSingle(m_dynamicBackendHandle, address, value) != 0;
    if (!ok) {
        qWarning() << "[Modbus动态库写失败] 地址:" << address << "值:" << value;
        const QString reason = QStringLiteral("动态库写入失败 address=%1").arg(address);
        handleCommunicationFailure(reason);
    }
    return ok;
}

bool ModbusTCPClient::writeMultipleRegisters(int startAddress, const QVector<quint16> &values)
{
    if (!registerRangeValid(startAddress, values.size()) || values.size() > 123 || !isConnected()) {
        return false;
    }

    if (m_backendWriteMultiple && m_dynamicBackendHandle) {
        const bool ok = m_backendWriteMultiple(m_dynamicBackendHandle,
                                               startAddress,
                                               values.constData(),
                                               values.size()) != 0;
        if (!ok) {
            const QString reason = QStringLiteral("动态库批量写入失败 start=%1 count=%2")
                                       .arg(startAddress)
                                       .arg(values.size());
            handleCommunicationFailure(reason);
        }
        return ok;
    }
    for (int i = 0; i < values.size(); ++i) {
        if (!writeSingleRegister(startAddress + i, values.at(i))) {
            return false;
        }
    }
    return true;
}

bool ModbusTCPClient::readHoldingRegisterSync(int address, quint16 &value)
{
    QVector<quint16> values;
    if (!readHoldingRegistersSync(address, 1, values) || values.isEmpty()) {
        return false;
    }
    value = values.first();
    return true;
}

bool ModbusTCPClient::readHoldingRegistersSync(int startAddress, int count, QVector<quint16> &values)
{
    values.clear();
    if (!registerRangeValid(startAddress, count) || count > 125 || !isConnected()) {
        return false;
    }
    m_activeReadSession = m_sessionGeneration;

    if (!m_backendReadHolding || !m_dynamicBackendHandle) {
        qWarning() << "[Modbus动态库读失败] 未找到读取函数";
        return false;
    }

    values.resize(count);
    const int readCount = m_backendReadHolding(m_dynamicBackendHandle,
                                               startAddress,
                                               count,
                                               values.data(),
                                               values.size());
    if (m_activeReadSession != m_sessionGeneration) {
        values.clear();
        return false;
    }
    if (readCount <= 0) {
        const QString reason = QStringLiteral("动态库同步读取失败 address=%1 count=%2")
                                   .arg(startAddress)
                                   .arg(count);
        qWarning() << "[Modbus动态库读失败] 地址:" << startAddress << "数量:" << count;
        handleCommunicationFailure(reason);
        values.clear();
        return false;
    }

    const int actualCount = qMin(readCount, count);
    values.resize(actualCount);
    for (int i = 0; i < actualCount; ++i) {
        updateRegisterValue(startAddress + i, values.at(i));
    }
    return true;
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
    if (m_activeReadSession != 0 && m_activeReadSession != m_sessionGeneration) {
        return;
    }
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

