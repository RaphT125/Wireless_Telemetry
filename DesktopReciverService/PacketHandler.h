//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_PACKETHANDLER_H
#define WIRELESS_TELEMETRY_PACKETHANDLER_H

#include <asio/ip/udp.hpp>
#include <cstddef>
#include <cstdint>
#include <array>
#include <unordered_map>
#include <vector>
#include "ProcessedCanData.h"


class PacketHandler {
public:
    const uint8_t HANDSHAKE = 0x1;
    const uint8_t CANHEADER = 0x2;

    struct body {
        std::uint16_t timeOffsetMs;
        bool          littleEndian;
        std::array<std::uint8_t, 13> rawFrame;
    };

    struct packet {
        std::uint8_t header;
        std::uint32_t bootId;
        std::uint64_t baseTimeMs;
        std::uint32_t sequence;
        std::uint8_t  frameCount;
        std::vector<body> body;
    };

    struct datapoint {
        std::uint16_t timeMs;
        ProcessedCanData procssedData;
    };

    struct linkState {
        bool          haveLast     = false;
        std::uint32_t lastBootId   = 0;
        std::uint32_t lastSequence = 0;
        std::uint64_t received     = 0;
        std::uint64_t lost         = 0;
        std::uint64_t baseTimeMs   = 0;  
    };

    void handlePacket(const std::uint8_t* data, std::size_t size, uint32_t ip);

private:
    void handshakeHandler(const std::uint8_t* data, std::size_t size, uint32_t ip);
    void dataPacketHandler(const std::uint8_t* data, std::size_t size, uint64_t ip);
    void lastSeenSequence(std::uint32_t ip, std::uint32_t bootId, std::uint64_t baseTimeMs, std::uint32_t sequence);
    std::unordered_map<std::uint32_t, linkState> m_lastSeen;
    void deserializePacket(const std::vector<body> data, uint32_t ip);
};



#endif //WIRELESS_TELEMETRY_PACKETHANDLER_H
