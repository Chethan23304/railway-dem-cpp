#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "DemCore.hpp"
#include "DemEventConfig.hpp"
#include "NvmStorage.hpp"
#include "EvtLogger.hpp"
#include "KavachEth.hpp"
#include "KavachConditions.hpp"
#include "ModbusTcp.hpp"
#include "DCM_DSL.hpp"
#include "DCM_DSP.hpp"
#include "DCM_DSD.hpp"

static void printDtcReport(DemCore& dem) {
    DemCore::DtcFilter filter{};
    Dem_DTCType dtc = 0;
    uint8_t     status = 0;
    uint16_t    count  = 0;
    printf("\n========================================\n");
    printf("  DTC Report (UDS 0x19)\n");
    printf("========================================\n");
    dem.setDTCFilter(DEM_UDS_STATUS_TF, filter);
    dem.getNumberOfFilteredDTC(filter, count);
    printf("Active failed DTCs: %d\n\n", count);
    dem.setDTCFilter(DEM_UDS_STATUS_TF, filter);
    while (dem.getNextFilteredDTC(filter, dtc, status) == E_OK)
        printf("  DTC: 0x%06X  UDS: 0x%02X\n", dtc, status);
}

static void runScenario(KavachConditions& cond) {
    printf("\n=== Step 1: Normal Operation ===\n");
    cond.setSpeed(60, 80); cond.setBrake(5.0f);
    cond.setRfid(true, true); cond.setRadio(0, 1000);
    cond.setSignal(1, 60);   cond.setMode(0x02);
    cond.setRollback(false, 0); cond.setSos(false);
    cond.checkAll(); sleep(1);

    printf("\n=== Step 2: Over Speeding ===\n");
    cond.setSpeed(120, 80); cond.checkAll(); sleep(1);

    printf("\n=== Step 3: SPAD ===\n");
    cond.setSignal(0, 30); cond.checkAll(); sleep(1);

    printf("\n=== Step 4: SOS ===\n");
    cond.setSos(true); cond.checkAll(); sleep(1);

    printf("\n=== Step 5: Radio Loss ===\n");
    cond.setRadio(0, 6000); cond.checkAll(); sleep(1);

    printf("\n=== Step 6: Roll Back ===\n");
    cond.setRollback(true, 10); cond.checkAll(); sleep(1);

    printf("\n=== Step 7: RFID Wrong Location ===\n");
    cond.setRfid(true, false); cond.checkAll(); sleep(1);

    printf("\n=== Step 8: Low Brake Pressure ===\n");
    cond.setBrake(1.5f); cond.checkAll(); sleep(1);

    printf("\n=== Step 9: Mode TRIP ===\n");
    cond.setMode(0x07); cond.checkAll(); sleep(1);
}

static void runDcmDemo(DCM_DSD& dsd) {
    uint8_t req[8], resp[256];
    printf("\n\n========================================\n");
    printf("  DCM UDS Services\n");
    printf("========================================\n");
    req[0]=0x10; req[1]=0x03;
    dsd.dispatch(req, 2, resp, sizeof(resp));
    req[0]=0x22; req[1]=0xF1; req[2]=0x90;
    dsd.dispatch(req, 3, resp, sizeof(resp));
    req[0]=0x22; req[1]=0xF1; req[2]=0x00;
    dsd.dispatch(req, 3, resp, sizeof(resp));
    req[0]=0x19; req[1]=0x01; req[2]=DEM_UDS_STATUS_TF;
    dsd.dispatch(req, 3, resp, sizeof(resp));
    req[0]=0x19; req[1]=0x02; req[2]=DEM_UDS_STATUS_TF;
    dsd.dispatch(req, 3, resp, sizeof(resp));
    req[0]=0x27; req[1]=0x01;
    dsd.dispatch(req, 2, resp, sizeof(resp));
    uint32_t key = 0xDEADBEEFU ^ 0xCAFEBABEU;
    req[0]=0x27; req[1]=0x02;
    req[2]=static_cast<uint8_t>((key>>24)&0xFF);
    req[3]=static_cast<uint8_t>((key>>16)&0xFF);
    req[4]=static_cast<uint8_t>((key>> 8)&0xFF);
    req[5]=static_cast<uint8_t>( key     &0xFF);
    dsd.dispatch(req, 6, resp, sizeof(resp));
    req[0]=0x10; req[1]=0x01;
    dsd.dispatch(req, 2, resp, sizeof(resp));
}

// -------------------------------------------------------
// Show ALL failed events from current run
// -------------------------------------------------------
static void showAllFailed(EvtLogger& logger, DemCore& dem) {
    printf("\n");
    printf("+==============================================================+\n");
    printf("|              ALL FAILED EVENTS - CURRENT RUN                |\n");
    printf("+==============================================================+\n");

    Dem_EventIdType allIds[] = {
        KAVACH_EVT_OVERSPEED, KAVACH_EVT_SPAD,
        KAVACH_EVT_SOS,       KAVACH_EVT_RADIO_LOSS,
        KAVACH_EVT_ROLLBACK,  KAVACH_EVT_RFID,
        KAVACH_EVT_BRAKE_CMD, KAVACH_EVT_MODE_TR
    };

    int shown = 0;
    for (auto id : allIds) {
        if (!logger.alreadyLogged(id)) continue;
        shown++;
        uint8_t uds = 0;
        dem.getEventStatus(id, uds);
        printf("|\n");
        printf("|  [%d] EventId  : 0x%04X\n", shown, id);
        printf("|      Name     : %s\n", getEventName(id));
        printf("|      DTC      : 0x%06X\n", getDTC(id));
        printf("|      UDS      : 0x%02X\n", uds);
        printf("|      Severity : %s\n",
               (id>=0x00A1&&id<=0x00A4) ? "HIGH"   :
               (id==0x00A5)             ? "MEDIUM" :
               (id>=0x00B1)             ? "LOW"    : "MEDIUM");
        printf("|      Status   : FAILED\n");
    }

    if (shown == 0) {
        printf("|  No failed events in this run.\n");
    } else {
        printf("|\n");
        printf("|  Total: %d failed events\n", shown);
        printf("|  Full details in: logs/events_failed.csv\n");
    }
    printf("+==============================================================+\n\n");
}

