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

void KavachConditions::setSpeed   (uint8_t a, uint8_t p) { m_speed  = {a, p}; }
void KavachConditions::setBrake   (float pr)              { m_brake  = {pr}; }
void KavachConditions::setRfid    (bool tr, bool cl)      { m_rfid   = {tr, cl}; }
void KavachConditions::setRadio   (uint32_t lr, uint32_t now) { m_radio = {lr, now}; }
void KavachConditions::setSignal  (uint8_t asp, uint8_t spd)  { m_signal = {asp, spd}; }
void KavachConditions::setMode    (uint8_t m)             { m_modeData = {m}; }
void KavachConditions::setRollback(bool rb, uint8_t spd)  { m_roll  = {rb, spd}; }
void KavachConditions::setSos     (bool pr)               { m_sos   = {pr}; }

void KavachConditions::reportFailed(Dem_EventIdType id,
                                     uint8_t msgType,
                                     const uint8_t payload[8]) {
    // Log each FAILED event only ONCE per session
    if (m_logger.alreadyLogged(id)) return;

    // Report to DEM core
    m_dem.setEventStatus(id, DEM_EVENT_STATUS_FAILED);

    // Get resulting UDS status byte
    uint8_t uds = 0;
    m_dem.getEventStatus(id, uds);

    // Log to file once
    m_logger.logFailed(id, getDTC(id), uds, 1, "KavachConditions");

    // Send over Ethernet to Kavach simulator
    KavachEthFrame frame{};
    frame.msgType  = msgType;
    frame.eventId  = id;
    frame.dtc      = getDTC(id);
    frame.severity = (id >= 0x00A1 && id <= 0x00A4) ? DEM_SEVERITY_HIGH :
                     (id == 0x00A5)                  ? DEM_SEVERITY_MEDIUM :
                     (id == 0x00A6)                  ? DEM_SEVERITY_HIGH :
                     (id >= 0x00B1)                  ? DEM_SEVERITY_LOW :
                                                       DEM_SEVERITY_MEDIUM;
    frame.status   = DEM_EVENT_STATUS_FAILED;
    if (payload) memcpy(frame.payload, payload, 8);
    m_eth.sendEvent(frame);
}

void KavachConditions::checkAll() {
    uint8_t payload[8] = {};

    // -------------------------------------------------------
    // CONDITION 1: Over Speeding
    // Reason: actual speed exceeds permitted speed
    // -------------------------------------------------------
    if (m_speed.actual > m_speed.permitted && m_speed.permitted > 0) {
        memset(payload, 0, 8);
        payload[0] = m_speed.actual;      // actual speed km/h
        payload[1] = m_speed.permitted;   // permitted speed km/h
        printf("[Check] OVERSPEED: actual=%d > permitted=%d\n",
               m_speed.actual, m_speed.permitted);
        reportFailed(KAVACH_EVT_OVERSPEED,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 2: SPAD - Signal Passed At Danger
    // Reason: signal is RED (aspect=0) and train is moving (speed>0)
    // -------------------------------------------------------
    if (m_signal.aspect == 0 && m_signal.speed > 0) {
        memset(payload, 0, 8);
        payload[0] = m_signal.aspect;   // 0 = RED
        payload[1] = m_signal.speed;    // train speed
        printf("[Check] SPAD: RED signal, speed=%d\n", m_signal.speed);
        reportFailed(KAVACH_EVT_SPAD,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 3: SOS Received
    // Reason: SOS button pressed by loco pilot or received from station
    // -------------------------------------------------------
    if (m_sos.pressed) {
        memset(payload, 0, 8);
        payload[0] = 0x01;   // SOS active
        printf("[Check] SOS: button pressed\n");
        reportFailed(KAVACH_EVT_SOS,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 4: Radio Loss
    // Reason: no radio packet received for more than 5000ms
    // -------------------------------------------------------
    if (m_radio.nowMs > m_radio.lastRxMs &&
        (m_radio.nowMs - m_radio.lastRxMs) > 5000U) {
        uint32_t gap = m_radio.nowMs - m_radio.lastRxMs;
        memset(payload, 0, 8);
        payload[0] = static_cast<uint8_t>((gap >> 8) & 0xFF);
        payload[1] = static_cast<uint8_t>( gap        & 0xFF);
        printf("[Check] RADIO LOSS: gap=%ums > 5000ms\n", gap);
        reportFailed(KAVACH_EVT_RADIO_LOSS,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 5: Roll Back
    // Reason: train moving backward (rollback=true) while speed > 0
    // -------------------------------------------------------
    if (m_roll.rollback && m_roll.speed > 0) {
        memset(payload, 0, 8);
        payload[0] = m_roll.speed;   // speed during rollback
        printf("[Check] ROLLBACK: backward movement speed=%d\n",
               m_roll.speed);
        reportFailed(KAVACH_EVT_ROLLBACK,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 6: RFID Miss or Wrong Location
    // Reason: RFID tag not read OR tag read at wrong location
    // -------------------------------------------------------
    if (!m_rfid.tagRead || !m_rfid.correctLocation) {
        memset(payload, 0, 8);
        payload[0] = m_rfid.tagRead ? 0x01 : 0x00;         // tag read?
        payload[1] = m_rfid.correctLocation ? 0x01 : 0x00; // correct location?
        printf("[Check] RFID: tagRead=%d correctLocation=%d\n",
               m_rfid.tagRead, m_rfid.correctLocation);
        reportFailed(KAVACH_EVT_RFID,
                     KAVACH_ETH_MSG_INFO_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 7: Brake Command - Low Pressure
    // Reason: brake pressure below minimum threshold (2.0 bar)
    // -------------------------------------------------------
    if (m_brake.pressure < 2.0f) {
        memset(payload, 0, 8);
        payload[0] = static_cast<uint8_t>(m_brake.pressure * 10);
        printf("[Check] BRAKE: pressure=%.1f bar < 2.0 bar minimum\n",
               m_brake.pressure);
        reportFailed(KAVACH_EVT_BRAKE_CMD,
                     KAVACH_ETH_MSG_CRITICAL_EVENT, payload);
    }

    // -------------------------------------------------------
    // CONDITION 8: Mode TRIP
    // Reason: Kavach forced train into TRIP mode due to danger
    // -------------------------------------------------------
    if (m_modeData.mode == 0x07) {
        memset(payload, 0, 8);
        payload[0] = m_modeData.mode;
        printf("[Check] MODE TRIP: emergency brake applied\n");
        reportFailed(KAVACH_EVT_MODE_TR,
                     KAVACH_ETH_MSG_MODE_CHANGE, payload);
    }

    // -------------------------------------------------------
    // NOTE: Mode changes (FS, LS, SR etc.) are NOT faults.
    // They are normal operating mode transitions sent as info
    // to the Kavach simulator but NOT logged as FAILED events.
    // -------------------------------------------------------
}
