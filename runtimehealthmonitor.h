#ifndef RUNTIMEHEALTHMONITOR_H
#define RUNTIMEHEALTHMONITOR_H

#include <QObject>
#include <QTimer>

class RuntimeHealthMonitor final : public QObject
{
public:
    explicit RuntimeHealthMonitor(QObject *parent = nullptr);

    void start();

private:
    void sample();
    static qint64 readResidentMemoryKb();

    QTimer m_timer;
    int m_lastFdCount = -1;
    int m_lastMalitlCount = -1;
    qint64 m_lastRssKb = -1;
    int m_sampleCount = 0;
};

#endif // RUNTIMEHEALTHMONITOR_H
