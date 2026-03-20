**调试/日志 使用说明**

- **目的**: 本说明介绍如何启用或限制程序中的 `qDebug()`/`qCDebug()` 输出。工程中已实现两种机制：全局开关 `debug`（在运行时启/关所有 `qDebug`），以及按类别的 `QLoggingCategory`（运行时可细粒度控制）。

- **相关源码**:
  - 全局开关定义: [debug.h](debug.h)
  - 全局消息处理与 `debug` 初始化: [main.cpp](main.cpp)
  - 示例类别: `app.mainwindow` 在 [mainwindow.cpp](mainwindow.cpp)
  - 示例类别: `app.modbustcpclient` 在 [modbustcpclient.cpp](modbustcpclient.cpp)

- **快速启用（全局）**: 默认情况下 `main.cpp` 安装了一个消息处理器，在 `debug==0` 时会丢弃所有 `QtDebugMsg`（即 `qDebug()` / `qCDebug()`）。要开启调试输出：

```
export DEBUG=1
./your_app [--debug]
```

或者直接通过命令行参数：

```
./your_app --debug
```

注: `qWarning()`、`qCritical()` 不受此开关过滤，仍然会输出。

- **按类别细化（QLoggingCategory）**: 工程中部分文件使用了 `Q_LOGGING_CATEGORY`，你可以用 `QT_LOGGING_RULES` 在运行时启/禁特定类别。

示例：只打开 `app.mainwindow`，关闭 `app.modbustcpclient`：

```
DEBUG=1 QT_LOGGING_RULES="app.mainwindow.debug=true;app.modbustcpclient.debug=false" ./your_app
```

或单独指定：

```
export DEBUG=1
export QT_LOGGING_RULES="app.mainwindow.debug=true"
./your_app
```

说明：由于工程同时使用了全局 `debug` 变量和 `qInstallMessageHandler`，必须先将 `DEBUG=1`（或 `--debug`）打开全局 qDebug 输出，然后再使用 `QT_LOGGING_RULES` 对类别进行细化，否则所有 debug 级别消息会被全局过滤掉。

- **在代码中控制类别（可选）**: 你可以在程序初始化阶段调用：

```
QLoggingCategory::setFilterRules("app.mainwindow.debug=true;app.modbustcpclient.debug=false");
```

把这行放在 `QApplication` 创建后且在希望产生日志之前。

- **如何为新文件添加类别**:
  1. 在文件顶部添加 `#include <QLoggingCategory>`。
  2. 声明类别：`Q_LOGGING_CATEGORY(lcFoo, "app.foo")`。
  3. 在日志处使用 `qCDebug(lcFoo) << "消息";`。
  4. （可选）若希望文件中原有大量 `qDebug()` 在不改动每一行的前提下使用该类别，可在文件顶部添加局部宏：

```
#ifdef qDebug
#undef qDebug
#endif
#define qDebug() qCDebug(lcFoo)
```

- **快速检查**:
  - 全部打开：`DEBUG=1 ./your_app`。
  - 只看主窗口相关日志：`DEBUG=1 QT_LOGGING_RULES="app.mainwindow.debug=true" ./your_app`。

如果需要，我可以：
- 生成一个示例 `run_debug.sh` 启动脚本；
- 或者把工程中更多文件按类别化并添加说明。 
