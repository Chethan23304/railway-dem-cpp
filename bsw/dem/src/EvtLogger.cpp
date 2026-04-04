#include "EvtLogger.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sys/stat.h>

EvtLogger::EvtLogger(const std::string& logDir) : m_logDir(logDir) {
    mkdir(logDir.c_str(), 0755);

    // ---- File 1: Failed events ----
    m_failedCsv = fopen((logDir + "/events_failed.csv").c_str(), "w");
    m_failedTxt = fopen((logDir + "/events_failed.txt").c_str(), "w");

    if (m_failedCsv)
        fprintf(m_failedCsv,
            "No,Timestamp,EventId,EventName,DTC,"
            "EventType,UDS_Status,Occurrences,Severity,Source\n");

    if (m_failedTxt) {
        fprintf(m_failedTxt,
            "\n==============================================\n"
            "  Railway DEM - FAILED Events Log\n"
            "  Session: %s\n"
            "==============================================\n",
            timestamp().c_str());
        fprintf(m_failedTxt,
            "%-4s %-20s %-10s %-24s %-8s %-4s %-8s %-8s\n",
            "No","Timestamp","DTC","EventName","UDS","Occ","Severity","Source");
        fprintf(m_failedTxt,
            "%-4s %-20s %-10s %-24s %-8s %-4s %-8s %-8s\n",
            "----","--------------------","----------",
            "------------------------","--------","----","--------","--------");
    }

    // ---- File 2: ReadByIdentifier results ----
    m_rbiCsv = fopen((logDir + "/events_readbyid.csv").c_str(), "w");
    m_rbiTxt = fopen((logDir + "/events_readbyid.txt").c_str(), "w");

    if (m_rbiCsv)
        fprintf(m_rbiCsv,
            "QueryNo,EventId,EventName,DTC,UDS_Status,"
            "Occurrences,Severity,OriginalTimestamp\n");

    if (m_rbiTxt) {
        fprintf(m_rbiTxt,
            "\n==============================================\n"
            "  Railway DEM - ReadByIdentifier Query Log\n"
            "  Session: %s\n"
            "==============================================\n",
            timestamp().c_str());
        fprintf(m_rbiTxt,
            "%-4s %-10s %-24s %-10s %-8s %-4s %-8s %-20s\n",
            "No","EventId","EventName","DTC","UDS","Occ","Severity","Timestamp");
        fprintf(m_rbiTxt,
            "%-4s %-10s %-24s %-10s %-8s %-4s %-8s %-20s\n",
            "----","----------","------------------------",
            "----------","--------","----","--------","--------------------");
    }

    printf("[EvtLogger] Failed log  -> %s/events_failed.csv\n",
           logDir.c_str());
    printf("[EvtLogger] RBI log     -> %s/events_readbyid.csv\n",
           logDir.c_str());
}

EvtLogger::~EvtLogger() {
    std::string ts = timestamp();
    if (m_failedTxt) {
        fprintf(m_failedTxt,
            "\n  Session Ended: %s | Total FAILED Events: %d\n"
            "==============================================\n",
            ts.c_str(), m_failedCount);
        fclose(m_failedTxt);
    }
    if (m_failedCsv) fclose(m_failedCsv);
    if (m_rbiTxt) {
        fprintf(m_rbiTxt,
            "\n  Session Ended: %s | Total RBI Queries: %d\n"
            "==============================================\n",
            ts.c_str(), m_rbiCount);
        fclose(m_rbiTxt);
    }
    if (m_rbiCsv) fclose(m_rbiCsv);

    printf("[EvtLogger] Closed. FAILED=%d  RBI queries=%d\n",
           m_failedCount, m_rbiCount);
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

    m_failedCount++;
    m_logged[eventId] = true;

    std::string ts = timestamp();

    // Save entry for later RBI lookup
    FailedEntry& fe = m_entries[eventId];
    fe.dtc         = dtc;
    fe.udsStatus   = udsStatus;
    fe.occurrences = occurrences;
    strncpy(fe.timestamp, ts.c_str(), 31);

    writeFailedCsv(ts, eventId, dtc, udsStatus, occurrences, source);
    writeFailedTxt(ts, eventId, dtc, udsStatus, occurrences, source);

    printf("[EvtLogger] #%d FAILED EventId=0x%04X %-22s DTC=0x%06X\n",
           m_failedCount, eventId, getEventName(eventId), dtc);
}

void EvtLogger::logPassed(Dem_EventIdType eventId, Dem_DTCType dtc,
                           const std::string& source) {
    (void)eventId; (void)dtc; (void)source;
    // PASSED events not written to any file — only FAILED are logged
}

void EvtLogger::writeFailedCsv(const std::string& ts,
                                 Dem_EventIdType id, Dem_DTCType dtc,
                                 uint8_t uds, uint8_t occ,
                                 const std::string& src) {
    if (!m_failedCsv) return;
    fprintf(m_failedCsv,
        "%d,%s,0x%04X,%s,0x%06X,FAILED,0x%02X,%d,%s,%s\n",
        m_failedCount, ts.c_str(), id, getEventName(id),
        dtc, uds, occ, getSeverity(id).c_str(), src.c_str());
    fflush(m_failedCsv);
}

void EvtLogger::writeFailedTxt(const std::string& ts,
                                 Dem_EventIdType id, Dem_DTCType dtc,
                                 uint8_t uds, uint8_t occ,
                                 const std::string& src) {
    if (!m_failedTxt) return;
    fprintf(m_failedTxt,
        "%-4d %-20s 0x%06X %-24s 0x%02X %-4d %-8s %-8s\n",
        m_failedCount, ts.c_str(), dtc,
        getEventName(id), uds, occ,
        getSeverity(id).c_str(), src.c_str());
    fflush(m_failedTxt);
}

void EvtLogger::readByIdentifier(Dem_EventIdType eventId) {
    // Console output — clean box format
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

    const FailedEntry& fe = m_entries[eventId];
    printf("|  DTC Code   : 0x%06X                                        |\n", fe.dtc);
    printf("|  UDS Status : 0x%02X                                            |\n", fe.udsStatus);
    printf("|  Severity   : %-51s|\n", getSeverity(eventId).c_str());
    printf("|  Occurrences: %-51d|\n", fe.occurrences);
    printf("|  Logged At  : %-51s|\n", fe.timestamp);
    printf("|  Status     : FAILED                                           |\n");
    printf("+-----------------------------------------------------------------+\n\n");

    // Write to RBI log files
    m_rbiCount++;
    writeRbiCsv(eventId, fe);
    writeRbiTxt(eventId, fe);
}

void EvtLogger::writeRbiCsv(Dem_EventIdType id, const FailedEntry& e) {
    if (!m_rbiCsv) return;
    fprintf(m_rbiCsv,
        "%d,0x%04X,%s,0x%06X,0x%02X,%d,%s,%s\n",
        m_rbiCount, id, getEventName(id),
        e.dtc, e.udsStatus, e.occurrences,
        getSeverity(id).c_str(), e.timestamp);
    fflush(m_rbiCsv);
}

void EvtLogger::writeRbiTxt(Dem_EventIdType id, const FailedEntry& e) {
    if (!m_rbiTxt) return;
    fprintf(m_rbiTxt,
        "%-4d 0x%04X     %-24s 0x%06X 0x%02X %-4d %-8s %-20s\n",
        m_rbiCount, id, getEventName(id),
        e.dtc, e.udsStatus, e.occurrences,
        getSeverity(id).c_str(), e.timestamp);
    fflush(m_rbiTxt);
}
