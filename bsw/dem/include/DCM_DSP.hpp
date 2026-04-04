#pragma once
#include "StdTypes.hpp"
#include "DemCore.hpp"
#include "EvtLogger.hpp"
#include "DCM_DSL.hpp"

constexpr uint16_t DSP_RESP_BUF_SIZE = 256U;

// DCM_DSP: executes each UDS service and fills the response buffer
class DCM_DSP {
public:
    DCM_DSP(DemCore& dem, EvtLogger& logger, DCM_DSL& dsl);

    // Each method fills respBuf and returns response length
    uint16_t process_0x10(const uint8_t* req, uint16_t reqLen,
                          uint8_t* resp, uint16_t maxLen);

    uint16_t process_0x14(const uint8_t* req, uint16_t reqLen,
                          uint8_t* resp, uint16_t maxLen);

    uint16_t process_0x19(const uint8_t* req, uint16_t reqLen,
                          uint8_t* resp, uint16_t maxLen);

    uint16_t process_0x22(const uint8_t* req, uint16_t reqLen,
                          uint8_t* resp, uint16_t maxLen);

    uint16_t process_0x27(const uint8_t* req, uint16_t reqLen,
                          uint8_t* resp, uint16_t maxLen);

    // Build a negative response
    uint16_t buildNRC(uint8_t sid, uint8_t nrc,
                      uint8_t* resp) const;

private:
    DemCore&   m_dem;
    EvtLogger& m_logger;
    DCM_DSL&   m_dsl;
};
