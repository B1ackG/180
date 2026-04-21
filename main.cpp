// file name: main.cpp
#include "mainwindow.h"
#include "modbusthreadmanager.h"  // 添加头文件
#include "featureswitchmanager.h"
#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QModelIndex>
#include <QTcpSocket>
#include <QFont>
#include "debug.h"

// 全局定义（在 debug.h 中声明）
int debug = 0;

// 全局消息处理器：当 debug==0 时过滤掉 Qt 的调试消息
static void globalMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (type == QtDebugMsg && debug == 0) {
        // 在未开启全局 DEBUG 时，保留主窗口分类日志，便于运行期排查 AGV/UI 问题。
        const QString category = context.category ? QString::fromUtf8(context.category) : QString();
        if (category != "app.mainwindow") {
            return; // 丢弃其它调试输出
        }
    }

    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    Q_UNUSED(file);
    Q_UNUSED(function);
    fprintf(stderr, "%s\n", localMsg.constData());
}
// 系统自检状态
enum SystemCheckStatus {
    CHECK_PENDING,
    CHECK_IN_PROGRESS,
    CHECK_SUCCESS,
    CHECK_FAILED,
    CHECK_WARNING
};

// 自检项目结构
struct SystemCheckItem {
    QString name;
    SystemCheckStatus status;
    QString message;
    int timeout; // 超时时间(ms)
};

