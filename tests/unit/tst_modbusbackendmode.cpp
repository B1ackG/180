#include "modbusbackendmode.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class TestModbusBackendMode : public QObject
{
    Q_OBJECT
private slots:
    void missingFileDefaultsDynamic();
    void dynamicValue();
    void staticValue();
    void staticWithSpacesAndCase();
    void unknownDefaultsDynamic();
    void saveRoundTrip();
    void saveKeepsOtherGroups();
    void saveRejectsDirectory();
};

void TestModbusBackendMode::missingFileDefaultsDynamic()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("missing.ini"));
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Dynamic);
}

void TestModbusBackendMode::dynamicValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QVERIFY(saveModbusBackendMode(ModbusBackendMode::Dynamic, path));
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Dynamic);
}

void TestModbusBackendMode::staticValue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QVERIFY(saveModbusBackendMode(ModbusBackendMode::Static, path));
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Static);
}

void TestModbusBackendMode::staticWithSpacesAndCase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("ModbusBackend/mode"), QStringLiteral(" STATIC "));
    settings.sync();
    QCOMPARE(parseModbusBackendMode(QStringLiteral(" STATIC ")), ModbusBackendMode::Static);
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Static);
}

void TestModbusBackendMode::unknownDefaultsDynamic()
{
    QCOMPARE(parseModbusBackendMode(QStringLiteral("unknown")), ModbusBackendMode::Dynamic);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("ModbusBackend/mode"), QStringLiteral("unknown"));
    settings.sync();
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Dynamic);
}

void TestModbusBackendMode::saveRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QVERIFY(saveModbusBackendMode(ModbusBackendMode::Static, path));
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Static);
    QVERIFY(saveModbusBackendMode(ModbusBackendMode::Dynamic, path));
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Dynamic);
}

void TestModbusBackendMode::saveKeepsOtherGroups()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(QStringLiteral("Polling/teaching_write_device_id"), 7);
    settings.sync();
    QVERIFY(saveModbusBackendMode(ModbusBackendMode::Static, path));
    QSettings after(path, QSettings::IniFormat);
    QCOMPARE(after.value(QStringLiteral("Polling/teaching_write_device_id")).toInt(), 7);
    QCOMPARE(loadModbusBackendMode(path), ModbusBackendMode::Static);
}

void TestModbusBackendMode::saveRejectsDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY(!saveModbusBackendMode(ModbusBackendMode::Static, dir.path(), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestModbusBackendMode)
#include "tst_modbusbackendmode.moc"
