#pragma once
#include <cstdint>
#include <string>
#include <modbus/modbus.h>

struct KavachSensorData {
    uint16_t speedActual{0};
    uint16_t speedPermitted{0};
    uint16_t speedWarning{0};
    uint16_t rfSignalBars{0};
    uint16_t sectionSpeed{0};
    uint16_t brakePipe{0};
    uint8_t  signalAspect{0};
    uint16_t tagId{0};
    uint16_t tagStatus{0};
    uint16_t rfid1Status{0};
    uint16_t rfid2Status{0};
    uint16_t rfid1LastTag{0};
    uint16_t rfid2LastTag{0};
    bool overspeed{false};
    bool radioLost{false};
    uint16_t emergencyBrake{0};
    uint16_t sosState{0};
    uint16_t rfidStatus{0};
    uint8_t  kavachMode{0};
    bool valid{false};
};

class KavachSensor {
public:
    KavachSensor(const std::string& ip, uint16_t port);
    ~KavachSensor();
    bool connect();
    void disconnect();
    KavachSensorData readAll();
private:
    std::string m_ip;
    uint16_t m_port;
    modbus_t* m_ctx;
    bool m_connected;
};
