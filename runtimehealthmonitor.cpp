#include "runtimehealthmonitor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtGlobal>

namespace {
constexpr int kDefaultIntervalMs = 60 * 1000;
constexpr int kFdWarningThreshold = 256;
constexpr int kFdChangeLogThreshold = 8;
constexpr qint64 kRssChangeLogThresholdKb = 32 * 1024;
constexpr int kHourlySampleCount = 60;
}

RuntimeHealthMonitor::RuntimeHealthMonitor(QObject *parent)
    : QObject(parent)
{
    bool ok = false;
    const int configuredInterval = qEnvironmentVariableIntValue("HMI_HEALTH_INTERVAL_MS", &ok);
    m_timer.setInterval(ok ? qMax(1000, configuredInterval) : kDefaultIntervalMs);
    m_timer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, [this]() { sample(); });
}

void RuntimeHealthMonitor::start()
{
    sample();
    m_timer.start();
}

void RuntimeHealthMonitor::sample()
{
#ifdef Q_OS_LINUX
    const QString fdPath = QStringLiteral("/proc/%1/fd").arg(QCoreApplication::applicationPid());
    const QDir fdDir(fdPath);
    const QFileInfoList descriptors = fdDir.entryInfoList(
        QDir::Files | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);

    int malitlCount = 0;
    for (const QFileInfo &descriptor : descriptors) {
        if (descriptor.symLinkTarget().contains(QStringLiteral("malitl"))) {
            ++malitlCount;
        }
    }

    const int fdCount = descriptors.size();
    const qint64 rssKb = readResidentMemoryKb();
    ++m_sampleCount;

    const bool firstSample = m_lastFdCount < 0;
    const bool notableChange = qAbs(fdCount - m_lastFdCount) >= kFdChangeLogThreshold
        || malitlCount != m_lastMalitlCount
        || qAbs(rssKb - m_lastRssKb) >= kRssChangeLogThresholdKb;
    const bool hourlySample = (m_sampleCount % kHourlySampleCount) == 0;

    const bool thresholdExceeded = fdCount >= kFdWarningThreshold || malitlCount > 0;
    if (thresholdExceeded && (firstSample || notableChange || hourlySample)) {
        qWarning() << "[Health] resource threshold exceeded"
                   << "fd=" << fdCount
                   << "malitl=" << malitlCount
                   << "rss_kb=" << rssKb;
    } else if (firstSample || notableChange || hourlySample) {
        qInfo() << "[Health]"
                << "fd=" << fdCount
                << "malitl=" << malitlCount
                << "rss_kb=" << rssKb;
    }

    m_lastFdCount = fdCount;
    m_lastMalitlCount = malitlCount;
    m_lastRssKb = rssKb;
#endif
}

qint64 RuntimeHealthMonitor::readResidentMemoryKb()
{
#ifdef Q_OS_LINUX
    QFile statusFile(QStringLiteral("/proc/self/status"));
    if (!statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }

    QTextStream stream(&statusFile);
    QString line;
    while (stream.readLineInto(&line)) {
        if (!line.startsWith(QStringLiteral("VmRSS:"))) {
            continue;
        }
        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        return fields.size() >= 2 ? fields.at(1).toLongLong() : -1;
    }
#endif
    return -1;
}
