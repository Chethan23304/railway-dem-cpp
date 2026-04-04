#pragma once
#include <string>
#include "StdTypes.hpp"

class EvtLogger {
public:
    EvtLogger(const std::string& logDir = "logs");
    ~EvtLogger();

    void logFailed(Dem_EventIdType eventId,
                   Dem_DTCType     dtc,
                   uint8_t         udsStatus,
                   uint8_t         occurrences,
                   const std::string& source);

    void logPassed(Dem_EventIdType eventId,
                   Dem_DTCType     dtc,
                   const std::string& source);

    void readByIdentifier(Dem_EventIdType eventId);

    bool alreadyLogged(Dem_EventIdType eventId) const;

public:
    int  getRunNumber() const { return m_runNumber; }
private:
    std::string m_logDir;

    // File 1: only FAILED events
    FILE* m_failedCsv  = nullptr;
    FILE* m_failedTxt  = nullptr;

    // File 2: only ReadByIdentifier query results
    FILE* m_rbiCsv     = nullptr;
    FILE* m_rbiTxt     = nullptr;

    bool    m_logged[0x00C0] = {};
    int     m_failedCount    = 0;
    int     m_rbiCount       = 0;
    int     m_runNumber      = 0;

    // Store last failed entry per eventId for RBI lookup
    struct FailedEntry {
        Dem_DTCType dtc        = 0;
        uint8_t     udsStatus  = 0;
        uint8_t     occurrences= 0;
        char        timestamp[32] = {};
    };
    FailedEntry m_entries[0x00C0] = {};

    void writeFailedCsv(const std::string& ts, Dem_EventIdType id,
                        Dem_DTCType dtc, uint8_t uds, uint8_t occ,
                        const std::string& src);
    void writeFailedTxt(const std::string& ts, Dem_EventIdType id,
                        Dem_DTCType dtc, uint8_t uds, uint8_t occ,
                        const std::string& src);

    void writeRbiCsv(Dem_EventIdType id, const FailedEntry& e);
    void writeRbiTxt(Dem_EventIdType id, const FailedEntry& e);

    std::string timestamp() const;
    std::string getSeverity(Dem_EventIdType id) const;
};
