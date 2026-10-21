//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_RECEIVER_H
#define WIRELESS_TELEMETRY_RECEIVER_H

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

// Listens for UDP packets on a port and hands each one to a callback.
// Runs its own background thread, so start() returns immediately.
class Receiver {
public:

    // Called once per packet, on the receiver's background thread.
    // `data` is only valid for the duration of the call - copy it if you need it later.
    using PacketHandler = std::function<void(const std::uint8_t* data,
                                             std::size_t size,
                                             const asio::ip::udp::endpoint& from)>;

    Receiver(std::uint16_t port, PacketHandler handler);
    ~Receiver();

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    void start();

    void stop();

    bool running() const { return running_; }
    std::uint16_t port() const { return port_; }

private:
    void doReceive();

    std::uint16_t port_;
    PacketHandler handler_;

    asio::io_context io_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_;
    std::array<std::uint8_t, 2048> buffer_{};

    std::thread thread_;
    std::atomic<bool> running_{false};
};


#endif //WIRELESS_TELEMETRY_RECEIVER_H
