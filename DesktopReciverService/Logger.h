//
// Created by Raphael on 2026-11-02.
//


#include <time.h>
#include <stdio.h>
#include <share.h>
#include <direct.h>
#include <string>
#include <unordered_map>
#include <mutex>

#ifndef WIRELESS_TELEMETRY_LOGGER_H
#define WIRELESS_TELEMETRY_LOGGER_H

#define LOGGER_DIR "C:/ProgramData/Wireless_Telemtry"
#define LOGGER_NAME "serverlog.txt"
#define LOGGER_PATH LOGGER_DIR "/" LOGGER_NAME



class Logger {
public:
    enum class SevLevel {ERR,WARN,INFO,DEBUG};
    static Logger& get();
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void logEvent(SevLevel sevLevel, std::string message);
private:
    FILE* g_log = NULL;
    std::string toString(SevLevel sevLevel);
    std::unordered_map<std::string, time_t> lastSent = {};
    std::mutex m_mutex;

};


#endif //WIRELESS_TELEMETRY_LOGGER_H
