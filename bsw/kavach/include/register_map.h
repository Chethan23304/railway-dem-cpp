#pragma once
#include <cstdint>

// Kavach MMI / Hardware Register Addresses (Modbus holding registers)
constexpr uint16_t MMI_V_PERMITTED         = 266;  // Permitted speed
constexpr uint16_t MMI_V_TRAIN             = 273;  // Actual train speed
constexpr uint16_t MMI_V_WARNING           = 275;  // Warning speed
constexpr uint16_t V_MAXTRAIN              = 278;  // Max train speed (brake pipe)
constexpr uint16_t MMI_SIGNAL_ASPECT       = 289;  // Signal aspect
constexpr uint16_t MMI_RF_SIGNAL_BAR_COUNT = 294;  // RF signal bar count
constexpr uint16_t MMI_TAG_ID              = 295;  // RFID tag ID
constexpr uint16_t MMI_TAG_STATUS_BITS     = 298;  // Tag status bits
constexpr uint16_t MMI_SECTION_SPEED       = 365;  // Section speed
constexpr uint16_t HD_RFID1_STATUS         = 425;  // RFID1 hardware status
constexpr uint16_t HD_RFID2_STATUS         = 426;  // RFID2 hardware status
constexpr uint16_t HD_RFID1_LTD            = 427;  // RFID1 last tag detection
constexpr uint16_t HD_RFID2_LTD            = 428;  // RFID2 last tag detection
