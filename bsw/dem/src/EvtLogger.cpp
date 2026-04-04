#include "EvtLogger.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sys/stat.h>

EvtLogger::EvtLogger(const std::string& logDir) : m_logDir(logDir) {
    mkdir(logDir.c_str(), 0755);

    std::string csvPath  = logDir + "/events.csv";
    std::string txtPath  = logDir + "/events.txt";
    std::string jsonPath = logDir + "/events.json";

    m_csvFile  = fopen(csvPath.c_str(),  "w");
    m_txtFile  = fopen(txtPath.c_str(),  "w");
    m_jsonFile = fopen(jsonPath.c_str(), "w");

    // CSV header
    if (m_csvFile)
        fprintf(m_csvFile,
            "No,Timestamp,EventId,EventName,DTC,"
            "EventType,UDS_Status,Occurrences,Severity,Source\n");

    // TXT header
    if (m_txtFile) {
        fprintf(m_txtFile,
            "\n==============================================\n"
            "  Railway DEM - Kavach Event Log\n"
            "  Session: %s\n"
            "==============================================\n",
            timestamp().c_str());
        fprintf(m_txtFile,
            "%-4s %-20s %-10s %-24s %-10s %-8s %-4s %-8s\n",
            "No","Timestamp","DTC","EventName",
            "Type","UDS","Occ","Source");
        fprintf(m_txtFile,
            "%-4s %-20s %-10s %-24s %-10s %-8s %-4s %-8s\n",
            "----","--------------------","----------",
            "------------------------","----------",
            "--------","----","--------");
    }

    // JSON header
    if (m_jsonFile) {
        fprintf(m_jsonFile, "[\n");
        m_firstJson = true;
    }

    printf("[EvtLogger] Initialized -> %s/\n", logDir.c_str());
}

EvtLogger::~EvtLogger() {
    std::string ts = timestamp();
    if (m_csvFile)  fclose(m_csvFile);
    if (m_txtFile) {
        fprintf(m_txtFile,
            "\n  Session Ended: %s | Total Events: %d\n"
            "==============================================\n",
            ts.c_str(), m_eventCount);
        fclose(m_txtFile);
    }
    if (m_jsonFile) {
        fprintf(m_jsonFile, "\n]\n");
        fclose(m_jsonFile);
    }
    printf("[EvtLogger] Closed. Total events: %d\n", m_eventCount);
}

std::string EvtLogger::timestamp() const {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}

std::string EvtLogger::getSeverity(Dem_EventIdType id) const {
    if (id >= 0x00A1U && id <= 0x00A6U) {
        if (id == 0x00A5U) return "MEDIUM";
        return "HIGH";
    }
    if (id >= 0x00B1U) return "LOW";
    return "MEDIUM";
}

bool EvtLogger::alreadyLogged(Dem_EventIdType eventId) const {
    if (eventId >= 0x00C0U) return false;
    return m_logged[eventId];
}

void EvtLogger::logFailed(Dem_EventIdType eventId, Dem_DTCType dtc,
                           uint8_t udsStatus, uint8_t occurrences,
                           const std::string& source) {
    if (eventId >= 0x00C0U) return;
    m_logged[eventId] = true;
    m_eventCount++;
    std::string ts  = timestamp();
    std::string sev = getSeverity(eventId);
    writeCSV(ts, eventId, dtc, "FAILED", udsStatus, occurrences, source);
    writeTXT(ts, eventId, dtc, "FAILED", udsStatus, occurrences, source);
    writeJSON(ts, eventId, dtc, "FAILED", udsStatus, occurrences, source);
    printf("[EvtLogger] #%d FAILED EventId=0x%04X %-22s DTC=0x%06X\n",
           m_eventCount, eventId, getEventName(eventId), dtc);
}

void EvtLogger::logPassed(Dem_EventIdType eventId, Dem_DTCType dtc,
                           const std::string& source) {
    std::string ts = timestamp();
    writeTXT(ts, eventId, dtc, "PASSED", 0, 0, source);
}

void EvtLogger::writeCSV(const std::string& ts, Dem_EventIdType id,
                          Dem_DTCType dtc, const std::string& type,
                          uint8_t uds, uint8_t occ,
                          const std::string& src) {
    if (!m_csvFile) return;
    fprintf(m_csvFile,
        "%d,%s,0x%04X,%s,0x%06X,%s,0x%02X,%d,%s,%s\n",
        m_eventCount, ts.c_str(), id, getEventName(id),
        dtc, type.c_str(), uds, occ,
        getSeverity(id).c_str(), src.c_str());
    fflush(m_csvFile);
}

void EvtLogger::writeTXT(const std::string& ts, Dem_EventIdType id,
                          Dem_DTCType dtc, const std::string& type,
                          uint8_t uds, uint8_t occ,
                          const std::string& src) {
    if (!m_txtFile) return;
    fprintf(m_txtFile,
        "%-4d %-20s 0x%06X %-24s %-10s 0x%02X %-4d %-8s\n",
        m_eventCount, ts.c_str(), dtc,
        getEventName(id), type.c_str(), uds, occ, src.c_str());
    fflush(m_txtFile);
}

void EvtLogger::writeJSON(const std::string& ts, Dem_EventIdType id,
                           Dem_DTCType dtc, const std::string& type,
                           uint8_t uds, uint8_t occ,
                           const std::string& src) {
    if (!m_jsonFile) return;
    if (!m_firstJson) fprintf(m_jsonFile, ",\n");
    fprintf(m_jsonFile,
        "  {\n"
        "    \"no\": %d,\n"
        "    \"timestamp\": \"%s\",\n"
        "    \"event_id\": \"0x%04X\",\n"
        "    \"event_name\": \"%s\",\n"
        "    \"dtc\": \"0x%06X\",\n"
        "    \"event_type\": \"%s\",\n"
        "    \"uds_status\": \"0x%02X\",\n"
        "    \"occurrences\": %d,\n"
        "    \"severity\": \"%s\",\n"
        "    \"source\": \"%s\"\n"
        "  }",
        m_eventCount, ts.c_str(), id, getEventName(id),
        dtc, type.c_str(), uds, occ,
        getSeverity(id).c_str(), src.c_str());
    fflush(m_jsonFile);
    m_firstJson = false;
}

void EvtLogger::readByIdentifier(Dem_EventIdType eventId) {
    printf("\n");
    printf("+-----------------------------------------------------------------+\n");
    printf("|  ReadByIdentifier  EventId: 0x%04X  %-27s|\n",
           eventId, getEventName(eventId));
    printf("+-----------------------------------------------------------------+\n");

    if (!alreadyLogged(eventId)) {
        printf("|  No FAILED events recorded for this Event ID                   |\n");
        printf("+-----------------------------------------------------------------+\n\n");
        return;
    }

    printf("|  DTC Code : 0x%06X                                          |\n",
           getDTC(eventId));
    printf("|  Severity : %-51s|\n", getSeverity(eventId).c_str());
    printf("|  Status   : FAILED (logged in this session)                    |\n");
    printf("+-----------------------------------------------------------------+\n");
    printf("|  Check logs/events.csv for full history with timestamps         |\n");
    printf("+-----------------------------------------------------------------+\n\n");
}
