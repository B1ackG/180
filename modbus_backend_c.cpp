/*
 * =============================================================================
 * 用「四层」读本文件（先看这段再往下翻）
 * -----------------------------------------------------------------------------
 * libmodbus.a = 真正的 Modbus（连网、拼帧、粘包半包、读写寄存器）—— 很厚
 * 本文件      = 外壳，几乎不做协议，只做下面四件事：
 *
 *   【检查】参数合不合法？handle 空不空？有没有连上？
 *   【加锁】同一连接别被两个线程同时用
 *   【接线】把主程序的调用，转到官方函数（modbus_xxx）
 *   【会话】create/destroy/connect 管「盒子」的一生
 *
 * 主程序从不直接调 .a，只通过 QLibrary 调下面这些 modbus_backend_xxx。
 * 所以本文件像插座转接头：电（协议）在 .a 里，转接头只负责插上、防呆、别抢线。
 *
 * 每个函数里我会用【检查】【加锁】【接线】标出来，方便你对照。
 * =============================================================================
 */

#include <cerrno>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

#include <modbus/modbus.h> /* .a 的说明书：官方函数声明都在这 */

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

namespace {

/* 【检查】用：一次最多读/写多少个寄存器（协议上限），先挡荒唐参数 */
constexpr int kMaxRegistersPerRead = 125;
constexpr int kMaxRegistersPerWrite = 123;

/*
 * 【会话】用的小盒子 Backend
 * - 主程序拿到的 void*，其实就是 Backend*
 * - ctx：官方连接对象；nullptr = 还没连上 PLC
 * - mu ：这把锁专管「别多人同时碰这个 ctx」
 *
 * 注意：盒子里没有「Modbus 协议实现」，只有「指向官方对象的指针」。
 */
struct Backend {
    modbus_t *ctx = nullptr;
    int slaveId = 1;
    std::mutex mu;
};

} // namespace

extern "C" { /* 名字保持 C 风格，QLibrary 才能 resolve 到 */

/* -------------------------------------------------------------------------- */
/* 【会话】造一个空盒子（还没连 PLC）                                           */
/* -------------------------------------------------------------------------- */
void *modbus_backend_create()
{
    /* 没有【接线】到 .a：这里只 new 盒子，官方 ctx 还是空的 */
    return new Backend();
}

/* -------------------------------------------------------------------------- */
/* 【会话】扔掉盒子；若还连着，先让 .a 关掉连接                                 */
/* -------------------------------------------------------------------------- */
void modbus_backend_destroy(void *handle)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b) {
        return;
    }

    /* 【接线】若连过：调用 .a 的 close/free，真正释放官方资源 */
    if (b->ctx) {
        modbus_close(b->ctx); /* → 进 libmodbus.a */
        modbus_free(b->ctx);  /* → 进 libmodbus.a */
        b->ctx = nullptr;
    }

    delete b; /* 扔掉我们的盒子 */
}

/* -------------------------------------------------------------------------- */
/* 【会话】连接 PLC                                                             */
/* 流程：【检查】→【加锁】→（必要时清旧连接）→【接线】连调一串官方函数           */
/* -------------------------------------------------------------------------- */
int modbus_backend_connect(void *handle, const char *host, int port, int slave_id)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】盒子、IP、端口是否像样 */
    if (!b || !host || port <= 0 || port > 65535) {
        return 0; /* 失败；下面成功返回 1 */
    }

    /* 【加锁】连接过程中不允许别人同时 read/write */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 若以前连过：先【接线】拆掉旧的，再新建（重连） */
    if (b->ctx) {
        modbus_close(b->ctx); /* → .a */
        modbus_free(b->ctx);  /* → .a */
        b->ctx = nullptr;
    }

    /* -------- 以下整段都是【接线】：真正干活的全是 modbus_xxx（.a 里） -------- */

    /* 1) 创建 TCP 版官方上下文（此时可能还没连上） */
    b->ctx = modbus_new_tcp(host, port); /* → .a */
    if (!b->ctx) {
        return 0;
    }

    /* 2) 设置从站号 */
    b->slaveId = slave_id <= 0 ? 1 : slave_id;
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) { /* → .a */
        modbus_free(b->ctx); /* → .a */
        b->ctx = nullptr;
        return 0;
    }

    /* 3) 设置「等多久算超时」（默认 1 秒；可用环境变量改）—— 仍是调 .a */
    timeval responseTimeout {};
    responseTimeout.tv_sec = 1;
    responseTimeout.tv_usec = 0;
    {
        const char *timeoutEnv = std::getenv("MODBUS_RESPONSE_TIMEOUT_MS");
        if (timeoutEnv && *timeoutEnv) {
            const int timeoutMs = std::atoi(timeoutEnv);
            if (timeoutMs >= 200) {
                responseTimeout.tv_sec = timeoutMs / 1000;
                responseTimeout.tv_usec = (timeoutMs % 1000) * 1000;
            }
        }
    }
    modbus_set_response_timeout(b->ctx, responseTimeout.tv_sec, responseTimeout.tv_usec); /* → .a */

    /* 4) 真正 TCP 连接；粘包等细节从这里开始由 .a 内部处理 */
    if (modbus_connect(b->ctx) != 0) { /* → .a ：这里才算连上 PLC */
        modbus_free(b->ctx);
        b->ctx = nullptr;
        return 0;
    }

    /* 5) 可选：改 socket 选项（不是 Modbus 协议本身，不懂可跳过） */
    const int sock = modbus_get_socket(b->ctx); /* → .a ：取出底层套接字 */
    if (sock >= 0) {
        const int one = 1;
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
    }

    return 1; /* 外壳只负责报告成功；协议细节都在上面那些 → .a 调用里 */
}

