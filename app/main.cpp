#include <iostream>
#include <thread>
#include <chrono>
#include "DemCore.hpp"
#include "EvtLogger.hpp"
#include "KavachConditions.hpp"
#include "KavachEth.hpp"
#include "KavachUdp.hpp"
#include "KavachSensor.hpp"
#include "ModbusTcp.hpp"

int main() {
    printf("=== Kavach DEM - Live Sensor Mode ===\n");
    printf("Reading from DMI at 192.168.0.110:1502\n\n");

    // Exact constructors from headers
    DemCore    dem;
    EvtLogger  logger("logs");
    KavachEth  eth("192.168.0.110", 1502, 5601);
    ModbusTcp  modbus("192.168.0.110", 1502);
    KavachUdp  udp("192.168.0.110", 5600);
    KavachConditions cond(dem, logger, eth, modbus);
    KavachSensor     sensor;

    sensor.connect();

    uint32_t nowMs    = 0;
    uint32_t lastRxMs = 0;
    int      cycle    = 0;

    while (true) {
        cycle++;
        nowMs += 500;
        printf("\n--- Cycle %d ---\n", cycle);

        // 1. READ live data from DMI Modbus registers
        KavachSensorData data = sensor.readAll();

        if (data.valid) {
            // Simulate radio lastRx — update if signal bars > 0
            if (data.rfSignalBars > 0)
                lastRxMs = nowMs;

            // 2. FEED exact method signatures from KavachConditions.hpp
            cond.setSpeed(data.speedActual, data.speedPermitted);
            cond.setBrake((float)data.brakePipe / 10.0f);
            cond.setSos(data.sosState != 0);
            cond.setSignal(data.signalAspect, data.speedActual);
            cond.setRfid(data.rfidStatus != 0, data.rfidStatus == 1);
            cond.setRadio(lastRxMs, nowMs);
            cond.setMode(data.kavachMode);
            cond.setRollback(false, 0);

            // 3. CHECK all conditions → detect failures → log DTCs
            cond.checkAll();

            // 4. Build coilFlags from live data
            uint8_t coils = 0;
            if (data.speedActual > data.speedPermitted) coils |= 0x01;
            if (data.signalAspect == 2)                 coils |= 0x02;
            if (data.sosState != 0)                     coils |= 0x04;
            if (data.brakePipe < 20)                    coils |= 0x08;
            if ((nowMs - lastRxMs) > 5000)              coils |= 0x10;
            if (data.emergencyBrake != 0)               coils |= 0x20;
            if (data.rfidStatus == 0)                   coils |= 0x40;

            // 5. SEND snapshot to DMI screen via UDP
            // sendSnapshot(dem, kavachMode, speedActual, speedPermit, dcmSession, coilFlags)
            udp.sendSnapshot(dem,
                             data.kavachMode,
                             data.speedActual,
                             data.speedPermitted,
                             0x01,   // DCM session = default
                             coils);
        } else {
            printf("[WARN] Sensor read failed - retrying next cycle\n");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}
