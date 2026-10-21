//
// Created by Raphael on 2026-10-19.
//

#include "PacketHandler.h"

#include <cstdio>
class
void PacketHandler::handlePacket(const std::uint8_t* data, std::size_t size,
                                 const asio::ip::udp::endpoint& from) {
    std::printf("[%s:%u] %zu bytes:", from.address().to_string().c_str(),
                static_cast<unsigned>(from.port()), size);
    for (std::size_t i = 0; i < size && i < 32; ++i) std::printf(" %02X", data[i]);
    if (size > 32) std::printf(" ...");
    std::printf("\n");
    std::fflush(stdout);
}
