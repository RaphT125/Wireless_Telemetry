//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_TELEMETRYSERVICE_H
#define WIRELESS_TELEMETRY_TELEMETRYSERVICE_H

#include "Network/Header/Receiver.h"
#include "Network/Header/Tunneling.h"
#include "DataHandler/Header/PacketHandler.h"
#include "DataHandler/Header/CANDeserializer.h"
#include "PostProcessedData/Header/TelemtryRepsoitory.h"
#include "PostProcessedData/Header/GrafanaLiveStream.h"
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include "Utils/serv_config.h"
#include "Network/Header/WireguardTableReader.h"
#include <thread>
#include "Utils/serv_config.h"


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
