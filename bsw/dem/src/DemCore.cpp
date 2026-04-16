#include "DemCore.hpp"
#include "Debounce.hpp"
#include <cstring>
#include <cstdio>

DemCore::DemCore() {
    m_udsStatus.fill(0);
    m_debounce.fill(0);
    m_timeDebounce.fill(0);
    m_currentStatus.fill(DEM_EVENT_STATUS_PASSED);
    m_memory.fill(EventMemoryEntry{});
    m_memCount = 0;
    printf("[DemCore] Initialized - %d slots, %d events configured\n",
           MAX_EVENT_MEMORY, DEM_NUM_EVENTS);
}

bool DemCore::isValidEvent(Dem_EventIdType id) const {
    return (id >= DEM_FIRST_EVENT_ID && id <= DEM_LAST_EVENT_ID);
}

void DemCore::updateUdsStatus(Dem_EventIdType id, Dem_EventStatus status) {
    uint8_t& uds = m_udsStatus[id];
    if (status == DEM_EVENT_STATUS_FAILED) {
        uds |= DEM_UDS_STATUS_TF;
        uds |= DEM_UDS_STATUS_TFTOC;
        uds |= DEM_UDS_STATUS_PDTC;
        uds |= DEM_UDS_STATUS_CDTC;
        uds |= DEM_UDS_STATUS_TFSLC;
    } else if (status == DEM_EVENT_STATUS_PASSED) {
        uds &= static_cast<uint8_t>(~DEM_UDS_STATUS_TF);
        uds &= static_cast<uint8_t>(~DEM_UDS_STATUS_TFTOC);
        uds &= static_cast<uint8_t>(~DEM_UDS_STATUS_PDTC);
    }
}

DemCore::EventMemoryEntry* DemCore::findEntry(Dem_EventIdType id) {
    for (uint8_t i = 0; i < m_memCount; i++)
        if (m_memory[i].eventId == id) return &m_memory[i];
    return nullptr;
}

DemCore::EventMemoryEntry* DemCore::allocEntry(Dem_EventIdType id) {
    EventMemoryEntry* e = nullptr;
    if (m_memCount < MAX_EVENT_MEMORY) {
        e = &m_memory[m_memCount++];
    } else {
        // Displace oldest entry
        for (uint8_t i = 0; i < MAX_EVENT_MEMORY - 1; i++)
            m_memory[i] = m_memory[i + 1];
        e = &m_memory[MAX_EVENT_MEMORY - 1];
    }
    *e = EventMemoryEntry{};
    e->eventId = id;
    e->dtc     = getDTC(id);
    return e;
}

Std_ReturnType DemCore::setEventStatus(Dem_EventIdType id,
                                        Dem_EventStatus status) {
    if (!isValidEvent(id)) {
        printf("[DemCore] ERROR: Invalid EventId 0x%04X\n", id);
        return E_NOT_OK;
    }

    // Use Debounce class for PREFAILED handling
    if (status == DEM_EVENT_STATUS_PREFAILED) {
        static Debounce deb(DebounceCfg{DebounceType::Counter,
                                        DEBOUNCE_THRESHOLD});
        auto result = deb.reportPrefailed();
        if (result != DebounceResult::Failed) {
            m_currentStatus[id] = DEM_EVENT_STATUS_PREFAILED;
            return E_OK;
        }
        status = DEM_EVENT_STATUS_FAILED; // threshold reached
    } else if (status == DEM_EVENT_STATUS_PASSED) {
        m_debounce[id] = 0;
    }

    m_currentStatus[id] = status;
    updateUdsStatus(id, status);

    if (status == DEM_EVENT_STATUS_FAILED) {
        EventMemoryEntry* e = findEntry(id);
        if (!e) e = allocEntry(id);
        if (e) {
            e->udsStatusByte = m_udsStatus[id];
            e->occurrences++;
            e->currentStatus = status;
            printf("[DemCore] Event 0x%04X FAILED DTC=0x%06X occ=%d\n",
                   id, getDTC(id), e->occurrences);
        }
    } else if (status == DEM_EVENT_STATUS_PASSED) {
        EventMemoryEntry* e = findEntry(id);
        if (e) {
            e->udsStatusByte = m_udsStatus[id];
            e->currentStatus = status;
            e->agingCounter++;
        }
    }
    return E_OK;
}

Std_ReturnType DemCore::getEventStatus(Dem_EventIdType id,
                                        uint8_t& udsOut) const {
    if (!isValidEvent(id)) return E_NOT_OK;
    udsOut = m_udsStatus[id];
    return E_OK;
}

Std_ReturnType DemCore::clearDTC(Dem_DTCType dtc) {
    if (dtc == DEM_DTC_GROUP_ALL) {
        m_udsStatus.fill(0);
        m_debounce.fill(0);
        m_currentStatus.fill(DEM_EVENT_STATUS_PASSED);
        m_memory.fill(EventMemoryEntry{});
        m_memCount = 0;
        printf("[DemCore] ClearDTC ALL - OK\n");
        return E_OK;
    }
    printf("[DemCore] ClearDTC 0x%06X\n", dtc);
    return E_OK;
}

Std_ReturnType DemCore::setDTCFilter(Dem_DTCStatusMask mask,
                                      DtcFilter& filter) const {
    filter.statusMask   = mask;
    filter.currentIndex = 0;
    return E_OK;
}

Std_ReturnType DemCore::getNextFilteredDTC(DtcFilter& filter,
                                            Dem_DTCType& dtcOut,
                                            uint8_t& statusOut) const {
    while (filter.currentIndex < 0x00C2U) {
        uint16_t id = filter.currentIndex++;
        if (!isValidEvent(static_cast<Dem_EventIdType>(id))) continue;
        if ((m_udsStatus[id] & filter.statusMask) != 0) {
            dtcOut    = getDTC(static_cast<Dem_EventIdType>(id));
            statusOut = m_udsStatus[id];
            return E_OK;
        }
    }
    return E_NOT_OK;
}

Std_ReturnType DemCore::getNumberOfFilteredDTC(DtcFilter& filter,
                                                uint16_t& countOut) const {
    countOut = 0;
    for (uint16_t id = 0; id < 0x00C2U; id++) {
        if (!isValidEvent(static_cast<Dem_EventIdType>(id))) continue;
        if ((m_udsStatus[id] & filter.statusMask) != 0)
            countOut++;
    }
    return E_OK;
}

void DemCore::mainFunction() {
    // Time-based debounce processing - called periodically
}

void DemCore::restoreEntry(const EventMemoryEntry& e) {
    m_memory[m_memCount++] = e;
    m_udsStatus[e.eventId] = e.udsStatusByte;
    m_currentStatus[e.eventId] = e.currentStatus;
    printf("[DemCore] Restored DTC=0x%06X occ=%d\n",
           e.dtc, e.occurrences);
}
