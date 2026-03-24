#ifndef POSEPROVIDER_H
#define POSEPROVIDER_H

#include <QObject>

class PoseProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double x READ x WRITE setX NOTIFY poseChanged)
    Q_PROPERTY(double y READ y WRITE setY NOTIFY poseChanged)
    Q_PROPERTY(double z READ z WRITE setZ NOTIFY poseChanged)
    Q_PROPERTY(double roll READ roll WRITE setRoll NOTIFY poseChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY poseChanged)
    Q_PROPERTY(double yaw READ yaw WRITE setYaw NOTIFY poseChanged)

public:
    explicit PoseProvider(QObject *parent = nullptr) : QObject(parent),
        m_x(0), m_y(0), m_z(0), m_roll(0), m_pitch(0), m_yaw(0) {}

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }
    double roll() const { return m_roll; }
    double pitch() const { return m_pitch; }
    double yaw() const { return m_yaw; }

public slots:
    void setX(double v) { if (m_x != v) { m_x = v; emit poseChanged(); } }
    void setY(double v) { if (m_y != v) { m_y = v; emit poseChanged(); } }
    void setZ(double v) { if (m_z != v) { m_z = v; emit poseChanged(); } }
    void setRoll(double v) { if (m_roll != v) { m_roll = v; emit poseChanged(); } }
    void setPitch(double v) { if (m_pitch != v) { m_pitch = v; emit poseChanged(); } }
    void setYaw(double v) { if (m_yaw != v) { m_yaw = v; emit poseChanged(); } }

signals:
    void poseChanged();

private:
    double m_x, m_y, m_z;
    double m_roll, m_pitch, m_yaw;
};

#endif // POSEPROVIDER_H
