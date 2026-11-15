//
// Created by Raphael on 2026-10-19.
//

#ifndef WIRELESS_TELEMETRY_CANMAP_H
#define WIRELESS_TELEMETRY_CANMAP_H
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <string>

class CANMap {
    public:
        struct signal {
            const char* name;
            int offset;
            int byteLength;
            bool isSigned;
            const char* unit;
            double scale;
            double channelOffset;
        };
        struct CanMessageDef {
            std::string name;
            std::uint8_t messageLength;
            bool littleEndian;
            std::vector<signal> signals;
        };
        static const std::unordered_map<std::uint32_t, CanMessageDef> table() {
            static const std::unordered_map<std::uint32_t, CanMessageDef> t = {
                {0x1, {"ENG1", 8, false, {
                    {"RPM",0, 2, true, "rpm", 1, 0},
                    {"ACC_PEDAL",2, 2, true, "%", 0.1, 0},
                    {"COOLANT",4, 2, true, "C", 0.1, 0},
                    {"OIL_P",6, 2, true, "psi", 1, 0}
                }}},
                {0x2, {"ACCEL", 8, false, {
                    {"GPS_SPEED",0, 2, true, "kph", 1, 0},
                    {"INLINE_ACC",2, 2, true, "G", 0.1, 0},
                    {"LATERAL_ACC",4, 2, true, "G", 0.1, 0},
                    {"VERTICAL_ACC",6, 2, true, "G", 0.1, 0}
                }}},
                {0x3, {"GPS", 8, false, {
                    {"GPS_LONG",0, 4, true, "deg", 1e-7, 0},
                    {"GPS_LAT",4, 4, true, "deg", 1e-7, 0}
                }}},
                {0x4, {"ENG2", 8, false, {
                    {"OIL_TEMP",0, 2, true, "C", 0.1, 0},
                    {"MANIFPRESS",2, 2, true, "psi", 1, 0},
                    {"BATT",4, 2, true, "V", 0.1, 0},
                    {"O2",6, 2, true, "labda", 0.01, 0}
                }}},
                {0x5, {"SUS1", 8, true, {
                    {"RL_WS",0, 2, true, "KPH", 0.1, 0},
                    {"RR_WS",2, 2, true, "KPH", 0.1, 0},
                    {"R_BP",4, 2, true, "PSI", 1, 0},
                    {"F_BP",6, 2, true, "PSI", 1, 0}
                }}},
                {0x6, {"SUS2", 8, true, {
                    {"FL_SP",0, 2, true, "mm", 0.1, 0},
                    {"FR_SP",2, 2, true, "mm", 0.1, 0},
                    {"RL_SP",4, 2, true, "mm", 0.1, 0},
                    {"RR_SP",6, 2, true, "mm", 0.1, 0}
                }}},
                {0x7, {"BODY", 8, true, {
                    {"STEERING",0, 2, true, "deg", 0.1, 0},
                    {"GEAR",2, 2, true, "", 1, 0},
                    {"THROTTLE",4, 2, true, "mm", 0.1, 0},
                    {"FLATSHIFT",6, 2, true, "", 1, 0}
                }}}
            };
            return t;
        }
};


#endif //WIRELESS_TELEMETRY_CANMAP_H
