#include "KavachSensor.hpp"
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
    d.speedActual = readReg(273);
    d.speedPermitted = readReg(266);
    d.speedWarning = readReg(275);
    d.rfSignalBars = readReg(294);
    d.sectionSpeed = readReg(365);
    d.brakePipe = readReg(280);
    d.signalAspect = static_cast<uint8_t>(readReg(50));
    d.tagId = readReg(100);
    d.overspeed = (d.speedActual > d.speedPermitted && d.speedPermitted > 0);
    d.radioLost = (d.rfSignalBars == 0);
    d.valid = true;
    printf("[KavachSensor] Speed=%d/%d Aspect=%d TagId=%d\n", d.speedActual, d.speedPermitted, d.signalAspect, d.tagId);
    return d;
}
