//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_TELEMETRYSERVICE_H
#define WIRELESS_TELEMETRY_TELEMETRYSERVICE_H

#include "Receiver.h"   // must come before <windows.h> (Asio includes winsock itself)
#include "Tunneling.h"
#include "PacketHandler.h"
#include "CANDeserializer.h"
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

class TelemetryService {
public:
    ~TelemetryService();

    // Brings up the WireGuard tunnel (10.8.0.1), then starts the UDP receiver.
    // Needs Administrator. Throws if either step fails.
    void initialization(std::uint16_t port = 5005);

    // Stops the receiver, then removes the tunnel. Safe to call more than once.
    // A Windows service control handler (SERVICE_CONTROL_STOP / _SHUTDOWN) can call this too.
    void shutdown();

    // True from a successful initialization() until shutdown() has fully finished.
    bool running() const { return started_ && !stopped_; }

private:
    // Members are destroyed in reverse order, so `tunnel` (declared first) is torn down
    // last - after the receiver that uses it has stopped.
    Tunneling tunnel;
    PacketHandler packetHandler;          // must be declared before `receiver`
    CANDeserializer candeserializer;
    std::unique_ptr<Receiver> receiver;
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};

    // Windows calls this on Ctrl+C, closing the console window, logoff, or shutdown.
    // It must be static: Windows can't call a member function (it has no `this`).
    static BOOL WINAPI onclose(DWORD type);
    static TelemetryService* instance;   // lets the static callback reach the object
};


#endif //WIRELESS_TELEMETRY_TELEMETRYSERVICE_H
