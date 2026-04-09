#include <iostream>
#include <string>
#include "DemCore.hpp"
#include "EvtLogger.hpp"
#include "NvmStorage.hpp"
#include "KavachConditions.hpp"
#include "KavachEth.hpp"
#include "KavachUdp.hpp"
#include "KavachSensor.hpp"
#include "ModbusTcp.hpp"
#include "DCM_DSL.hpp"
#include "DCM_DSP.hpp"
#include "DCM_DSD.hpp"

static void printDtcReport(DemCore& dem) {
    printf("\n========================================\n");
    printf("  DTC Report (UDS 0x19)\n");
    printf("========================================\n");
    uint8_t count = dem.getEventMemoryCount();
    printf("Active failed DTCs: %d\n\n", count);
    for (uint8_t i = 0; i < count; i++) {
        auto e = dem.getEventMemoryEntry(i);
        printf("  DTC: 0x%06X  UDS: 0x%02X\n", e.dtc, e.udsStatusByte);
    }
}

static void runDcmDemo(DCM_DSD& dsd) {
    printf("\n========================================\n");
    printf("  DCM UDS Services\n");
    printf("========================================\n");
    uint8_t req[16]{}, resp[64]{};
    uint16_t len = 0;

    req[0]=0x10; req[1]=0x01;
    len = dsd.dispatch(req, 2, resp, sizeof(resp));
    printf("[DCM] SID=0x10 DiagSessionCtrl    | OK         RespLen=%d\n", len);

    req[0]=0x22; req[1]=0xF1; req[2]=0x90;
    len = dsd.dispatch(req, 3, resp, sizeof(resp));
    printf("[DCM] SID=0x22 ReadDataById       | OK         RespLen=%d\n", len);

    req[0]=0x22; req[1]=0xF1; req[2]=0x95;
    len = dsd.dispatch(req, 3, resp, sizeof(resp));
    printf("[DCM] SID=0x22 ReadDataById       | OK         RespLen=%d\n", len);

    req[0]=0x19; req[1]=0x01; req[2]=0x0F;
    len = dsd.dispatch(req, 3, resp, sizeof(resp));
    printf("[DCM] SID=0x19 ReadDTCInfo        | OK         RespLen=%d\n", len);

    req[0]=0x19; req[1]=0x02; req[2]=0x0F;
    len = dsd.dispatch(req, 3, resp, sizeof(resp));
    printf("[DCM] SID=0x19 ReadDTCInfo        | OK         RespLen=%d\n", len);

    req[0]=0x27; req[1]=0x01;
    len = dsd.dispatch(req, 2, resp, sizeof(resp));
    printf("[DCM] SID=0x27 SecurityAccess     | OK         RespLen=%d\n", len);

    req[0]=0x27; req[1]=0x02; req[2]=0x00; req[3]=0x00; req[4]=0x00; req[5]=0x00;
    len = dsd.dispatch(req, 6, resp, sizeof(resp));
    printf("[DCM] SID=0x27 SecurityAccess     | OK         RespLen=%d\n", len);

    req[0]=0x10; req[1]=0x01;
    len = dsd.dispatch(req, 2, resp, sizeof(resp));
    printf("[DCM] SID=0x10 DiagSessionCtrl    | OK         RespLen=%d\n", len);
}

int main() {
    printf("========================================\n");
    printf("  Railway DEM/DCM - Kavach DMI (C++)\n");
    printf("  DMI: 192.168.0.110:1502\n");
    printf("========================================\n\n");

    DemCore    dem;
    EvtLogger  logger("logs");
    NvmStorage nvm{"dem_nvram.bin"};
    KavachEth  eth{"192.168.0.110", 1502, 5601};
    ModbusTcp  modbus{"192.168.0.110", 1502};
    KavachUdp  udp{"192.168.0.110", 5600};
    KavachConditions cond(dem, logger, eth, modbus);
    KavachSensor     sensor(modbus);
    DCM_DSL  dsl{};
    DCM_DSP  dsp{dem, logger, dsl};
    DCM_DSD  dsd{dsl, dsp};

    nvm.restore(dem);

    // Read live sensor data once from DMI
    printf("\n[MAIN] Reading live sensor data from DMI...\n");
    KavachSensorData data = sensor.readAll();
    if (!data.valid) {
        printf("[WARN] DMI not reachable - using safe defaults\n");
        data.speedActual = 0; data.speedPermitted = 110;
        data.rfSignalBars = 5; data.brakePipe = 50;
        data.sosState = 0; data.rfidStatus = 1;
        data.kavachMode = 0; data.overspeed = false;
    }

    printf("\n[MAIN] Running Kavach scenario...\n");

    printf("\n=== Step 1: Normal Operation ===\n");
    cond.setSpeed(data.speedActual, data.speedPermitted);
    cond.setBrake((float)data.brakePipe / 10.0f);
    cond.setSos(false);
    cond.setSignal(1, data.speedActual);
    cond.setRfid(true, true);
    cond.setRadio(0, 0);
    cond.setMode(data.kavachMode);
    cond.setRollback(false, 0);
    cond.checkAll();

    printf("\n=== Step 2: Over Speeding (Live: %d/%d km/h) ===\n",
           data.speedActual, data.speedPermitted);
    cond.setSpeed(data.speedActual, data.speedPermitted);
    cond.checkAll();

    printf("\n=== Step 3: SPAD ===\n");
    cond.setSignal(2, data.speedActual);
    cond.checkAll();

    printf("\n=== Step 4: SOS ===\n");
    cond.setSos(true);
    cond.checkAll();

    printf("\n=== Step 5: Radio Loss ===\n");
    cond.setRadio(0, 6000);
    cond.checkAll();

    printf("\n=== Step 6: Roll Back ===\n");
    cond.setRollback(true, 10);
    cond.checkAll();

    printf("\n=== Step 7: RFID Wrong Location ===\n");
    cond.setRfid(true, false);
    cond.checkAll();

    printf("\n=== Step 8: Low Brake Pressure ===\n");
    cond.setBrake(1.5f);
    cond.checkAll();

    printf("\n=== Step 9: Mode TRIP ===\n");
    cond.setMode(0x07);
    cond.checkAll();

    udp.sendSnapshot(dem, 0x07,
                     (uint8_t)data.speedActual,
                     (uint8_t)data.speedPermitted,
                     0x01, 0xFF);

    printDtcReport(dem);
    runDcmDemo(dsd);

    printf("\n========================================\n");
    printf("  What would you like to do?\n");
    printf("----------------------------------------\n");
    printf("  1  or  all  -> Show all failed events\n");
    printf("  2  or  rbi  -> ReadByIdentifier query\n");
    printf("  q           -> Quit\n");
    printf("========================================\n");

    std::string choice;
    while (true) {
        printf("Choice: ");
        std::cin >> choice;
        if (choice == "1" || choice == "all") {
            printDtcReport(dem);
        } else if (choice == "2" || choice == "rbi") {
            printf("[RBI] No RBI entries this run.\n");
        } else if (choice == "q") {
            printf("Exiting.\n");
            break;
        }
    }

    nvm.store(dem);
    printf("\n[MAIN] Done.\n");
    return 0;
}
