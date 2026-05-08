# Modbus 动态后端（libmodbus_backend.so）保姆级教程

本文整合工程架构、交叉编译、主程序构建、示教器（设备）部署、网络改址与开机启动配置，便于从零复制到新 RK356x / aarch64 示教器。

---

## 1. 架构思路

- **应用层**：`ModbusTCPClient`、`AGVModbusManager` **不再手写 Modbus TCP 报文**。运行时通过 **`QLibrary`** 加载共享库 **`libmodbus_backend.so`**，所有 PDU/MBAP 细节由后端完成。
- **后端 `.so`**：对外暴露稳定的 **C ABI**（`extern "C"`），符号名需与进程内 `resolve` **完全一致**。内部链接 **官方 libmodbus**（推荐静态 **`libmodbus.a`**），协议实现全部在官方库里。
- **好处**：升级协议栈只需替换 **`libmodbus_backend.so`**（及必要时重链静态 libmodbus）；业务进程与 libmodbus 版本通过后端 `.so` 解耦。
- **与 Qt Serial Bus 的关系**：若主程序 `.pro` 仍包含 `QT += serialbus`，交叉套件必须安装对应 **`qtserialbus` 模块**。采用 **纯 QLibrary + 后端 `.so`** 的路线时，主程序 **可不依赖** Qt Serial Bus，仅需 **`QT += network`**（以及现有 GUI 模块），更易在裁剪版 Qt SDK 上通过编译。**二者不要混用两套 TCP 栈**：要么 Serial Bus，要么动态后端 + libmodbus。

---

## 2. 交叉编译官方 libmodbus（aarch64）

使用带 **`configure`** 的发行版源码包（例如 **`libmodbus-3.1.11`**），避免依赖宿主机 `autoreconf`。

典型步骤（前缀、`CC`、`HOST` 按你的 RK3562 工具链调整）：

```bash
tar xf libmodbus-3.1.11.tar.gz
cd libmodbus-3.1.11

./configure \
  --host=aarch64-linux-gnu \
  --prefix=/path/to/third_party/libmodbus-aarch64 \
  --enable-static \
  --disable-shared

make -C src
make -C src install
```

产物示例目录 **`third_party/libmodbus-aarch64`**：

- `include/modbus/modbus.h` 等  
- `lib/libmodbus.a`

---

## 3. 后端动态库：`modbus_backend_c.cpp` → `libmodbus_backend.so`

工程内参考实现：`modbus_backend_c.cpp`（需单独编成 `.so`，**不要**与主程序链进同一可执行文件）。

### 3.1 必须导出符号（与 `QLibrary::resolve` 一致）

- `modbus_backend_create` / `modbus_backend_destroy`
- `modbus_backend_connect`（`host, port, slave_id`）/ `modbus_backend_disconnect` / `modbus_backend_is_connected`
- `modbus_backend_read_holding_registers`
- `modbus_backend_write_single_register`
- 可选：`modbus_backend_read_input_registers`、`modbus_backend_write_multiple_registers`（若进程内 `resolve` 了则必须实现）

### 3.2 内部 API

使用 libmodbus：`modbus_new_tcp`、`modbus_connect`、`modbus_read_registers`、`modbus_write_register` 等。

### 3.3 编译示例（aarch64）

```bash
aarch64-linux-gnu-g++ -shared -fPIC -O2 -Wall \
  modbus_backend_c.cpp \
  -I/path/to/third_party/libmodbus-aarch64/include/modbus \
  /path/to/third_party/libmodbus-aarch64/lib/libmodbus.a \
  -o libmodbus_backend.so
```

链接 **`libmodbus.a`** 可避免设备上再装一份 `libmodbus.so`，部署更简单。

---

## 4. Qt 应用侧行为（环境变量）

| 组件 | 环境变量 | 行为 |
|------|-----------|------|
| `ModbusTCPClient` | `MODBUS_BACKEND_LIB` | 指向 `.so` **完整路径**；未设置或加载失败则报错，**无手写 TCP 回退** |
| `AGVModbusManager` | `AGV_MODBUS_BACKEND_LIB`，若为空则用 `MODBUS_BACKEND_LIB` | 同上 |