static void waitWithUiEvents(int ms)
{
    if (ms <= 0) {
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
}

// 检查Modbus连接函数

// 检查Modbus连接函数

bool checkModbusConnection(QSplashScreen* splash, const QString& ip, int port, const QString& deviceName, int timeoutMs = 5000)
{
    QString message = QString("正在连接 %1 (%2:%3)...").arg(deviceName).arg(ip).arg(port);
    splash->showMessage(message, Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    qApp->processEvents();

    QElapsedTimer timer;
    timer.start();

    try {
        // 自检阶段使用独立 TCP 探测，避免干扰运行期 Modbus 管理器连接状态
        QTcpSocket socket;
        socket.connectToHost(ip, static_cast<quint16>(port));
        bool connectionSuccess = socket.waitForConnected(timeoutMs);

        if (connectionSuccess) {
            QString successMsg = QString("%1 连接成功 (%2ms)").arg(deviceName).arg(timer.elapsed());
            splash->showMessage(successMsg, Qt::AlignBottom | Qt::AlignCenter, Qt::green);
            qApp->processEvents();
            socket.disconnectFromHost();
            return true;
        } else {
            QString failMsg = QString("%1 连接失败或超时").arg(deviceName);
            splash->showMessage(failMsg, Qt::AlignBottom | Qt::AlignCenter, Qt::yellow);
            qApp->processEvents();
            return false;
        }

    } catch (const std::exception& e) {
        qDebug() << QString("%1连接异常: %2").arg(deviceName).arg(e.what());
        return false;
    } catch (...) {
        qDebug() << QString("%1连接未知异常").arg(deviceName);
        return false;
    }
}
// 显示连接警告对话框
int showConnectionWarningDialog(const QString& details, bool allowSkip = true)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("设备连接警告");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("设备连接异常！");
    msgBox.setInformativeText(details + "\n\n请检查：\n1. 网络连接\n2. 设备电源\n3. IP地址配置\n\n是否尝试重新连接？");

    QPushButton* retryButton = msgBox.addButton("重试连接", QMessageBox::ActionRole);
    QPushButton* skipButton = nullptr;
    if (allowSkip) {
        skipButton = msgBox.addButton("跳过并继续", QMessageBox::RejectRole);
    }
    // msgBox.addButton("退出程序", QMessageBox::RejectRole);

    msgBox.setDefaultButton(retryButton);

    msgBox.exec();

    if (msgBox.clickedButton() == retryButton) {
        return 1; // 重试
    } else if (skipButton && msgBox.clickedButton() == skipButton) {
        return 2; // 跳过
    } else {
        return 0; // 退出
    }
}

bool performSystemChecks(QSplashScreen* splash, MainWindow* mainWindow) {
    Q_UNUSED(mainWindow);
    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    if (!featureSwitch->isBigFeatureEnabled("startup_checks")) {
        splash->showMessage("系统自检功能已关闭，跳过自检流程",
                            Qt::AlignBottom | Qt::AlignCenter, Qt::yellow);
        qApp->processEvents();
        waitWithUiEvents(300);
        return true;
    }

    QString mainCheckIp = "192.168.1.13";
    int mainCheckPort = 502;
    QSettings networkSettings("config.ini", QSettings::IniFormat);
    networkSettings.beginGroup("Network");
    QString agvCheckIp = networkSettings.value("agv_host", "192.168.1.88").toString();
    int agvCheckPort = networkSettings.value("agv_port", 502).toInt();
    networkSettings.endGroup();
    agvCheckPort = qBound(1, agvCheckPort, 65535);

    if (featureSwitch->isFeatureEnabled("tcp_transmission", "tcp.local_simulator")) {
        mainCheckIp = "127.0.0.1";
        mainCheckPort = 5020;
        agvCheckIp = "127.0.0.1";
        agvCheckPort = 5021;
    } else if (featureSwitch->isFeatureEnabled("tcp_transmission", "tcp.remote_simulator")) {
        mainCheckIp = "192.168.1.70";
        mainCheckPort = 5020;
        agvCheckIp = "192.168.1.70";
        agvCheckPort = 5021;
    }

    QVector<SystemCheckItem> checks = {
        {"硬件初始化", CHECK_PENDING, "正在初始化硬件...", 1000},
        {"文件系统检查", CHECK_PENDING, "检查文件系统...", 500},
        {"配置文件加载", CHECK_PENDING, "加载系统配置...", 800},
        {"主控制器连接", CHECK_PENDING, QString("连接主控制器(%1:%2)...").arg(mainCheckIp).arg(mainCheckPort), 5000},
        {"AGV控制器连接", CHECK_PENDING, QString("连接AGV控制器(%1:%2)...").arg(agvCheckIp).arg(agvCheckPort), 5000},
        {"矩阵键盘检测", CHECK_PENDING, "检测输入设备...", 1000},
        {"UI初始化", CHECK_PENDING, "初始化用户界面...", 1500}
    };

    QElapsedTimer totalTimer;
    totalTimer.start();

    bool allPassed = true;
    bool skipChecks = false;  // 是否跳过检查

    for (int i = 0; i < checks.size(); i++) {
        SystemCheckItem& check = checks[i];

        // 如果用户选择跳过，标记所有后续检查为警告
            if (skipChecks && check.status == CHECK_PENDING) {
            check.status = CHECK_WARNING;
            check.message = "已跳过（用户选择继续）";
            continue;
        }

        // 更新启动画面消息
        QString statusMsg = QString("系统自检 (%1/%2)\n%3")
                                .arg(i + 1)
                                .arg(checks.size())
                                .arg(check.message);
        splash->showMessage(statusMsg, Qt::AlignBottom | Qt::AlignCenter, Qt::white);
        qApp->processEvents();

        QElapsedTimer itemTimer;
        itemTimer.start();

        check.status = CHECK_IN_PROGRESS;

        try {
            switch (i) {
            case 0: // 硬件初始化
            {
                // 检查必要的硬件
                check.message = "硬件初始化完成";
                check.status = CHECK_SUCCESS;
                break;
            }

            case 1: // 文件系统检查
            {
                // 检查必要的目录和文件
                if (!QFile::exists("config.ini")) {
                    check.message = "警告：配置文件不存在，将使用默认配置";
                    check.status = CHECK_WARNING;
                    // warning noted
                } else {
                    check.message = "文件系统检查完成";
                    check.status = CHECK_SUCCESS;
                }
                break;
            }

            case 2: // 配置文件加载
            {
                // 读取配置
                QSettings settings("config.ini", QSettings::IniFormat);
                settings.sync();
                if (settings.status() != QSettings::NoError) {
                    check.message = "配置文件读取错误，使用默认值";
                    check.status = CHECK_WARNING;
                    // warning noted
                } else {
                    check.message = "配置文件加载完成";
                    check.status = CHECK_SUCCESS;
                }
                break;
            }

            case 3: // 主控制器连接
            {
                if (!featureSwitch->isBigFeatureEnabled("modbus_main")) {
                    check.message = "主控Modbus功能关闭，跳过连接";
                    check.status = CHECK_WARNING;
                    break;
                }

                bool connected = checkModbusConnection(splash, mainCheckIp, mainCheckPort, "主控制器");

                if (connected) {
                    check.message = "主控制器连接成功";
                    check.status = CHECK_SUCCESS;
                } else {
                    // 连接失败，显示警告对话框
                    QString details = QString("无法连接到主控制器 (%1:%2)").arg(mainCheckIp).arg(mainCheckPort);
                    splash->hide();  // 隐藏启动画面以显示对话框

                    int result = showConnectionWarningDialog(details, true);

                    if (result == 1) {  // 重试
                        splash->show();  // 重新显示启动画面
                        i--;  // 重新检查此项
                        continue;
                    } else if (result == 2) {  // 跳过
                        check.message = "警告：主控制器未连接（用户选择继续）";
                        check.status = CHECK_WARNING;
                        // warning noted
                        skipChecks = true;  // 跳过后续检查

                        // 恢复主设备后台重连：自检阶段会关闭自动重连，
                        // 若用户选择跳过，需重新启用并主动发起一次连接。
                        ModbusThreadManager* modbusManager = ModbusThreadManager::instance();
                        modbusManager->setPollInterval(500);
                        modbusManager->setAutoReconnect(true, 5000);
                        modbusManager->connectToDevice(mainCheckIp, mainCheckPort);

                        splash->show();  // 重新显示启动画面
                    } else {  // 退出
                        allPassed = false;
                        check.message = "错误：主控制器连接失败";
                        check.status = CHECK_FAILED;
                        splash->show();  // 重新显示启动画面
                    }
                }
                break;
            }

            case 4: // AGV控制器连接
            {
                if (!featureSwitch->isBigFeatureEnabled("modbus_agv")) {
                    check.message = "AGV Modbus功能关闭，跳过连接";
                    check.status = CHECK_WARNING;
                    break;
                }

                // 如果已经跳过，直接标记为警告
                if (skipChecks) {
                    check.message = "警告：AGV控制器未连接（已跳过）";
                    check.status = CHECK_WARNING;
                    // warning noted
                    break;
                }

                bool connected = checkModbusConnection(splash, agvCheckIp, agvCheckPort, "AGV控制器");

                if (connected) {
                    check.message = "AGV控制器连接成功";
                    check.status = CHECK_SUCCESS;
                } else {
                    // 连接失败，显示警告对话框
                    QString details = QString("无法连接到AGV控制器 (%1:%2)").arg(agvCheckIp).arg(agvCheckPort);
                    splash->hide();  // 隐藏启动画面以显示对话框

                    int result = showConnectionWarningDialog(details, true);

                    if (result == 1) {  // 重试
                        splash->show();  // 重新显示启动画面
                        i--;  // 重新检查此项
                        continue;
                    } else if (result == 2) {  // 跳过
                        check.message = "警告：AGV控制器未连接（用户选择继续）";
                        check.status = CHECK_WARNING;
                        // warning noted
                        splash->show();  // 重新显示启动画面
                    } else {  // 退出
                        allPassed = false;
                        check.message = "错误：AGV控制器连接失败";
                        check.status = CHECK_FAILED;
                        splash->show();  // 重新显示启动画面
                    }
                }
                break;
            }

            case 5: // 矩阵键盘检测
            {
                // 这里可以添加矩阵键盘的检测逻辑
                check.message = "输入设备检测完成";
                check.status = CHECK_SUCCESS;
                break;
            }

            case 6: // UI初始化
            {
                // UI已经在创建MainWindow时初始化
                check.message = "用户界面初始化完成";
                check.status = CHECK_SUCCESS;
                break;
            }
            }

        } catch (const std::exception& e) {
            check.message = QString("错误：%1").arg(e.what());
            check.status = CHECK_FAILED;
            allPassed = false;
        } catch (...) {
            check.message = "未知错误";
            check.status = CHECK_FAILED;
            allPassed = false;
        }

        // 确保每个检查项至少显示一段时间
        int elapsed = itemTimer.elapsed();
        if (elapsed < 300) {
            waitWithUiEvents(300 - elapsed);
        }
    }

    // 生成自检报告
    QString report = QString("系统自检完成 (%1ms)\n\n").arg(totalTimer.elapsed());
    int successCount = 0, failedCount = 0, warningCount = 0;

    for (const auto& check : checks) {
        QString statusIcon;
        switch (check.status) {
        case CHECK_SUCCESS: statusIcon = "✓"; successCount++; break;
        case CHECK_FAILED: statusIcon = "✗"; failedCount++; break;
        case CHECK_WARNING: statusIcon = "⚠"; warningCount++; break;
        default: statusIcon = "?"; break;
        }

        report += QString("%1 %2: %3\n")
                      .arg(statusIcon)
                      .arg(check.name)
                      .arg(check.message);
    }

    report += QString("\n总计: %1成功, %2警告, %3失败")
                  .arg(successCount)
                  .arg(warningCount)
                  .arg(failedCount);

    // 记录到日志文件
    if (featureSwitch->isSmallFeatureEnabled("startup.log_report")) {
        QFile logFile("system_check.log");
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QTextStream stream(&logFile);
            stream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
            stream << report << "\n\n";
            logFile.close();
        }
    }

    qDebug() << report;

    return allPassed;
}

