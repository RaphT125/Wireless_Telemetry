//
// Created by Raphael on 2026-10-24.
//
#include "GrafanaLiveStream.h"
#include "Logger.h"
#include <map>
#include <tuple>
#include <unordered_map>
#include <utility>

    GrafanaLiveStream::GrafanaLiveStream() {
        m_session.SetUrl(cpr::Url{m_url});
        m_session.SetHeader(cpr::Header{{"Authorization", "Bearer " + m_token},
                                        {"Content-Type", "text/plain"}});
        m_session.SetTimeout(cpr::Timeout{500});
    }

    void GrafanaLiveStream::grafanaLiveConnection(const std::vector<PacketHandler::datapoint>& points) {
        if (points.empty()) return;

        static const std::unordered_map<std::string, std::string> groupOf = [] {
            std::unordered_map<std::string, std::string> r;
            for (const auto& [id, def] : CANMap::table()) {
                for (const auto& s : def.signals) {
                    r[s.name] = def.name;
                }
            }
            return r;
        }();

        std::map<std::tuple<std::string, std::string, std::uint64_t>, std::string> lines;
        for (const auto& p : points) {
            auto it = groupOf.find(p.procssedData.name);
            if (it == groupOf.end()) continue;
            const std::string& group = it->second;
            if (group == "GPS") {
                std::uint64_t& last = m_lastGpsMs[p.device];
                if (p.timeMs != last) {
                    if (p.timeMs < last + 150) continue;
                    last = p.timeMs;
                }
            }
            std::string& fields = lines[{group, p.device, p.timeMs}];
            if (!fields.empty()) fields += ",";
            fields += std::string{p.procssedData.name} + "=" + std::to_string(p.procssedData.value);
        }

        std::string body;
        for (const auto& [key, fields] : lines) {
            const auto& [group, device, timeMs] = key;
            body += group + ",device=" + device + " " + fields + " "
                  + std::to_string(timeMs * 1000000ULL) + "\n";
        }

        m_session.SetBody(cpr::Body{body});
        cpr::Response r = m_session.Post();
        if (r.error) {
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "Grafana Live push failed: " + r.error.message);
            printf("[live] push failed: %s\n", r.error.message.c_str());
        } else if (r.status_code != 200) {
            Logger::get().logEvent(Logger::SevLevel::WARN,
                "Grafana Live push rejected: HTTP " + std::to_string(r.status_code) + " " + r.text);
            printf("[live] HTTP %ld: %s\n", r.status_code, r.text.c_str());
        }
    }
