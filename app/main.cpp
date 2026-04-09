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
#include "KavachUdp.hpp"
#include "KavachSensor.hpp"
#include "DCM_DSL.hpp"
#include "DCM_DSP.hpp"
#include "DCM_DSD.hpp"

static constexpr int CONFIRM_CYCLES = 4;

static void printDtcReport(DemCore& dem) {
    printf("\n========================================\n");
    printf("  DTC Report (UDS 0x19)\n");
    printf("========================================\n");
    uint8_t count = dem.getEventMemoryCount();
    printf("  Active failed DTCs: %d\n\n", count);
    if (count == 0) {
        printf("  No faults detected - all conditions normal.\n");
    } else {
        for (uint8_t i = 0; i < count; i++) {
            auto e = dem.getEventMemoryEntry(i);
            printf("  DTC: 0x%06X  UDS: 0x%02X\n",
                   e.dtc, e.udsStatusByte);
        }
    }
    printf("========================================\n");
}

static void runMenu(EvtLogger& logger, DemCore& dem) {
    char buf[32]{};
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
        for (int i = 0; buf[i]; i++)
            if (buf[i] == '\n') { buf[i] = '\0'; break; }

        if (strcmp(buf,"1")==0 || strcmp(buf,"all")==0) {
            printDtcReport(dem);

        } else if (strcmp(buf,"2")==0 || strcmp(buf,"rbi")==0) {
            printf("\n+-------+----------+------------------------+----------+--------+\n");
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
            printf("|  0000 = Query ALL  |  b or q = Back                           |\n");
            printf("+-------+----------+------------------------+----------+--------+\n");
            while (true) {
                printf("\nEnter Event ID: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin)) break;
                for (int i = 0; buf[i]; i++)
                    if (buf[i] == '\n') { buf[i] = '\0'; break; }
                if (strcmp(buf,"b")==0 || strcmp(buf,"B")==0 ||
                    strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
                    printf("Back to menu.\n"); break;
                }
                unsigned int parsed = 0;
                if (sscanf(buf, "%x", &parsed) != 1) {
                    printf("  Invalid. Enter hex like A1. b to go back.\n");
                    continue;
                }
                Dem_EventIdType evtId =
                    static_cast<Dem_EventIdType>(parsed);
                if (evtId == 0x0000U) {
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

        } else if (strcmp(buf,"q")==0 || strcmp(buf,"Q")==0) {
            printf("Exiting.\n");
            break;
        } else {
            printf("  Unknown option.\n");
        }
    }
}

int main() {
    printf("\n========================================\n");
    printf("  Railway DEM/DCM - Kavach Live Mode\n");
    printf("  DMI:  192.168.0.110:1502\n");
    printf("  UDP:  192.168.0.110:5600\n");
    printf("  Confirmation: %d consecutive cycles\n",
           CONFIRM_CYCLES);
    printf("========================================\n\n");

    DemCore          dem{};
    NvmStorage       nvm{"dem_nvram.bin"};
    EvtLogger        logger{"logs"};
    KavachEth        eth{"192.168.0.110", 1502, 5601};
    ModbusTcp        modbus{"192.168.0.110", 1502};
    KavachUdp        udp{"192.168.0.110", KAVACH_UDP_PORT};
    KavachConditions cond{dem, logger, eth, modbus};
    KavachSensor     sensor{"192.168.0.110", 1502};

    DCM_DSL  dsl{};
    DCM_DSP  dsp{dem, logger, dsl};
    DCM_DSD  dsd{dsl, dsp};

    nvm.restore(dem);
    modbus.connect();

    if (!sensor.connect())
        printf("[WARN] Sensor not connected - will retry\n");

    // ---- Debounce counters (one per condition) ----
    int cnt_speed  = 0;
    int cnt_brake  = 0;
    int cnt_sos    = 0;
    int cnt_signal = 0;
    int cnt_radio  = 0;
    int cnt_rfid   = 0;

    // ---- Track which events already confirmed this session ----
    bool confirmed_speed  = false;
    bool confirmed_brake  = false;
    bool confirmed_sos    = false;
    bool confirmed_signal = false;
    bool confirmed_radio  = false;
    bool confirmed_rfid   = false;

    uint32_t nowMs    = 0;
    uint32_t lastRxMs = 0;

    printf("[MAIN] Reading live DMI data...\n");
    printf("[MAIN] Each condition must fail %d consecutive cycles"
           " to be confirmed.\n\n", CONFIRM_CYCLES);

    for (int cycle = 1; cycle <= CONFIRM_CYCLES; cycle++) {
        nowMs += 500;

        printf("--- Cycle %d / %d ---\n", cycle, CONFIRM_CYCLES);

        KavachSensorData data = sensor.readAll();

        if (!data.valid) {
            printf("[WARN] Sensor read failed - retrying...\n");
            sensor.disconnect();
            sleep(1);
            sensor.connect();
            cycle--;   // retry this cycle
            continue;
        }

        // Update radio timer
        if (data.rfSignalBars > 0) lastRxMs = nowMs;

        // ---- Count consecutive failures per condition ----

        // Overspeed
        if (data.speedActual > 0 &&
            data.speedPermitted > 0 &&
            data.speedActual > data.speedPermitted) {
            cnt_speed++;
            printf("  [SPEED]  %d > %d  cnt=%d/%d\n",
                   data.speedActual, data.speedPermitted,
                   cnt_speed, CONFIRM_CYCLES);
        } else {
            cnt_speed = 0;
        }

        // Brake pressure
        float bpBar = static_cast<float>(data.brakePipe) / 10.0f;
        if (data.brakePipe > 0 && bpBar < 2.0f) {
            cnt_brake++;
            printf("  [BRAKE]  %.1f bar < 2.0  cnt=%d/%d\n",
                   bpBar, cnt_brake, CONFIRM_CYCLES);
        } else {
            cnt_brake = 0;
        }

        // SOS
        if (data.sosState == 1) {
            cnt_sos++;
            printf("  [SOS]    pressed  cnt=%d/%d\n",
                   cnt_sos, CONFIRM_CYCLES);
        } else {
            cnt_sos = 0;
        }

        // Signal SPAD (aspect 3 = danger AND moving)
        if (data.signalAspect == 3 && data.speedActual > 0) {
            cnt_signal++;
            printf("  [SIGNAL] aspect=%d speed=%d  cnt=%d/%d\n",
                   data.signalAspect, data.speedActual,
                   cnt_signal, CONFIRM_CYCLES);
        } else {
            cnt_signal = 0;
        }

        // Radio loss
        if ((nowMs - lastRxMs) > 5000U) {
            cnt_radio++;
            printf("  [RADIO]  gap=%ums  cnt=%d/%d\n",
                   (nowMs - lastRxMs), cnt_radio, CONFIRM_CYCLES);
        } else {
            cnt_radio = 0;
        }

        // RFID — tag must be present (tagId > 0) AND hardware OK
        if (data.tagId == 0 || data.rfidStatus != 0) {
            cnt_rfid++;
            printf("  [RFID]   tagId=%d hw=%d  cnt=%d/%d\n",
                   data.tagId, data.rfidStatus,
                   cnt_rfid, CONFIRM_CYCLES);
        } else {
            cnt_rfid = 0;
        }

        // ---- Confirm events that hit 4 cycles ----
        if (cnt_speed >= CONFIRM_CYCLES && !confirmed_speed) {
            confirmed_speed = true;
            printf("\n  [CONFIRMED] OVERSPEED -> reporting to DEM\n\n");
            cond.setSpeed(static_cast<uint8_t>(data.speedActual),
                          static_cast<uint8_t>(data.speedPermitted));
            cond.checkAll();
        }
        if (cnt_brake >= CONFIRM_CYCLES && !confirmed_brake) {
            confirmed_brake = true;
            printf("\n  [CONFIRMED] BRAKE FAULT -> reporting to DEM\n\n");
            cond.setBrake(bpBar);
            cond.checkAll();
        }
        if (cnt_sos >= CONFIRM_CYCLES && !confirmed_sos) {
            confirmed_sos = true;
            printf("\n  [CONFIRMED] SOS -> reporting to DEM\n\n");
            cond.setSos(true);
            cond.checkAll();
        }
        if (cnt_signal >= CONFIRM_CYCLES && !confirmed_signal) {
            confirmed_signal = true;
            printf("\n  [CONFIRMED] SPAD -> reporting to DEM\n\n");
            cond.setSignal(static_cast<uint8_t>(data.signalAspect),
                           static_cast<uint8_t>(data.speedActual));
            cond.checkAll();
        }
        if (cnt_radio >= CONFIRM_CYCLES && !confirmed_radio) {
            confirmed_radio = true;
            printf("\n  [CONFIRMED] RADIO LOSS -> reporting to DEM\n\n");
            cond.setRadio(lastRxMs, nowMs);
            cond.checkAll();
        }
        if (cnt_rfid >= CONFIRM_CYCLES && !confirmed_rfid) {
            confirmed_rfid = true;
            printf("\n  [CONFIRMED] RFID FAULT -> reporting to DEM\n\n");
            cond.setRfid(data.tagId > 0, data.rfidStatus == 0);
            cond.checkAll();
        }

        // Send UDP snapshot every cycle
        uint8_t coils = KavachUdp::buildCoilFlags(dem);
        udp.sendSnapshot(dem,
                         data.kavachMode,
                         static_cast<uint8_t>(data.speedActual),
                         static_cast<uint8_t>(data.speedPermitted),
                         static_cast<uint8_t>(dsl.getCurrentSession()),
                         coils);

        usleep(500000);  // 500ms between cycles
    }

    // ---- Print final DTC report ----
    printf("\n========================================\n");
    printf("  4-Cycle Confirmation Complete\n");
    printf("========================================\n");
    printf("  Overspeed   : %s\n", confirmed_speed  ? "CONFIRMED FAILED" : "OK");
    printf("  Brake       : %s\n", confirmed_brake  ? "CONFIRMED FAILED" : "OK");
    printf("  SOS         : %s\n", confirmed_sos    ? "CONFIRMED FAILED" : "OK");
    printf("  SPAD        : %s\n", confirmed_signal ? "CONFIRMED FAILED" : "OK");
    printf("  Radio Loss  : %s\n", confirmed_radio  ? "CONFIRMED FAILED" : "OK");
    printf("  RFID        : %s\n", confirmed_rfid   ? "CONFIRMED FAILED" : "OK");
    printf("========================================\n");

    printDtcReport(dem);

    // Save NvM
    nvm.store(dem);

    // Interactive menu
    runMenu(logger, dem);

    printf("\n[MAIN] Done.\n");
    printf("  events_failed.csv -> current run\n");
    printf("  rbi_history.csv   -> all runs history\n\n");
    return 0;
}
