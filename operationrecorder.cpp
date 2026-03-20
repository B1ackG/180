#include "operationrecorder.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDebug>
#include <QThread>
#include <QDateTime>

OperationRecorder::OperationRecorder(QObject *parent)
    : QObject{parent}
    , m_tcpSocket(nullptr)
    , m_tcpEnabled(false)
    , m_tcpServerIp(WIN7_IP)
    , m_tcpServerPort(WIN7_PORT)
    , m_reconnectTimer(nullptr)
{
    // 初始化TCP传输
    m_tcpSocket = new QTcpSocket(this);

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
    m_tcpEnabled = enabled;

    if (enabled) {
        connectTcpSocket();
    } else {
        disconnectTcpSocket();
    }
}

void OperationRecorder::setTcpServer(const QString &ip, quint16 port)
{
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

    // 将记录转换为JSON格式
    QJsonObject jsonRecord = record.toJson();
    QJsonDocument doc(jsonRecord);
    QByteArray data = doc.toJson(QJsonDocument::Compact);

    // 添加换行符作为分隔符
    data.append("\n");

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

        // 短暂延迟，避免发送过快
        QThread::msleep(10);
    }

    // 继续发送剩余记录
    if (!m_tcpSendQueue.isEmpty()) {
        QTimer::singleShot(100, this, &OperationRecorder::sendQueuedRecords);
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

    qDebug() << "连接TCP服务器:" << m_tcpServerIp << ":" << m_tcpServerPort;
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
