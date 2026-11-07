//
// Created by Raphael on 2026-10-19.
//

#include "TelemetryService.h"
#include <cstdio>
#include <iostream>
#include "Logger.h"
namespace {
const wchar_t* kKeyFile = L"C:\\ProgramData\\Wireless_Telemtry\\desktop.key";
} // namespace

TelemetryService* TelemetryService::instance = nullptr;

TelemetryService::~TelemetryService() {
    SetConsoleCtrlHandler(onclose, FALSE);
    instance = nullptr;
    shutdown();
}

void TelemetryService::initialization(std::uint16_t port) {
    instance = this;
    SetConsoleCtrlHandler(onclose, TRUE);

    tunnel.start(kKeyFile, wireguardTableReader.loadPeers());
    peerRefreshThread = std::thread(&WireguardTableReader::refreshLoop, &wireguardTableReader, std::ref(tunnel));
    std::printf("WireGuard tunnel up: %s/%u, UDP port %u\n", Tunneling::kAddress,
                static_cast<unsigned>(Tunneling::kPrefixLength), static_cast<unsigned>(Tunneling::kPort));
    std::printf("Desktop public key (give this to the laptop and ESP32): %s\n",
                tunnel.publicKeyBase64().c_str());

    telemtryRepsoitory = std::make_unique<TelemtryRepsoitory>();
    telemtryRepsoitory->initializeSchema();
    packetHandler.setRepository(telemtryRepsoitory.get());

    grafanaLiveStream = std::make_unique<GrafanaLiveStream>();
    packetHandler.setLiveStream(grafanaLiveStream.get());

    receiver = std::make_unique<Receiver>(port, [this](const std::uint8_t* data, std::size_t size,
                                                       const asio::ip::udp::endpoint& from) {
        std::uint32_t ip = from.address().is_v4() ? from.address().to_v4().to_uint() : 0;
        packetHandler.handlePacket(data, size, ip);
    });
    receiver->start();
    started_ = true;
    Logger::get().logEvent(Logger::SevLevel::INFO,
        "server started: tunnel " + std::string(Tunneling::kAddress) +
        ", listening on UDP " + std::to_string(port));
}

void TelemetryService::shutdown() {
    wireguardTableReader.stop();
    if (peerRefreshThread.joinable()) peerRefreshThread.join();
    if (receiver) receiver->stop();
    tunnel.stop();
    if (started_ && !stopped_) {
        Logger::get().logEvent(Logger::SevLevel::INFO, "server stopped");
    }
    stopped_ = true;
}

BOOL WINAPI TelemetryService::onclose(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (instance) instance->shutdown();
            return TRUE;
    }
    return FALSE;
}
