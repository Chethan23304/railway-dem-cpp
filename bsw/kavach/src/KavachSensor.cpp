#include "KavachSensor.hpp"
#include "register_map.h"
#include <cstdio>
#include <cstring>

KavachSensor::KavachSensor(const std::string& ip, uint16_t port)
    : m_ip(ip), m_port(port), m_ctx(nullptr), m_connected(false) {}

KavachSensor::~KavachSensor() { disconnect(); }

bool KavachSensor::connect() {
    if (m_connected) return true;
    m_ctx = modbus_new_tcp(m_ip.c_str(), m_port);
    if (!m_ctx) return false;
    modbus_set_response_timeout(m_ctx, 2, 0);
    modbus_set_slave(m_ctx, 1);
    modbus_set_debug(m_ctx, 0);
    if (modbus_connect(m_ctx) == -1) {
        modbus_free(m_ctx);
        m_ctx = nullptr;
        return false;
    }
    m_connected = true;
    printf("[KavachSensor] Connected to %s:%u\n", m_ip.c_str(), m_port);
    return true;
}

void KavachSensor::disconnect() {
    if (m_ctx) {
        if (m_connected) modbus_close(m_ctx);
        modbus_free(m_ctx);
        m_ctx = nullptr;
        m_connected = false;
    }
}

KavachSensorData KavachSensor::readAll() {
    KavachSensorData d{};
    if (!m_connected && !connect()) return d;
    auto readReg = [this](uint16_t addr) { uint16_t v=0; modbus_read_registers(m_ctx, addr, 1, &v); return v; };
    d.speedActual    = readReg(MMI_V_TRAIN);
    d.speedPermitted = readReg(MMI_V_PERMITTED);
    d.speedWarning   = readReg(MMI_V_WARNING);
    d.rfSignalBars   = readReg(MMI_RF_SIGNAL_BAR_COUNT);
    d.sectionSpeed   = readReg(MMI_SECTION_SPEED);
    d.brakePipe      = readReg(V_MAXTRAIN);
    d.signalAspect   = static_cast<uint8_t>(readReg(MMI_SIGNAL_ASPECT));
    d.tagId          = readReg(MMI_TAG_ID);
    d.tagStatus      = readReg(MMI_TAG_STATUS_BITS);
    d.rfid1Status    = readReg(HD_RFID1_STATUS);
    d.rfid2Status    = readReg(HD_RFID2_STATUS);
    d.rfid1LastTag   = readReg(HD_RFID1_LTD);
    d.rfid2LastTag   = readReg(HD_RFID2_LTD);
    d.overspeed      = (d.speedActual > d.speedPermitted && d.speedPermitted > 0);
    d.radioLost      = (d.rfSignalBars == 0);
    d.valid          = true;
    printf("[KavachSensor] Speed=%d/%d Aspect=%d TagId=%d TagStatus=0x%X\n",
           d.speedActual, d.speedPermitted, d.signalAspect, d.tagId, d.tagStatus);
    printf("[KavachSensor] RFID1: status=0x%X lastTag=%d  RFID2: status=0x%X lastTag=%d\n",
           d.rfid1Status, d.rfid1LastTag, d.rfid2Status, d.rfid2LastTag);
    return d;
}
