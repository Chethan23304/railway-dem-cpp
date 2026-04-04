#pragma once
#include <string>
#include "StdTypes.hpp"

// EvtLogger: logs DEM events to CSV, TXT and JSON files
class EvtLogger {
public:
    EvtLogger(const std::string& logDir = "logs");
    ~EvtLogger();

    // Log a FAILED event (call once per event)
    void logFailed(Dem_EventIdType eventId,
                   Dem_DTCType     dtc,
                   uint8_t         udsStatus,
                   uint8_t         occurrences,
                   const std::string& source);

    // Log a PASSED event
    void logPassed(Dem_EventIdType eventId,
                   Dem_DTCType     dtc,
                   const std::string& source);

    // Query event history by event ID (ReadByIdentifier)
    void readByIdentifier(Dem_EventIdType eventId);

    // Check if already logged this event in this session
    bool alreadyLogged(Dem_EventIdType eventId) const;

private:
    std::string m_logDir;
    FILE*       m_csvFile  = nullptr;
    FILE*       m_txtFile  = nullptr;
    FILE*       m_jsonFile = nullptr;
    bool        m_firstJson = true;
    int         m_eventCount = 0;

    // Per-event logged flag (max event id 0x00C0)
    bool        m_logged[0x00C0] = {};

    void        writeCSV(const std::string& ts, Dem_EventIdType id,
                         Dem_DTCType dtc, const std::string& type,
                         uint8_t uds, uint8_t occ,
                         const std::string& src);
    void        writeTXT(const std::string& ts, Dem_EventIdType id,
                         Dem_DTCType dtc, const std::string& type,
                         uint8_t uds, uint8_t occ,
                         const std::string& src);
    void        writeJSON(const std::string& ts, Dem_EventIdType id,
                          Dem_DTCType dtc, const std::string& type,
                          uint8_t uds, uint8_t occ,
                          const std::string& src);
    std::string timestamp() const;
    std::string getSeverity(Dem_EventIdType id) const;
};
