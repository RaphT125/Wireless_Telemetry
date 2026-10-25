//
// Created by Raphael on 2026-10-19.
//

#include "Receiver.h"

#include <utility>

Receiver::Receiver(std::uint16_t port, PacketHandler handler)
    : port_(port), handler_(std::move(handler)), socket_(io_) {}

Receiver::~Receiver() {
    stop();
}

void Receiver::start() {
    if (running_) return;

    if (thread_.joinable()) thread_.join();   // clean up after a previous stop()
    io_.restart();

    asio::ip::udp::endpoint local(asio::ip::udp::v4(), port_);
    socket_.open(local.protocol());
    socket_.bind(local);                      // throws if the port is taken
    socket_.set_option(asio::socket_base::receive_buffer_size(8 * 1024 * 1024));   // 8 MB: ride out stalls instead of dropping packets

    running_ = true;
    doReceive();
    thread_ = std::thread([this] { io_.run(); });
}

void Receiver::stop() {
    if (!running_.exchange(false)) return;

    // Close the socket from the io thread so the pending receive finishes
    // with operation_aborted and io_.run() returns.
    asio::post(io_, [this] {
        asio::error_code ec;
        socket_.close(ec);
    });

    // If stop() was called from inside the packet handler we are on the io
    // thread ourselves, so we must not join it.
    if (thread_.joinable() && thread_.get_id() != std::this_thread::get_id()) {
        thread_.join();
    }
}

void Receiver::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), sender_,
        [this](const asio::error_code& ec, std::size_t bytes) {
            if (ec == asio::error::operation_aborted) return;   // socket closed by stop()

            if (!ec && bytes > 0 && handler_) {
                try {
                    handler_(buffer_.data(), bytes, sender_);
                } catch (...) {
                    // A bad packet handler must never kill the listener.
                }
            }

            // Keep listening, including after transient errors.
            if (running_) doReceive();
        });
}
