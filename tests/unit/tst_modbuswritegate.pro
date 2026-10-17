include(../support/test_common.pri)
QT += widgets
TARGET = tst_modbuswritegate
SOURCES += tst_modbuswritegate.cpp \
    ../../modbuswritegate.cpp \
    ../../modbusthreadmanager.cpp \
    ../../modbustcpclient.cpp \
    ../../registerbitutils.cpp \
    ../support/sliderlinkstubs.cpp
HEADERS += ../../modbuswritegate.h \
    ../../modbusthreadmanager.h \
    ../../modbustcpclient.h \
    ../../imodbusclient.h \
    ../../registerbitutils.h \
    ../../modbusasync.h