实现要点：

- `ensureDynamicBackendLoaded()`：`QLibrary::load`，对必需符号逐一 **`resolve`**；缺一即失败卸载。
- 连接、读写一律通过函数指针调用后端。

---

## 5. 主程序构建（RK3562 / aarch64）

```bash
export PATH=/path/to/toolchain/bin:$PATH
qmake -spec linux-aarch64-gnu-g++ CONFIG+=release
make -j$(nproc)
```

注意：

- **`180.pro` / `624.pro` 一般不编入 `modbus_backend_c.cpp`**：主程序只依赖 **`QLibrary`** 运行时加载后端。
- 交叉编译时 **`qmake` 指向的必须是目标架构的 Qt**，否则会链接出 x86 对象或找不到模块。
- 若 `.pro` 中有 `!qtHaveModule(serialbus): error(...)`，宿主机构建需安装 **`libqt5serialbus5-dev`**；**交叉套件**必须在 sysroot/SDK 里提供 **`serialbus`**，否则会报错。纯动态后端路线可考虑从 `.pro` 去掉 `serialbus`（若代码已不再包含 `QModbus*`）。

---

## 6. 设备部署目录与运行环境（示例）

约定（可按现场调整）：

| 用途 | 路径 |
|------|------|
| 主程序 | `/userfs/app/180` 或 `/userfs/app/624` |
| 后端 | `/userfs/app/lib/libmodbus_backend.so` |
| 自备其它 `.so` | 同样放在 `/userfs/app/lib/` |

### 6.1 `LD_LIBRARY_PATH`

启动前确保：

```bash
export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
```

### 6.2 后端路径（必选其一）

```bash
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so   # 可选：AGV 单独后端时指向另一文件
```

### 6.3 Wayland / Qt

与现场保持一致，通常已有 **`/etc/profile.d/qt_env.sh`**。启动脚本里：

```bash
[ -f /etc/profile.d/qt_env.sh ] && . /etc/profile.d/qt_env.sh
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-wayland}
[ -z "${XDG_RUNTIME_DIR:-}" ] && export XDG_RUNTIME_DIR=/run/user/0
```

### 6.4 推荐：`/etc/profile.d/` 登录环境（可选）

创建 **`/etc/profile.d/180_modbus_env.sh`**（644）：

```sh
export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
```

### 6.5 推荐：统一启动脚本 **`/userfs/app/start_app.sh`**

```sh
#!/bin/sh
set -e
[ -f /etc/profile ] && . /etc/profile
[ -f /etc/profile.d/qt_env.sh ] && . /etc/profile.d/qt_env.sh
export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
if [ -z "${MODBUS_BACKEND_LIB:-}" ] && [ -f /userfs/app/lib/libmodbus_backend.so ]; then
  export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
fi
if [ -z "${AGV_MODBUS_BACKEND_LIB:-}" ] && [ -n "${MODBUS_BACKEND_LIB:-}" ]; then
  export AGV_MODBUS_BACKEND_LIB="$MODBUS_BACKEND_LIB"
fi
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-wayland}
[ -z "${XDG_RUNTIME_DIR:-}" ] && export XDG_RUNTIME_DIR=/run/user/0
exec /userfs/app/624 "$@"   # 或 /userfs/app/180
```

```bash
chmod 755 /userfs/app/start_app.sh
```

---

## 7. 示教器网络：修改 IP 与网关（静态）

现场多为 **Buildroot**，静态网卡在 **`/etc/network/interfaces`**。

### 7.1 编辑前备份

```bash
cp -a /etc/network/interfaces /etc/network/interfaces.bak_$(date +%Y%m%d_%H%M%S)
```

### 7.2 示例（网卡名以设备为准，常见 `eth1`）

```
auto eth1
iface eth1 inet static
address 192.168.0.245
netmask 255.255.255.0
gateway 192.168.0.1
```

