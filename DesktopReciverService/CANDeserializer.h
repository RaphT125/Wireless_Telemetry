//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_CANDESERIALIZER_H
#define WIRELESS_TELEMETRY_CANDESERIALIZER_H
#include <cstddef>
#include <cstdint>
#include <vector>
#include "CANMap.h"
#include "PacketHandler.h"
#include "ProcessedCanData.h"

class CANDeserializer {
public:
    using ProcessedCanData = ::ProcessedCanData;
    static std::vector<ProcessedCanData> deserializer(const PacketHandler::body* data);
private:
    static std::uint64_t checkEndian(const PacketHandler::body* data, int byteLength, int offset);
    static std::vector<ProcessedCanData> deserializeProcessedCanData(const PacketHandler::body* data, uint64_t canID);

};


#endif //WIRELESS_TELEMETRY_CANDESERIALIZER_H
