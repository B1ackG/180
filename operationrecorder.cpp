#include "operationrecorder.h"

#include "mappingconfig.h"
#include "modbuswritegate.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QSettings>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#include <QSaveFile>

namespace {
QStringList parseCsvList(const QString &raw)
{
    QStringList result;
    const QStringList parts = raw.split(',', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        const QString trimmed = p.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    return result;
}
}

OperationRecorder::OperationRecorder(QObject *parent)
    : QObject{parent}
    , m_tcpSocket(nullptr)
    , m_tcpEnabled(false)
    , m_tcpServerIp(WIN7_IP)
    , m_tcpServerPort(WIN7_PORT)
    , m_reconnectTimer(nullptr)
{
    // 初始化TCP传输（仅明文TCP）
    m_tcpSocket = new QTcpSocket(this);
    loadSecuritySettings();

    // 连接TCP信号
    connect(m_tcpSocket, &QTcpSocket::connected, this, &OperationRecorder::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &OperationRecorder::onTcpDisconnected);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &OperationRecorder::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &OperationRecorder::onTcpDataWritten);

    // 初始化重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(kTcpReconnectIntervalMs);
    connect(m_reconnectTimer, &QTimer::timeout, this, &OperationRecorder::onReconnectTimeout);

    m_connectTimeoutTimer = new QTimer(this);
    m_connectTimeoutTimer->setSingleShot(true);
    m_connectTimeoutTimer->setInterval(kTcpConnectTimeoutMs);
    connect(m_connectTimeoutTimer, &QTimer::timeout, this, [this]() {
        if (!m_tcpSocket || m_tcpSocket->state() != QAbstractSocket::ConnectingState) {
            return;
        }
        qWarning() << "连接日志服务器超时:" << m_tcpServerIp << ":" << m_tcpServerPort;
        m_tcpSocket->abort();
        emit tcpTransmissionError(QStringLiteral("连接日志服务器超时"));
        scheduleReconnect();
    });

    loadSpoolIntoQueue();
}

void OperationRecorder::loadSecuritySettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("OperationLogTransport");

    const QByteArray allowedHostsEnv = qgetenv("AGV_LOG_ALLOWED_HOSTS");
    if (!allowedHostsEnv.isEmpty()) {
        m_allowedHosts = parseCsvList(QString::fromUtf8(allowedHostsEnv));
    } else {
        m_allowedHosts = parseCsvList(settings.value("allowed_hosts", "127.0.0.1,192.168.1.70").toString());
    }

    const QByteArray signKeyEnv = qgetenv("AGV_LOG_SIGNING_KEY");
    if (!signKeyEnv.isEmpty()) {
        m_signingKey = signKeyEnv;
    } else {
        m_signingKey = settings.value("signing_key", "").toByteArray();
    }
    settings.endGroup();
}

void OperationRecorder::ensureAllowedHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.isEmpty() || !qgetenv("AGV_LOG_ALLOWED_HOSTS").isEmpty()) {
        return;
    }

    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("OperationLogTransport");
    QStringList allowed = parseCsvList(
        settings.value("allowed_hosts", "127.0.0.1,192.168.1.70").toString());
    if (!allowed.contains(trimmed)) {
        allowed.append(trimmed);
        settings.setValue("allowed_hosts", allowed.join(QLatin1Char(',')));
        settings.sync();
    }
    settings.endGroup();
}

bool OperationRecorder::validateTransportPolicy(QString *reason) const
{
    if (!m_allowedHosts.isEmpty() && !m_allowedHosts.contains(m_tcpServerIp)) {
        if (reason) {
            *reason = QString("目标主机不在白名单中: %1").arg(m_tcpServerIp);
        }
        return false;
    }

    return true;
}

