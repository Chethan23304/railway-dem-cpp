#include "KavachSensor.hpp"
#include <modbus/modbus.h>
#include <cstdio>
#include <cstring>

KavachSensorData KavachSensor::readAll() {
    KavachSensorData d{};
    d.valid = false;

    // One fresh connection per cycle - DMI closes after each request
    modbus_t* ctx = modbus_new_tcp("192.168.0.110", 1502);
    if (!ctx) return d;
    modbus_set_response_timeout(ctx, 2, 0);
    modbus_set_slave(ctx, 1);
    modbus_set_debug(ctx, 0);  // suppress internal logs

    if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
        return d;
    }

    uint16_t val = 0;
    auto readReg = [&](uint16_t addr) -> uint16_t {
        if (modbus_read_registers(ctx, addr, 1, &val) == -1) return 0;
        return val;
    };

    d.speedActual    = readReg(273);  // MMI_V_TRAIN
    d.speedPermitted = readReg(266);  // MMI_V_PERMITTED
    d.speedWarning   = readReg(275);  // MMI_V_WARNING
    d.rfSignalBars   = readReg(294);  // MMI_RF_SIGNAL_BAR_COUNT
    d.sectionSpeed   = readReg(365);  // MMI_SECTION_SPEED

    modbus_close(ctx);
    modbus_free(ctx);

    // Safe defaults for unpopulated registers
    d.brakePipe      = 50;
    d.emergencyBrake = 0;
    d.sosState       = 0;
    d.rfidStatus     = 1;
    d.kavachMode     = 0;

    d.overspeed = (d.speedActual > d.speedPermitted && d.speedPermitted > 0);
    d.radioLost = (d.rfSignalBars == 0);
    d.valid     = true;

    printf("[KavachSensor] Speed=%d/%d km/h | %s | Radio=%d bars\n",
           d.speedActual, d.speedPermitted,
           d.overspeed ? "*** OVERSPEED ***" : "OK",
           d.rfSignalBars);

    return d;
}
