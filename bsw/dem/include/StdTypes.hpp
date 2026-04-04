#pragma once
#include <cstdint>
#include <cstddef>

// AUTOSAR base types in C++
using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using sint8  = int8_t;
using sint16 = int16_t;
using sint32 = int32_t;

using Std_ReturnType    = uint8_t;
using Dem_EventIdType   = uint16_t;
using Dem_EventStatus   = uint8_t;
using Dem_DTCType       = uint32_t;
using Dem_DTCStatusMask = uint8_t;

constexpr Std_ReturnType E_OK     = 0x00U;
constexpr Std_ReturnType E_NOT_OK = 0x01U;

// DEM event status values
constexpr Dem_EventStatus DEM_EVENT_STATUS_PASSED    = 0x00U;
constexpr Dem_EventStatus DEM_EVENT_STATUS_FAILED    = 0x01U;
constexpr Dem_EventStatus DEM_EVENT_STATUS_PREPASSED = 0x02U;
constexpr Dem_EventStatus DEM_EVENT_STATUS_PREFAILED = 0x03U;

// UDS Status byte bits
constexpr uint8_t DEM_UDS_STATUS_TF    = 0x01U;  // testFailed
constexpr uint8_t DEM_UDS_STATUS_TFTOC = 0x02U;  // testFailedThisOpCycle
constexpr uint8_t DEM_UDS_STATUS_PDTC  = 0x04U;  // pendingDTC
constexpr uint8_t DEM_UDS_STATUS_CDTC  = 0x08U;  // confirmedDTC
constexpr uint8_t DEM_UDS_STATUS_TFSLC = 0x20U;  // testFailedSinceLastClear

// DEM severity levels
constexpr uint8_t DEM_SEVERITY_LOW    = 0U;
constexpr uint8_t DEM_SEVERITY_MEDIUM = 1U;
constexpr uint8_t DEM_SEVERITY_HIGH   = 2U;

// DTC group
constexpr Dem_DTCType DEM_DTC_GROUP_ALL = 0xFFFFFFU;