QByteArray OperationRecorder::buildSignedPayload(const OperationRecord &record) const
{
    const QJsonObject payloadObj = record.toJson();
    const QJsonDocument payloadDoc(payloadObj);
    const QByteArray payload = payloadDoc.toJson(QJsonDocument::Compact);
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    const QByteArray nonce = QCryptographicHash::hash(
        QByteArray::number(ts) + payload,
        QCryptographicHash::Sha256).toHex().left(16);

    QJsonObject envelope;
    envelope["payload"] = QString::fromUtf8(payload.toBase64());
    envelope["ts"] = QString::number(ts);
    envelope["nonce"] = QString::fromUtf8(nonce);
    envelope["alg"] = "HMAC-SHA256";

    if (!m_signingKey.isEmpty()) {
        const QByteArray canonical = payload + '|' + QByteArray::number(ts) + '|' + nonce;
        const QByteArray sig = QMessageAuthenticationCode::hash(canonical, m_signingKey, QCryptographicHash::Sha256).toHex();
        envelope["sig"] = QString::fromUtf8(sig);
    }

    QJsonDocument envelopeDoc(envelope);
    QByteArray out = envelopeDoc.toJson(QJsonDocument::Compact);
    out.append("\n");
    return out;
}

OperationRecorder::~OperationRecorder()
{
    persistSendQueue();
    disconnectTcpSocket();
}

void OperationRecorder::addRecord(const OperationRecord &record)
{
    OperationRecord normalized = record;
    if (!ModbusWriteGate::shouldAppendOperationHistoryRecord()) {
        const QString note = QStringLiteral("当前不是本机示教，操作未执行");
        const QString existing = normalized.newValue.toString();
        normalized.newValue = existing.isEmpty() ? note : existing + QStringLiteral("；") + note;
    }

    MappingConfig::instance()->normalizeOperationRecord(normalized);

    // 限制记录数量
    if (m_records.size() >= m_maxRecords) {
        m_records.removeFirst();
    }

    m_records.append(normalized);
    emit recordAdded(normalized);

    // 如果TCP传输已启用，发送记录到服务器（内部处理排队/重连）。
    if (m_tcpEnabled) {
        sendRecordToServer(normalized);
    }

    //qDebug() << "记录操作:" << normalized.toString();
}

void OperationRecorder::clear()
{
    m_records.clear();
    emit recordsCleared();
}

bool OperationRecorder::saveToFile(const QString &filename)
{
    QJsonArray jsonArray;
    for (const auto &record : m_records) {
        jsonArray.append(record.toJson());
    }

    QJsonDocument doc(jsonArray);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();
    return true;
}

bool OperationRecorder::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }

    m_records.clear();
    QJsonArray jsonArray = doc.array();
    for (const auto &jsonValue : jsonArray) {
        OperationRecord record = OperationRecord::fromJson(jsonValue.toObject());
        MappingConfig::instance()->normalizeOperationRecord(record);
        m_records.append(record);
    }

    return true;
}

bool OperationRecorder::exportToText(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "=== 操作记录报告 ===\n";
    stream << "生成时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    stream << "总记录数: " << m_records.size() << "\n\n";

    // 按页面分组
    QMap<QString, QList<OperationRecord>> pageGroups;
    for (const auto &record : m_records) {
        pageGroups[record.pageName].append(record);
    }

    // 按页面输出
    for (auto it = pageGroups.begin(); it != pageGroups.end(); ++it) {
        stream << "\n===== 页面: " << it.key() << " =====\n";
        for (const auto &record : it.value()) {
            stream << record.toString() << "\n";
        }
    }

    file.close();
    return true;
}

QList<OperationRecord> OperationRecorder::getPageRecords(const QString &pageName) const
{
    QList<OperationRecord> result;
    for (const auto &record : m_records) {
        if (record.pageName == pageName) {
            result.append(record);
        }
    }
    return result;
}

QList<OperationRecord> OperationRecorder::getControlRecords(const QString &controlName) const
{
    QList<OperationRecord> result;
    for (const auto &record : m_records) {
        if (record.controlName == controlName) {
            result.append(record);
        }
    }
    return result;
}

int OperationRecorder::pageRecordCount(const QString &pageName) const
{
    int count = 0;
    for (const auto &record : m_records) {
        if (record.pageName == pageName) {
            count++;
        }
    }
    return count;
}

// ============ TCP传输相关方法 ============

void OperationRecorder::enableTcpTransmission(bool enabled)
{
    loadSecuritySettings();
    m_tcpEnabled = enabled;

    if (enabled) {
        connectTcpSocket();
    } else {
        disconnectTcpSocket();
    }
}

