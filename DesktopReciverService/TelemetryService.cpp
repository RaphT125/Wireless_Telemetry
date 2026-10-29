//
// Created by Raphael on 2026-10-19.
//

#include "TelemetryService.h"
#include <cstdio>
#include <iostream>

namespace {

// The desktop's WireGuard key lives here (created automatically on the first run).
const wchar_t* kKeyFile = L"C:\\ProgramData\\WirelessTelemetry\\desktop.key";

// Devices allowed into the tunnel. An empty list is fine for a first test: the adapter
// still comes up with 10.8.0.1, there is just nobody who can connect yet.
// Add each device's public key (shown in its WireGuard app) and its tunnel IP, e.g.:
//     { "laptop", Tunneling::keyFromBase64("<laptop public key>"), "10.8.0.2" },
//     { "esp32",  Tunneling::keyFromBase64("<esp32 public key>"),  "10.8.0.3" },
std::vector<Tunneling::Peer> tunnelPeers() {

    return {
        {"laptop", Tunneling::keyFromBase64("wcEUhJzv/PI1ffanpHcVUIRcZwM7sO5NlboOeMB/ECA="), "10.8.0.2" },
        {"laptop", Tunneling::keyFromBase64("3qa2491ATKHrdET0k9BNyQ5tfNIf1L+2rO1J+4w5RBc="), "10.8.0.3" },
        {"phone", Tunneling::keyFromBase64("GC8ucrMpooc8/TazDN/X7wTOiUI9xk9lMlMI291YnDQ="), "10.8.0.4" },
        {"nick", Tunneling::keyFromBase64("5G9v6UsUOhsFTw2mV2nTSFG0144OsMfEY9Z/d+J1R1o="), "10.8.0.5" }
    };
}


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

    // 1. Tunnel first, so 10.8.0.1 exists before anything listens on it.
    tunnel.start(kKeyFile, tunnelPeers());
    std::printf("WireGuard tunnel up: %s/%u, UDP port %u\n", Tunneling::kAddress,
                static_cast<unsigned>(Tunneling::kPrefixLength), static_cast<unsigned>(Tunneling::kPort));
    std::printf("Desktop public key (give this to the laptop and ESP32): %s\n",
                tunnel.publicKeyBase64().c_str());

    //2. connect to locally hosted postgress database
    telemtryRepsoitory = std::make_unique<TelemtryRepsoitory>();
    telemtryRepsoitory->initializeSchema();
    packetHandler.setRepository(telemtryRepsoitory.get());   // PacketHandler uses it, TelemetryService owns it

    //3. live stream to Grafana
    grafanaLiveStream = std::make_unique<GrafanaLiveStream>();
    packetHandler.setLiveStream(grafanaLiveStream.get());

    receiver = std::make_unique<Receiver>(port, [this](const std::uint8_t* data, std::size_t size,
                                                       const asio::ip::udp::endpoint& from) {
        // Sender's tunnel IP as a number (e.g. 10.8.0.3), used to keep per-device state.
        std::uint32_t ip = from.address().is_v4() ? from.address().to_v4().to_uint() : 0;
        packetHandler.handlePacket(data, size, ip);
    });
    receiver->start();
    started_ = true;
}

void TelemetryService::shutdown() {
    if (receiver) receiver->stop();
    tunnel.stop();
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
