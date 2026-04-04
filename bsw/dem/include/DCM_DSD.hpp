#pragma once
#include "StdTypes.hpp"
#include "DCM_DSL.hpp"
#include "DCM_DSP.hpp"

constexpr uint16_t DSD_RESP_BUF_SIZE = 256U;

// DCM_DSD: entry point for all UDS requests
// 1. Receives raw bytes
// 2. Extracts SID
// 3. Checks session access via DSL
// 4. Routes to correct DSP handler
// 5. Returns response bytes
class DCM_DSD {
public:
    DCM_DSD(DCM_DSL& dsl, DCM_DSP& dsp);

    // Main dispatch: call this with every incoming UDS request
    uint16_t dispatch(const uint8_t* req, uint16_t reqLen,
                      uint8_t* respBuf, uint16_t respBufSize);

    // Print last request/response summary
    void printLastExchange() const;

private:
    DCM_DSL& m_dsl;
    DCM_DSP& m_dsp;

    // For printLastExchange
    uint8_t  m_lastSid     = 0;
    uint8_t  m_lastNRC     = 0;
    uint16_t m_lastRespLen = 0;
    bool     m_lastWasNRC  = false;

    void printHex(const char* label,
                  const uint8_t* buf, uint16_t len) const;
    const char* sidName(uint8_t sid) const;
};
