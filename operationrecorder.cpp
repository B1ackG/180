#include "operationrecorder.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QSettings>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QFile>
#ifndef QT_NO_SSL
#include <QSslConfiguration>
#include <QSslCertificate>
#endif

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
    // 初始化TCP传输
#ifndef QT_NO_SSL
    m_tcpSocket = new QSslSocket(this);
#else
    m_tcpSocket = new QTcpSocket(this);
#endif
    loadSecuritySettings();

    // 连接TCP信号
    connect(m_tcpSocket, &QTcpSocket::connected, this, &OperationRecorder::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &OperationRecorder::onTcpDisconnected);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &OperationRecorder::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &OperationRecorder::onTcpDataWritten);

    // 初始化重连定时器
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(5000); // 5秒重连间隔
    connect(m_reconnectTimer, &QTimer::timeout, this, &OperationRecorder::onReconnectTimeout);
}

void OperationRecorder::loadSecuritySettings()
{
    QSettings settings("config.ini", QSettings::IniFormat);
    settings.beginGroup("OperationLogTransport");

    const QByteArray tlsEnv = qgetenv("AGV_LOG_TLS");
    if (!tlsEnv.isEmpty()) {
        const QByteArray normalized = tlsEnv.trimmed().toLower();
        m_tcpUseTls = (normalized == "1" || normalized == "true" || normalized == "on");
    } else {
        m_tcpUseTls = settings.value("tls_enabled", true).toBool();
    }

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

#ifndef QT_NO_SSL
    if (QSslSocket *sslSocket = qobject_cast<QSslSocket *>(m_tcpSocket)) {
        QSslConfiguration sslConfig = sslSocket->sslConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyPeer);

        const QByteArray caEnv = qgetenv("AGV_LOG_TLS_CA_FILE");
        const QString caPath = caEnv.isEmpty()
            ? settings.value("tls_ca_file", "").toString()
            : QString::fromUtf8(caEnv);
        if (!caPath.trimmed().isEmpty()) {
            QFile caFile(caPath.trimmed());
            if (caFile.open(QIODevice::ReadOnly)) {
                const QList<QSslCertificate> certs = QSslCertificate::fromData(caFile.readAll(), QSsl::Pem);
                if (!certs.isEmpty()) {
                    sslConfig.setCaCertificates(certs);
                }
            }
        }

        sslSocket->setSslConfiguration(sslConfig);
    } else {
        // 理论上仅在运行时SSL类型不匹配时触发，回退纯TCP保证可用性。
        m_tcpUseTls = false;
    }
#else
    // 目标Qt未启用SSL时，自动回退纯TCP。
    m_tcpUseTls = false;
#endif
    settings.endGroup();
}

bool OperationRecorder::validateTransportPolicy(QString *reason) const
{
    constexpr int kMinSigningKeyLength = 16;

    if (!m_allowedHosts.isEmpty() && !m_allowedHosts.contains(m_tcpServerIp)) {
        if (reason) {
            *reason = QString("目标主机不在白名单中: %1").arg(m_tcpServerIp);
        }
        return false;
    }

    if (!m_tcpUseTls && m_signingKey.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("TLS关闭时必须配置签名密钥(AGV_LOG_SIGNING_KEY)");
        }
        return false;
    }

    if (!m_tcpUseTls && m_signingKey.size() < kMinSigningKeyLength) {
        if (reason) {
            *reason = QStringLiteral("签名密钥长度过短，至少需要16字节");
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
    disconnectTcpSocket();
}

void OperationRecorder::addRecord(const OperationRecord &record)
{
    // 限制记录数量
    if (m_records.size() >= m_maxRecords) {
        m_records.removeFirst();
    }

    m_records.append(record);
    emit recordAdded(record);

    // 如果TCP传输已启用，发送记录到服务器
    if (m_tcpEnabled) {
        sendRecordToServer(record);
    }

    //qDebug() << "记录操作:" << record.toString();
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
    loadSecuritySettings();
    m_tcpServerIp = ip;
    m_tcpServerPort = port;

    // 如果已经连接，需要重新连接
    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        disconnectTcpSocket();
        connectTcpSocket();
    }
}

void OperationRecorder::sendAllRecordsToServer()
{
    if (!m_tcpEnabled || !isTcpConnected()) {
        qWarning() << "TCP传输未启用或未连接，无法发送所有记录";
        emit tcpTransmissionError("TCP传输未启用或未连接");
        return;
    }

    // 将所有记录添加到发送队列
    m_tcpSendQueue.clear();
    for (const auto &record : m_records) {
        m_tcpSendQueue.append(record);
    }

    qDebug() << "开始发送所有记录到服务器，共" << m_tcpSendQueue.size() << "条记录";

    // 开始发送队列中的记录
    sendQueuedRecords();
}

void OperationRecorder::sendRecordToServer(const OperationRecord &record)
{
    if (!m_tcpEnabled) {
        return;
    }

    if (!isTcpConnected()) {
        // 如果未连接，尝试连接
        connectTcpSocket();
        // 将记录添加到队列，等待连接成功后再发送
        m_tcpSendQueue.append(record);
        return;
    }

    const QByteArray data = buildSignedPayload(record);

    // 发送数据
    qint64 bytesWritten = m_tcpSocket->write(data);

    if (bytesWritten == -1) {
        qWarning() << "发送数据失败:" << m_tcpSocket->errorString();
        // 添加到队列等待重试
        m_tcpSendQueue.append(record);
    } else {
        qDebug() << "发送记录到服务器:" << record.controlName << "操作:" << record.operation;
    }
}

void OperationRecorder::sendQueuedRecords()
{
    if (m_tcpSendQueue.isEmpty() || !isTcpConnected()) {
        if (m_tcpSendQueue.isEmpty()) {
            emit tcpTransmissionComplete();
        }
        return;
    }

    // 每次发送最多10条记录，避免阻塞
    int sendCount = qMin(10, m_tcpSendQueue.size());

    for (int i = 0; i < sendCount; ++i) {
        OperationRecord record = m_tcpSendQueue.takeFirst();
        sendRecordToServer(record);
    }

    // 继续发送剩余记录
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
        qWarning() << "日志传输策略阻止连接:" << policyError;
        emit tcpTransmissionError(policyError);
        return;
    }

    qDebug() << "连接日志服务器:" << m_tcpServerIp << ":" << m_tcpServerPort << "TLS:" << m_tcpUseTls;
#ifndef QT_NO_SSL
    if (m_tcpUseTls) {
        QSslSocket *sslSocket = qobject_cast<QSslSocket*>(m_tcpSocket);
        if (sslSocket) {
            sslSocket->connectToHostEncrypted(m_tcpServerIp, m_tcpServerPort);
            return;
        }
    }
#endif
    m_tcpSocket->connectToHost(m_tcpServerIp, m_tcpServerPort);
}

void OperationRecorder::disconnectTcpSocket()
{
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
    emit tcpConnectionStatusChanged(true);

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

    // 如果TCP传输已启用，启动重连定时器
    if (m_tcpEnabled) {
        m_reconnectTimer->start();
    }
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

    // 如果TCP传输已启用，启动重连定时器
    if (m_tcpEnabled && !m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void OperationRecorder::onTcpDataWritten(qint64 bytes)
{
    qDebug() << "已发送" << bytes << "字节到TCP服务器";
}

void OperationRecorder::onReconnectTimeout()
{
    qDebug() << "尝试重新连接TCP服务器...";
    connectTcpSocket();
}
