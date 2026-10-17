include(../support/test_common.pri)
QT += widgets network
TARGET = tst_modbus_tcp_integration
INCLUDEPATH += $$PWD

MODBUS_SIM_ROOT = $$(MODBUS_SIM_ROOT)
isEmpty(MODBUS_SIM_ROOT): error("MODBUS_SIM_ROOT is not set")
!exists($$MODBUS_SIM_ROOT/modbusslave.cpp): error("modbusslave.cpp not found in $$MODBUS_SIM_ROOT")
!exists($$MODBUS_SIM_ROOT/modbusslave.h): error("modbusslave.h not found in $$MODBUS_SIM_ROOT")
INCLUDEPATH += $$MODBUS_SIM_ROOT

SOURCES += tst_modbus_tcp_integration.cpp \
    $$MODBUS_SIM_ROOT/modbusslave.cpp \
    ../../modbustcpclient.cpp \
    ../../modbusthreadmanager.cpp \
    ../../modbuswritegate.cpp \
    ../../registerbitutils.cpp \
    ../../motionwriteplan.cpp \
    ../../modbusregistercodec.cpp \
    ../../motionstoprecoverycontroller.cpp \
    ../support/sliderlinkstubs.cpp
HEADERS += $$MODBUS_SIM_ROOT/modbusslave.h \
    ../../modbustcpclient.h \
    ../../modbusthreadmanager.h \
    ../../imodbusclient.h \
    ../../modbuswritegate.h \
    ../../registerbitutils.h \
    ../../motionwriteplan.h \
    ../../modbusregistercodec.h \
    ../../motionstoprecoverycontroller.h \
    ../../modbusasync.h
