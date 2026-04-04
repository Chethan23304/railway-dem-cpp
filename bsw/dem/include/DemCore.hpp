#pragma once
#include <array>
#include "StdTypes.hpp"
#include "DemEventConfig.hpp"

// DemCore: manages event status, UDS status byte, event memory
class DemCore {
public:
    static constexpr uint8_t MAX_EVENT_MEMORY    = 8U;
    static constexpr uint8_t DEBOUNCE_THRESHOLD  = 3U;

    struct EventMemoryEntry {
        Dem_EventIdType eventId        = 0;
        Dem_DTCType     dtc            = 0;
        uint8_t         udsStatusByte  = 0;
        uint8_t         occurrences    = 0;
        uint8_t         agingCounter   = 0;
        Dem_EventStatus currentStatus  = DEM_EVENT_STATUS_PASSED;
    };

    struct DtcFilter {
        Dem_DTCStatusMask statusMask   = 0;
        uint8_t           currentIndex = 0;
    };

    DemCore();

    Std_ReturnType setEventStatus(Dem_EventIdType eventId,
                                  Dem_EventStatus status);

    Std_ReturnType getEventStatus(Dem_EventIdType eventId,
                                  uint8_t& udsStatusOut) const;

    Std_ReturnType clearDTC(Dem_DTCType dtc);

    Std_ReturnType setDTCFilter(Dem_DTCStatusMask mask,
                                DtcFilter& filter) const;

    Std_ReturnType getNextFilteredDTC(DtcFilter& filter,
                                      Dem_DTCType& dtcOut,
                                      uint8_t& statusOut) const;

    Std_ReturnType getNumberOfFilteredDTC(DtcFilter& filter,
                                          uint16_t& countOut) const;

    void mainFunction();

    // NvM helpers - access internal state
    uint8_t          getEventMemoryCount() const { return m_memCount; }
    EventMemoryEntry getEventMemoryEntry(uint8_t i) const { return m_memory[i]; }
    void             restoreEntry(const EventMemoryEntry& e);

private:
    // UDS status table indexed by event index (0-based)
    std::array<uint8_t,          0x00C0> m_udsStatus    {};
    std::array<uint8_t,          0x00C0> m_debounce     {};
    std::array<uint8_t,          0x00C0> m_timeDebounce {};
    std::array<Dem_EventStatus,  0x00C0> m_currentStatus{};

    std::array<EventMemoryEntry, MAX_EVENT_MEMORY> m_memory{};
    uint8_t m_memCount = 0;

    void updateUdsStatus(Dem_EventIdType id, Dem_EventStatus status);
    EventMemoryEntry* findEntry(Dem_EventIdType id);
    EventMemoryEntry* allocEntry(Dem_EventIdType id);
    bool isValidEvent(Dem_EventIdType id) const;
};
