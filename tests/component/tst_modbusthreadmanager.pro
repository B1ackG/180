include(../support/test_common.pri)
include(../support/modbus_static_link.pri)
QT += widgets
TARGET = tst_modbusthreadmanager
INCLUDEPATH += $$PWD
SOURCES += tst_modbusthreadmanager.cpp \
    fakemodbusclient.cpp \
    ../../mainmodbusconnector.cpp \
    ../../modbusthreadmanager.cpp \
    ../../modbustcpclient.cpp \
    ../../modbuswritegate.cpp \
    ../../registerbitutils.cpp \
    ../../motionwriteplan.cpp \
    ../../modbusregistercodec.cpp \
    ../support/sliderlinkstubs.cpp
HEADERS += fakemodbusclient.h \
    ../../mainmodbusconnector.h \
    ../../modbusthreadmanager.h \
    ../../modbustcpclient.h \
    ../../imodbusclient.h \
    ../../modbuswritegate.h \
    ../../registerbitutils.h \
    ../../motionwriteplan.h \
    ../../modbusregistercodec.h \
    ../../modbusasync.h
