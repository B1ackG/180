#!/usr/bin/env bash
# 本地一键测试。不连接真实 PLC，不启动 180 主界面。
# 待停止状态只在进程内存中；真实 PLC 仍需通讯看门狗和实机验收。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD="${ROOT}/build-tests"
MODE="${1:-all}"
export MODBUS_SIM_ROOT="${MODBUS_SIM_ROOT:-/home/l/myAssistant/my-assistant}"
export MODBUS_BACKEND_LIB="${MODBUS_BACKEND_LIB:-${ROOT}/libmodbus_backend.so}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

if ! command -v qmake >/dev/null 2>&1; then
    echo "错误: 未找到 qmake" >&2
    exit 1
fi
if ! command -v make >/dev/null 2>&1; then
    echo "错误: 未找到 make" >&2
    exit 1
fi

run_layer() {
    local layer="$1"
    if [[ "${layer}" == "integration" ]]; then
        if [[ ! -f "${MODBUS_BACKEND_LIB}" ]]; then
            echo "错误: 未找到 MODBUS_BACKEND_LIB=${MODBUS_BACKEND_LIB}" >&2
            exit 1
        fi
        if [[ ! -f "${MODBUS_SIM_ROOT}/modbusslave.cpp" || ! -f "${MODBUS_SIM_ROOT}/modbusslave.h" ]]; then
            echo "错误: MODBUS_SIM_ROOT 缺少 modbusslave: ${MODBUS_SIM_ROOT}" >&2
            exit 1
        fi
    fi

    local dir="${BUILD}/${layer}"
    rm -rf "${dir}"
    mkdir -p "${dir}"
    (
        cd "${dir}"
        qmake "${ROOT}/tests/${layer}/${layer}.pro"
        make -j"$(nproc)"
    )

    local found=0
    local bin
    while IFS= read -r bin; do
        found=1
        echo "== ${bin} =="
        "${bin}"
    done < <(find "${dir}" -type f -name 'tst_*' -perm -111 | sort)
    if [[ "${found}" -ne 1 ]]; then
        echo "错误: ${layer} 没有生成测试程序" >&2
        exit 1
    fi
}

case "${MODE}" in
    unit|component|integration)
        run_layer "${MODE}"
        ;;
    all)
        run_layer unit
        run_layer component
        run_layer integration
        ;;
    *)
        echo "错误: 参数必须是 unit、component、integration 或 all" >&2
        exit 1
        ;;
esac
