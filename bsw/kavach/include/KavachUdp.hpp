#pragma once
#include <string>
#include <netinet/in.h>
#include "StdTypes.hpp"
#include "KavachUdpFrame.hpp"
#include "DemCore.hpp"

// ============================================================
// KavachUdp - sends DEM state to Qt DMI over UDP
// Completely independent - does not touch any existing module
// No connection needed - Qt DMI just binds port 5600 and reads
// ============================================================
class KavachUdp {
public:
    KavachUdp(const std::string& dmiIp  = "192.168.0.110",
              uint16_t           port   = KAVACH_UDP_PORT,
              DemCore*           dem    = nullptr);
    ~KavachUdp();

    // Send one event frame - call this when any event FAILS
    void sendEvent(uint8_t  msgType,
                   uint16_t eventId,
                   uint32_t dtc,
                   uint8_t  udsStatus,
                   uint8_t  severity);

    // Send full DEM snapshot - call after scenario completes
    // coilFlags: pack all active event flags into one byte
    void sendSnapshot(DemCore& dem,
                      uint8_t  kavachMode,
                      uint8_t  speedActual,
                      uint8_t  speedPermit,
                      uint8_t  dcmSession,
                      uint8_t  coilFlags);

    // Send heartbeat - call periodically to show DEM is alive
    void sendHeartbeat();

    // Build coilFlags byte from DemCore active events
    static uint8_t buildCoilFlags(DemCore& dem);

    bool isReady() const { return m_sock >= 0; }

private:
    DemCore*           m_dem = nullptr;
    int                m_sock = -1;
    struct sockaddr_in m_dmiAddr{};
    uint32_t           m_hbCount = 0;

    void sendFrame(const KavachUdpFrame& frame);
};
