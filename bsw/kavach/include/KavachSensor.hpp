#pragma once
#include <cstdint>
#include "ModbusTcp.hpp"

struct KavachSensorData {
    // LIVE from DMI
    uint16_t speedActual;       // Reg[273] MMI_V_TRAIN
    uint16_t speedPermitted;    // Reg[266] MMI_V_PERMITTED
    uint16_t speedWarning;      // Reg[275] MMI_V_WARNING
    uint16_t rfSignalBars;      // Reg[294] MMI_RF_SIGNAL_BAR_COUNT
    uint16_t sectionSpeed;      // Reg[365] MMI_SECTION_SPEED

    // DERIVED from live data
    bool     overspeed;         // speedActual > speedPermitted
    bool     radioLost;         // rfSignalBars == 0

    // NOT populated by DMI — safe defaults
    uint16_t brakePipe;         // assumed OK
    uint16_t emergencyBrake;
    uint16_t sosState;
    uint16_t rfidStatus;
    uint8_t  kavachMode;

    bool valid;
};

class KavachSensor {
public:
    explicit KavachSensor(ModbusTcp& modbus) : m_modbus(modbus) {}
    bool connect() { return true; }
    KavachSensorData readAll();
private:
    ModbusTcp& m_modbus;
};
