//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_TELEMETRYSERVICE_H
#define WIRELESS_TELEMETRY_TELEMETRYSERVICE_H

#include "Receiver.h"
#include "Tunneling.h"
#include "PacketHandler.h"
#include "CANDeserializer.h"
#include "TelemtryRepsoitory.h"
#include "GrafanaLiveStream.h"
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "serv_config.h"
#include "WireguardTableReader.h"
#include <thread>
#include "serv_config.h"


class TelemetryService {
public:
    ~TelemetryService();

    void initialization(std::uint16_t port = WIREGUARDPORT);
    void shutdown();

    bool running() const { return started_ && !stopped_; }

private:

    Tunneling tunnel;
    PacketHandler packetHandler;
    std::unique_ptr<TelemtryRepsoitory>telemtryRepsoitory;
    std::unique_ptr<GrafanaLiveStream> grafanaLiveStream;
    CANDeserializer candeserializer;
    std::unique_ptr<Receiver> receiver;
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};


    static BOOL WINAPI onclose(DWORD type);
    static TelemetryService* instance;

    WireguardTableReader wireguardTableReader;
    std::thread peerRefreshThread;
};


#endif //WIRELESS_TELEMETRY_TELEMETRYSERVICE_H
