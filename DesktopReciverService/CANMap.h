//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_CANMAP_H
#define WIRELESS_TELEMETRY_CANMAP_H
#include <cstdint>
#include <vector>
#include <unordered_map>

class CANMap {
    public:
        struct Signal {
            const char* name;
            int offset;
            int byteLength;
            bool littleEndian;
            bool isSigned;        // true = can be negative
            const char* unit;
            double scale;
            double channelOffset;
        };
        struct CanMessageDef {
            std::uint8_t messageLength;
            std::vector<Signal> signals;
        };
        static const std::unordered_map<std::uint32_t, CanMessageDef> table() {
            static const std::unordered_map<std::uint32_t, CanMessageDef> t = {
                {0x1, {8, {
                    {"RPM",0, 2, false, false, "rpm", 1, 0},
                    {"ACC_PEDAL",2, 2, false, false, "%", 0.1, 0},
                    {"COOLANT",4, 2, false, false, "C", 0.1, 0},
                    {"OIL_P",6, 2, false, false, "psi", 1, 0}
                }}},
                {0x2, {8, {
                    {"GPS_SPEED",0, 2, false, false, "kph", 1, 0},
                    {"INLINE_ACC",2, 2, false, true, "G", 0.1, 0},
                    {"LATERAL_ACC",4, 2, false, true, "G", 0.1, 0},
                    {"VERTICAL_ACC",6, 2, false, true, "G", 0.1, 0}
                }}},
                {0x3, {8, {
                    {"GPS_LONG",0, 4, false, true, "deg", 1e-7, 0},
                    {"GPS_LAT",4, 4, false, true, "deg", 1e-7, 0}
                }}},
                {0x4, {8, {
                    {"OIL_TEMP",0, 2, false, false, "C", 0.1, 0},
                    {"MANIFPRESS",2, 2, false, false, "psi", 1, 0},
                    {"BATT",4, 2, false, false, "V", 0.1, 0},
                    {"O2",6, 2, false, false, "labda", 0.01, 0}
                }}},
                {0x5, {8, {
                    {"RL_WS",0, 2, false, false, "KPH", 0.1, 0},
                    {"RR_WS",2, 2, false, false, "KPH", 0.1, 0},
                    {"R_BP",4, 2, false, false, "PSI", 1, 0},
                    {"F_BP",6, 2, false, false, "PSI", 1, 0}
                }}},
                {0x6, {8, {
                    {"FL_SP",0, 2, false, false, "mm", 0.1, 0},
                    {"FR_SP",2, 2, false, false, "mm", 0.1, 0},
                    {"RL_SP",4, 2, false, false, "mm", 0.1, 0},
                    {"RR_SP",6, 2, false, false, "mm", 0.1, 0}
                }}},
                {0x7, {8, {
                    {"STEERING",0, 2, false, true, "deg", 0.1, 0},
                    {"GEAR",2, 2, false, false, "", 1, 0},
                    {"THROTTLE",4, 2, false, false, "mm", 0.1, 0},
                    {"FLATSHIFT",6, 2, false, false, "", 1, 0}
                }}}
            };
            return t;
        }
};


#endif //WIRELESS_TELEMETRY_CANMAP_H
