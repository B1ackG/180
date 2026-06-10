#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SO_PATH="${ROOT_DIR}/libmodbus_backend.so"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <app_path> [app_args...]" >&2
  exit 1
fi

APP_PATH="$1"
shift || true

if [[ ! -x "${APP_PATH}" ]]; then
  echo "error: app not executable: ${APP_PATH}" >&2
  exit 1
fi

if [[ ! -f "${SO_PATH}" ]]; then
  echo "info: ${SO_PATH} not found, building first..."
  "${ROOT_DIR}/build_modbus_backend_local.sh"
fi

export MODBUS_BACKEND_LIB="${SO_PATH}"
export AGV_MODBUS_BACKEND_LIB="${SO_PATH}"

echo "MODBUS_BACKEND_LIB=${MODBUS_BACKEND_LIB}"
echo "AGV_MODBUS_BACKEND_LIB=${AGV_MODBUS_BACKEND_LIB}"
echo "Running: ${APP_PATH} $*"

exec "${APP_PATH}" "$@"
