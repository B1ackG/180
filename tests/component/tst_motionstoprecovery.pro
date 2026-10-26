include(../support/test_common.pri)
include(../support/modbus_static_link.pri)
QT += widgets
TARGET = tst_motionstoprecovery
INCLUDEPATH += $$PWD
SOURCES += tst_motionstoprecovery.cpp \
    fakemodbusclient.cpp \
    ../../motionstoprecoverycontroller.cpp \
    ../../modbusthreadmanager.cpp \
    ../../modbustcpclient.cpp \
    ../../modbuswritegate.cpp \
    ../../registerbitutils.cpp \
    ../support/sliderlinkstubs.cpp
HEADERS += fakemodbusclient.h \
    ../../motionstoprecoverycontroller.h \
    ../../modbusthreadmanager.h \
    ../../modbustcpclient.h \
    ../../imodbusclient.h \
    ../../modbuswritegate.h \
    ../../registerbitutils.h \
    ../../modbusasync.h
