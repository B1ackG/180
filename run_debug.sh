#!/bin/sh
# 示例启动脚本：方便使用 DEBUG 环境变量 和 QT_LOGGING_RULES
# 用法：
#   ./run_debug.sh [app_path] [mode] [rules]
# 示例：
#   ./run_debug.sh              # 使用默认 ./your_app（需要先修改脚本中的默认）
#   ./run_debug.sh ./build/myapp debug
#   ./run_debug.sh ./build/myapp mainwindow
#   ./run_debug.sh ./build/myapp custom "app.mainwindow.debug=true;app.modbustcpclient.debug=false"

APP=${1:-./build/RK3562J_ARM-Release/190_20260108}
MODE=${2:-normal}

case "$MODE" in
  normal)
    echo "Running: $APP"
    exec "$APP"
    ;;
  debug)
    echo "Running with DEBUG=1 (enable all qDebug)"
    DEBUG=1 exec "$APP"
    ;;
  mainwindow)
    echo "Running with app.mainwindow enabled"
    DEBUG=1 QT_LOGGING_RULES="app.mainwindow.debug=true" exec "$APP"
    ;;
  custom)
    RULES=${3:-""}
    echo "Running with custom QT_LOGGING_RULES: $RULES"
    DEBUG=1 QT_LOGGING_RULES="$RULES" exec "$APP"
    ;;
  *)
    cat <<-USAGE
Usage: $0 [app_path] [mode] [rules]
  mode:
    normal     - 直接运行（默认）
    debug      - 全局开启 DEBUG=1
    mainwindow - 仅开启 app.mainwindow 类别日志
    custom     - 自定义 QT_LOGGING_RULES（第三个参数传规则字符串）

Examples:
  $0 ./build/my_app debug
  $0 ./build/my_app mainwindow
  $0 ./build/my_app custom "app.mainwindow.debug=true;app.modbustcpclient.debug=false"
USAGE
    exit 1
    ;;
esac
