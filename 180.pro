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
    modebuttonstyler.cpp \
    navigationicon.cpp \
    matrixkeymonitor.cpp \
    matrixkeythreadmanager.cpp \
    modbustcpclient.cpp \
    modbusthreadmanager.cpp \
    modbusstringregisters.cpp \
    modbuswritegate.cpp \
    modbusvariables.cpp \
    operationrecorder.cpp \
    poseprovider.cpp \
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
    modbustcpclient.h \
    modbusthreadmanager.h \
    modbusstringregisters.h \
    modbuswritegate.h \
    modbusvariables.h \
    operationrecorder.h \
    poseprovider.h \
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
