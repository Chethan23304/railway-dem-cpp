#pragma once
#include "StdTypes.hpp"
#include "DemCore.hpp"
#include "EvtLogger.hpp"
#include "KavachEth.hpp"

// KavachConditions: checks all Kavach safety conditions and reports to DEM
class KavachConditions {
public:
    struct SpeedData   { uint8_t actual = 0;  uint8_t permitted = 0; };
    struct BrakeData   { float   pressure = 5.0f; };
    struct RfidData    { bool    tagRead = true; bool correctLocation = true; };
    struct RadioData   { uint32_t lastRxMs = 0; uint32_t nowMs = 0; };
    struct SignalData  { uint8_t aspect = 1; uint8_t speed = 0; };
    struct ModeData    { uint8_t mode = 0; };
    struct RollData    { bool    rollback = false; uint8_t speed = 0; };
    struct SosData     { bool    pressed = false; };

    KavachConditions(DemCore& dem, EvtLogger& logger, KavachEth& eth);

    // Set simulated sensor values
    void setSpeed(uint8_t actual, uint8_t permitted);
    void setBrake(float pressure);
    void setRfid(bool tagRead, bool correctLocation);
    void setRadio(uint32_t lastRxMs, uint32_t nowMs);
    void setSignal(uint8_t aspect, uint8_t trainSpeed);
    void setMode(uint8_t mode);
    void setRollback(bool rollback, uint8_t speed);
    void setSos(bool pressed);

    // Run all condition checks - call once per scenario step
    void checkAll();

private:
    DemCore&   m_dem;
    EvtLogger& m_logger;
    KavachEth& m_eth;

    SpeedData  m_speed{};
    BrakeData  m_brake{};
    RfidData   m_rfid{};
    RadioData  m_radio{};
    SignalData m_signal{};
    ModeData   m_modeData{};
    RollData   m_roll{};
    SosData    m_sos{};

    // Helpers - send + log each event only once
    void reportFailed(Dem_EventIdType id, uint8_t msgType,
                      const uint8_t payload[8]);
};
