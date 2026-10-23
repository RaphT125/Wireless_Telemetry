//
// Created by Raphael on 2026-10-19.
//

#include "CANDeserializer.h"
#include <array>
    std::vector<ProcessedCanData> CANDeserializer::deserializer(const PacketHandler::body* data) {
        std::vector<ProcessedCanData> processedCanData = {};
        uint64_t canID = checkEndian(data,4,0);
        std::vector<ProcessedCanData> temp = deserializeProcessedCanData(data,canID);
        processedCanData.insert(processedCanData.end(),temp.begin(),temp.end());
        return processedCanData;
    }

     std::uint64_t CANDeserializer::checkEndian(const PacketHandler::body* data, int byteLength, int offset) {
        std::uint64_t byteData = 0;
        if (data->littleEndian) {
            for (int i = offset+byteLength-1; i >= offset; i--) {
                byteData = (byteData<<8) | data->rawFrame[i];
            }
        }else {
            for (int i = offset; i < offset+byteLength; i++) {
                byteData = (byteData<<8) | data->rawFrame[i];
            }
        }
        return byteData;
    }
     std::vector<ProcessedCanData> CANDeserializer::deserializeProcessedCanData(const PacketHandler::body* data, uint64_t canID) {
        const auto& table = CANMap::table();
        auto it = table.find(canID);
        if (it == table.end()) {
            //include log for invalid CAN message
            return {};
        }
        const CANMap::CanMessageDef& def = it->second;
        if (data->rawFrame[4] != def.messageLength) {
            //throw log for invalid length of CAN message data
            return{};
        }
        std::vector<ProcessedCanData> processedCanData = {};
        for (const CANMap::signal& entry : def.signals) {
            ProcessedCanData CANMessage;
            std::uint64_t raw = checkEndian(data, entry.byteLength, 5 + entry.offset);
            std::int64_t value = static_cast<std::int64_t>(raw);
            int bits = entry.byteLength * 8;
            if (entry.isSigned && bits < 64 && (raw & (1ULL << (bits - 1)))) {
                value -= (1LL << bits);   // e.g. 0xFFF4 -> -12
            }
            double ProcessedValue = static_cast<double>(value) * entry.scale + entry.channelOffset;
            CANMessage.value = ProcessedValue;
            CANMessage.name = entry.name;
            CANMessage.unit = entry.unit;
            processedCanData.push_back(CANMessage);
        }
        return processedCanData;
    }
