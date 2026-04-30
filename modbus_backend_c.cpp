#include <cerrno>
#include <mutex>
#include <string>
#include <vector>

#include <modbus/modbus.h>

namespace {
constexpr int kMaxRegistersPerRead = 125;
constexpr int kMaxRegistersPerWrite = 123;

struct Backend {
    modbus_t *ctx = nullptr;
    int slaveId = 1;
    std::mutex mu;
};
} // namespace

extern "C" {

void *modbus_backend_create()
{
    return new Backend();
}

void modbus_backend_destroy(void *handle)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b) {
        return;
    }
    if (b->ctx) {
        modbus_close(b->ctx);
        modbus_free(b->ctx);
        b->ctx = nullptr;
    }
    delete b;
}

int modbus_backend_connect(void *handle, const char *host, int port, int slave_id)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b || !host || port <= 0 || port > 65535) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(b->mu);

    if (b->ctx) {
        modbus_close(b->ctx);
        modbus_free(b->ctx);
        b->ctx = nullptr;
    }

    b->ctx = modbus_new_tcp(host, port);
    if (!b->ctx) {
        return 0;
    }

    b->slaveId = slave_id <= 0 ? 1 : slave_id;
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) {
        modbus_free(b->ctx);
        b->ctx = nullptr;
        return 0;
    }

    timeval responseTimeout {};
    responseTimeout.tv_sec = 1;
    responseTimeout.tv_usec = 0;
    modbus_set_response_timeout(b->ctx, responseTimeout.tv_sec, responseTimeout.tv_usec);

    if (modbus_connect(b->ctx) != 0) {
        modbus_free(b->ctx);
        b->ctx = nullptr;
        return 0;
    }

    return 1;
}

void modbus_backend_disconnect(void *handle)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b) {
        return;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    if (b->ctx) {
        modbus_close(b->ctx);
        modbus_free(b->ctx);
        b->ctx = nullptr;
    }
}

int modbus_backend_is_connected(void *handle)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    return b->ctx ? 1 : 0;
}

int modbus_backend_read_holding_registers(void *handle, int start_addr, int count, uint16_t *out_values, int out_capacity)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b || !out_values || count <= 0 || out_capacity < count || count > kMaxRegistersPerRead) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    if (!b->ctx) {
        return -1;
    }
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) {
        return -1;
    }
    return modbus_read_registers(b->ctx, start_addr, count, out_values);
}

int modbus_backend_read_input_registers(void *handle, int start_addr, int count, uint16_t *out_values, int out_capacity)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b || !out_values || count <= 0 || out_capacity < count || count > kMaxRegistersPerRead) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    if (!b->ctx) {
        return -1;
    }
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) {
        return -1;
    }
    return modbus_read_input_registers(b->ctx, start_addr, count, out_values);
}

int modbus_backend_write_single_register(void *handle, int addr, uint16_t value)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    if (!b->ctx) {
        return 0;
    }
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) {
        return 0;
    }
    return modbus_write_register(b->ctx, addr, value) == 1 ? 1 : 0;
}

int modbus_backend_write_multiple_registers(void *handle, int start_addr, const uint16_t *values, int count)
{
    auto *b = static_cast<Backend *>(handle);
    if (!b || !values || count <= 0 || count > kMaxRegistersPerWrite) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(b->mu);
    if (!b->ctx) {
        return 0;
    }
    if (modbus_set_slave(b->ctx, b->slaveId) != 0) {
        return 0;
    }
    return modbus_write_registers(b->ctx, start_addr, count, values) == count ? 1 : 0;
}

} // extern "C"
