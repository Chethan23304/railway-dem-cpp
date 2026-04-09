#include "DCM_DSP.hpp"
#include "DemEventConfig.hpp"
#include <cstring>

constexpr uint8_t NRC_SUB_NOT_SUPPORTED  = 0x12U;
constexpr uint8_t NRC_INCORRECT_LENGTH   = 0x13U;
constexpr uint8_t NRC_CONDITIONS_NOT_MET = 0x22U;
constexpr uint8_t NRC_REQUEST_OUT_RANGE  = 0x31U;
constexpr uint8_t NRC_CONDITIONS_NOT_MET2= 0x22U;
constexpr uint8_t NRC_INVALID_KEY        = 0x35U;

DCM_DSP::DCM_DSP(DemCore& dem, EvtLogger& logger, DCM_DSL& dsl)
    : m_dem(dem), m_logger(logger), m_dsl(dsl) {}

uint16_t DCM_DSP::buildNRC(uint8_t sid, uint8_t nrc, uint8_t* resp) const {
    resp[0] = 0x7F;
    resp[1] = sid;
    resp[2] = nrc;
    return 3U;
}

// 0x10 DiagnosticSessionControl
uint16_t DCM_DSP::process_0x10(const uint8_t* req, uint16_t reqLen,
                                 uint8_t* resp, uint16_t maxLen) {
    (void)maxLen;
    if (reqLen < 2) return buildNRC(0x10, NRC_INCORRECT_LENGTH, resp);
    if (m_dsl.changeSession(req[1]) != E_OK)
        return buildNRC(0x10, NRC_SUB_NOT_SUPPORTED, resp);
    resp[0] = 0x50; resp[1] = req[1];
    resp[2] = 0x00; resp[3] = 0x19;
    resp[4] = 0x01; resp[5] = 0xF4;
    return 6U;
}

// 0x14 ClearDiagnosticInformation
uint16_t DCM_DSP::process_0x14(const uint8_t* req, uint16_t reqLen,
                                 uint8_t* resp, uint16_t maxLen) {
    (void)maxLen;
    if (reqLen < 4) return buildNRC(0x14, NRC_INCORRECT_LENGTH, resp);
    Dem_DTCType dtcGroup =
        (static_cast<uint32_t>(req[1]) << 16) |
        (static_cast<uint32_t>(req[2]) <<  8) |
         static_cast<uint32_t>(req[3]);
    if (m_dem.clearDTC(dtcGroup) != E_OK)
        return buildNRC(0x14, NRC_CONDITIONS_NOT_MET, resp);
    resp[0] = 0x54;
    return 1U;
}

