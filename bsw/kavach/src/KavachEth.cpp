#include "KavachEth.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

KavachEth::KavachEth(const std::string& simIp,
                     uint16_t simPort, uint16_t rxPort) {
    // TX socket - send to Kavach simulator
    m_txSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_txSock < 0) { perror("[KavachEth] TX socket"); return; }

    memset(&m_simAddr, 0, sizeof(m_simAddr));
    m_simAddr.sin_family      = AF_INET;
    m_simAddr.sin_port        = htons(simPort);
    m_simAddr.sin_addr.s_addr = inet_addr(simIp.c_str());

    // RX socket - listen for replies
    m_rxSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_rxSock >= 0) {
        int reuse = 1;
        setsockopt(m_rxSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        struct sockaddr_in rx{};
        rx.sin_family      = AF_INET;
        rx.sin_port        = htons(rxPort);
        rx.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(m_rxSock, reinterpret_cast<sockaddr*>(&rx), sizeof(rx));
    }

    printf("[KavachEth] TX -> %s:%u  RX <- 0.0.0.0:%u\n",
           simIp.c_str(), simPort, rxPort);
}

KavachEth::~KavachEth() {
    if (m_txSock >= 0) close(m_txSock);
    if (m_rxSock >= 0) close(m_rxSock);
    printf("[KavachEth] Closed\n");
}

void KavachEth::packFrame(const KavachEthFrame& f, uint8_t buf[16]) const {
    memset(buf, 0, 16);
    buf[0]  = f.msgType;
    buf[1]  = static_cast<uint8_t>((f.eventId >> 8) & 0xFF);
    buf[2]  = static_cast<uint8_t>( f.eventId       & 0xFF);
    buf[3]  = static_cast<uint8_t>((f.dtc >> 16) & 0xFF);
    buf[4]  = static_cast<uint8_t>((f.dtc >>  8) & 0xFF);
    buf[5]  = static_cast<uint8_t>( f.dtc        & 0xFF);
    buf[6]  = f.severity;
    buf[7]  = f.status;
    memcpy(&buf[8], f.payload, 8);
}

bool KavachEth::unpackFrame(const uint8_t buf[16], int len,
                             KavachEthFrame& f) const {
    if (len < 8) return false;
    f.msgType  = buf[0];
    f.eventId  = static_cast<uint16_t>((buf[1] << 8) | buf[2]);
    f.dtc      = (static_cast<uint32_t>(buf[3]) << 16) |
                 (static_cast<uint32_t>(buf[4]) <<  8) |
                  static_cast<uint32_t>(buf[5]);
    f.severity = buf[6];
    f.status   = buf[7];
    if (len >= 16) memcpy(f.payload, &buf[8], 8);
    return true;
}

void KavachEth::sendEvent(const KavachEthFrame& frame) {
    if (m_txSock < 0) return;
    uint8_t buf[16];
    packFrame(frame, buf);
    sendto(m_txSock, buf, 16, 0,
           reinterpret_cast<const sockaddr*>(&m_simAddr),
           sizeof(m_simAddr));
    printf("[KavachEth] TX -> MsgType=0x%02X EventId=0x%04X "
           "DTC=0x%06X Sev=%d\n",
           frame.msgType, frame.eventId, frame.dtc, frame.severity);
}

void KavachEth::sendHeartbeat() {
    m_heartbeatCount++;
    KavachEthFrame hb{};
    hb.msgType      = KAVACH_ETH_MSG_HEARTBEAT;
    hb.payload[0]   = static_cast<uint8_t>((m_heartbeatCount >> 24) & 0xFF);
    hb.payload[1]   = static_cast<uint8_t>((m_heartbeatCount >> 16) & 0xFF);
    hb.payload[2]   = static_cast<uint8_t>((m_heartbeatCount >>  8) & 0xFF);
    hb.payload[3]   = static_cast<uint8_t>( m_heartbeatCount        & 0xFF);
    sendEvent(hb);
}

bool KavachEth::receiveFrame(KavachEthFrame& frameOut, int timeoutMs) {
    if (m_rxSock < 0) return false;
    struct timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(m_rxSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    uint8_t buf[16];
    int n = static_cast<int>(recv(m_rxSock, buf, sizeof(buf), 0));
    if (n <= 0) return false;
    return unpackFrame(buf, n, frameOut);
}

void KavachEth::tick() {
    m_tickCount++;
    if (m_tickCount % 100 == 0) sendHeartbeat();
    KavachEthFrame rx{};
    if (receiveFrame(rx, 5))
        printf("[KavachEth] RX <- MsgType=0x%02X EventId=0x%04X\n",
               rx.msgType, rx.eventId);
}
