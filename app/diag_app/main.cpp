#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "DemCore.hpp"
#include "DemEventConfig.hpp"
#include "NvmStorage.hpp"
#include "EvtLogger.hpp"
#include "KavachEth.hpp"
#include "KavachConditions.hpp"

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
    printf("\n");
}

static void runScenario(KavachConditions& cond) {
    printf("\n========================================\n");
    printf("  Step 1: Normal Operation (no faults)\n");
    printf("========================================\n");
    cond.setSpeed(60, 80);
    cond.setBrake(5.0f);
    cond.setRfid(true, true);
    cond.setRadio(0, 1000);
    cond.setSignal(1, 60);
    cond.setMode(0x02);
    cond.setRollback(false, 0);
    cond.setSos(false);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 2: Over Speeding (120 > 80)\n");
    printf("========================================\n");
    cond.setSpeed(120, 80);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 3: SPAD - RED signal + moving\n");
    printf("========================================\n");
    cond.setSignal(0, 30);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 4: SOS Received\n");
    printf("========================================\n");
    cond.setSos(true);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 5: Radio Loss (6000ms gap)\n");
    printf("========================================\n");
    cond.setRadio(0, 6000);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 6: Roll Back detected\n");
    printf("========================================\n");
    cond.setRollback(true, 10);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 7: RFID - Wrong location\n");
    printf("========================================\n");
    cond.setRfid(true, false);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 8: Brake pressure low (1.5 bar)\n");
    printf("========================================\n");
    cond.setBrake(1.5f);
    cond.checkAll();
    sleep(1);

    printf("\n========================================\n");
    printf("  Step 9: Mode = TRIP (0x07)\n");
    printf("========================================\n");
    cond.setMode(0x07);
    cond.checkAll();
    sleep(1);

    printf("\n  All scenario steps complete.\n");
}

int main() {
    printf("\n========================================\n");
    printf("  Railway DEM/DCM - Kavach DMI (C++)\n");
    printf("  Kavach Simulator: 192.168.0.110:1502\n");
    printf("========================================\n\n");

    // Create all objects
    DemCore       dem{};
    NvmStorage    nvm{"dem_nvram.bin"};
    EvtLogger     logger{"logs"};
    KavachEth     eth{"192.168.0.110", 1502, 5601};
    KavachConditions cond{dem, logger, eth};

    // Restore previous DTCs from NvM
    nvm.restore(dem);

    printf("\n[MAIN] Starting Kavach scenario...\n");
    sleep(1);

    // Run the demo scenario
    runScenario(cond);

    // Print DTC report
    printDtcReport(dem);

    // Interactive ReadByIdentifier
    {
        char input[32];
        uint16_t eid = 0;
        printf("========================================\n");
        printf("  ReadByIdentifier Query\n");
        printf("  Enter Event ID hex (e.g. A1 = OverSpeed)\n");
        printf("  Type q to quit\n");
        printf("========================================\n\n");
        while (true) {
            printf("Enter EventId (hex): ");
            fflush(stdout);
            if (!fgets(input, sizeof(input), stdin)) break;
            if (input[0] == 'q' || input[0] == 'Q') break;
            if (sscanf(input, "%hx", &eid) == 1)
                logger.readByIdentifier(static_cast<Dem_EventIdType>(eid));
            else
                printf("  Invalid. Example: A1 or 00A1\n\n");
        }
    }

    // Save to NvM
    nvm.store(dem);

    printf("[MAIN] Done. Check logs/ folder for event files.\n\n");
    return 0;
}
