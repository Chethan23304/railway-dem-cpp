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

    // Shows current run result + ALL previous runs for this ID
    void readByIdentifier(Dem_EventIdType eventId);

    bool alreadyLogged(Dem_EventIdType eventId) const;

private:
    std::string m_logDir;

    // Current run only - overwrite each time
    FILE* m_failedCsv  = nullptr;
    FILE* m_failedTxt  = nullptr;

    // All runs history - append always
    FILE* m_rbiCsv     = nullptr;

    int  m_failedCount = 0;
    int  m_rbiCount    = 0;
    int  m_runNumber   = 0;

    bool m_logged[0x00C0] = {};

    struct FailedEntry {
        Dem_DTCType dtc         = 0;
        uint8_t     udsStatus   = 0;
        uint8_t     occurrences = 0;
        char        timestamp[32] = {};
        char        source[32]    = {};
    };
    FailedEntry m_entries[0x00C0] = {};

    // Read run number from history file
    int  readRunNumber();

    // Write current run failed event
    void writeFailedCsv(const std::string& ts, Dem_EventIdType id,
                        Dem_DTCType dtc, uint8_t uds, uint8_t occ,
                        const std::string& src);
    void writeFailedTxt(const std::string& ts, Dem_EventIdType id,
                        Dem_DTCType dtc, uint8_t uds, uint8_t occ,
                        const std::string& src);

    // Append one RBI query result to history file
    void appendRbiHistory(Dem_EventIdType id, const FailedEntry& e);

    // Read and print all previous RBI history for one event ID
    void printRbiHistory(Dem_EventIdType id);

    std::string timestamp() const;
    std::string getSeverity(Dem_EventIdType id) const;
};
