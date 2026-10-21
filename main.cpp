// Entry point: starts the telemetry service and waits until it is stopped
// (Ctrl+C, closing the console window, logoff or shutdown).
//
// Usage: telemetry_service.exe [port]     (default port 5005)

#include "TelemetryService.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    std::uint16_t port = 5005;
    if (argc > 1) port = static_cast<std::uint16_t>(std::atoi(argv[1]));

    TelemetryService service;

    try {
        service.initialization(port);
    } catch (const std::exception& e) {
        // Covers both the tunnel and the receiver. Tunnel errors are usually
        // "not running as Administrator" or "wireguard.dll not next to the .exe".
        std::cerr << "Startup failed: " << e.what() << "\n";
        return 1;
    }

    std::cout << "Listening on UDP port " << port << " (Ctrl+C to quit)\n";

    // The close handler registered by the service calls shutdown(),
    // which makes running() false and lets us leave this loop.
    while (service.running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Stopped.\n";
    return 0;
}
