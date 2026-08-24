#include "buttonmodbusmapping.h"

#include <QMap>
#include <initializer_list>

namespace ButtonModbusMapping {
namespace {

QString normalizeBitText(QString bit)
{
    bit = bit.trimmed();
    if (bit == QStringLiteral("空位")
        || bit == QStringLiteral("(空位)")
        || bit == QStringLiteral("无")
        || bit == QStringLiteral("(无)")) {
        return QString();
    }
    if (bit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
        bit = bit.mid(3).trimmed();
    }
    return bit;
}

RegisterSpec parseLegacyRegisterString(const QString &raw)
{
    RegisterSpec spec;
    const QString t = raw.trimmed();
    if (t.isEmpty() || t == QStringLiteral("无")) {
        return spec;
    }
    if (!t.startsWith(QStringLiteral("主控:")) && !t.startsWith(QStringLiteral("AGV:"))) {
        return spec;
    }

    const int colon = t.indexOf(QLatin1Char(':'));
    spec.device = t.left(colon);
    const QString after = t.mid(colon + 1);
    int i = 0;
    while (i < after.size() && after.at(i).isDigit()) {
        ++i;
    }
    spec.address = after.left(i);
    QString rest = after.mid(i).trimmed();

    if (rest.startsWith(QLatin1Char('('))) {
        const int close = rest.indexOf(QLatin1Char(')'));
        QString inner = close > 0 ? rest.mid(1, close - 1) : rest.mid(1);
        if (inner.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            inner = inner.mid(3).trimmed();
        }
        spec.bit = inner;
        rest = close >= 0 ? rest.mid(close + 1).trimmed() : QString();
    }

    if (rest.startsWith(QLatin1Char('='))) {
        spec.value1 = rest.mid(1).trimmed();
    } else if (!rest.isEmpty()) {
        spec.value1 = rest;
    }
    return spec;
}

RegisterSpec makeSpec(const QString &device,
                      const QString &address,
                      const QString &bit = QString(),
                      const QString &value1 = QString(),
                      const QString &value2 = QString(),
                      const QString &value3 = QString())
{
    RegisterSpec spec;
    spec.device = device;
    spec.address = address;
    spec.bit = bit;
    spec.value1 = value1;
    spec.value2 = value2;
    spec.value3 = value3;
    return spec;
}

QList<RegisterSpec> specList(std::initializer_list<RegisterSpec> list)
{
    QList<RegisterSpec> out;
    for (const auto &spec : list) {
        out.append(spec);
    }
    return out;
}

const QMap<QString, Binding> &knownBindings()
{
    static const QMap<QString, Binding> bindings = {
        {QStringLiteral("TBtn_Interlocking"), {
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("8192"), QString(), QStringLiteral("1"), QStringLiteral("0"))}),
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("8192"), QString(), QStringLiteral("1"), QStringLiteral("0"))}),
            true}},
        {QStringLiteral("TBtn_ControlMode"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("100"), QString(), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("500"), QString(), QStringLiteral("1"), QStringLiteral("2"))}),
            true}},
        {QStringLiteral("techBtn_AGV_OA"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("50"), QStringLiteral("13"), QStringLiteral("0"), QStringLiteral("1"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1"))}),
            true}},
        {QStringLiteral("techBtn_AGV_Park"), {
            specList({
                makeSpec(QStringLiteral("AGV"), QStringLiteral("51"), QStringLiteral("3"), QStringLiteral("1"), QStringLiteral("0")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("51"), QStringLiteral("4"), QStringLiteral("0"), QStringLiteral("1"))
            }),
            specList({
                makeSpec(QStringLiteral("AGV"), QStringLiteral("0"), QStringLiteral("9"), QStringLiteral("1"), QStringLiteral("0")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("0"), QStringLiteral("10"), QStringLiteral("0"), QStringLiteral("1")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("5014"), QString(), QStringLiteral("驻车长度参数"))
            }),
            true}},
        {QStringLiteral("btn_ForceControl"), {
            {},
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("400"), QString(), QStringLiteral("1"), QStringLiteral("0"))}),
            false}},
        {QStringLiteral("techBtn_resetSixAxies"), {
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("615"), QStringLiteral("1"), QStringLiteral("1"))}),
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("615"), QStringLiteral("1"), QStringLiteral("1"))}),
            false}},
        {QStringLiteral("techBtn_spare_1"), {{}, {}, false}},
        {QStringLiteral("techBtn_spare_2"), {{}, {}, false}},
        {QStringLiteral("Btn_bigForceControl"), {
            {},
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("404"), QString(), QStringLiteral("0"))}),
            false}},
        {QStringLiteral("Btn_smallForceControl"), {
            {},
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("404"), QString(), QStringLiteral("1"))}),
            false}},
        {QStringLiteral("btnFrontBack"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("155"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("0"))}),
            true}},
        {QStringLiteral("btnFrontOnly"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("155"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("1"))}),
            true}},
        {QStringLiteral("btnParallel"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("155"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("2"))}),
            true}},
        {QStringLiteral("btnLateral"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("155"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("3"))}),
            true}},
        {QStringLiteral("btnRotate"), {
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("155"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"))}),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("4"))}),
            true}},
        {QStringLiteral("steeringModeSelector"), {
            specList({
                makeSpec(QStringLiteral("AGV"), QStringLiteral("50"), QStringLiteral("10")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("50"), QStringLiteral("11")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("50"), QStringLiteral("12")),
            }),
            specList({makeSpec(QStringLiteral("AGV"), QStringLiteral("2"), QString(), QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2/3/4"))}),
            true}},
        {QStringLiteral("TBtn_MoveMode"), {
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("126"), QString(), QStringLiteral("2"), QStringLiteral("1"))}),
            specList({makeSpec(QStringLiteral("主控"), QStringLiteral("525"), QString(), QStringLiteral("2"), QStringLiteral("1"))}),
            true}},
        {QStringLiteral("TBtn_RemoveWarning"), {
            {},
            specList({
                makeSpec(QStringLiteral("主控"), QStringLiteral("290"), QString(), QStringLiteral("1")),
                makeSpec(QStringLiteral("AGV"), QStringLiteral("290"), QString(), QStringLiteral("1"))
            }),
            false}},
    };
    return bindings;
}