void OperationRecorder::setTcpServer(const QString &ip, quint16 port)
{
    ensureAllowedHost(ip);
    loadSecuritySettings();
    const bool endpointChanged = (m_tcpServerIp != ip) || (m_tcpServerPort != port);
    m_tcpServerIp = ip;
    m_tcpServerPort = port;

    if (!endpointChanged || !m_tcpEnabled || !m_tcpSocket) {
        return;
    }
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket->abort();
    }
    connectTcpSocket();
}

void OperationRecorder::sendAllRecordsToServer()
{
    if (!m_tcpEnabled || !isTcpConnected()) {
        qWarning() << "TCP传输未启用或未连接，无法发送待发记录";
        emit tcpTransmissionError("TCP传输未启用或未连接");
        return;
    }

    if (m_tcpSendQueue.isEmpty()) {
        qDebug() << "没有待发送的操作记录";
        emit tcpTransmissionComplete();
        return;
    }

    qDebug() << "开始补发待发送记录，共" << m_tcpSendQueue.size() << "条记录";
    sendQueuedRecords();
}

void OperationRecorder::sendRecordToServer(const OperationRecord &record)
{
    if (!m_tcpEnabled) {
        return;
    }

    if (!isTcpConnected() || !m_tcpSendQueue.isEmpty()) {
        if (!isTcpConnected()) {
            connectTcpSocket();
        }
        enqueueRecordIfPossible(record);
        if (isTcpConnected()) {
            QTimer::singleShot(30, this, &OperationRecorder::sendQueuedRecords);
        }
        return;
    }

    if (!writeRecordNow(record)) {
        enqueueRecordIfPossible(record);
        if (isTcpConnected()) {
            QTimer::singleShot(30, this, &OperationRecorder::sendQueuedRecords);
        }
    }
}

bool OperationRecorder::writeRecordNow(const OperationRecord &record)
{
    if (!m_tcpSocket || !isTcpConnected()) {
        return false;
    }

    const QByteArray data = buildSignedPayload(record);
    const qint64 bytesWritten = m_tcpSocket->write(data);
    if (bytesWritten <= 0) {
        qWarning() << "发送数据失败:" << m_tcpSocket->errorString();
        return false;
    }
    m_tcpSocket->flush();
    qDebug() << "发送记录到服务器:" << record.controlName << "操作:" << record.operation;
    return true;
}

void OperationRecorder::sendQueuedRecords()
{
    if (m_tcpSendQueue.isEmpty() || !isTcpConnected()) {
        if (m_tcpSendQueue.isEmpty()) {
            emit tcpTransmissionComplete();
        } else {
            connectTcpSocket();
        }
        return;
    }

    // 每次发送最多10条记录，避免阻塞 UI 线程。
    const int sendCount = qMin(10, m_tcpSendQueue.size());
    for (int i = 0; i < sendCount; ++i) {
        const OperationRecord record = m_tcpSendQueue.takeFirst();
        if (!writeRecordNow(record)) {
            m_tcpSendQueue.prepend(record);
            break;
        }
    }
    persistSendQueue();

    if (!m_tcpSendQueue.isEmpty()) {
        QTimer::singleShot(30, this, &OperationRecorder::sendQueuedRecords);
    } else {
        emit tcpTransmissionComplete();
    }
}

void OperationRecorder::connectTcpSocket()
{
    if (!m_tcpEnabled || m_tcpSocket->state() == QAbstractSocket::ConnectingState ||
        m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        return;
    }

    QString policyError;
    if (!validateTransportPolicy(&policyError)) {
        m_transportPolicyBlocked = true;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool shouldReport = (policyError != m_lastTransportPolicyError)
            || ((nowMs - m_lastTransportPolicyErrorMs) > 3000);

        if (shouldReport) {
            m_lastTransportPolicyError = policyError;
            m_lastTransportPolicyErrorMs = nowMs;
            qWarning() << "日志传输策略阻止连接:" << policyError;
            emit tcpTransmissionError(policyError);
        } else {
            qDebug() << "日志传输策略阻止连接(节流):" << policyError;
        }
        return;
    }

    m_transportPolicyBlocked = false;

    // 降低 Nagle 延迟，保证按行 JSON 尽快到达接收端。
    m_tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    qDebug() << "连接日志服务器:" << m_tcpServerIp << ":" << m_tcpServerPort;
    m_tcpSocket->connectToHost(m_tcpServerIp, m_tcpServerPort);
    if (m_connectTimeoutTimer) {
        m_connectTimeoutTimer->start();
    }
}

