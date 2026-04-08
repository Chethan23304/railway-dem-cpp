#include "KavachSensor.hpp"
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Exactly mirrors the Python script that works
static uint16_t rawReadReg(const char* ip, int port, uint16_t address) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct timeval tv{3, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(sock);
        return 0;
    }

    // Exact same bytes Python sends: struct.pack(">HHHBBHH", 1, 0, 6, 1, 3, addr, 1)
    uint8_t req[12] = {
        0x00, 0x01,                          // Transaction ID
        0x00, 0x00,                          // Protocol ID
        0x00, 0x06,                          // Length
        0x01,                                // Unit ID
        0x03,                                // FC03 read holding registers
        (uint8_t)(address >> 8),             // Address high
        (uint8_t)(address & 0xFF),           // Address low
        0x00, 0x01                           // Count = 1
    };

    if (send(sock, req, sizeof(req), 0) != sizeof(req)) {
        ::close(sock);
        return 0;
    }

    uint8_t resp[256]{};
    int n = recv(sock, resp, sizeof(resp), 0);
    ::close(sock);

    if (n < 11 || (resp[7] & 0x80)) return 0;  // error response
    return (uint16_t)((resp[9] << 8) | resp[10]);
}

KavachSensorData KavachSensor::readAll() {
    KavachSensorData d{};

    d.speedPermitted = rawReadReg("192.168.0.110", 1502, 266);
    d.speedActual    = rawReadReg("192.168.0.110", 1502, 273);
    d.speedWarning   = rawReadReg("192.168.0.110", 1502, 275);
    d.signalAspect   = rawReadReg("192.168.0.110", 1502, 289);
    d.rfSignalBars   = rawReadReg("192.168.0.110", 1502, 294);
    d.rfidStatus     = rawReadReg("192.168.0.110", 1502, 425);
    d.sosState       = rawReadReg("192.168.0.110", 1502, 457);
    d.emergencyBrake = rawReadReg("192.168.0.110", 1502, 469);
    d.brakePipe      = rawReadReg("192.168.0.110", 1502, 478);
    d.kavachMode     = 0;
    d.valid          = true;

    printf("[KavachSensor] Speed=%d/%d km/h | Warn=%d | Signal=%d | "
           "Radio=%d bars | Brake=%d | SOS=%d | RFID=%d\n",
           d.speedActual, d.speedPermitted, d.speedWarning,
           d.signalAspect, d.rfSignalBars, d.brakePipe,
           d.sosState, d.rfidStatus);
    return d;
}
