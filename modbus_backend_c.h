#ifndef MODBUS_BACKEND_C_H
#define MODBUS_BACKEND_C_H

#include <cstdint>

extern "C" {

void *modbus_backend_create();
void modbus_backend_destroy(void *handle);
int modbus_backend_connect(void *handle, const char *host, int port, int slaveId);
void modbus_backend_disconnect(void *handle);
int modbus_backend_is_connected(void *handle);
int modbus_backend_read_holding_registers(void *handle, int start, int count,
                                          std::uint16_t *dest, int capacity);
int modbus_backend_read_input_registers(void *handle, int start, int count,
                                        std::uint16_t *dest, int capacity);
int modbus_backend_write_single_register(void *handle, int address,
                                         std::uint16_t value);
int modbus_backend_write_multiple_registers(void *handle, int start,
                                            const std::uint16_t *values, int count);

}

#endif