**网关必须与网段一致**：例如地址 `192.168.0.x`，网关一般为 `192.168.0.1`；若仍填 `192.168.1.1`，可能导致默认路由异常。

### 7.3 生效方式

```bash
ifdown eth1 2>/dev/null || true
ifup eth1
```

或 **重启**。  
**注意**：改 IP 后 SSH 原地址会断开；开发机网卡需改到同一网段（例如 `192.168.0.x/24`）才能重新登录。

### 7.4 现象说明

若配置错误，可能出现 **`169.254.x.x`** 链路本地地址；此时应优先检查 **`interfaces`**、网线/VLAN 与 PC 网段。

---

## 8. 开机自启动（Buildroot `init.d` 示例）

现场常见 **`/etc/init.d/S99startup`**。示例改为启动 **`624`**：

```sh
#!/bin/sh
sleep 3
. /etc/profile
source /etc/profile.d/qt_env.sh
/bin/setkeycode
/bin/recoverytool

if [ -f /userfs/app/624 ]; then
  echo "Starting custom application..."
  /userfs/app/624 --platform wayland &
fi
```

修改后：

```bash
chmod 755 /etc/init.d/S99startup
```

若你看到仍是 **`190_20260108`**，多半是 **连错了另一台设备**，或 **`S99startup` 未保存到当前机**（多台示教器 IP 重复时尤其容易混淆）。用 **`hostname` / `ip addr`** 确认后再改。

---

## 9. 从「已配置」设备克隆到新机（可选）

在 PC 上备份一套可直接下发的文件，例如：

```bash
mkdir -p ~/backup_teacher
scp root@OLD_IP:/userfs/app/180 ~/backup_teacher/
scp root@OLD_IP:/userfs/app/lib/libmodbus_backend.so ~/backup_teacher/
scp root@OLD_IP:/userfs/app/config.ini ~/backup_teacher/
scp root@OLD_IP:/userfs/app/feature_switches.ini ~/backup_teacher/
scp root@OLD_IP:/etc/profile.d/qt_env.sh ~/backup_teacher/
```

新机上下发并恢复权限后，再按 **§6、§7、§8** 核对环境与启动项。

---

## 10. 验收清单

- [ ] 进程环境中 **`MODBUS_BACKEND_LIB`**（或 AGV 使用 **`AGV_MODBUS_BACKEND_LIB`**）已设置且路径存在。
- [ ] **`file /userfs/app/lib/libmodbus_backend.so`** 显示为 **`aarch64`**（或与你 CPU 一致）。
- [ ] **`ldd /userfs/app/lib/libmodbus_backend.so`** 无 **not found**（若静态链 libmodbus，依赖应很少）。
- [ ] **`cat /proc/<pid>/maps | grep libmodbus_backend`** 能看到已映射。
- [ ] 故意删除或错误路径 **`MODBUS_BACKEND_LIB`** 时，Modbus 操作 **明确报错**，而不是静默成功。
- [ ] **静态 IP / 网关** 与现场交换机网段一致，PC 能 **`ping`** 通设备。

---

## 11. 常见问题

| 现象 | 排查 |
|------|------|
| `Qt module serialbus not found` | 交叉 Qt 无 Serial Bus；装 SDK 模块或从 `.pro` 移除 `serialbus`（若已改用纯动态后端） |
| `未加载Modbus官方动态库，请设置MODBUS_BACKEND_LIB` | 未 export 或 systemd 启动未继承 **`/etc/profile.d`** |
| 后端 `load` 失败 | **`LD_LIBRARY_PATH`**、后端依赖 `.so` 是否在同目录；用 **`ldd`** |
| 改 IP 后 SSH 挂 | 用 **新 IP** 登录；PC 改 **同网段** |
| 开机仍是旧程序 | 查 **`/etc/init.d/S99startup`** 是否指向正确二进制；确认只有一台设备占用该 IP |

---

## 12. 最小命令备忘（设备上）

```bash
# 环境（当前 shell）
export LD_LIBRARY_PATH=/userfs/app/lib:$LD_LIBRARY_PATH
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so

# 启动（示例）
/userfs/app/180 --platform wayland
```