void OperationRecorder::disconnectTcpSocket()
{
    if (m_connectTimeoutTimer && m_connectTimeoutTimer->isActive()) {
        m_connectTimeoutTimer->stop();
    }
    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
    }

    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

void OperationRecorder::onTcpConnected()
{
    qDebug() << "TCP服务器连接成功";
    m_lastTcpError.clear();
    m_lastTcpErrorMs = 0;
    m_transportPolicyBlocked = false;
    m_lastTransportPolicyError.clear();
    m_lastTransportPolicyErrorMs = 0;
    emit tcpConnectionStatusChanged(true);

    if (m_connectTimeoutTimer && m_connectTimeoutTimer->isActive()) {
        m_connectTimeoutTimer->stop();
    }

    // 停止重连定时器
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    // 发送队列中等待的记录
    if (!m_tcpSendQueue.isEmpty()) {
        sendQueuedRecords();
    }
}

void OperationRecorder::onTcpDisconnected()
{
    qDebug() << "TCP服务器连接断开";
    emit tcpConnectionStatusChanged(false);

    scheduleReconnect();
}

void OperationRecorder::onTcpError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    const QString error = m_tcpSocket->errorString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool shouldReport = (error != m_lastTcpError);

    if (shouldReport) {
        m_lastTcpError = error;
        m_lastTcpErrorMs = nowMs;
        qWarning() << "TCP连接错误:" << error;
        emit tcpTransmissionError(error);
    } else {
        qDebug() << "TCP连接错误(节流):" << error;
    }

    scheduleReconnect();
}

void OperationRecorder::onTcpDataWritten(qint64 bytes)
{
    qDebug() << "已发送" << bytes << "字节到TCP服务器";
}

void OperationRecorder::scheduleReconnect()
{
    if (!m_tcpEnabled || !m_tcpSocket || !m_reconnectTimer || m_reconnectTimer->isActive()) {
        return;
    }
    const auto state = m_tcpSocket->state();
    if (state == QAbstractSocket::ConnectingState || state == QAbstractSocket::ConnectedState) {
        return;
    }
    m_reconnectTimer->start();
}

void OperationRecorder::onReconnectTimeout()
{
    qDebug() << "尝试重新连接TCP服务器...";
    connectTcpSocket();
}

bool OperationRecorder::enqueueRecordIfPossible(const OperationRecord &record)
{
    if (m_tcpSendQueue.size() >= kMaxTcpQueueSize) {
        // 队列满时丢弃最旧记录，防止内存增长拖慢主线程。
        m_tcpSendQueue.removeFirst();
    }
    m_tcpSendQueue.append(record);
    persistSendQueue();
    return true;
}

QString OperationRecorder::pendingSpoolPath() const
{
    return QStringLiteral("pending_operation_log.jsonl");
}

void OperationRecorder::persistSendQueue() const
{
    QSaveFile file(pendingSpoolPath());
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法保存待发操作记录:" << file.errorString();
        return;
    }
    for (const OperationRecord &record : m_tcpSendQueue) {
        file.write(QJsonDocument(record.toJson()).toJson(QJsonDocument::Compact));
        file.write("\n");
    }
    if (!file.commit()) {
        qWarning() << "提交待发操作记录失败:" << file.errorString();
    }
}

void OperationRecorder::loadSpoolIntoQueue()
{
    QFile file(pendingSpoolPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) {
            continue;
        }
        if (m_tcpSendQueue.size() >= kMaxTcpQueueSize) {
            m_tcpSendQueue.removeFirst();
        }
        m_tcpSendQueue.append(OperationRecord::fromJson(doc.object()));
    }
}