bool specsMatch(const RegisterSpec &a, const RegisterSpec &b)
{
    return a.device == b.device
        && a.address == b.address
        && normalizeBitText(a.bit) == normalizeBitText(b.bit)
        && a.value1 == b.value1
        && a.value2 == b.value2
        && a.value3 == b.value3;
}

bool listsMatch(const QList<RegisterSpec> &a, const QList<RegisterSpec> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i) {
        if (!specsMatch(a.at(i), b.at(i))) {
            return false;
        }
    }
    return true;
}

QList<RegisterSpec> splitCompositeBitSpec(const RegisterSpec &spec)
{
    QList<RegisterSpec> out;
    const QString bitText = spec.bit.trimmed();
    if (!bitText.contains(QLatin1Char('/'))) {
        out.append(spec);
        return out;
    }

    const QStringList bitParts = bitText.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (bitParts.isEmpty()) {
        out.append(spec);
        return out;
    }

    const QString v1 = spec.value1.trimmed();
    const QString v2 = spec.value2.trimmed();
    const bool twoBitMode = (bitParts.size() == 2
                             && v1.size() == 2 && v2.size() == 2
                             && (v1.at(0) == QLatin1Char('0') || v1.at(0) == QLatin1Char('1'))
                             && (v1.at(1) == QLatin1Char('0') || v1.at(1) == QLatin1Char('1'))
                             && (v2.at(0) == QLatin1Char('0') || v2.at(0) == QLatin1Char('1'))
                             && (v2.at(1) == QLatin1Char('0') || v2.at(1) == QLatin1Char('1')));

    for (int i = 0; i < bitParts.size(); ++i) {
        RegisterSpec piece = spec;
        QString pieceBit = bitParts.at(i).trimmed();
        if (pieceBit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            pieceBit = pieceBit.mid(3).trimmed();
        }
        piece.bit = pieceBit;
        if (twoBitMode) {
            piece.value1 = QString(v1.at(i));
            piece.value2 = QString(v2.at(i));
        }
        out.append(piece);
    }
    return out;
}

void appendUnique(QList<RegisterSpec> &target, const RegisterSpec &spec)
{
    if (!spec.isConfigured()) {
        return;
    }
    for (const auto &existing : target) {
        if (existing.device == spec.device
            && existing.address == spec.address
            && existing.bit == spec.bit) {
            return;
        }
    }
    target.append(spec);
}

bool hasPrefixKeys(const QSettings &settings, const QString &prefix)
{
    return settings.contains(prefix + QStringLiteral("_device")) || settings.contains(prefix);
}

} // namespace

bool parseNumber(const QString &text, int &out)
{
    bool ok = false;
    out = text.trimmed().toInt(&ok);
    return ok;
}

