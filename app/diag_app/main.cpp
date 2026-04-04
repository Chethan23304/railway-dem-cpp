#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "DemCore.hpp"
#include "DemEventConfig.hpp"
#include "NvmStorage.hpp"
#include "EvtLogger.hpp"
#include "KavachEth.hpp"
#include "KavachConditions.hpp"
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

static void runDcmDemo(DCM_DSD& dsd, EvtLogger& logger) {
    uint8_t req[8], resp[256];

    printf("\n\n========================================\n");
    printf("  DCM UDS Services\n");
    printf("========================================\n");

    // Switch to Extended session
    req[0]=0x10; req[1]=0x03;
    dsd.dispatch(req, 2, resp, sizeof(resp));

    // Read VIN
    req[0]=0x22; req[1]=0xF1; req[2]=0x90;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // Read Active Session
    req[0]=0x22; req[1]=0xF1; req[2]=0x86;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // Read Event Count
    req[0]=0x22; req[1]=0xF1; req[2]=0x00;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // Count failed DTCs
    req[0]=0x19; req[1]=0x01; req[2]=DEM_UDS_STATUS_TF;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // List all failed DTCs
    req[0]=0x19; req[1]=0x02; req[2]=DEM_UDS_STATUS_TF;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // Detail on OverSpeed DTC
    req[0]=0x19; req[1]=0x06;
    req[2]=0x00; req[3]=0xA1; req[4]=0x01;
    dsd.dispatch(req, 5, resp, sizeof(resp));

    // Security Access
    req[0]=0x27; req[1]=0x01;
    dsd.dispatch(req, 2, resp, sizeof(resp));
    uint32_t key = 0xDEADBEEFU ^ 0xCAFEBABEU;
    req[0]=0x27; req[1]=0x02;
    req[2]=static_cast<uint8_t>((key>>24)&0xFF);
    req[3]=static_cast<uint8_t>((key>>16)&0xFF);
    req[4]=static_cast<uint8_t>((key>> 8)&0xFF);
    req[5]=static_cast<uint8_t>( key     &0xFF);
    dsd.dispatch(req, 6, resp, sizeof(resp));

    // Clear all DTCs
    req[0]=0x14; req[1]=0xFF; req[2]=0xFF; req[3]=0xFF;
    dsd.dispatch(req, 4, resp, sizeof(resp));

    // Verify cleared
    req[0]=0x19; req[1]=0x01; req[2]=DEM_UDS_STATUS_TF;
    dsd.dispatch(req, 3, resp, sizeof(resp));

    // Return to Default session
    req[0]=0x10; req[1]=0x01;
    dsd.dispatch(req, 2, resp, sizeof(resp));

    // ClearDTC in Default session - should be DENIED
    req[0]=0x14; req[1]=0xFF; req[2]=0xFF; req[3]=0xFF;
    dsd.dispatch(req, 4, resp, sizeof(resp));

    // ---- ReadByIdentifier queries -> written to events_readbyid.csv ----
    printf("\n========================================\n");
    printf("  ReadByIdentifier Queries\n");
    printf("  (written to events_readbyid.csv)\n");
    printf("========================================\n");
    logger.readByIdentifier(KAVACH_EVT_OVERSPEED);
    logger.readByIdentifier(KAVACH_EVT_SPAD);
    logger.readByIdentifier(KAVACH_EVT_SOS);
    logger.readByIdentifier(KAVACH_EVT_RADIO_LOSS);
    logger.readByIdentifier(KAVACH_EVT_ROLLBACK);
    logger.readByIdentifier(KAVACH_EVT_RFID);
    logger.readByIdentifier(KAVACH_EVT_BRAKE_CMD);
    logger.readByIdentifier(KAVACH_EVT_MODE_TR);
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
    KavachConditions cond{dem, logger, eth};
    DCM_DSL          dsl{};
    DCM_DSP          dsp{dem, logger, dsl};
    DCM_DSD          dsd{dsl, dsp};

    nvm.restore(dem);
    printf("\n[MAIN] Running Kavach scenario...\n");
    sleep(1);

    runScenario(cond);
    printDtcReport(dem);
    runDcmDemo(dsd, logger);

    nvm.store(dem);
    printf("\n[MAIN] Done.\n");
    printf("  events_failed.csv   -> all FAILED events\n");
    printf("  events_readbyid.csv -> all RBI query results\n\n");
    return 0;
}
