//
// Created by Raphael on 2026-11-02.
//

#include "Logger.h"

    Logger::Logger() {
            _mkdir(LOGGER_DIR);
    }

    Logger& Logger::get() {
        static Logger logger;
        return logger;
    }

    void Logger::logEvent(SevLevel sevLevel, std::string message) {
        m_mutex.lock();
        if (g_log == nullptr) {
            g_log = _fsopen(LOGGER_PATH, "a", _SH_DENYWR);
            if (g_log == nullptr) {
                m_mutex.unlock();
                return;
            }
        }
        time_t now = time(nullptr);

        struct tm t;
        localtime_s(&t, &now);
        auto lastTime = lastSent.find(message);

        if (lastTime == lastSent.end() || difftime(now, lastTime->second) > 5.0 ) {
            char timeBuf[32];
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &t);

            std::string msg = std::string(timeBuf) + " " + toString(sevLevel) + ": " + message;
            fprintf(g_log, "%s\n", msg.c_str());
            fflush(g_log);
            lastSent[message] = now;
        }
        m_mutex.unlock();
    }

    Logger::~Logger() {
        if (g_log != nullptr) {
            fclose(g_log);
            g_log = nullptr;
        }
    }

    std::string Logger::toString(SevLevel sevLevel) {
    switch (sevLevel) {
        case SevLevel::INFO:
            return "INFO";
        case SevLevel::DEBUG:
            return "DEBUG";
        case SevLevel::WARN:
            return "WARN";
        case SevLevel::ERR:
            return "ERROR";
        default:
            return "";
        }
    }