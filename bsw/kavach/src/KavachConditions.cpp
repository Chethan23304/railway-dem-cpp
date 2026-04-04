#include "KavachConditions.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <cstring>

KavachConditions::KavachConditions(DemCore& dem,
                                    EvtLogger& logger,
                                    KavachEth& eth)
    : m_dem(dem), m_logger(logger), m_eth(eth) {
    printf("[KavachConditions] Ready\n");
}

void KavachConditions::setSpeed   (uint8_t a, uint8_t p)
    { m_speed  = {a, p}; }
void KavachConditions::setBrake   (float pr)
    { m_brake  = {pr}; }
void KavachConditions::setRfid    (bool tr, bool cl)
    { m_rfid   = {tr, cl}; }
void KavachConditions::setRadio   (uint32_t lr, uint32_t now)
    { m_radio  = {lr, now}; }
void KavachConditions::setSignal  (uint8_t asp, uint8_t spd)
    { m_signal = {asp, spd}; }
void KavachConditions::setMode    (uint8_t m)
    { m_modeData = {m}; }
void KavachConditions::setRollback(bool rb, uint8_t spd)
    { m_roll   = {rb, spd}; }
void KavachConditions::setSos     (bool pr)
    { m_sos    = {pr}; }

void KavachConditions::reportFailed(Dem_EventIdType id,
                                     uint8_t msgType,
                                     const uint8_t payload[8]) {
    if (m_logger.alreadyLogged(id)) return;

    // Report to DEM
    m_dem.setEventStatus(id, DEM_EVENT_STATUS_FAILED);

    // Get UDS status
    uint8_t uds = 0;
    m_dem.getEventStatus(id, uds);

    // Log once
    m_logger.logFailed(id, getDTC(id), uds, 1, "KavachConditions");

    // Send to Kavach simulator
    KavachEthFrame frame{};
    frame.msgType  = msgType;
    frame.eventId  = id;
    frame.dtc      = getDTC(id);
    frame.severity = (id >= 0x00A1 && id <= 0x00A6) ? DEM_SEVERITY_HIGH :
                     (id >= 0x00B1)                  ? DEM_SEVERITY_LOW  :
                                                       DEM_SEVERITY_MEDIUM;
    frame.status   = DEM_EVENT_STATUS_FAILED;
    if (payload) memcpy(frame.payload, payload, 8);
    m_eth.sendEvent(frame);
}

void KavachConditions::checkAll() {
    uint8_t payload[8] = {};

    // 1. Over Speeding
    if (m_speed.actual > m_speed.permitted && m_speed.permitted > 0) {
        payload[0] = m_speed.actual;
        payload[1] = m_speed.permitted;
        reportFailed(KAVACH_EVT_OVERSPEED,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 2. SPAD - Signal Passed At Danger (RED=0, moving)
    memset(payload, 0, 8);
    if (m_signal.aspect == 0 && m_signal.speed > 0) {
        payload[0] = m_signal.aspect;
        payload[1] = m_signal.speed;
        reportFailed(KAVACH_EVT_SPAD,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 3. SOS received
    memset(payload, 0, 8);
    if (m_sos.pressed) {
        payload[0] = 0x01;
        reportFailed(KAVACH_EVT_SOS,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 4. Radio Loss (timeout > 5000 ms)
    memset(payload, 0, 8);
    if (m_radio.nowMs > m_radio.lastRxMs &&
        (m_radio.nowMs - m_radio.lastRxMs) > 5000U) {
        uint32_t gap = m_radio.nowMs - m_radio.lastRxMs;
        payload[0] = static_cast<uint8_t>((gap >> 8) & 0xFF);
        payload[1] = static_cast<uint8_t>( gap        & 0xFF);
        reportFailed(KAVACH_EVT_RADIO_LOSS,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 5. Roll Back
    memset(payload, 0, 8);
    if (m_roll.rollback && m_roll.speed > 0) {
        payload[0] = m_roll.speed;
        reportFailed(KAVACH_EVT_ROLLBACK,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 6. RFID Miss
    memset(payload, 0, 8);
    if (!m_rfid.tagRead || !m_rfid.correctLocation) {
        payload[0] = m_rfid.tagRead ? 0x01 : 0x00;
        payload[1] = m_rfid.correctLocation ? 0x01 : 0x00;
        reportFailed(KAVACH_EVT_RFID,
                     KAVACH_ETH_MSG_INFO_EVENT, payload);
    }

    // 7. Brake Command
    memset(payload, 0, 8);
    if (m_brake.pressure < 2.0f) {
        payload[0] = static_cast<uint8_t>(m_brake.pressure * 10);
        reportFailed(KAVACH_EVT_BRAKE_CMD,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // 8. Mode Trip
    memset(payload, 0, 8);
    if (m_modeData.mode == 0x07) {
        payload[0] = m_modeData.mode;
        reportFailed(KAVACH_EVT_MODE_TR,
                     KAVACH_ETH_MSG_MODE_CHANGE, payload);
    }

    // 9. Full Supervision mode active
    memset(payload, 0, 8);
    if (m_modeData.mode == 0x02) {
        payload[0] = m_modeData.mode;
        reportFailed(KAVACH_EVT_MODE_FS,
                     KAVACH_ETH_MSG_MODE_CHANGE, payload);
    }
}
