//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_PACKETHANDLER_H
#define WIRELESS_TELEMETRY_PACKETHANDLER_H

#include <asio/ip/udp.hpp>
#include <cstddef>
#include <cstdint>

// Decides what to do with each UDP packet the Receiver gets.
// Right now it prints the bytes; decoding the ESP32 data will go here later.
class PacketHandler {
public:
    // Called on the receiver's background thread. `data` is only valid during the call.
    void handlePacket(const std::uint8_t* data, std::size_t size,
                      const asio::ip::udp::endpoint& from);
};


#endif //WIRELESS_TELEMETRY_PACKETHANDLER_H