---

## 13. 本次实操保姆级模板（可直接复用）

下面是我们最近在示教器上实际执行并验证通过的一套流程：  
目标：配置动态库环境、修改设备 IP、修改开机启动程序。

### 13.1 连接设备并检查现状

```bash
sshpass -p '1234' ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@192.168.1.245
```

登录后检查：

```bash
hostname
ip -4 addr show eth1
ls -la /userfs/app
sed -n '1,160p' /etc/network/interfaces
sed -n '1,160p' /etc/init.d/S99startup
```

### 13.2 下发运行文件（示例清单）

确保设备具备：

- `/userfs/app/180`
- `/userfs/app/lib/libmodbus_backend.so`
- `/userfs/app/config.ini`
- `/userfs/app/feature_switches.ini`
- `/etc/profile.d/qt_env.sh`

### 13.3 配置动态库运行环境（持久）

创建 `/etc/profile.d/180_modbus_env.sh`：

```bash
cat > /etc/profile.d/180_modbus_env.sh <<'EOF'
export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
EOF
chmod 644 /etc/profile.d/180_modbus_env.sh
```

可选创建统一启动脚本 `/userfs/app/start_180.sh`：

```bash
cat > /userfs/app/start_180.sh <<'EOF'
#!/bin/sh
set -e
[ -f /etc/profile ] && . /etc/profile
[ -f /etc/profile.d/qt_env.sh ] && . /etc/profile.d/qt_env.sh
export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-wayland}
[ -z "${XDG_RUNTIME_DIR:-}" ] && export XDG_RUNTIME_DIR=/run/user/0
exec /userfs/app/180 "$@"
EOF
chmod 755 /userfs/app/start_180.sh
```

### 13.4 修改开机启动为 `/userfs/app/180`

> Buildroot 设备一般改 `/etc/init.d/S99startup`。

```bash
cp -a /etc/init.d/S99startup /etc/init.d/S99startup.bak_$(date +%Y%m%d_%H%M%S)
cat > /etc/init.d/S99startup <<'EOF'
#!/bin/sh

sleep 3

. /etc/profile
source /etc/profile.d/qt_env.sh

/bin/setkeycode
/bin/recoverytool

export LD_LIBRARY_PATH=/userfs/app/lib:${LD_LIBRARY_PATH:-}
export MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so
export AGV_MODBUS_BACKEND_LIB=/userfs/app/lib/libmodbus_backend.so

if [ -f /userfs/app/180 ] ;then
   echo "Starting custom application..."
    /userfs/app/180 --platform wayland &
elif [ -f /userfs/app/SmartRobot ] ;then
    /userfs/app/SmartRobot --platform wayland &
else
    sh /etc/init.d/app_setup.sh
fi
EOF
chmod 755 /etc/init.d/S99startup
```

### 13.5 修改示教器 IP（示例：`192.168.1.245 -> 192.168.1.24`）

```bash
cp -a /etc/network/interfaces /etc/network/interfaces.bak_$(date +%Y%m%d_%H%M%S)
sed -ri 's/^address[[:space:]]+.*/address 192.168.1.24/' /etc/network/interfaces
```

如需同步网关（同网段建议）：

```bash
sed -ri 's/^gateway[[:space:]]+.*/gateway 192.168.1.254/' /etc/network/interfaces
```

重载网络：

```bash
ifdown eth1 || true
ifup eth1
```

### 13.6 变更后验证

设备上：

```bash
ip -4 addr show eth1
sed -n '1,120p' /etc/init.d/S99startup
. /etc/profile.d/180_modbus_env.sh
echo "$MODBUS_BACKEND_LIB"
echo "$AGV_MODBUS_BACKEND_LIB"
```

PC 上：

```bash
ping -c 2 192.168.1.24
ping -c 2 192.168.1.245
```

预期：新 IP 通，旧 IP 不通。

---

*文档版本：基于工程实践整理；现场路径与网卡名以设备为准。*
