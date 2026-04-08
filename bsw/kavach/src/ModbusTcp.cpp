#include "ModbusTcp.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>

ModbusTcp::ModbusTcp(const std::string& ip, uint16_t port)
    : m_ip(ip), m_port(port) {
    printf("[ModbusTcp] Target: %s:%u\n", ip.c_str(), port);
}

ModbusTcp::~ModbusTcp() {
    disconnect();
}

Std_ReturnType ModbusTcp::connect() {
    // Create Modbus TCP context
    m_ctx = modbus_new_tcp(m_ip.c_str(), static_cast<int>(m_port));
    if (!m_ctx) {
        printf("[ModbusTcp] ERROR: Cannot create context\n");
        return E_NOT_OK;
    }

    // Set response timeout to 2 seconds
    modbus_set_response_timeout(m_ctx, 2, 0);

    // Try to connect
    if (modbus_connect(m_ctx) == -1) {
        printf("[ModbusTcp] Cannot connect to %s:%u -> %s\n",
               m_ip.c_str(), m_port, modbus_strerror(errno));
        printf("[ModbusTcp] Running in OFFLINE mode - writes skipped\n");
        modbus_free(m_ctx);
        m_ctx       = nullptr;
        m_connected = false;
        return E_NOT_OK;
    }

    m_connected = true;
    printf("[ModbusTcp] Connected to %s:%u\n", m_ip.c_str(), m_port);
    return E_OK;
}

void ModbusTcp::disconnect() {
    if (m_ctx) {
        if (m_connected) modbus_close(m_ctx);
        modbus_free(m_ctx);
        m_ctx       = nullptr;
        m_connected = false;
        printf("[ModbusTcp] Disconnected\n");
    }
}

Std_ReturnType ModbusTcp::reconnect() {
    disconnect();
    return connect();
}

Std_ReturnType ModbusTcp::writeDtcRegisters(DemCore& dem) {
    if (!m_connected || !m_ctx) return E_NOT_OK;

    uint8_t  count = dem.getEventMemoryCount();
    uint16_t regs[14] = {};

    // Register 0: DTC count
    regs[MB_REG_DTC_COUNT] = count;

    // Registers 1-9: up to 3 DTCs (3 regs each: high, low, uds)
    uint8_t limit = count < 3U ? count : 3U;
    for (uint8_t i = 0; i < limit; i++) {
        auto e = dem.getEventMemoryEntry(i);
        uint16_t baseReg = static_cast<uint16_t>(1U + i * 3U);
        regs[baseReg + 0] = static_cast<uint16_t>((e.dtc >> 16) & 0xFFFFU);
        regs[baseReg + 1] = static_cast<uint16_t>( e.dtc        & 0xFFFFU);
        regs[baseReg + 2] = e.udsStatusByte;
    }

    int rc = modbus_write_registers(m_ctx, MB_REG_DTC_COUNT,
                                    10, regs);
    if (rc == -1) {
        printf("[ModbusTcp] Write DTC regs FAILED: %s\n",
               modbus_strerror(errno));
        return E_NOT_OK;
    }
    printf("[ModbusTcp] DTC registers written: count=%d\n", count);
    return E_OK;
}

Std_ReturnType ModbusTcp::writeEventCoils(uint8_t coilIndex, bool active) {
    if (!m_connected || !m_ctx) return E_NOT_OK;

    int rc = modbus_write_bit(m_ctx, coilIndex, active ? 1 : 0);
    if (rc == -1) {
        printf("[ModbusTcp] Write coil %d FAILED: %s\n",
               coilIndex, modbus_strerror(errno));
        return E_NOT_OK;
    }
    return E_OK;
}

Std_ReturnType ModbusTcp::writeMode(uint8_t mode) {
    if (!m_connected || !m_ctx) return E_NOT_OK;
    int rc = modbus_write_register(m_ctx, MB_REG_KAVACH_MODE, mode);
    if (rc == -1) return E_NOT_OK;
    printf("[ModbusTcp] Mode register written: 0x%02X\n", mode);
    return E_OK;
}

Std_ReturnType ModbusTcp::writeSpeed(uint8_t actual, uint8_t permitted) {
    if (!m_connected || !m_ctx) return E_NOT_OK;
    uint16_t regs[2] = { actual, permitted };
    int rc = modbus_write_registers(m_ctx, MB_REG_SPEED_ACTUAL, 2, regs);
    if (rc == -1) return E_NOT_OK;
    return E_OK;
}

Std_ReturnType ModbusTcp::writeSession(uint8_t session) {
    if (!m_connected || !m_ctx) return E_NOT_OK;
    int rc = modbus_write_register(m_ctx, MB_REG_SESSION, session);
    if (rc == -1) return E_NOT_OK;
    return E_OK;
}

Std_ReturnType ModbusTcp::readInputRegister(uint16_t address,
                                             uint16_t& valueOut) {
    if (!m_connected || !m_ctx) return E_NOT_OK;
    uint16_t val = 0;
    int rc = modbus_read_input_registers(m_ctx, address, 1,
                                          reinterpret_cast<uint16_t*>(&val));
    if (rc == -1) return E_NOT_OK;
    valueOut = val;
    return E_OK;
}

Std_ReturnType ModbusTcp::pushSnapshot(DemCore& dem,
                                        uint8_t mode,
                                        uint8_t speedActual,
                                        uint8_t speedPermit,
                                        uint8_t session) {
    if (!m_connected || !m_ctx) {
        printf("[ModbusTcp] OFFLINE - snapshot skipped\n");
        return E_NOT_OK;
    }

    printf("[ModbusTcp] Pushing full snapshot...\n");
    writeDtcRegisters(dem);
    writeMode(mode);
    writeSpeed(speedActual, speedPermit);
    writeSession(session);
    return E_OK;
}

Std_ReturnType ModbusTcp::readHoldingRegister(uint16_t address, uint16_t& valueOut) {
    // DMI closes connection after each request — reconnect every time
    disconnect();
    if (connect() != E_OK) return E_NOT_OK;

    uint16_t val = 0;
    if (modbus_read_registers(m_ctx, address, 1, &val) == -1) {
               address, modbus_strerror(errno));
        disconnect();
        return E_NOT_OK;
    }
    valueOut = val;
    disconnect();
    return E_OK;
}
