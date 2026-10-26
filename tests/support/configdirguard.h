#ifndef CONFIGDIRGUARD_H
#define CONFIGDIRGUARD_H

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>

class ConfigDirGuard
{
public:
    ConfigDirGuard()
        : m_oldPath(QDir::currentPath())
    {
        if (m_dir.isValid()) {
            QDir::setCurrent(m_dir.path());
        }
    }

    ~ConfigDirGuard()
    {
        QDir::setCurrent(m_oldPath);
    }

    bool isValid() const
    {
        return m_dir.isValid();
    }

    void setTeachingDeviceId(int id) const
    {
        QFile file(QStringLiteral("config.ini"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }
        file.write(QStringLiteral("[Polling]\nteaching_write_device_id=%1\n").arg(id).toUtf8());
        file.close();
    }

    void setModbusBackendMode(const QString &mode) const
    {
        QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("ModbusBackend"));
        settings.setValue(QStringLiteral("mode"), mode);
        settings.endGroup();
        settings.sync();
    }

private:
    QTemporaryDir m_dir;
    QString m_oldPath;
};

#endif
