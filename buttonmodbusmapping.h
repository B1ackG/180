#ifndef BUTTONMODBUSMAPPING_H
#define BUTTONMODBUSMAPPING_H

#include <QList>
#include <QSettings>
#include <QString>

/**
 * 功能控制台「Modbus 按键：显示与寄存器」的映射：默认值、config.ini 覆盖、加载规则。
 * 显式保存为「无」的槽位不会再被程序默认值填回去。
 */
namespace ButtonModbusMapping {

constexpr int kMaxTargetsPerDirection = 3;

struct RegisterSpec {
    QString device = QStringLiteral("无");
    QString address;
    QString bit;
    QString value1;
    QString value2;
    QString value3;
    bool isConfigured() const {
        return device != QStringLiteral("无") && !address.trimmed().isEmpty();
    }
};

struct Binding {
    QList<RegisterSpec> reads;
    QList<RegisterSpec> writes;
    bool readForUiSync = false;
};

Binding defaultBinding(const QString &objectName);
Binding resolvedBinding(const QString &objectName);

RegisterSpec loadRegisterSpec(QSettings &settings,
                              const QString &keyPrefix,
                              const RegisterSpec &fallback);
QList<RegisterSpec> loadRegisterSpecs(QSettings &settings,
                                      const QString &basePrefix,
                                      const QList<RegisterSpec> &fallbacks);
QString composeLegacyRegisterString(const RegisterSpec &spec);

bool parseNumber(const QString &text, int &out);
int addressOr(const RegisterSpec &spec, int fallback);
int bitOr(const RegisterSpec &spec, int fallback);
int stateValueOr(const RegisterSpec &spec, int stateIndex, int fallback);

} // namespace ButtonModbusMapping

#endif
