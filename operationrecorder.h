#ifndef OPERATIONRECORDER_H
/**
 * @file operationrecorder.h
 * @brief 操作记录器的声明，用于记录用户/系统操作以便回放或审计。
 *
 * 详细说明: 提供记录、保存和回放操作序列的接口，适用于调试和事件追踪。
 *
 * 使用示例:
 * @code
 * #include "operationrecorder.h"
 * OperationRecorder recorder;
 * recorder.recordEvent("start");
 * @endcode
 */
#define OPERATIONRECORDER_H

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QAbstractSocket>
#include <QTcpSocket>
#include <QTimer>

#define WIN7_IP "192.168.1.70"
#define WIN7_PORT 12345

struct OperationRecord {
    QDateTime timestamp;      // 操作时间
    QString pageName;         // 页面名称
    QString controlName;      // 控件名称
    QString controlType;      // 控件类型（SliderEdit/PushButton等）
    QString operation;        // 操作类型（valueChanged/clicked等）
    QVariant oldValue;        // 旧值
    QVariant newValue;        // 新值

    // 转换为字符串用于显示
    QString toString() const {
        return QString("[%1] %2 -> %3: %4 -> %5")
            .arg(timestamp.toString("hh:mm:ss"))
            .arg(controlName)
            .arg(operation)
            .arg(oldValue.toString())
            .arg(newValue.toString());
    }

    // 转换为JSON用于保存
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["timestamp"] = timestamp.toString(Qt::ISODate);
        obj["pageName"] = pageName;
        obj["controlName"] = controlName;
        obj["controlType"] = controlType;
        obj["operation"] = operation;
        obj["oldValue"] = oldValue.toString();
        obj["newValue"] = newValue.toString();
        return obj;
    }

    // 从JSON恢复
    static OperationRecord fromJson(const QJsonObject &obj) {
        OperationRecord record;
        record.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        record.pageName = obj["pageName"].toString();
        record.controlName = obj["controlName"].toString();
        record.controlType = obj["controlType"].toString();
        record.operation = obj["operation"].toString();
        record.oldValue = obj["oldValue"].toString();
        record.newValue = obj["newValue"].toString();
        return record;
    }
};
// operationrecorder.h 添加新的操作类型
enum OperationType {
    SLIDER_CHANGE,      // 滑块值改变
    BUTTON_CLICK,       // 按钮点击
    TOOLBUTTON_CLICK,   // ToolButton点击
    TOOLBUTTON_TOGGLE,  // ToolButton切换状态
    LINEEDIT_CHANGE,    // LineEdit文本改变
    LOGIN_ATTEMPT       // 登录尝试
};

