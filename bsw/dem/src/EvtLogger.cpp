#include "EvtLogger.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

EvtLogger::EvtLogger(const std::string& logDir) : m_logDir(logDir) {
    mkdir(logDir.c_str(), 0755);

    // ---- Current run files: OVERWRITE each run ----
    m_failedCsv = fopen((logDir + "/events_failed.csv").c_str(), "w");
    m_failedTxt = fopen((logDir + "/events_failed.txt").c_str(), "w");

    if (m_failedCsv)
        fprintf(m_failedCsv,
            "No,Timestamp,EventId,EventName,DTC,"
            "UDS_Status,Occurrences,Severity,Source\n");

    if (m_failedTxt) {
        fprintf(m_failedTxt,
            "\n==============================================\n"
            "  Railway DEM - Current Run Failed Events\n"
            "  Started: %s\n"
            "==============================================\n",
            timestamp().c_str());
        fprintf(m_failedTxt,
            "%-4s %-20s %-10s %-24s %-8s %-4s %-8s\n",
            "No","Timestamp","DTC","EventName","UDS","Occ","Source");
        fprintf(m_failedTxt,
            "%-4s %-20s %-10s %-24s %-8s %-4s %-8s\n",
            "----","--------------------","----------",
            "------------------------","--------","----","--------");
    }

    // ---- RBI history file: APPEND across all runs ----
    m_runNumber = readRunNumber();

    m_rbiCsv = fopen((logDir + "/rbi_history.csv").c_str(), "a");
    if (m_rbiCsv) {
        fseek(m_rbiCsv, 0, SEEK_END);
        long sz = ftell(m_rbiCsv);
        if (sz == 0)
            fprintf(m_rbiCsv,
                "Run,QueryNo,Timestamp,EventId,EventName,"
                "DTC,UDS_Status,Occurrences,Severity,Source\n");
    }

    printf("[EvtLogger] Run #%d started\n", m_runNumber);
    printf("[EvtLogger] Current run -> events_failed.csv\n");
    printf("[EvtLogger] Full history -> rbi_history.csv\n");
}

EvtLogger::~EvtLogger() {
    if (m_failedTxt) {
        fprintf(m_failedTxt,
            "\n  Run #%d ended: %s | Events: %d\n"
            "==============================================\n",
            m_runNumber, timestamp().c_str(), m_failedCount);
        fclose(m_failedTxt);
    }
    if (m_failedCsv) fclose(m_failedCsv);
    if (m_rbiCsv)    fclose(m_rbiCsv);
    printf("[EvtLogger] Run #%d closed. Failed=%d RBI=%d\n",
           m_runNumber, m_failedCount, m_rbiCount);
}

int EvtLogger::readRunNumber() {
    // Count existing runs in rbi_history to get next run number
    FILE* f = fopen((m_logDir + "/rbi_history.csv").c_str(), "r");
    if (!f) return 1;
    char line[512];
    int maxRun = 0;
    while (fgets(line, sizeof(line), f)) {
        int run = 0;
        if (sscanf(line, "%d,", &run) == 1 && run > maxRun)
            maxRun = run;
    }
    fclose(f);
    return maxRun + 1;
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

    // Save in-memory for RBI lookup later
    FailedEntry& fe = m_entries[eventId];
    fe.dtc         = dtc;
    fe.udsStatus   = udsStatus;
    fe.occurrences = occurrences;
    strncpy(fe.timestamp, ts.c_str(), 31);
    strncpy(fe.source,    source.c_str(), 31);

    writeFailedCsv(ts, eventId, dtc, udsStatus, occurrences, source);
    writeFailedTxt(ts, eventId, dtc, udsStatus, occurrences, source);

    // Console: current run only
    printf("[EvtLogger] #%d FAILED EventId=0x%04X %-22s DTC=0x%06X\n",
           m_failedCount, eventId, getEventName(eventId), dtc);
}

void EvtLogger::logPassed(Dem_EventIdType eventId, Dem_DTCType dtc,
                           const std::string& source) {
    (void)eventId; (void)dtc; (void)source;
}

