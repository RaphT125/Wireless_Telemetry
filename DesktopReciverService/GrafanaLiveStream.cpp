//
// Created by Raphael on 2026-10-24.
//
#include "GrafanaLiveStream.h"
#include <map>
#include <utility>

    // Set up the connection once: URL, headers and timeout never change between pushes.
    GrafanaLiveStream::GrafanaLiveStream() {
        m_session.SetUrl(cpr::Url{m_url});
        m_session.SetHeader(cpr::Header{{"Authorization", "Bearer " + m_token},
                                        {"Content-Type", "text/plain"}});
        m_session.SetTimeout(cpr::Timeout{500});
    }

    // One POST per packet, and one line per (device, timestamp): all signals sampled at the
    // same moment share a line, so Grafana gets one full row instead of one row per signal.
    void GrafanaLiveStream::grafanaLiveConnection(const std::vector<PacketHandler::datapoint>& points) {
        if (points.empty()) return;

        // (device, timeMs) -> "RPM=6500,ACC_PEDAL=50,..."
        std::map<std::pair<std::string, std::uint64_t>, std::string> lines;
        for (const auto& p : points) {
            std::string& fields = lines[{p.device, p.timeMs}];
            if (!fields.empty()) fields += ",";
            fields += std::string{p.procssedData.name} + "=" + std::to_string(p.procssedData.value);
        }

        std::string body;
        for (const auto& [key, fields] : lines) {
            const auto& [device, timeMs] = key;
            body += "car,device=" + device + " " + fields + " "
                  + std::to_string(timeMs * 1000000ULL) + "\n";
        }

        m_session.SetBody(cpr::Body{body});
        cpr::Response r = m_session.Post();       // reuses the same TCP connection every time
        if (r.error) {
            printf("[live] push failed: %s\n", r.error.message.c_str());
        } else if (r.status_code != 200) {
            printf("[live] HTTP %ld: %s\n", r.status_code, r.text.c_str());
        }
    }
