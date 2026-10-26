MODBUS_PROJECT_ROOT = $$PWD/../..
MODBUS_STATIC_ROOT = $$(MODBUS_STATIC_ROOT)
isEmpty(MODBUS_STATIC_ROOT) {
    contains(QT_ARCH, arm64)|contains(QT_ARCH, aarch64) {
        MODBUS_STATIC_ROOT = $$MODBUS_PROJECT_ROOT/third_party/libmodbus-aarch64
    } else {
        MODBUS_STATIC_ROOT = $$MODBUS_PROJECT_ROOT/third_party/libmodbus-local
    }
}
message(MODBUS_STATIC_ROOT=$$MODBUS_STATIC_ROOT)
!exists($$MODBUS_STATIC_ROOT/include/modbus/modbus.h): \
    error("modbus.h not found in $$MODBUS_STATIC_ROOT/include/modbus")
!exists($$MODBUS_STATIC_ROOT/lib/libmodbus.a): \
    error("libmodbus.a not found in $$MODBUS_STATIC_ROOT/lib")
INCLUDEPATH += $$MODBUS_STATIC_ROOT/include
PRE_TARGETDEPS += $$MODBUS_STATIC_ROOT/lib/libmodbus.a
LIBS += $$MODBUS_STATIC_ROOT/lib/libmodbus.a
SOURCES += $$MODBUS_PROJECT_ROOT/modbusbackendmode.cpp \
    $$MODBUS_PROJECT_ROOT/modbus_backend_c.cpp
HEADERS += $$MODBUS_PROJECT_ROOT/modbusbackendmode.h \
    $$MODBUS_PROJECT_ROOT/modbus_backend_c.h