// 0x19 ReadDTCInformation
uint16_t DCM_DSP::process_0x19(const uint8_t* req, uint16_t reqLen,
                                 uint8_t* resp, uint16_t maxLen) {
    if (reqLen < 2) return buildNRC(0x19, NRC_INCORRECT_LENGTH, resp);
    uint8_t subFunc = req[1];
    resp[0] = 0x59;
    resp[1] = subFunc;
    uint16_t respLen = 2U;

    if (subFunc == 0x01U) {
        if (reqLen < 3) return buildNRC(0x19, NRC_INCORRECT_LENGTH, resp);
        Dem_DTCStatusMask mask = req[2];
        DemCore::DtcFilter filter{};
        uint16_t count = 0;
        m_dem.setDTCFilter(mask, filter);
        m_dem.getNumberOfFilteredDTC(filter, count);
        resp[respLen++] = mask;
        resp[respLen++] = 0x01;
        resp[respLen++] = static_cast<uint8_t>((count >> 8) & 0xFF);
        resp[respLen++] = static_cast<uint8_t>( count       & 0xFF);

    } else if (subFunc == 0x02U) {
        if (reqLen < 3) return buildNRC(0x19, NRC_INCORRECT_LENGTH, resp);
        Dem_DTCStatusMask mask = req[2];
        resp[respLen++] = mask;
        DemCore::DtcFilter filter{};
        m_dem.setDTCFilter(mask, filter);
        Dem_DTCType dtc = 0;
        uint8_t status = 0;
        while (m_dem.getNextFilteredDTC(filter, dtc, status) == E_OK) {
            if (respLen + 4U >= maxLen) break;
            resp[respLen++] = static_cast<uint8_t>((dtc >> 16) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>((dtc >>  8) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>( dtc        & 0xFF);
            resp[respLen++] = status;
        }

    } else if (subFunc == 0x06U) {
        if (reqLen < 5) return buildNRC(0x19, NRC_INCORRECT_LENGTH, resp);
        Dem_DTCType dtc =
            (static_cast<uint32_t>(req[2]) << 16) |
            (static_cast<uint32_t>(req[3]) <<  8) |
             static_cast<uint32_t>(req[4]);
        bool found = false;
        for (uint8_t i = 0; i < m_dem.getEventMemoryCount(); i++) {
            auto e = m_dem.getEventMemoryEntry(i);
            if (e.dtc != dtc) continue;
            resp[respLen++] = static_cast<uint8_t>((dtc >> 16) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>((dtc >>  8) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>( dtc        & 0xFF);
            resp[respLen++] = e.udsStatusByte;
            resp[respLen++] = e.occurrences;
            resp[respLen++] = e.agingCounter;
            found = true;
            break;
        }
        if (!found) return buildNRC(0x19, NRC_REQUEST_OUT_RANGE, resp);

    } else {
        return buildNRC(0x19, NRC_SUB_NOT_SUPPORTED, resp);
    }
    return respLen;
}

// 0x22 ReadDataByIdentifier
uint16_t DCM_DSP::process_0x22(const uint8_t* req, uint16_t reqLen,
                                 uint8_t* resp, uint16_t maxLen) {
    (void)maxLen;
    if (reqLen < 3) return buildNRC(0x22, NRC_INCORRECT_LENGTH, resp);
    uint16_t dataId =
        (static_cast<uint16_t>(req[1]) << 8) |
         static_cast<uint16_t>(req[2]);
    resp[0] = 0x62;
    resp[1] = req[1];
    resp[2] = req[2];
    uint16_t respLen = 3U;

    if (dataId == 0xF190U) {
        memcpy(&resp[respLen], "KAVACH0001234567", 17);
        respLen += 17;
    } else if (dataId == 0xF186U) {
        resp[respLen++] = m_dsl.getCurrentSession();
    } else if (dataId == 0xF18CU) {
        memcpy(&resp[respLen], "RAIL-DEM-CPP-001", 16);
        respLen += 16;
    } else if (dataId == 0xF100U) {
        resp[respLen++] = m_dem.getEventMemoryCount();
    } else {
        // Event ID as DID — look it up in event memory
        bool found = false;
        for (uint8_t i = 0; i < m_dem.getEventMemoryCount(); i++) {
            auto e = m_dem.getEventMemoryEntry(i);
            if (e.eventId != dataId) continue;
            resp[respLen++] = e.udsStatusByte;
            resp[respLen++] = static_cast<uint8_t>((e.dtc >> 16) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>((e.dtc >>  8) & 0xFF);
            resp[respLen++] = static_cast<uint8_t>( e.dtc        & 0xFF);
            resp[respLen++] = e.occurrences;
            found = true;
            break;
        }
        if (!found) return buildNRC(0x22, NRC_REQUEST_OUT_RANGE, resp);
    }
    return respLen;
}

// 0x27 SecurityAccess
uint16_t DCM_DSP::process_0x27(const uint8_t* req, uint16_t reqLen,
                                 uint8_t* resp, uint16_t maxLen) {
    (void)maxLen;
    if (reqLen < 2) return buildNRC(0x27, NRC_INCORRECT_LENGTH, resp);
    uint8_t subFunc = req[1];
    resp[0] = 0x67;
    resp[1] = subFunc;
    uint16_t respLen = 2U;

    if (subFunc == 0x01U) {
        uint32_t seed = 0;
        if (m_dsl.requestSeed(seed) != E_OK)
            return buildNRC(0x27, NRC_CONDITIONS_NOT_MET2, resp);
        resp[respLen++] = static_cast<uint8_t>((seed >> 24) & 0xFF);
        resp[respLen++] = static_cast<uint8_t>((seed >> 16) & 0xFF);
        resp[respLen++] = static_cast<uint8_t>((seed >>  8) & 0xFF);
        resp[respLen++] = static_cast<uint8_t>( seed        & 0xFF);
    } else if (subFunc == 0x02U) {
        if (reqLen < 6) return buildNRC(0x27, NRC_INCORRECT_LENGTH, resp);
        uint32_t key =
            (static_cast<uint32_t>(req[2]) << 24) |
            (static_cast<uint32_t>(req[3]) << 16) |
            (static_cast<uint32_t>(req[4]) <<  8) |
             static_cast<uint32_t>(req[5]);
        if (m_dsl.validateKey(key) != E_OK)
            return buildNRC(0x27, NRC_INVALID_KEY, resp);
    } else {
        return buildNRC(0x27, NRC_SUB_NOT_SUPPORTED, resp);
    }
    return respLen;
}
