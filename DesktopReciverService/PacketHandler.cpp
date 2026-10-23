//
// Created by Raphael on 2026-10-19.
//

#include "PacketHandler.h"
#include <cstdio>
#include <cstring>
#include "CANDeserializer.h"

    void PacketHandler::handlePacket(const std::uint8_t* data, std::size_t size, uint32_t ip = 0) {
        uint8_t header;
        memcpy(&header, data, sizeof(uint8_t));
        if (header == HANDSHAKE) {
            handshakeHandler(data, size, ip);
        } else {
            dataPacketHandler(data, size, ip);
        }
    }

    void PacketHandler::dataPacketHandler(const std::uint8_t* data, std::size_t size, uint64_t ip) {
        // Header is 19 bytes: header(1) bootId(4) baseTimeMs(8) sequence(4) frameCount(1) littleEndian(1)
        if (size < 19) {
            //throw log: packet too short to be valid
            return;
        }
        uint8_t frameCount;
        memcpy(&frameCount, data + 17, sizeof(uint8_t));

        if (size != 19 + 15 * static_cast<size_t>(frameCount)) {
            //throw log: corrupted packet (size doesn't match frameCount)
            return;
        }

        uint32_t bootId;
        uint64_t baseTimeMs;
        uint32_t sequence;
        memcpy(&bootId,     data + 1,  sizeof(uint32_t));
        memcpy(&baseTimeMs, data + 5,  sizeof(uint64_t));
        memcpy(&sequence,   data + 13, sizeof(uint32_t));
        bool littleEndian = data[18] != 0;
        lastSeenSequence(ip, bootId, baseTimeMs, sequence);

        std::vector<body> canMessages;
        for (int i = 0; i < frameCount; i++) {
            size_t base = 19 + 15 * i;
            body canMessage;
            memcpy(&canMessage.timeOffsetMs,   data + base + 0, sizeof(uint16_t));
            memcpy(canMessage.rawFrame.data(), data + base + 2, 13);
            canMessage.littleEndian = littleEndian;
            canMessages.push_back(canMessage);
        }
        deserializePacket(canMessages, ip);
    }


    void PacketHandler::lastSeenSequence(std::uint32_t ip, std::uint32_t bootId, std::uint64_t baseTimeMs, std::uint32_t sequence) {
        linkState& link = m_lastSeen[ip];
        link.received++;

        if (!link.haveLast || bootId != link.lastBootId) {
            link.haveLast     = true;
            link.lastBootId   = bootId;
            link.lastSequence = sequence;
            return;
        }

        if (sequence > link.lastSequence) {
            link.lost += sequence - link.lastSequence - 1;   // numbers skipped = lost
            link.lastSequence = sequence;
        }
    }

    void PacketHandler::handshakeHandler(const std::uint8_t* data, std::size_t size, uint32_t ip) {
        if (size != 18) {
            //throw log: handshake must be exactly the 18-byte header
            return;
        }
        uint32_t bootId;
        uint64_t baseTimeMs;
        uint32_t sequence;
        memcpy(&bootId,     data + 1,  sizeof(uint32_t));
        memcpy(&baseTimeMs, data + 5,  sizeof(uint64_t));
        memcpy(&sequence,   data + 13, sizeof(uint32_t));

        // New connection (or the ESP32 rebooted): start this device's state fresh.
        linkState& link = m_lastSeen[ip];
        link = linkState{};
        link.haveLast     = true;
        link.lastBootId   = bootId;
        link.lastSequence = sequence;
        link.baseTimeMs   = baseTimeMs;
    }

    void PacketHandler::deserializePacket(const std::vector<body> data, uint32_t ip) {
            std::vector<ProcessedCanData> canMessages = {};
            for (auto& canMessage : data) {
               std::vector<ProcessedCanData> temp = CANDeserializer::deserializer(&canMessage);
               canMessages.insert(canMessages.end(),temp.begin(),temp.end());
            }
    }