/* -------------------------------------------------------------------------- */
/* 【会话】断开，但保留盒子（以后还能再 connect）                               */
/* -------------------------------------------------------------------------- */
void modbus_backend_disconnect(void *handle)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b) {
        return;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 【接线】 */
    if (b->ctx) {
        modbus_close(b->ctx); /* → .a */
        modbus_free(b->ctx);  /* → .a */
        b->ctx = nullptr;
    }
}

/* -------------------------------------------------------------------------- */
/* 查询是否已连接：外壳自己看 ctx 空不空即可，不必问 PLC                         */
/* -------------------------------------------------------------------------- */
int modbus_backend_is_connected(void *handle)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b) {
        return 0;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 无【接线】到协议读写；只看指针 */
    return b->ctx ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
/* 读保持寄存器（功能码 0x03）                                                   */
/* 典型结构：【检查】→【加锁】→【接线】一行交给 .a                               */
/* -------------------------------------------------------------------------- */
int modbus_backend_read_holding_registers(void *handle, int start_addr, int count,
                                          uint16_t *out_values, int out_capacity)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】盒子、输出缓冲、数量是否合法 */
    if (!b || !out_values || count <= 0 || out_capacity < count || count > kMaxRegistersPerRead) {
        return -1;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 【检查】连上了吗？ */
    if (!b->ctx) {
        return -1;
    }

    /* 【接线】先设从站，再读——下面两行才是「真正的 Modbus」 */
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) { /* → .a */
        return -1;
    }
    return modbus_read_registers(b->ctx, start_addr, count, out_values); /* → .a ★核心 */
    /* 粘包半包、MBAP、重试等：都在这一行调用进去的 .a 代码里，不在本文件 */
}

/* -------------------------------------------------------------------------- */
/* 读输入寄存器（功能码 0x04）—— 结构和上面几乎一样，只是换了一个官方函数         */
/* -------------------------------------------------------------------------- */
int modbus_backend_read_input_registers(void *handle, int start_addr, int count,
                                        uint16_t *out_values, int out_capacity)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b || !out_values || count <= 0 || out_capacity < count || count > kMaxRegistersPerRead) {
        return -1;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 【检查】 */
    if (!b->ctx) {
        return -1;
    }

    /* 【接线】 */
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) { /* → .a */
        return -1;
    }
    return modbus_read_input_registers(b->ctx, start_addr, count, out_values); /* → .a ★ */
}

/* -------------------------------------------------------------------------- */
/* 写单个寄存器（功能码 0x06）                                                   */
/* -------------------------------------------------------------------------- */
int modbus_backend_write_single_register(void *handle, int addr, uint16_t value)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b) {
        return 0;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 【检查】 */
    if (!b->ctx) {
        return 0;
    }

    /* 【接线】 */
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) { /* → .a */
        return 0;
    }
    /* ★核心：真正写寄存器在 .a；外壳只把「成功/失败」收成 1/0 */
    return modbus_write_register(b->ctx, addr, value) == 1 ? 1 : 0; /* → .a */
}

/* -------------------------------------------------------------------------- */
/* 写多个寄存器（功能码 0x10）                                                   */
/* -------------------------------------------------------------------------- */
int modbus_backend_write_multiple_registers(void *handle, int start_addr,
                                            const uint16_t *values, int count)
{
    auto *b = static_cast<Backend *>(handle);

    /* 【检查】 */
    if (!b || !values || count <= 0 || count > kMaxRegistersPerWrite) {
        return 0;
    }

    /* 【加锁】 */
    std::lock_guard<std::mutex> lock(b->mu);

    /* 【检查】 */
    if (!b->ctx) {
        return 0;
    }

    /* 【接线】 */
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) { /* → .a */
        return 0;
    }
    return modbus_write_registers(b->ctx, start_addr, count, values) == count ? 1 : 0; /* → .a ★ */
}

} // extern "C"

/*
 * =============================================================================
 * 读完自测（能答上来就说明看懂了）：
 * 1. 拼 Modbus 报文、处理粘包的代码在本文件吗？ → 不在，在 libmodbus.a
 * 2. 本文件大部分 if 在干什么？ → 【检查】防呆
 * 3. mutex 在干什么？ → 【加锁】防多线程抢同一连接
 * 4. modbus_read_registers 那一行在干什么？ → 【接线】进入 .a 真正通讯
 * =============================================================================
 */
