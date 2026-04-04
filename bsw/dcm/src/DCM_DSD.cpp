#include "DCM_DSD.hpp"
#include <cstdio>

DCM_DSD::DCM_DSD(DCM_DSL& dsl, DCM_DSP& dsp)
    : m_dsl(dsl), m_dsp(dsp) {}

const char* DCM_DSD::sidName(uint8_t sid) const {
    switch (sid) {
        case 0x10: return "DiagSessionCtrl";
        case 0x11: return "ECUReset";
        case 0x14: return "ClearDTC";
        case 0x19: return "ReadDTCInfo";
        case 0x22: return "ReadDataById";
        case 0x27: return "SecurityAccess";
        case 0x2E: return "WriteDataById";
        case 0x31: return "RoutineControl";
        default:   return "Unknown";
    }
}

void DCM_DSD::printHex(const char* label,
                        const uint8_t* buf, uint16_t len) const {
    printf("  %-8s [%2d]: ", label, len);
    for (uint16_t i = 0; i < len && i < 24; i++)
        printf("%02X ", buf[i]);
    if (len > 24) printf("...");
    printf("\n");
}

uint16_t DCM_DSD::dispatch(const uint8_t* req, uint16_t reqLen,
                             uint8_t* resp, uint16_t respBufSize) {
    if (!req || reqLen < 1 || !resp) return 0;

    uint8_t sid = req[0];
    m_lastSid   = sid;

    // Check session access
    if (m_dsl.checkSessionAccess(sid) != E_OK) {
        resp[0] = 0x7F; resp[1] = sid; resp[2] = 0x25;
        m_lastWasNRC  = true;
        m_lastNRC     = 0x25;
        m_lastRespLen = 3;
        printf("[DCM] SID=0x%02X %-18s | DENIED (session restriction) "
               "NRC=0x25\n", sid, sidName(sid));
        return 3;
    }

    uint16_t respLen = 0;
    switch (sid) {
        case 0x10: respLen = m_dsp.process_0x10(req, reqLen, resp, respBufSize); break;
        case 0x14: respLen = m_dsp.process_0x14(req, reqLen, resp, respBufSize); break;
        case 0x19: respLen = m_dsp.process_0x19(req, reqLen, resp, respBufSize); break;
        case 0x22: respLen = m_dsp.process_0x22(req, reqLen, resp, respBufSize); break;
        case 0x27: respLen = m_dsp.process_0x27(req, reqLen, resp, respBufSize); break;
        default:
            resp[0] = 0x7F; resp[1] = sid; resp[2] = 0x11;
            respLen = 3;
            break;
    }

    m_lastRespLen = respLen;
    m_lastWasNRC  = (respLen >= 3 && resp[0] == 0x7F);
    m_lastNRC     = m_lastWasNRC ? resp[2] : 0x00;

    if (m_lastWasNRC)
        printf("[DCM] SID=0x%02X %-18s | NRC=0x%02X  RespLen=%d\n",
               sid, sidName(sid), m_lastNRC, respLen);
    else
        printf("[DCM] SID=0x%02X %-18s | OK         RespLen=%d\n",
               sid, sidName(sid), respLen);

    return respLen;
}

void DCM_DSD::printLastExchange() const {
    printf("  Last SID=0x%02X %-18s | ", m_lastSid, sidName(m_lastSid));
    if (m_lastWasNRC)
        printf("NRC=0x%02X  RespLen=%d\n", m_lastNRC, m_lastRespLen);
    else
        printf("OK         RespLen=%d\n", m_lastRespLen);
}