class OperationRecorder : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * 初始化操作记录器及网络/持久化相关资源。
     *
     * @param parent 父对象
     * @since 1.0.0
     */
    explicit OperationRecorder(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     *
     * 释放 TCP 连接、定时器及缓冲队列。
     */
    ~OperationRecorder();

    /**
     * @brief 添加操作记录
     *
     * 将一条操作记录加入内存队列并发出 `recordAdded()`。
     *
     * @param record 要添加的记录
     */
    void addRecord(const OperationRecord &record);

    /**
     * 使用示例:
     * @code
     * OperationRecord r; r.pageName = "main"; r.controlName = "BtnStart";
     * r.operation = "click";
     * recorder.addRecord(r);
     * @endcode
     */

    /**
     * @brief 获取当前记录列表（只读引用）
     * @return 当前内存中的记录列表引用
     * @note 为线程安全访问请改为返回拷贝或加锁保护
     */
    const QList<OperationRecord>& records() const { return m_records; }

    /**
     * @brief 清空所有记录
     */
    void clear();

    /**
     * @brief 将记录保存为 JSON 文件
     * @param filename 目标文件路径
     * @return 成功返回 true
     */
    bool saveToFile(const QString &filename);

    /**
     * 使用示例:
     * @code
     * recorder.saveToFile("records.json");
     * recorder.loadFromFile("records.json");
     * @endcode
     */

    /**
     * @brief 从 JSON 文件加载记录
     * @param filename 源文件路径
     * @return 成功返回 true
     * @note 加载方式（覆盖/合并）以实现为准
     */
    bool loadFromFile(const QString &filename);

    /**
     * @brief 导出为可读文本文件
     * @param filename 目标路径
     * @return 成功返回 true
     */
    bool exportToText(const QString &filename);

    /**
     * @brief 获取指定页面的记录
     * @param pageName 页面名
     * @return 过滤后的记录列表
     */
    QList<OperationRecord> getPageRecords(const QString &pageName) const;

    /**
     * @brief 获取指定控件的记录
     * @param controlName 控件名
     * @return 过滤后的记录列表
     */
    QList<OperationRecord> getControlRecords(const QString &controlName) const;

    /**
     * @brief 获取记录总数
     * @return 总记录数
     */
    int recordCount() const { return m_records.size(); }
    int pageRecordCount(const QString &pageName) const;

    /**
     * @brief 启用/禁用 TCP 传输
     * @param enabled true 启用
     * @note 启用后需先调用 `setTcpServer()`
     */
    void enableTcpTransmission(bool enabled);

    /**
     * @brief 设置远端 TCP 服务器地址与端口
     * @param ip 服务器 IP
     * @param port 端口号
     */
    void setTcpServer(const QString &ip, quint16 port);

    /**
     * @brief 将日志服务器 IP 合并进 config.ini 白名单（不覆盖 AGV_LOG_ALLOWED_HOSTS）
     */
    static void ensureAllowedHost(const QString &host);

    /**
     * @brief 将所有记录通过 TCP 发送到配置的服务器
     * @note 需先设置服务器并启用 TCP 传输
     */
    void sendAllRecordsToServer();

    /**
     * 使用示例:
     * @code
     * recorder.setTcpServer("192.168.1.100", 12346);
     * recorder.enableTcpTransmission(true);
     * recorder.sendAllRecordsToServer();
     * @endcode
     */

    /**
     * @brief 是否已连接到 TCP 服务器
     * @return true 表示已连接
     */
    bool isTcpConnected() const { return m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState; }

signals:
    void recordAdded(const OperationRecord &record);
    void recordsCleared();
    void tcpConnectionStatusChanged(bool connected);
    void tcpTransmissionComplete();
    void tcpTransmissionError(const QString &error);

private slots:
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError socketError);
    void onTcpDataWritten(qint64 bytes);
    void onReconnectTimeout();

private:
    static constexpr int kMaxTcpQueueSize = 2000;

    QList<OperationRecord> m_records;
    int m_maxRecords = 1000; // 最大记录数

    // TCP传输相关成员
    QTcpSocket *m_tcpSocket;
    bool m_tcpEnabled;
    QString m_tcpServerIp;
    quint16 m_tcpServerPort;
    QStringList m_allowedHosts;
    QByteArray m_signingKey;
    QTimer *m_reconnectTimer;
    QList<OperationRecord> m_tcpSendQueue;  // 待发送的记录队列
    QString m_lastTcpError;
    qint64 m_lastTcpErrorMs = 0;
    bool m_transportPolicyBlocked = false;
    QString m_lastTransportPolicyError;
    qint64 m_lastTransportPolicyErrorMs = 0;

    // TCP发送相关方法
    void loadSecuritySettings();
    bool validateTransportPolicy(QString *reason = nullptr) const;
    QByteArray buildSignedPayload(const OperationRecord &record) const;
    bool enqueueRecordIfPossible(const OperationRecord &record);
    void sendRecordToServer(const OperationRecord &record);
    void sendQueuedRecords();
    void connectTcpSocket();
    void disconnectTcpSocket();
};

#endif // OPERATIONRECORDER_H
