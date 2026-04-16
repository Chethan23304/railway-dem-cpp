#pragma once
#include "StdTypes.hpp"

// ================================================================
// Kavach DMI Event IDs
// ================================================================

// Kavach Operating Modes
constexpr Dem_EventIdType KAVACH_EVT_MODE_SB  = 0x0001U;  // Stand By
constexpr Dem_EventIdType KAVACH_EVT_MODE_FS  = 0x0002U;  // Full Supervision
constexpr Dem_EventIdType KAVACH_EVT_MODE_LS  = 0x0003U;  // Limited Supervision
constexpr Dem_EventIdType KAVACH_EVT_MODE_SR  = 0x0004U;  // Staff Responsible
constexpr Dem_EventIdType KAVACH_EVT_MODE_SH  = 0x0005U;  // Shunting
constexpr Dem_EventIdType KAVACH_EVT_MODE_OS  = 0x0006U;  // On Sight
constexpr Dem_EventIdType KAVACH_EVT_MODE_TR  = 0x0007U;  // Trip
constexpr Dem_EventIdType KAVACH_EVT_MODE_PT  = 0x0008U;  // Post Trip
constexpr Dem_EventIdType KAVACH_EVT_MODE_RV  = 0x0009U;  // Reverse
constexpr Dem_EventIdType KAVACH_EVT_MODE_SF  = 0x000FU;  // System Failure

// Kavach Critical Safety Events
constexpr Dem_EventIdType KAVACH_EVT_OVERSPEED = 0x00A1U;  // Over Speeding
constexpr Dem_EventIdType KAVACH_EVT_SPAD      = 0x00A2U;  // Signal Passed At Danger
constexpr Dem_EventIdType KAVACH_EVT_SOS       = 0x00A3U;  // SOS Received
constexpr Dem_EventIdType KAVACH_EVT_ROLLBACK  = 0x00A4U;  // Roll Back
constexpr Dem_EventIdType KAVACH_EVT_RADIO_LOSS= 0x00A5U;  // Radio Loss
constexpr Dem_EventIdType KAVACH_EVT_BRAKE_CMD = 0x00A6U;  // Brake Command

// Kavach Info Events
constexpr Dem_EventIdType KAVACH_EVT_RFID = 0x00B1U;  // RFID Tag Read
constexpr Dem_EventIdType KAVACH_EVT_SIG  = 0x00B2U;  // Aspect Change
constexpr Dem_EventIdType KAVACH_EVT_MA   = 0x00B3U;  // MA Update

// Kavach Communication Fault Events
constexpr Dem_EventIdType KAVACH_EVT_MODBUS_FAULT = 0x00C0U;  // Modbus TCP fault
constexpr Dem_EventIdType KAVACH_EVT_UDP_FAULT    = 0x00C1U;  // UDP send fault

constexpr uint16_t DEM_NUM_EVENTS    = 21U;
constexpr Dem_EventIdType DEM_FIRST_EVENT_ID = 0x0001U;
constexpr Dem_EventIdType DEM_LAST_EVENT_ID  = 0x00C1U;

// DTC code for each event
constexpr Dem_DTCType getDTC(Dem_EventIdType id) {
    switch(id) {
        case KAVACH_EVT_MODE_SB:   return 0x010101U;
        case KAVACH_EVT_MODE_FS:   return 0x010201U;
        case KAVACH_EVT_MODE_LS:   return 0x010301U;
        case KAVACH_EVT_MODE_SR:   return 0x010401U;
        case KAVACH_EVT_MODE_SH:   return 0x010501U;
        case KAVACH_EVT_MODE_OS:   return 0x010601U;
        case KAVACH_EVT_MODE_TR:   return 0x010701U;
        case KAVACH_EVT_MODE_PT:   return 0x010801U;
        case KAVACH_EVT_MODE_RV:   return 0x010901U;
        case KAVACH_EVT_MODE_SF:   return 0x010F01U;
        case KAVACH_EVT_OVERSPEED: return 0x00A101U;
        case KAVACH_EVT_SPAD:      return 0x00A201U;
        case KAVACH_EVT_SOS:       return 0x00A301U;
        case KAVACH_EVT_ROLLBACK:  return 0x00A401U;
        case KAVACH_EVT_RADIO_LOSS:return 0x00A501U;
        case KAVACH_EVT_BRAKE_CMD: return 0x00A601U;
        case KAVACH_EVT_RFID:      return 0x00B101U;
        case KAVACH_EVT_SIG:       return 0x00B201U;
        case KAVACH_EVT_MA:        return 0x00B301U;
        case KAVACH_EVT_MODBUS_FAULT: return 0x00C101U;
        case KAVACH_EVT_UDP_FAULT:    return 0x00C301U;
        default:                   return 0x000000U;
    }
}

constexpr const char* getEventName(Dem_EventIdType id) {
    switch(id) {
        case KAVACH_EVT_MODE_SB:   return "Stand_By";
        case KAVACH_EVT_MODE_FS:   return "Full_Supervision";
        case KAVACH_EVT_MODE_LS:   return "Limited_Supervision";
        case KAVACH_EVT_MODE_SR:   return "Staff_Responsible";
        case KAVACH_EVT_MODE_SH:   return "Shunting";
        case KAVACH_EVT_MODE_OS:   return "On_Sight";
        case KAVACH_EVT_MODE_TR:   return "Trip";
        case KAVACH_EVT_MODE_PT:   return "Post_Trip";
        case KAVACH_EVT_MODE_RV:   return "Reverse";
        case KAVACH_EVT_MODE_SF:   return "System_Failure";
        case KAVACH_EVT_OVERSPEED: return "Over_Speeding";
        case KAVACH_EVT_SPAD:      return "SPAD";
        case KAVACH_EVT_SOS:       return "SOS_Received";
        case KAVACH_EVT_ROLLBACK:  return "Roll_Back";
        case KAVACH_EVT_RADIO_LOSS:return "Radio_Loss";
        case KAVACH_EVT_BRAKE_CMD: return "Brake_Command";
        case KAVACH_EVT_RFID:      return "RFID_Tag_Read";
        case KAVACH_EVT_SIG:       return "Aspect_Change";
        case KAVACH_EVT_MA:        return "MA_Update";
        case KAVACH_EVT_MODBUS_FAULT: return "Modbus_TCP_Fault";
        case KAVACH_EVT_UDP_FAULT:    return "UDP_Send_Fault";
        default:                   return "UNKNOWN";
    }
}
