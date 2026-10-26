QT       += core gui network widgets qml quick quickwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17 thread

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# 本机 Debug 编译虚拟外部按键；示教器 Release 不带该宏、不显示面板。
CONFIG(debug, debug|release) {
    DEFINES += ENABLE_VIRTUAL_MATRIX_KEYS
}

SOURCES += \
    agvmodbusmanager.cpp \
    animationmanager.cpp \
    batterywidget.cpp \
    buttonmodbusmapping.cpp \
    enablebuttonworker.cpp \
    featureswitchmanager.cpp \
    featureswitchwidget.cpp \
    main.cpp \
    maindevicemodbusapi.cpp \
    mainmodbuslabelmapper.cpp \
    mainmodbuspoller.cpp \
    mainwindow.cpp \
    mainwindow_lifecycle.cpp \
    mainmodbusconnector.cpp \
    mainmodbusstatus.cpp \
    mappingconfig.cpp \
    modbusregistercodec.cpp \
    motionwriteplan.cpp \
    motionstoprecoverycontroller.cpp \
    registerbitutils.cpp \
    modebuttonstyler.cpp \
    navigationicon.cpp \
    matrixkeymonitor.cpp \
    matrixkeythreadmanager.cpp \
    modbusbackendmode.cpp \
    modbus_backend_c.cpp \
    modbustcpclient.cpp \
    modbusthreadmanager.cpp \
    modbusstringregisters.cpp \
    modbuswritegate.cpp \
    modbusvariables.cpp \
    operationrecorder.cpp \
    poseprovider.cpp \
    popupcoordinator.cpp \
    runtimehealthmonitor.cpp \
    speedmodeselector.cpp \
    steeringmodeselector.cpp \
    techarcgauge.cpp \
    techchamfertoolbutton.cpp \
    techpushbutton.cpp \
    techshapes.cpp \
    techslideredit.cpp \
    techsliderlabel.cpp \
    techspeedgauge.cpp \
    techvirtualkeyboard.cpp \
    virtualmatrixkeypanel.cpp

HEADERS += \
    agvmodbusmanager.h \
    animationmanager.h \
    buttonmodbusmapping.h \
    enablebuttonworker.h \
    featureswitchmanager.h \
    featureswitchwidget.h \
    maindevicemodbusapi.h \
    mainmodbuslabelmapper.h \
    mainmodbuspoller.h \
    mainwindow.h \
    mainmodbusconnector.h \
    mainmodbusstatus.h \
    mappingconfig.h \
    modebuttonstyler.h \
    navigationicon.h \
    matrixkeymonitor.h \
    matrixkeythreadmanager.h \
    modbusbackendmode.h \
    modbus_backend_c.h \
    modbustcpclient.h \
    modbusthreadmanager.h \
    modbusasync.h \
    imodbusclient.h \
    modbusregistercodec.h \
    modbusstringregisters.h \
    motionwriteplan.h \
    sixaxisrunmode.h \
    motionstoprecoverycontroller.h \
    registerbitutils.h \
    modbuswritegate.h \
    modbusvariables.h \
    operationrecorder.h \
    poseprovider.h \
    popupcoordinator.h \
    runtimehealthmonitor.h \
    speedmodeselector.h \
    steeringmodeselector.h \
    techarcgauge.h \
    techchamfertoolbutton.h \
    techpushbutton.h \
    techshapes.h \
    techslideredit.h \
    techsliderlabel.h \
    techspeedgauge.h \
    techvirtualkeyboard.h \
    virtualmatrixkeypanel.h \
    batterywidget.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

# 静态链接官方 libmodbus.a，供控制台选择的 static 模式使用。
# 目标架构必须看 Qt 的 QT_ARCH，不能用宿主机 uname。
MODBUS_STATIC_ROOT = $$(MODBUS_STATIC_ROOT)
isEmpty(MODBUS_STATIC_ROOT) {
    contains(QT_ARCH, arm64)|contains(QT_ARCH, aarch64) {
        MODBUS_STATIC_ROOT = $$PWD/third_party/libmodbus-aarch64
    } else {
        MODBUS_STATIC_ROOT = $$PWD/third_party/libmodbus-local
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

# 生成文件相对 OUT_PWD（影子构建目录），不要写回源码根目录。
# UI_DIR=. 时源码树会留下 ui_*.h，影子构建的 -I$${PWD} 会优先吃到旧头文件。
UI_DIR = build/ui
MOC_DIR = build/moc
OBJECTS_DIR = build/obj
RCC_DIR = build/rcc

# Doxygen docs target: run `make docs` to generate API docs.
docs.target = docs
docs.commands = mkdir -p docs/doxygen && doxygen Doxyfile
QMAKE_EXTRA_TARGETS += docs
