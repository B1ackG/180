#ifndef MOTIONSTOPRECOVERYCONTROLLER_H
#define MOTIONSTOPRECOVERYCONTROLLER_H

#include <QObject>
#include <QtGlobal>

#include <functional>

enum class MainMotionStopKind : quint8 {
    SixAxisJog613,
    RobotJog514
};

class ModbusThreadManager;
class QTimer;

void invalidateExternalMotionWritesOnDisconnect(ModbusThreadManager *manager,
                                                quint64 &robotSeq,
                                                quint64 &sixAxisSeq);

class MotionStopRecoveryController : public QObject
{
    Q_OBJECT
public:
    using WriteDispatcher = std::function<quint64(int address, quint16 value)>;

    explicit MotionStopRecoveryController(QObject *parent = nullptr);
    ~MotionStopRecoveryController() override;

    void setWriteDispatcher(WriteDispatcher dispatcher);
    void markMotionStarted(MainMotionStopKind kind);
    void requestMotionStop(MainMotionStopKind kind);
    void setMainConnected(bool connected);
    void handleWriteFinished(quint64 id, bool ok);
    bool canStartMotion() const;
    bool hasPendingStop() const;
    void retryPendingStops();

signals:
    void motionBlockedChanged(bool blocked);
    void pendingStopChanged(bool pending);

private:
    struct KindState {
        bool active = false;
        bool pending = false;
        bool inFlight = false;
        quint64 inFlightId = 0;
    };

    KindState &stateFor(MainMotionStopKind kind);
    const KindState &stateFor(MainMotionStopKind kind) const;
    int addressFor(MainMotionStopKind kind) const;
    void tryDispatch(MainMotionStopKind kind);
    void updateSignals();
    void startRetryTimer();
    void stopRetryTimer();

    WriteDispatcher m_dispatcher;
    KindState m_sixAxis;
    KindState m_robot;
    bool m_connected = false;
    bool m_blocked = false;
    bool m_pending = false;
    QTimer *m_retryTimer = nullptr;
};

inline bool externalJogPressAllowed(const MotionStopRecoveryController *controller)
{
    return !controller || controller->canStartMotion();
}

#endif
