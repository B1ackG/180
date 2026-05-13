#include <QtQml/qqmlprivate.h>
#include <QtCore/qdir.h>
#include <QtCore/qurl.h>

namespace QmlCacheGeneratedCode {
namespace _0x5f__BatteryWidget_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__DeviceCoordPanel_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__HistoryList_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__TechSpeedGauge_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__InclinometerCard_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__RobotTotalPowerCard_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}
namespace _0x5f__PoseDisplay_qml { 
    extern const unsigned char qmlData[];
    const QQmlPrivate::CachedQmlUnit unit = {
        reinterpret_cast<const QV4::CompiledData::Unit*>(&qmlData), nullptr, nullptr
    };
}

}
namespace {
struct Registry {
    Registry();
    ~Registry();
    QHash<QString, const QQmlPrivate::CachedQmlUnit*> resourcePathToCachedUnit;
    static const QQmlPrivate::CachedQmlUnit *lookupCachedUnit(const QUrl &url);
};

Q_GLOBAL_STATIC(Registry, unitRegistry)


Registry::Registry() {
        resourcePathToCachedUnit.insert(QStringLiteral("/BatteryWidget.qml"), &QmlCacheGeneratedCode::_0x5f__BatteryWidget_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/DeviceCoordPanel.qml"), &QmlCacheGeneratedCode::_0x5f__DeviceCoordPanel_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/HistoryList.qml"), &QmlCacheGeneratedCode::_0x5f__HistoryList_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/TechSpeedGauge.qml"), &QmlCacheGeneratedCode::_0x5f__TechSpeedGauge_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/InclinometerCard.qml"), &QmlCacheGeneratedCode::_0x5f__InclinometerCard_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/RobotTotalPowerCard.qml"), &QmlCacheGeneratedCode::_0x5f__RobotTotalPowerCard_qml::unit);
        resourcePathToCachedUnit.insert(QStringLiteral("/PoseDisplay.qml"), &QmlCacheGeneratedCode::_0x5f__PoseDisplay_qml::unit);
    QQmlPrivate::RegisterQmlUnitCacheHook registration;
    registration.version = 0;
    registration.lookupCachedQmlUnit = &lookupCachedUnit;
    QQmlPrivate::qmlregister(QQmlPrivate::QmlUnitCacheHookRegistration, &registration);
}

Registry::~Registry() {
    QQmlPrivate::qmlunregister(QQmlPrivate::QmlUnitCacheHookRegistration, quintptr(&lookupCachedUnit));
}

const QQmlPrivate::CachedQmlUnit *Registry::lookupCachedUnit(const QUrl &url) {
    if (url.scheme() != QLatin1String("qrc"))
        return nullptr;
    QString resourcePath = QDir::cleanPath(url.path());
    if (resourcePath.isEmpty())
        return nullptr;
    if (!resourcePath.startsWith(QLatin1Char('/')))
        resourcePath.prepend(QLatin1Char('/'));
    return unitRegistry()->resourcePathToCachedUnit.value(resourcePath, nullptr);
}
}
int QT_MANGLE_NAMESPACE(qInitResources_res)() {
    ::unitRegistry();
    Q_INIT_RESOURCE(res_qmlcache);
    return 1;
}
Q_CONSTRUCTOR_FUNCTION(QT_MANGLE_NAMESPACE(qInitResources_res))
int QT_MANGLE_NAMESPACE(qCleanupResources_res)() {
    Q_CLEANUP_RESOURCE(res_qmlcache);
    return 1;
}
