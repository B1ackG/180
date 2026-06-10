#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SO_PATH="${ROOT_DIR}/libmodbus_backend.so"
SRC_PATH="${ROOT_DIR}/modbus_backend_c.cpp"

if [[ ! -f "${SRC_PATH}" ]]; then
  echo "error: source file not found: ${SRC_PATH}" >&2
  exit 1
fi

echo "[1/4] Detecting build mode..."
USE_PKG_CONFIG=0
if pkg-config --exists libmodbus 2>/dev/null; then
  USE_PKG_CONFIG=1
  echo "  - Using system libmodbus via pkg-config."
else
  echo "  - System libmodbus not found, using local third_party build."
fi

if [[ "${USE_PKG_CONFIG}" -eq 0 ]]; then
  TP_DIR="${ROOT_DIR}/third_party"
  LIBMODBUS_VER="3.1.11"
  LIBMODBUS_ARCHIVE="libmodbus-${LIBMODBUS_VER}.tar.gz"
  LIBMODBUS_SRC_DIR="${TP_DIR}/libmodbus-${LIBMODBUS_VER}"
  LIBMODBUS_PREFIX="${TP_DIR}/libmodbus-local"

  mkdir -p "${TP_DIR}"

  if [[ ! -d "${LIBMODBUS_SRC_DIR}" ]]; then
    echo "[2/4] Downloading libmodbus source..."
    curl -L --fail -o "${TP_DIR}/${LIBMODBUS_ARCHIVE}" \
      "https://github.com/stephane/libmodbus/releases/download/v${LIBMODBUS_VER}/${LIBMODBUS_ARCHIVE}"
    tar xf "${TP_DIR}/${LIBMODBUS_ARCHIVE}" -C "${TP_DIR}"
  fi

  if [[ ! -f "${LIBMODBUS_PREFIX}/lib/libmodbus.a" ]]; then
    echo "[3/4] Building local static libmodbus..."
    pushd "${LIBMODBUS_SRC_DIR}" >/dev/null
    ./configure --prefix="${LIBMODBUS_PREFIX}" --enable-static --disable-shared
    make -C src -j"$(nproc)"
    make -C src install
    popd >/dev/null
  fi

  echo "[4/4] Building ${SO_PATH}..."
  g++ -shared -fPIC -O2 -Wall -std=c++17 \
    "${SRC_PATH}" \
    -I"${LIBMODBUS_PREFIX}/include" \
    "${LIBMODBUS_PREFIX}/lib/libmodbus.a" \
    -o "${SO_PATH}"
else
  echo "[2/4] Building ${SO_PATH}..."
  g++ -shared -fPIC -O2 -Wall -std=c++17 \
    "${SRC_PATH}" \
    $(pkg-config --cflags --libs libmodbus) \
    -o "${SO_PATH}"
fi

echo "[verify] file:"
file "${SO_PATH}"
echo "[verify] symbols:"
nm -D "${SO_PATH}" | rg "modbus_backend_(create|destroy|connect|disconnect|is_connected|read_holding_registers|write_single_register)" || true
echo "[verify] ldd:"
ldd "${SO_PATH}" || true

echo "Done: ${SO_PATH}"