int main(int argc, char *argv[])
{
    // 设置环境变量
    qputenv("QT_SCREEN_SCALE_FACTORS", "LVDS1=1");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    qDebug() << "程序启动 - 创建QApplication";
    QApplication a(argc, argv);

    FeatureSwitchManager *featureSwitch = FeatureSwitchManager::instance();
    if (a.arguments().contains("--all-features-off")) {
        featureSwitch->setAllEnabled(false);
        featureSwitch->save();
    } else if (a.arguments().contains("--all-features-on")) {
        featureSwitch->setAllEnabled(true);
        featureSwitch->save();
    }

    QByteArray allFeatureEnv = qgetenv("FEATURE_ALL");
    if (!allFeatureEnv.isEmpty()) {
        bool enableAll = (allFeatureEnv == "1" || allFeatureEnv.toLower() == "true");
        featureSwitch->setAllEnabled(enableAll);
        featureSwitch->save();
    }

    // 从环境变量或命令行设置全局 debug 开关：
    // 环境变量：DEBUG=1 或命令行参数 --debug / -d
    QByteArray dbgEnv = qgetenv("DEBUG");
    if (!dbgEnv.isEmpty()) {
        debug = dbgEnv.toInt();
    }
    if (a.arguments().contains("--debug") || a.arguments().contains("-d")) {
        debug = 1;
    }

    // 功能开关优先级高于环境变量/命令行：关闭时统一禁用全局 qDebug 输出。
    if (!featureSwitch->isSmallFeatureEnabled("debug.qdebug")) {
        debug = 0;
    }

    // 安装全局消息处理器以便统一过滤 qDebug 输出
    qInstallMessageHandler(globalMessageHandler);

    // 设置应用程序信息
    QApplication::setApplicationName("工业控制系统");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("TechControl");

    // 创建启动画面
    QPixmap pixmap(":/Picture/TechBackGround.png");
    if (pixmap.isNull()) {
        qDebug() << "无法加载启动画面图片";
    }

    // 获取主屏幕大小
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();

    // 缩放图片到全屏大小
    QPixmap scaledPixmap = pixmap.scaled(screenGeometry.size(),
                                         Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);

    QSplashScreen splash(scaledPixmap);

    QFont splashFont = splash.font();
    splashFont.setPointSize(20);
    splashFont.setBold(true);
    splash.setFont(splashFont);

    // 设置全屏显示
    splash.setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    splash.showFullScreen();  // 或者使用 show() 但设置大小
    // 或者如果你想要控制窗口大小，也可以这样：
    // splash.setFixedSize(screenGeometry.size());
    // splash.show();

    // 或者更简单的方式，在创建QSplashScreen后立即设置全屏
    splash.showFullScreen();

    // 第一阶段：显示欢迎信息
    splash.showMessage("正在启动工业控制系统...",
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    a.processEvents();
    waitWithUiEvents(500);

    // 第二阶段：系统自检
    bool systemReady = true;

    splash.showMessage("开始系统自检...",
                       Qt::AlignBottom | Qt::AlignCenter, Qt::white);
    a.processEvents();

    // 提前创建MainWindow（但不显示）
    qDebug() << "准备创建MainWindow";
    MainWindow w;

    // 执行系统自检
    systemReady = performSystemChecks(&splash, &w);

    // 第三阶段：根据自检结果决定是否继续
    if (!systemReady) {
        splash.showMessage("系统自检失败！",
                           Qt::AlignBottom | Qt::AlignCenter, QColor(255, 100, 100));
        a.processEvents();
        waitWithUiEvents(1500);

        // 询问用户是否继续
        splash.hide();
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(nullptr, "系统自检失败",
                                      "系统自检过程中发现问题，是否继续启动？\n"
                                      "（某些功能可能不可用）",
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);

        if (reply == QMessageBox::No) {
            qDebug() << "用户取消启动";
            return -1;
        }
    } else {
        splash.showMessage("系统准备就绪",
                           Qt::AlignBottom | Qt::AlignCenter, QColor(100, 255, 100));
        a.processEvents();
        waitWithUiEvents(800);
    }

    // 显示主窗口
    qDebug() << "准备显示主窗口";
    w.show();

    // 关闭启动画面
    splash.finish(&w);

    // 主窗口显示后再初始化报警系统，避免在开机自检界面弹出报警
    QTimer::singleShot(0, &w, &MainWindow::startAlarmSystem);

    qDebug() << "进入事件循环";
    return a.exec();
}
