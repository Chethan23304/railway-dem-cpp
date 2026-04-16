#pragma once
#include <string>
#include <modbus/modbus.h>
#include "StdTypes.hpp"
#include "DemCore.hpp"

// Modbus holding register addresses
constexpr uint16_t MB_REG_DTC_COUNT    = 0U;   // number of active DTCs
constexpr uint16_t MB_REG_DTC1_HIGH    = 1U;   // DTC1 upper 16 bits
constexpr uint16_t MB_REG_DTC1_LOW     = 2U;   // DTC1 lower 16 bits
constexpr uint16_t MB_REG_UDS1         = 3U;   // UDS status of DTC1
constexpr uint16_t MB_REG_DTC2_HIGH    = 4U;
constexpr uint16_t MB_REG_DTC2_LOW     = 5U;
constexpr uint16_t MB_REG_UDS2         = 6U;
constexpr uint16_t MB_REG_DTC3_HIGH    = 7U;
constexpr uint16_t MB_REG_DTC3_LOW     = 8U;
constexpr uint16_t MB_REG_UDS3         = 9U;
constexpr uint16_t MB_REG_KAVACH_MODE  = 10U;  // current Kavach mode
constexpr uint16_t MB_REG_SPEED_ACTUAL = 11U;  // actual speed km/h
constexpr uint16_t MB_REG_SPEED_PERMIT = 12U;  // permitted speed km/h
constexpr uint16_t MB_REG_SESSION      = 13U;  // DCM session (1=default,3=ext)

// Modbus coil addresses (bit flags)
constexpr uint16_t MB_COIL_OVERSPEED   = 0U;
constexpr uint16_t MB_COIL_SPAD        = 1U;
constexpr uint16_t MB_COIL_SOS         = 2U;
constexpr uint16_t MB_COIL_BRAKE       = 3U;
constexpr uint16_t MB_COIL_RADIO_LOSS  = 4U;
constexpr uint16_t MB_COIL_ROLLBACK    = 5U;
constexpr uint16_t MB_COIL_RFID_ERR    = 6U;
constexpr uint16_t MB_COIL_TRIP_MODE   = 7U;

// ModbusTcp: sends DEM/DCM state to Kavach DMI simulator
class ModbusTcp {
public:
    ModbusTcp(const std::string& ip   = "192.168.0.110",
              uint16_t           port = 1502,
              DemCore*           dem  = nullptr);
    ~ModbusTcp();

    // Connect to simulator - returns E_OK if connected
    Std_ReturnType connect();
    void           disconnect();
    bool           isConnected() const { return m_connected; }

    // Write all active DTCs from DemCore to holding registers
    Std_ReturnType writeDtcRegisters(DemCore& dem);

    // Write individual flag coils (call after each checkAll)
    Std_ReturnType writeEventCoils(uint8_t coilIndex, bool active);

    // Write Kavach mode register
    Std_ReturnType writeMode(uint8_t mode);

    // Write speed registers
    Std_ReturnType writeSpeed(uint8_t actual, uint8_t permitted);

    // Write DCM session register
    Std_ReturnType writeSession(uint8_t session);

    // Read input registers from simulator (e.g. commands back)
    Std_ReturnType readInputRegister(uint16_t address, uint16_t& valueOut);
    Std_ReturnType readHoldingRegister(uint16_t address, uint16_t& valueOut);
    Std_ReturnType readHoldingBlock(uint16_t start, uint16_t count, uint16_t* out);

    // Push full DEM snapshot - call once after scenario step
    Std_ReturnType pushSnapshot(DemCore& dem, uint8_t mode,
                                uint8_t speedActual, uint8_t speedPermit,
                                uint8_t session);

private:
    std::string  m_ip;
    uint16_t     m_port;
    modbus_t*    m_ctx       = nullptr;
    bool         m_connected = false;
    DemCore*     m_dem       = nullptr;

    // Retry connect on failure
    Std_ReturnType reconnect();

    // Report Modbus communication fault DTC
    void reportModbusFault(bool failed);
};
