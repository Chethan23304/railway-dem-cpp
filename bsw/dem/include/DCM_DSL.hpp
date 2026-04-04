#pragma once
#include "StdTypes.hpp"

// UDS Session types
constexpr uint8_t DCM_SESSION_DEFAULT     = 0x01U;
constexpr uint8_t DCM_SESSION_EXTENDED    = 0x03U;
constexpr uint8_t DCM_SESSION_PROGRAMMING = 0x02U;

// Security levels
constexpr uint8_t DCM_SEC_LOCKED   = 0x00U;
constexpr uint8_t DCM_SEC_UNLOCKED = 0x01U;

// Which sessions allow which SIDs
// DSL checks this before DSP executes
struct DslSessionRule {
    uint8_t sid;
    bool    allowedInDefault;
    bool    allowedInExtended;
    bool    allowedInProgramming;
};

// DCM_DSL: manages session state and session-based access control
class DCM_DSL {
public:
    DCM_DSL();

    // Change session (UDS 0x10)
    Std_ReturnType changeSession(uint8_t newSession);

    // Check if SID is allowed in current session
    // Returns E_OK if allowed, E_NOT_OK if not
    Std_ReturnType checkSessionAccess(uint8_t sid) const;

    // Security access - unlock with seed/key (simplified)
    Std_ReturnType requestSeed(uint32_t& seedOut);
    Std_ReturnType validateKey(uint32_t key);

    // Getters
    uint8_t getCurrentSession()  const { return m_session; }
    uint8_t getSecurityLevel()   const { return m_secLevel; }
    bool    isUnlocked()         const { return m_secLevel == DCM_SEC_UNLOCKED; }

    void    printStatus()        const;

    // Session timeout - call every 100ms
    void    tick();

private:
    uint8_t  m_session        = DCM_SESSION_DEFAULT;
    uint8_t  m_secLevel       = DCM_SEC_LOCKED;
    uint32_t m_sessionTimer   = 0;   // ticks since last request
    uint32_t m_lastSeed       = 0;
    bool     m_seedPending    = false;

    static constexpr uint32_t SESSION_TIMEOUT_TICKS = 50U; // 50 * 100ms = 5s
    static const DslSessionRule s_rules[];
    static const uint8_t        s_ruleCount;

    const char* sessionName(uint8_t s) const;
};
