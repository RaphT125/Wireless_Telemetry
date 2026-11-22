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
            int bitStart = 0;
            int bitLength = 0;
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
                    {"ACC_PEDAL",2, 2, true, "%", 0.01, 0},
                    {"COOLANT",4, 2, true, "C", 0.1, 0},
                    {"OIL_P",6, 2, true, "psi", 0.1, 0}
                }}},
                {0x2, {"ACCEL", 8, false, {
                    {"GPS_SPEED",0, 2, true, "kph", 0.1, 0},
                    {"INLINE_ACC",2, 2, true, "G", 0.01, 0},
                    {"LATERAL_ACC",4, 2, true, "G", 0.01, 0},
                    {"VERTICAL_ACC",6, 2, true, "G", 0.01, 0}
                }}},
                {0x3, {"GPS", 8, false, {
                    {"GPS_LONG",0, 4, true, "deg", 1e-7, 0},
                    {"GPS_LAT",4, 4, true, "deg", 1e-7, 0}
                }}},
                {0x4, {"ENG2", 8, false, {
                    {"OIL_TEMP",0, 2, true, "C", 0.1, 0},
                    {"MANIFPRESS",2, 2, true, "psi", 0.1, 0},
                    {"BATT",4, 2, true, "V", 0.1, 0},
                    {"O2",6, 2, true, "labda", 0.01, 0}
                }}},
                {0x5, {"SUS1", 8, true, {
                    {"RL_WS",0, 2, true, "KPH", 0.1, 0},
                    {"RR_WS",2, 2, true, "KPH", 0.1, 0},
                    {"R_BP",4, 2, true, "PSI", 0.1, 0},
                    {"F_BP",6, 2, true, "PSI", 1, 0}
                }}},
                {0x6, {"SUS2", 8, true, {
                    {"FL_SP",0, 2, true, "mm", 0.1, 0},
                    {"FR_SP",2, 2, true, "mm", 0.1, 0},
                    {"RL_SP",4, 2, true, "mm", 0.1, 0},
                    {"RR_SP",6, 2, true, "mm", 0.1, 0}
                }}},
                {0x7, {"BODY", 8, true, {
                    {"STEERING",0, 2, true, "deg", 1, 0},
                    {"GEAR",2, 2, true, "", 1, 0},
                    {"THROTTLE",4, 2, true, "mm", 0.01, 0},
                    {"FLATSHIFT",6, 2, true, "", 1, 0}
                }}},
                {0x360, {"ECU_ENGINE", 8, false, {
                    {"ECU_RPM", 0, 2, false, "rpm", 1, 0},
                    {"ECU_MAP", 2, 2, false, "kPa", 0.1, 0},
                    {"ECU_TPS", 4, 2, false, "%", 0.1, 0}
                }}},
                {0x361, {"ECU_PRESS", 8, false, {
                    {"ECU_FUEL_P", 0, 2, false, "kPa", 0.1, -101.3},
                    {"ECU_OIL_P", 2, 2, false, "kPa", 0.1, -101.3},
                    {"ECU_DEMAND", 4, 2, false, "%", 0.1, 0}
                }}},
                {0x368, {"ECU_LAMBDA", 8, false, {
                    {"ECU_LAMBDA1", 0, 2, false, "lambda", 0.001, 0}
                }}},
                {0x36B, {"ECU_BRAKE_FRONT", 8, false, {
                    {"ECU_BRAKE_P_FRONT", 0, 2, false, "kPa", 1, -101.3}
                }}},
                {0x370, {"ECU_SPEED", 8, false, {
                    {"ECU_VSPEED", 0, 2, false, "km/h", 0.1, 0}
                }}},
                {0x372, {"ECU_POWER", 8, false, {
                    {"ECU_BATT", 0, 2, false, "V", 0.1, 0},
                    {"ECU_BARO", 6, 2, false, "kPa", 0.1, 0}
                }}},
                {0x3E0, {"ECU_TEMPS", 8, false, {
                    {"ECU_COOLANT", 0, 2, false, "C", 0.1, -273.15},
                    {"ECU_AIR_T", 2, 2, false, "C", 0.1, -273.15},
                    {"ECU_FUEL_T", 4, 2, false, "C", 0.1, -273.15},
                    {"ECU_OIL_T", 6, 2, false, "C", 0.1, -273.15}
                }}},
                {0x3E4, {"ECU_STATUS", 8, false, {
                    {"ECU_NEUTRAL_SW", 1, 1, false, "", 1, 0, 7, 1},
                    {"ECU_REVERSE_SW", 1, 1, false, "", 1, 0, 6, 1},
                    {"ECU_GEAR_SW", 1, 1, false, "", 1, 0, 5, 1},
                    {"ECU_DECEL_CUT", 1, 1, false, "", 1, 0, 4, 1},
                    {"ECU_TRANSIENT_THROTTLE", 1, 1, false, "", 1, 0, 3, 1},
                    {"ECU_BRAKE_SW", 1, 1, false, "", 1, 0, 2, 1},
                    {"ECU_CLUTCH_SW", 1, 1, false, "", 1, 0, 1, 1},
                    {"ECU_OIL_P_LIGHT", 1, 1, false, "", 1, 0, 0, 1},
                    {"ECU_LAUNCH_ACTIVE", 2, 1, false, "", 1, 0, 7, 1},
                    {"ECU_LAUNCH_SW", 2, 1, false, "", 1, 0, 6, 1},
                    {"ECU_AUX_RPM_LIMIT", 2, 1, false, "", 1, 0, 5, 1},
                    {"ECU_BRAKE_FLUID_SW", 2, 1, false, "", 1, 0, 4, 1},
                    {"ECU_FLAT_SHIFT_SW", 2, 1, false, "", 1, 0, 3, 1},
                    {"ECU_CLUTCH_P_SENSOR", 2, 1, false, "", 1, 0, 2, 1},
                    {"ECU_TORQUE_REDUCTION", 2, 1, false, "", 1, 0, 1, 1},
                    {"ECU_TC_ENABLED", 3, 1, false, "", 1, 0, 7, 1},
                    {"ECU_TC_ACTIVE", 3, 1, false, "", 1, 0, 6, 1},
                    {"ECU_AC_REQUEST", 3, 1, false, "", 1, 0, 5, 1},
                    {"ECU_AC_OUTPUT", 3, 1, false, "", 1, 0, 4, 1},
                    {"ECU_FAN4", 3, 1, false, "", 1, 0, 3, 1},
                    {"ECU_FAN3", 3, 1, false, "", 1, 0, 2, 1},
                    {"ECU_FAN2", 3, 1, false, "", 1, 0, 1, 1},
                    {"ECU_FAN1", 3, 1, false, "", 1, 0, 0, 1},
                    {"ECU_TRIM_POT1", 4, 1, true, "", 1, 0},
                    {"ECU_TRIM_POT2", 5, 1, true, "", 1, 0},
                    {"ECU_TRIM_POT3", 6, 1, true, "", 1, 0},
                    {"ECU_CHECK_ENGINE", 7, 1, false, "", 1, 0, 7, 1},
                    {"ECU_BATTERY_LIGHT", 7, 1, false, "", 1, 0, 6, 1},
                    {"ECU_HAND_BRAKE", 7, 1, false, "", 1, 0, 1, 1},
                    {"ECU_TC_LIGHT", 7, 1, false, "", 1, 0, 0, 1}
                }}},
                {0x470, {"ECU_GEAR", 8, false, {
                    {"ECU_GEAR", 7, 1, true, "", 1, 0}
                }}},
                {0x476, {"ECU_BRAKE_REAR", 8, false, {
                    {"ECU_BRAKE_P_REAR", 0, 2, false, "kPa", 1, -101.3},
                    {"ECU_BRAKE_P_FRONT_RATIO", 2, 2, false, "%", 0.1, 0},
                    {"ECU_BRAKE_P_REAR_RATIO", 4, 2, false, "%", 0.1, 0},
                    {"ECU_BRAKE_P_DIFF", 6, 2, true, "kPa", 1, 0}
                }}}
            };
            return t;
        }
};


#endif //WIRELESS_TELEMETRY_CANMAP_H
