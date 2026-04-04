#pragma once
#include <string>
#include <netinet/in.h>
#include "StdTypes.hpp"

// Message type IDs sent to Kavach simulator
constexpr uint8_t KAVACH_ETH_MSG_MODE_CHANGE    = 0x01U;
constexpr uint8_t KAVACH_ETH_MSG_CRITICAL_EVENT = 0x02U;
constexpr uint8_t KAVACH_ETH_MSG_INFO_EVENT     = 0x03U;
constexpr uint8_t KAVACH_ETH_MSG_HEARTBEAT      = 0x04U;

// 16-byte Ethernet frame to Kavach simulator
struct KavachEthFrame {
    uint8_t  msgType  = 0;
    uint16_t eventId  = 0;
    uint32_t dtc      = 0;
    uint8_t  severity = 0;
    uint8_t  status   = 0;
    uint8_t  payload[8] = {};
};

// KavachEth: sends Kavach events to DMI simulator over Ethernet UDP
class KavachEth {
public:
    KavachEth(const std::string& simIp   = "192.168.0.110",
              uint16_t           simPort  = 1502,
              uint16_t           rxPort   = 5601);
    ~KavachEth();

    // Send one event frame to Kavach simulator
    void sendEvent(const KavachEthFrame& frame);

    // Send periodic heartbeat
    void sendHeartbeat();

    // Poll for incoming frames from simulator (non-blocking)
    bool receiveFrame(KavachEthFrame& frameOut, int timeoutMs = 10);

    // Called every cycle
    void tick();

private:
    int                m_txSock = -1;
    int                m_rxSock = -1;
    struct sockaddr_in m_simAddr{};
    uint32_t           m_heartbeatCount = 0;
    uint32_t           m_tickCount      = 0;

    void packFrame(const KavachEthFrame& frame, uint8_t buf[16]) const;
    bool unpackFrame(const uint8_t buf[16], int len,
                     KavachEthFrame& frame) const;
};
