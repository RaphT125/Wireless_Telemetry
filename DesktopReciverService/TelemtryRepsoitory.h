//
// Created by Raphael on 2026-10-23.
//

#ifndef WIRELESS_TELEMETRY_TELEMTRYREPSOITORY_H
#define WIRELESS_TELEMETRY_TELEMTRYREPSOITORY_H


#include <pqxx/pqxx>
#include <string>
#include <optional>
#include <stack>

#include "ProcessedCanData.h"
#include "PacketHandler.h"

class TelemtryRepsoitory {
public:
    explicit TelemtryRepsoitory();
    void initializeSchema();
    void saveDatapoint(const std::vector<PacketHandler::datapoint>& points);
    void updateLinkStatus(std::uint32_t ip, const PacketHandler::linkState& link);
private:
    std::optional<pqxx::connection> m_conn;
    std::stack<std::vector<PacketHandler::datapoint>> m_buffer;   // batches that failed to save
    void writeBatch(const std::vector<PacketHandler::datapoint>& points);
};


#endif //WIRELESS_TELEMETRY_TELEMTRYREPSOITORY_H
