#pragma once
#include "StdTypes.hpp"

// ============================================================
// Kavach UDP Frame - 20 bytes fixed
// C++ sends to  : 192.168.0.110:5600
// Qt DMI listens: 0.0.0.0:5600
// No connection needed - fire and forget
// ============================================================

constexpr uint16_t KAVACH_UDP_PORT        = 5600U;
constexpr uint8_t  KAVACH_UDP_MAGIC       = 0xCAU;

// Message types
constexpr uint8_t KAVACH_UDP_EVT_CRITICAL = 0x01U;
constexpr uint8_t KAVACH_UDP_EVT_MEDIUM   = 0x02U;
constexpr uint8_t KAVACH_UDP_EVT_INFO     = 0x03U;
constexpr uint8_t KAVACH_UDP_SNAPSHOT     = 0x04U;
constexpr uint8_t KAVACH_UDP_HEARTBEAT    = 0x05U;

// Coil flag bits packed into coilFlags byte
constexpr uint8_t COIL_OVERSPEED = 0x01U;  // bit 0
constexpr uint8_t COIL_SPAD      = 0x02U;  // bit 1
constexpr uint8_t COIL_SOS       = 0x04U;  // bit 2
constexpr uint8_t COIL_BRAKE     = 0x08U;  // bit 3
constexpr uint8_t COIL_RADIOLOSS = 0x10U;  // bit 4
constexpr uint8_t COIL_ROLLBACK  = 0x20U;  // bit 5
constexpr uint8_t COIL_RFID      = 0x40U;  // bit 6
constexpr uint8_t COIL_TRIP      = 0x80U;  // bit 7

#pragma pack(push, 1)
struct KavachUdpFrame {
    uint8_t  magic         = KAVACH_UDP_MAGIC;  // byte 0  always 0xCA
    uint8_t  msgType       = 0;                  // byte 1  message type
    uint16_t eventId       = 0;                  // byte 2-3  event ID
    uint32_t dtc           = 0;                  // byte 4-7  DTC code
    uint8_t  udsStatus     = 0;                  // byte 8   UDS status byte
    uint8_t  severity      = 0;                  // byte 9   0=LOW 1=MED 2=HIGH
    uint8_t  kavachMode    = 0;                  // byte 10  Kavach mode
    uint8_t  dtcCount      = 0;                  // byte 11  active DTC count
    uint8_t  speedActual   = 0;                  // byte 12  actual km/h
    uint8_t  speedPermit   = 0;                  // byte 13  permitted km/h
    uint8_t  dcmSession    = 0;                  // byte 14  DCM session
    uint8_t  coilFlags     = 0;                  // byte 15  packed event flags
    uint8_t  reserved[4]   = {};                 // byte 16-19 padding
};
#pragma pack(pop)
