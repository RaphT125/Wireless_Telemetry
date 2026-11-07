//
// Created by Raphael on 2026-10-24.
//

#ifndef WIRELESS_TELEMETRY_GRAFANALIVESTREAM_H
#define WIRELESS_TELEMETRY_GRAFANALIVESTREAM_H

#include <cpr/cpr.h>
#include <TelemtryRepsoitory.h>
#include "PacketHandler.h"
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <fstream>
#include "CANMap.h"
class GrafanaLiveStream {

    public:
        GrafanaLiveStream();
        void grafanaLiveConnection(const std::vector<PacketHandler::datapoint>& points);
    private:
    std::string m_token = [] {
        std::string token;
        if (const char* t = std::getenv("GRAFANA_TOKEN")) token = t;
        if (token.empty()) {
            std::ifstream file("C:\\ProgramData\\WirelessTelemetry\\grafana.key");
            std::getline(file, token);
        }
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) token.pop_back();
        if (token.empty())
            std::printf("[live] no Grafana token: set GRAFANA_TOKEN or create C:\\ProgramData\\WirelessTelemetry\\grafana.key\n");
        return token;
    }();
        std::string m_url = "http://127.0.0.1:3000/api/live/push/telemetry";
        cpr::Session m_session;
        std::map<std::string, std::uint64_t> m_lastGpsMs;
};


#endif //WIRELESS_TELEMETRY_GRAFANALIVESTREAM_H
