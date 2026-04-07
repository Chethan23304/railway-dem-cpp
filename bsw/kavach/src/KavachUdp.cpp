#include "KavachUdp.hpp"
#include "DemEventConfig.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

KavachUdp::KavachUdp(const std::string& dmiIp, uint16_t port) {
    m_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sock < 0) {
        perror("[KavachUdp] socket");
        return;
    }
    memset(&m_dmiAddr, 0, sizeof(m_dmiAddr));
    m_dmiAddr.sin_family      = AF_INET;
    m_dmiAddr.sin_port        = htons(port);
    m_dmiAddr.sin_addr.s_addr = inet_addr(dmiIp.c_str());

    printf("[KavachUdp] Ready -> %s:%u  (no connection needed)\n",
           dmiIp.c_str(), port);
}

KavachUdp::~KavachUdp() {
    if (m_sock >= 0) {
        close(m_sock);
        printf("[KavachUdp] Closed\n");
    }
}

void KavachUdp::sendFrame(const KavachUdpFrame& frame) {
    if (m_sock < 0) return;
    sendto(m_sock,
           reinterpret_cast<const void*>(&frame),
           sizeof(KavachUdpFrame), 0,
           reinterpret_cast<const sockaddr*>(&m_dmiAddr),
           sizeof(m_dmiAddr));
}

void KavachUdp::sendEvent(uint8_t  msgType, uint16_t eventId,
                           uint32_t dtc,     uint8_t  udsStatus,
                           uint8_t  severity) {
    KavachUdpFrame f{};
    f.msgType   = msgType;
    f.eventId   = eventId;
    f.dtc       = dtc;
    f.udsStatus = udsStatus;
    f.severity  = severity;
    sendFrame(f);
    printf("[KavachUdp] TX Event  -> EventId=0x%04X DTC=0x%06X "
           "Sev=%d UDS=0x%02X\n",
           eventId, dtc, severity, udsStatus);
}

void KavachUdp::sendSnapshot(DemCore& dem,
                              uint8_t kavachMode,
                              uint8_t speedActual,
                              uint8_t speedPermit,
                              uint8_t dcmSession,
                              uint8_t coilFlags) {
    KavachUdpFrame f{};
    f.msgType     = KAVACH_UDP_SNAPSHOT;
    f.dtcCount    = dem.getEventMemoryCount();  // exact count from DemCore
    f.kavachMode  = kavachMode;
    f.speedActual = speedActual;
    f.speedPermit = speedPermit;
    f.dcmSession  = dcmSession;
    f.coilFlags   = coilFlags;

    // Include first active DTC in snapshot frame
    if (f.dtcCount > 0) {
        auto e      = dem.getEventMemoryEntry(0);
        f.eventId   = e.eventId;
        f.dtc       = e.dtc;
        f.udsStatus = e.udsStatusByte;
        f.severity  = (e.eventId >= 0x00A1 && e.eventId <= 0x00A4) ? 2U :
                      (e.eventId == 0x00A5)                         ? 1U :
                      (e.eventId == 0x00A6)                         ? 2U : 0U;
    }

    sendFrame(f);
    printf("[KavachUdp] TX Snapshot -> DTCs=%d Mode=0x%02X "
           "Speed=%d/%d Coils=0x%02X\n",
           f.dtcCount, kavachMode, speedActual, speedPermit, coilFlags);
}

void KavachUdp::sendHeartbeat() {
    m_hbCount++;
    KavachUdpFrame f{};
    f.msgType  = KAVACH_UDP_HEARTBEAT;
    f.dtcCount = static_cast<uint8_t>(m_hbCount & 0xFFU);
    sendFrame(f);
    printf("[KavachUdp] TX Heartbeat #%u\n", m_hbCount);
}

uint8_t KavachUdp::buildCoilFlags(DemCore& dem) {
    uint8_t flags = 0;
    uint8_t uds   = 0;

    // Check each event - if TF bit set then event is FAILED
    dem.getEventStatus(KAVACH_EVT_OVERSPEED,  uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_OVERSPEED;

    dem.getEventStatus(KAVACH_EVT_SPAD,       uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_SPAD;

    dem.getEventStatus(KAVACH_EVT_SOS,        uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_SOS;

    dem.getEventStatus(KAVACH_EVT_BRAKE_CMD,  uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_BRAKE;

    dem.getEventStatus(KAVACH_EVT_RADIO_LOSS, uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_RADIOLOSS;

    dem.getEventStatus(KAVACH_EVT_ROLLBACK,   uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_ROLLBACK;

    dem.getEventStatus(KAVACH_EVT_RFID,       uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_RFID;

    dem.getEventStatus(KAVACH_EVT_MODE_TR,    uds);
    if (uds & DEM_UDS_STATUS_TF) flags |= COIL_TRIP;

    printf("[KavachUdp] CoilFlags=0x%02X (%s%s%s%s%s%s%s%s)\n",
           flags,
           (flags & COIL_OVERSPEED) ? "OVERSPEED " : "",
           (flags & COIL_SPAD)      ? "SPAD "      : "",
           (flags & COIL_SOS)       ? "SOS "       : "",
           (flags & COIL_BRAKE)     ? "BRAKE "     : "",
           (flags & COIL_RADIOLOSS) ? "RADIOLOSS " : "",
           (flags & COIL_ROLLBACK)  ? "ROLLBACK "  : "",
           (flags & COIL_RFID)      ? "RFID "      : "",
           (flags & COIL_TRIP)      ? "TRIP "      : "");
    return flags;
}
