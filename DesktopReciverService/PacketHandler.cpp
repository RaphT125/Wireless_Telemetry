//
// Created by Raphael on 2026-10-19.
//

#include "PacketHandler.h"
#include <cstdio>
#include <cstring>
#include "CANDeserializer.h"
#include "TelemtryRepsoitory.h"
#include "GrafanaLiveStream.h"
#include "Logger.h"


    void PacketHandler::handlePacket(const std::uint8_t* data, std::size_t size, uint32_t ip) {
        if (size < 1) {
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "dropped empty packet from " + asio::ip::address_v4(ip).to_string());
            return;
        }
        uint8_t header;
        memcpy(&header, data, sizeof(uint8_t));
        if (header == HANDSHAKE) {
            handshakeHandler(data, size, ip);
        } else {
            dataPacketHandler(data, size, ip);
        }
    }

    void PacketHandler::dataPacketHandler(const std::uint8_t* data, std::size_t size, uint32_t ip) {
        // Header is 19 bytes: header(1) bootId(4) baseTimeMs(8) sequence(4) frameCount(1) littleEndian(1)
        if (size < 19) {
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "dropped packet from " + asio::ip::address_v4(ip).to_string() +
                ": " + std::to_string(size) + " bytes, shorter than the 19-byte header");
            return;
        }
        uint8_t frameCount;
        memcpy(&frameCount, data + 17, sizeof(uint8_t));

        if (size != 19 + 15 * static_cast<size_t>(frameCount)) {
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "dropped corrupted packet from " + asio::ip::address_v4(ip).to_string() +
                ": " + std::to_string(size) + " bytes, expected " +
                std::to_string(19 + 15 * static_cast<size_t>(frameCount)) +
                " for " + std::to_string(frameCount) + " frames");
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
        deserializePacket(canMessages, ip, baseTimeMs);
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
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "dropped handshake from " + asio::ip::address_v4(ip).to_string() +
                ": " + std::to_string(size) + " bytes, expected 18");
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

    void PacketHandler::deserializePacket(const std::vector<body>& data, uint32_t ip, std::uint64_t baseTimeMs) {
            std::vector<datapoint> datapoints;
            const std::string device = asio::ip::address_v4(ip).to_string();   // e.g. "10.8.0.3"
            linkState& link = m_lastSeen[ip];
            for (auto& canMessage : data) {
               std::vector<ProcessedCanData> temp = CANDeserializer::deserializer(&canMessage);
                for (auto t : temp) {
                    datapoint point;
                    point.device = device;
                    point.timeMs = baseTimeMs + canMessage.timeOffsetMs;   // when this frame was sampled
                    point.boot_id = link.lastBootId;
                    point.procssedData = t;
                    datapoints.push_back(point);
                }
            }
            if (datapoints.empty()) return;
            if (m_liveStream != nullptr) {   // live first: the dashboard wants it fastest
                m_liveStream->grafanaLiveConnection(datapoints);
            }
            if (m_repository == nullptr) return;
            try {
                m_repository->saveDatapoint(datapoints);
            } catch (const std::exception& e) {
                // A database problem shouldn't stop the receiver.
                std::printf("Could not save %zu datapoints: %s\n", datapoints.size(), e.what());
            }
    }
