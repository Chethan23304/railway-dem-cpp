#include "DCM_DSL.hpp"
#include <cstdio>

const DslSessionRule DCM_DSL::s_rules[] = {
    { 0x10,   true,    true,     true  },
    { 0x11,   false,   true,     true  },
    { 0x14,   false,   true,     false },
    { 0x19,   true,    true,     false },
    { 0x22,   true,    true,     false },
    { 0x27,   false,   true,     true  },
    { 0x2E,   false,   false,    true  },
    { 0x31,   false,   true,     true  },
};
const uint8_t DCM_DSL::s_ruleCount =
    static_cast<uint8_t>(sizeof(DCM_DSL::s_rules) / sizeof(DCM_DSL::s_rules[0]));

DCM_DSL::DCM_DSL() {
    m_session      = DCM_SESSION_DEFAULT;
    m_secLevel     = DCM_SEC_LOCKED;
    m_sessionTimer = 0;
    m_seedPending  = false;
}

const char* DCM_DSL::sessionName(uint8_t s) const {
    switch (s) {
        case DCM_SESSION_DEFAULT:     return "DEFAULT";
        case DCM_SESSION_EXTENDED:    return "EXTENDED";
        case DCM_SESSION_PROGRAMMING: return "PROGRAMMING";
        default:                      return "UNKNOWN";
    }
}

Std_ReturnType DCM_DSL::changeSession(uint8_t newSession) {
    if (newSession != DCM_SESSION_DEFAULT &&
        newSession != DCM_SESSION_EXTENDED &&
        newSession != DCM_SESSION_PROGRAMMING)
        return E_NOT_OK;
    m_session      = newSession;
    m_sessionTimer = 0;
    if (newSession == DCM_SESSION_DEFAULT)
        m_secLevel = DCM_SEC_LOCKED;
    return E_OK;
}

Std_ReturnType DCM_DSL::checkSessionAccess(uint8_t sid) const {
    for (uint8_t i = 0; i < s_ruleCount; i++) {
        if (s_rules[i].sid != sid) continue;
        bool allowed = false;
        switch (m_session) {
            case DCM_SESSION_DEFAULT:     allowed = s_rules[i].allowedInDefault;     break;
            case DCM_SESSION_EXTENDED:    allowed = s_rules[i].allowedInExtended;    break;
            case DCM_SESSION_PROGRAMMING: allowed = s_rules[i].allowedInProgramming; break;
        }
        return allowed ? E_OK : E_NOT_OK;
    }
    return E_NOT_OK;
}

Std_ReturnType DCM_DSL::requestSeed(uint32_t& seedOut) {
    if (m_session == DCM_SESSION_DEFAULT) return E_NOT_OK;
    m_lastSeed    = 0xDEADBEEFU;
    m_seedPending = true;
    seedOut       = m_lastSeed;
    return E_OK;
}

Std_ReturnType DCM_DSL::validateKey(uint32_t key) {
    if (!m_seedPending) return E_NOT_OK;
    uint32_t expected = m_lastSeed ^ 0xCAFEBABEU;
    m_seedPending = false;
    if (key == expected) {
        m_secLevel = DCM_SEC_UNLOCKED;
        return E_OK;
    }
    return E_NOT_OK;
}

void DCM_DSL::tick() {
    if (m_session != DCM_SESSION_DEFAULT) {
        m_sessionTimer++;
        if (m_sessionTimer >= SESSION_TIMEOUT_TICKS)
            changeSession(DCM_SESSION_DEFAULT);
    }
}

void DCM_DSL::printStatus() const {
    printf("  Session=%-12s Security=%s\n",
           sessionName(m_session),
           m_secLevel == DCM_SEC_UNLOCKED ? "UNLOCKED" : "LOCKED");
}
