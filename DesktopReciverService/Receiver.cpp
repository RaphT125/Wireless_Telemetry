//
// Created by Raphael on 2026-10-19.
//

#include "Receiver.h"
#include "Logger.h"

#include <utility>

Receiver::Receiver(std::uint16_t port, PacketHandler handler)
    : port_(port), handler_(std::move(handler)), socket_(io_) {}

Receiver::~Receiver() {
    stop();
}

void Receiver::start() {
    if (running_) return;

    if (thread_.joinable()) thread_.join();
    io_.restart();

    asio::ip::udp::endpoint local(asio::ip::udp::v4(), port_);
    socket_.open(local.protocol());
    socket_.bind(local);
    socket_.set_option(asio::socket_base::receive_buffer_size(8 * 1024 * 1024));

    running_ = true;
    doReceive();
    thread_ = std::thread([this] { io_.run(); });
}

void Receiver::stop() {
    if (!running_.exchange(false)) return;

    /
    asio::post(io_, [this] {
        asio::error_code ec;
        socket_.close(ec);
    });


    if (thread_.joinable() && thread_.get_id() != std::this_thread::get_id()) {
        thread_.join();
    }
}

void Receiver::doReceive() {
    socket_.async_receive_from(
        asio::buffer(buffer_), sender_,
        [this](const asio::error_code& ec, std::size_t bytes) {
            if (ec == asio::error::operation_aborted) return;   // socket closed by stop()

            if (ec) {
                Logger::get().logEvent(Logger::SevLevel::ERR,
                    "UDP receive error on port " + std::to_string(port_) + ": " + ec.message());
            }

            if (!ec && bytes > 0 && handler_) {
                try {
                    handler_(buffer_.data(), bytes, sender_);
                } catch (...) {

                }
            }

            if (running_) doReceive();
        });
}