void EvtLogger::writeFailedCsv(const std::string& ts,
                                 Dem_EventIdType id, Dem_DTCType dtc,
                                 uint8_t uds, uint8_t occ,
                                 const std::string& src) {
    if (!m_failedCsv) return;
    fprintf(m_failedCsv,
        "%d,%s,0x%04X,%s,0x%06X,0x%02X,%d,%s,%s\n",
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
        "%-4d %-20s 0x%06X %-24s 0x%02X %-4d %-8s\n",
        m_failedCount, ts.c_str(), dtc,
        getEventName(id), uds, occ, src.c_str());
    fflush(m_failedTxt);
}

void EvtLogger::appendRbiHistory(Dem_EventIdType id,
                                   const FailedEntry& e) {
    if (!m_rbiCsv) return;
    m_rbiCount++;
    fprintf(m_rbiCsv,
        "%d,%d,%s,0x%04X,%s,0x%06X,0x%02X,%d,%s,%s\n",
        m_runNumber, m_rbiCount, timestamp().c_str(),
        id, getEventName(id),
        e.dtc, e.udsStatus, e.occurrences,
        getSeverity(id).c_str(), e.source);
    fflush(m_rbiCsv);
}

void EvtLogger::printRbiHistory(Dem_EventIdType id) {
    // Read rbi_history.csv and print only rows matching this event ID
    FILE* f = fopen((m_logDir + "/rbi_history.csv").c_str(), "r");
    if (!f) {
        printf("  No history file found.\n");
        return;
    }

    char line[512];
    int  found = 0;
    char searchId[12];
    snprintf(searchId, sizeof(searchId), "0x%04X", id);

    // Skip header line
    fgets(line, sizeof(line), f);

    printf("\n  %-4s %-20s %-24s %-10s %-8s %-4s\n",
           "Run","Timestamp","EventName","DTC","UDS","Occ");
    printf("  %-4s %-20s %-24s %-10s %-8s %-4s\n",
           "----","--------------------",
           "------------------------","----------","--------","----");

    while (fgets(line, sizeof(line), f)) {
        // Parse: Run,QueryNo,Timestamp,EventId,EventName,DTC,UDS,Occ,...
        int    run = 0, qno = 0, occ = 0;
        char   ts[32], eid[12], ename[32], dtc[12], uds[8], sev[10], src[32];
        int n = sscanf(line,
            "%d,%d,%31[^,],%11[^,],%31[^,],%11[^,],%7[^,],%d,%9[^,],%31s",
            &run, &qno, ts, eid, ename, dtc, uds, &occ, sev, src);
        if (n < 8) continue;

        // Match event ID
        if (strcmp(eid, searchId) != 0) continue;

        printf("  %-4d %-20s %-24s %-10s %-8s %-4d\n",
               run, ts, ename, dtc, uds, occ);
        found++;
    }
    fclose(f);

    if (found == 0)
        printf("  No previous history found for EventId %s\n", searchId);
    else
        printf("\n  Total records found: %d (across all runs)\n", found);
}

void EvtLogger::readByIdentifier(Dem_EventIdType eventId) {
    printf("\n");
    printf("+-----------------------------------------------------------------+\n");
    printf("|  ReadByIdentifier  EventId: 0x%04X  %-27s|\n",
           eventId, getEventName(eventId));
    printf("+-----------------------------------------------------------------+\n");

    // Show current run result
    if (alreadyLogged(eventId)) {
        const FailedEntry& fe = m_entries[eventId];
        printf("|  CURRENT RUN (#%d)                                             |\n",
               m_runNumber);
        printf("|  DTC Code   : 0x%06X                                        |\n",
               fe.dtc);
        printf("|  UDS Status : 0x%02X                                            |\n",
               fe.udsStatus);
        printf("|  Severity   : %-51s|\n", getSeverity(eventId).c_str());
        printf("|  Occurrences: %-51d|\n", fe.occurrences);
        printf("|  Logged At  : %-51s|\n", fe.timestamp);
        printf("|  Status     : FAILED                                           |\n");
        printf("+-----------------------------------------------------------------+\n");

        // Save this query to history
        appendRbiHistory(eventId, fe);
    } else {
        printf("|  Not triggered in current run                                   |\n");
        printf("+-----------------------------------------------------------------+\n");
    }

    // Always show full history from all previous runs
    printf("|  FULL HISTORY (all runs)                                        |\n");
    printf("+-----------------------------------------------------------------+\n");
    printRbiHistory(eventId);
    printf("+-----------------------------------------------------------------+\n\n");
}