// -------------------------------------------------------
// Interactive menu shown after scenario
// -------------------------------------------------------
static void runMenu(EvtLogger& logger, DemCore& dem) {
    char buf[32];

    while (true) {
        printf("\n========================================\n");
        printf("  What would you like to do?\n");
        printf("----------------------------------------\n");
        printf("  1  or  all  -> Show all failed events\n");
        printf("  2  or  rbi  -> ReadByIdentifier query\n");
        printf("  q           -> Quit\n");
        printf("========================================\n");
        printf("Choice: ");
        fflush(stdout);

        if (!fgets(buf, sizeof(buf), stdin)) break;

        // trim newline
        for (int i = 0; buf[i]; i++)
            if (buf[i] == '\n') { buf[i] = '\0'; break; }

        // ---- Option: show ALL failed ----
        if (strcmp(buf,"1")==0 || strcmp(buf,"all")==0 ||
            strcmp(buf,"ALL")==0) {
            showAllFailed(logger, dem);
            continue;
        }

        // ---- Option: ReadByIdentifier ----
        if (strcmp(buf,"2")==0 || strcmp(buf,"rbi")==0 ||
            strcmp(buf,"RBI")==0) {

            printf("\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            printf("| Input | Event ID | Event Name             | DTC      | Sev    |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            printf("|  A1   | 0x00A1   | Over_Speeding          | 0x00A101 | HIGH   |\n");
            printf("|  A2   | 0x00A2   | SPAD                   | 0x00A201 | HIGH   |\n");
            printf("|  A3   | 0x00A3   | SOS_Received           | 0x00A301 | HIGH   |\n");
            printf("|  A4   | 0x00A4   | Roll_Back              | 0x00A401 | HIGH   |\n");
            printf("|  A5   | 0x00A5   | Radio_Loss             | 0x00A501 | MEDIUM |\n");
            printf("|  A6   | 0x00A6   | Brake_Command          | 0x00A601 | HIGH   |\n");
            printf("|  B1   | 0x00B1   | RFID_Tag_Read          | 0x00B101 | LOW    |\n");
            printf("|  07   | 0x0007   | Trip                   | 0x010701 | MEDIUM |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            printf("|  0000 = Query ALL events                                      |\n");
            printf("|  b or q = Back to menu                                        |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");

            while (true) {
                printf("\nEnter Event ID: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                for (int i = 0; buf[i]; i++)
                    if (buf[i] == '\n') { buf[i] = '\0'; break; }

                if (strcmp(buf,"b")==0 || strcmp(buf,"B")==0 ||
                    strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
                    printf("Back to menu.\n");
                    break;
                }

                unsigned int parsed = 0;
                if (sscanf(buf, "%x", &parsed) != 1) {
                    printf("  Invalid. Enter hex like A1 or 00A1. 'b' to go back.\n");
                    continue;
                }

                Dem_EventIdType evtId =
                    static_cast<Dem_EventIdType>(parsed);

                if (evtId == 0x0000U) {
                    // Query ALL known events
                    Dem_EventIdType all[] = {
                        KAVACH_EVT_OVERSPEED, KAVACH_EVT_SPAD,
                        KAVACH_EVT_SOS,       KAVACH_EVT_RADIO_LOSS,
                        KAVACH_EVT_ROLLBACK,  KAVACH_EVT_RFID,
                        KAVACH_EVT_BRAKE_CMD, KAVACH_EVT_MODE_TR
                    };
                    for (auto id : all)
                        logger.readByIdentifier(id);
                } else {
                    logger.readByIdentifier(evtId);
                }
            }
            continue;
        }

        // ---- Quit ----
        if (strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
            printf("Exiting.\n");
            break;
        }

        printf("  Unknown option. Type 1, 2, or q.\n");
    }
}

int main() {
    printf("\n========================================\n");
    printf("  Railway DEM/DCM - Kavach DMI (C++)\n");
    printf("  Kavach Simulator: 192.168.0.110:1502\n");
    printf("========================================\n\n");

    DemCore          dem{};
    NvmStorage       nvm{"dem_nvram.bin"};
    EvtLogger        logger{"logs"};
    KavachEth        eth{"192.168.0.110", 1502, 5601};
    ModbusTcp        modbus{"192.168.0.110", 1502};
    modbus.connect();
    KavachConditions cond{dem, logger, eth, modbus};

    DCM_DSL  dsl{};
    DCM_DSP  dsp{dem, logger, dsl};
    DCM_DSD  dsd{dsl, dsp};

    nvm.restore(dem);
    printf("\n[MAIN] Running Kavach scenario...\n");
    sleep(1);

    runScenario(cond);
    modbus.pushSnapshot(dem, 0x07, 120, 80, 0x03);
    printDtcReport(dem);
    runDcmDemo(dsd);

    // Interactive menu
    runMenu(logger, dem);

    nvm.store(dem);
    printf("\n[MAIN] Done.\n");
    printf("  events_failed.csv  -> current run\n");
    printf("  rbi_history.csv    -> all runs history\n\n");
    return 0;
}
