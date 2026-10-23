//
// Created by Raphael on 2026-10-23.
//

#ifndef WIRELESS_TELEMETRY_PROCESSEDCANDATA_H
#define WIRELESS_TELEMETRY_PROCESSEDCANDATA_H

// One decoded signal, e.g. RPM = 6500 rpm.
// Lives in its own header so PacketHandler.h and CANDeserializer.h don't have to include each other.
struct ProcessedCanData {
    const char* name;
    const char* unit;
    double value;
};

#endif //WIRELESS_TELEMETRY_PROCESSEDCANDATA_H
