#pragma once
#include <cstdint>

struct KavachSensorData {
    uint16_t speedActual;
    uint16_t speedPermitted;
    uint16_t speedWarning;
    uint16_t signalAspect;
    uint16_t sosState;
    uint16_t rfSignalBars;
    uint16_t brakePipe;
    uint16_t emergencyBrake;
    uint16_t rfidStatus;
    uint8_t  kavachMode;
    bool     valid;
};

class KavachSensor {
public:
    KavachSensor() = default;
    bool connect() { return true; }
    KavachSensorData readAll();
};
