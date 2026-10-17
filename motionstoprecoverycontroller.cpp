#include "motionstoprecoverycontroller.h"

#include "modbusthreadmanager.h"

#include <QTimer>

namespace {
constexpr int kRetryIntervalMs = 500;
}

void invalidateExternalMotionWritesOnDisconnect(ModbusThreadManager *manager,
                                                quint64 &robotSeq,
                                                quint64 &sixAxisSeq)
{
    ++robotSeq;
    ++sixAxisSeq;
    if (!manager) {
        return;
    }
    manager->setWriteGeneration(ModbusThreadManager::kRobotExternalWriteChannel, robotSeq);
    manager->setWriteGeneration(ModbusThreadManager::kSixAxisExternalWriteChannel, sixAxisSeq);
}

MotionStopRecoveryController::MotionStopRecoveryController(QObject *parent)
    : QObject(parent)
    , m_retryTimer(new QTimer(this))
{
    m_retryTimer->setInterval(kRetryIntervalMs);
    m_retryTimer->setSingleShot(false);
    connect(m_retryTimer, &QTimer::timeout, this, &MotionStopRecoveryController::retryPendingStops);
}

MotionStopRecoveryController::~MotionStopRecoveryController()
{
    stopRetryTimer();
}

void MotionStopRecoveryController::setWriteDispatcher(WriteDispatcher dispatcher)
{
    m_dispatcher = std::move(dispatcher);
}

MotionStopRecoveryController::KindState &MotionStopRecoveryController::stateFor(MainMotionStopKind kind)
{
    return kind == MainMotionStopKind::SixAxisJog613 ? m_sixAxis : m_robot;
}

const MotionStopRecoveryController::KindState &MotionStopRecoveryController::stateFor(MainMotionStopKind kind) const
{
    return kind == MainMotionStopKind::SixAxisJog613 ? m_sixAxis : m_robot;
}

int MotionStopRecoveryController::addressFor(MainMotionStopKind kind) const
{
    return kind == MainMotionStopKind::SixAxisJog613 ? 613 : 514;
}

void MotionStopRecoveryController::markMotionStarted(MainMotionStopKind kind)
{
    stateFor(kind).active = true;
}

void MotionStopRecoveryController::requestMotionStop(MainMotionStopKind kind)
{
    KindState &state = stateFor(kind);
    state.active = false;
    state.pending = true;
    if (m_connected && !state.inFlight) {
        tryDispatch(kind);
    }
    updateSignals();
    if (state.pending && m_connected && !state.inFlight) {
        startRetryTimer();
    }
}

void MotionStopRecoveryController::setMainConnected(bool connected)
{
    m_connected = connected;
    if (!connected) {
        const MainMotionStopKind kinds[] = {
            MainMotionStopKind::SixAxisJog613,
            MainMotionStopKind::RobotJog514
        };
        for (MainMotionStopKind kind : kinds) {
            KindState &state = stateFor(kind);
            if (state.active || state.pending || state.inFlight) {
                state.active = false;
                state.pending = true;
            }
            state.inFlight = false;
            state.inFlightId = 0;
        }
        stopRetryTimer();
        updateSignals();
        return;
    }

    retryPendingStops();
}

void MotionStopRecoveryController::handleWriteFinished(quint64 id, bool ok)
{
    if (id == 0) {
        return;
    }

    const MainMotionStopKind kinds[] = {
        MainMotionStopKind::SixAxisJog613,
        MainMotionStopKind::RobotJog514
    };
    for (MainMotionStopKind kind : kinds) {
        KindState &state = stateFor(kind);
        if (!state.inFlight || state.inFlightId != id) {
            continue;
        }
        state.inFlight = false;
        state.inFlightId = 0;
        if (ok && m_connected) {
            state.pending = false;
            state.active = false;
        } else {
            state.pending = true;
        }
        updateSignals();
        if (state.pending && m_connected) {
            startRetryTimer();
        } else if (!hasPendingStop()) {
            stopRetryTimer();
        }
        return;
    }
}

bool MotionStopRecoveryController::canStartMotion() const
{
    return !hasPendingStop();
}

bool MotionStopRecoveryController::hasPendingStop() const
{
    return m_sixAxis.pending || m_robot.pending;
}

void MotionStopRecoveryController::retryPendingStops()
{
    if (!m_connected) {
        stopRetryTimer();
        return;
    }
    tryDispatch(MainMotionStopKind::SixAxisJog613);
    tryDispatch(MainMotionStopKind::RobotJog514);
    updateSignals();
    if (!hasPendingStop()) {
        stopRetryTimer();
    }
}

void MotionStopRecoveryController::tryDispatch(MainMotionStopKind kind)
{
    KindState &state = stateFor(kind);
    if (!m_connected || !state.pending || state.inFlight || !m_dispatcher) {
        return;
    }
    const quint64 id = m_dispatcher(addressFor(kind), 0);
    if (id == 0) {
        startRetryTimer();
        return;
    }
    state.inFlight = true;
    state.inFlightId = id;
}

void MotionStopRecoveryController::updateSignals()
{
    const bool pending = hasPendingStop();
    const bool blocked = !canStartMotion();
    if (pending != m_pending) {
        m_pending = pending;
        emit pendingStopChanged(pending);
    }
    if (blocked != m_blocked) {
        m_blocked = blocked;
        emit motionBlockedChanged(blocked);
    }
}

void MotionStopRecoveryController::startRetryTimer()
{
    if (m_retryTimer && !m_retryTimer->isActive()) {
        m_retryTimer->start();
    }
}

void MotionStopRecoveryController::stopRetryTimer()
{
    if (m_retryTimer) {
        m_retryTimer->stop();
    }
}