int addressOr(const RegisterSpec &spec, int fallback)
{
    int addr = fallback;
    if (spec.isConfigured() && parseNumber(spec.address, addr)) {
        return addr;
    }
    return fallback;
}

int bitOr(const RegisterSpec &spec, int fallback)
{
    int bit = fallback;
    if (parseNumber(spec.bit, bit) && bit >= 0 && bit <= 15) {
        return bit;
    }
    return fallback;
}

int stateValueOr(const RegisterSpec &spec, int stateIndex, int fallback)
{
    QString text;
    if (stateIndex <= 1) {
        text = spec.value1;
    } else if (stateIndex == 2) {
        text = spec.value2.trimmed().isEmpty() ? spec.value1 : spec.value2;
    } else {
        text = spec.value3.trimmed().isEmpty() ? spec.value1 : spec.value3;
    }
    int value = fallback;
    if (parseNumber(text, value)) {
        return value;
    }
    return fallback;
}

QString composeLegacyRegisterString(const RegisterSpec &spec)
{
    if (!spec.isConfigured()) {
        return QStringLiteral("无");
    }

    QString composed = spec.device + QLatin1Char(':') + spec.address.trimmed();
    if (!spec.bit.trimmed().isEmpty()) {
        QString bit = spec.bit.trimmed();
        if (!bit.startsWith(QStringLiteral("bit"), Qt::CaseInsensitive)) {
            bit = QStringLiteral("bit") + bit;
        }
        composed += QLatin1Char('(') + bit + QLatin1Char(')');
    }
    if (!spec.value1.trimmed().isEmpty()) {
        const QString val = spec.value1.trimmed();
        if (val.startsWith(QLatin1Char('+')) || val.contains(QLatin1Char('='))) {
            composed += val.startsWith(QLatin1Char('+')) ? val : (QLatin1Char('=') + val);
        } else {
            composed += QLatin1Char('=') + val;
        }
    }
    return composed;
}

RegisterSpec loadRegisterSpec(QSettings &settings,
                              const QString &keyPrefix,
                              const RegisterSpec &fallback)
{
    if (!hasPrefixKeys(settings, keyPrefix)) {
        return fallback;
    }

    RegisterSpec loaded;
    if (settings.contains(keyPrefix + QStringLiteral("_device"))) {
        loaded.device = settings.value(keyPrefix + QStringLiteral("_device"), QStringLiteral("无")).toString();
        loaded.address = settings.value(keyPrefix + QStringLiteral("_addr")).toString();
        loaded.bit = settings.value(keyPrefix + QStringLiteral("_bit")).toString();
        if (loaded.bit.isEmpty()) {
            loaded.bit = settings.value(keyPrefix + QStringLiteral("_extra")).toString();
        }
        loaded.bit = normalizeBitText(loaded.bit);
        loaded.value1 = settings.value(keyPrefix + QStringLiteral("_value1")).toString();
        if (loaded.value1.isEmpty()) {
            loaded.value1 = settings.value(keyPrefix + QStringLiteral("_value")).toString();
        }
        loaded.value2 = settings.value(keyPrefix + QStringLiteral("_value2")).toString();
        loaded.value3 = settings.value(keyPrefix + QStringLiteral("_value3")).toString();
    } else {
        loaded = parseLegacyRegisterString(settings.value(keyPrefix).toString());
    }

    if (!loaded.isConfigured()) {
        return RegisterSpec{};
    }

    if (loaded.bit.trimmed().isEmpty()
        && !settings.contains(keyPrefix + QStringLiteral("_bit"))
        && !settings.contains(keyPrefix + QStringLiteral("_extra"))) {
        loaded.bit = normalizeBitText(fallback.bit);
    }
    if (loaded.value1.trimmed().isEmpty()
        && !settings.contains(keyPrefix + QStringLiteral("_value1"))
        && !settings.contains(keyPrefix + QStringLiteral("_value"))) {
        loaded.value1 = fallback.value1;
    }
    if (loaded.value2.trimmed().isEmpty()
        && !settings.contains(keyPrefix + QStringLiteral("_value2"))) {
        loaded.value2 = fallback.value2;
    }
    if (loaded.value3.trimmed().isEmpty()
        && !settings.contains(keyPrefix + QStringLiteral("_value3"))) {
        loaded.value3 = fallback.value3;
    }
    return loaded;
}

QList<RegisterSpec> loadRegisterSpecs(QSettings &settings,
                                      const QString &basePrefix,
                                      const QList<RegisterSpec> &fallbacks)
{
    QList<RegisterSpec> loaded;
    loaded.reserve(kMaxTargetsPerDirection);

    const bool hasLegacyBase = hasPrefixKeys(settings, basePrefix);
    bool hasIndexed = false;
    for (int i = 0; i < kMaxTargetsPerDirection; ++i) {
        const QString indexedPrefix = QStringLiteral("%1_%2").arg(basePrefix).arg(i + 1);
        if (hasPrefixKeys(settings, indexedPrefix)) {
            hasIndexed = true;
            break;
        }
    }

    if (hasIndexed) {
        for (int i = 0; i < kMaxTargetsPerDirection; ++i) {
            const QString indexedPrefix = QStringLiteral("%1_%2").arg(basePrefix).arg(i + 1);
            const RegisterSpec fallback = (i < fallbacks.size()) ? fallbacks.at(i) : RegisterSpec{};
            if (hasPrefixKeys(settings, indexedPrefix)) {
                const RegisterSpec indexed = loadRegisterSpec(settings, indexedPrefix, fallback);
                for (const auto &piece : splitCompositeBitSpec(indexed)) {
                    appendUnique(loaded, piece);
                }
            } else if (i < fallbacks.size()) {
                appendUnique(loaded, fallbacks.at(i));
            }
        }
    } else if (hasLegacyBase) {
        const RegisterSpec legacy = loadRegisterSpec(
            settings, basePrefix, fallbacks.isEmpty() ? RegisterSpec{} : fallbacks.first());
        for (const auto &piece : splitCompositeBitSpec(legacy)) {
            appendUnique(loaded, piece);
        }
        for (int i = 1; i < fallbacks.size(); ++i) {
            appendUnique(loaded, fallbacks.at(i));
        }
    } else {
        for (const auto &fallback : fallbacks) {
            appendUnique(loaded, fallback);
        }
    }

    if (loaded.size() > kMaxTargetsPerDirection) {
        loaded = loaded.mid(0, kMaxTargetsPerDirection);
    }
    return loaded;
}

Binding defaultBinding(const QString &objectName)
{
    const auto &known = knownBindings();
    const auto it = known.find(objectName);
    if (it != known.end()) {
        return it.value();
    }
    return {};
}

void migrateLegacyConsoleDefaults(const QString &objectName, Binding &binding)
{
    if (objectName == QStringLiteral("techBtn_AGV_Park")) {
        const QList<RegisterSpec> oldWrites = specList({
            makeSpec(QStringLiteral("AGV"), QStringLiteral("0"), QStringLiteral("3"), QStringLiteral("1"), QStringLiteral("0")),
            makeSpec(QStringLiteral("AGV"), QStringLiteral("0"), QStringLiteral("4"), QStringLiteral("0"), QStringLiteral("1")),
            makeSpec(QStringLiteral("AGV"), QStringLiteral("5014"), QString(), QStringLiteral("驻车长度参数"))
        });
        if (listsMatch(binding.writes, oldWrites)) {
            binding.writes = defaultBinding(objectName).writes;
        }
    } else if (objectName == QStringLiteral("TBtn_RemoveWarning")) {
        const QList<RegisterSpec> oldWrites = specList({
            makeSpec(QStringLiteral("主控"), QStringLiteral("290"), QString(), QStringLiteral("1")),
            makeSpec(QStringLiteral("主控"), QStringLiteral("403"), QString(), QStringLiteral("0"))
        });
        if (listsMatch(binding.writes, oldWrites)) {
            binding.writes = defaultBinding(objectName).writes;
        }
    } else if (objectName == QStringLiteral("TBtn_MoveMode")) {
        const QList<RegisterSpec> oldReads = specList({
            makeSpec(QStringLiteral("主控"), QStringLiteral("126"), QString(), QStringLiteral("1"), QStringLiteral("2"))
        });
        if (listsMatch(binding.reads, oldReads)) {
            binding.reads = defaultBinding(objectName).reads;
        }
    }
}

Binding resolvedBinding(const QString &objectName)
{
    const Binding defaults = defaultBinding(objectName);
    QSettings settings(QStringLiteral("config.ini"), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("ButtonModbusMapping"));

    Binding loaded;
    loaded.readForUiSync = defaults.readForUiSync;
    loaded.reads = loadRegisterSpecs(settings, objectName + QStringLiteral("_read"), defaults.reads);
    loaded.writes = loadRegisterSpecs(settings, objectName + QStringLiteral("_write"), defaults.writes);
    settings.endGroup();

    migrateLegacyConsoleDefaults(objectName, loaded);
    return loaded;
}

} // namespace ButtonModbusMapping
