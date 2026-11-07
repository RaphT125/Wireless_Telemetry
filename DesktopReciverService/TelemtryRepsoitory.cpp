//
// Created by Raphael on 2026-10-23.
//

#include "TelemtryRepsoitory.h"
#include <chrono>
#include <cstdio>
#include <iostream>
#include "Logger.h"

namespace {
std::string toTimestamp(std::uint64_t ms) {
    using namespace std::chrono;
    sys_time<milliseconds> tp{milliseconds{static_cast<long long>(ms)}};
    auto day = floor<days>(tp);
    year_month_day ymd{day};
    hh_mm_ss<milliseconds> hms{tp - day};
    char buf[40];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02u %02lld:%02lld:%02lld.%03lld+00",
                  static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()),
                  static_cast<long long>(hms.hours().count()),
                  static_cast<long long>(hms.minutes().count()),
                  static_cast<long long>(hms.seconds().count()),
                  static_cast<long long>(hms.subseconds().count()));
    return buf;
}
}

TelemtryRepsoitory::TelemtryRepsoitory() {
    for (int attempt = 1; attempt <= 3; attempt++) {
        std::cout << "Please enter password for postgresDB: ";
        std::string password;
        std::getline(std::cin, password);
        try {
            {
                pqxx::connection admin{"host=127.0.0.1 port=5432 dbname=postgres user=postgres password=" + password};
                pqxx::nontransaction tx{admin};
                pqxx::result r = tx.exec("SELECT 1 FROM pg_database WHERE datname = 'telemetry'");
                if (r.empty()) {
                    tx.exec("CREATE DATABASE telemetry");
                }
            }
            m_conn.emplace("host=127.0.0.1 port=5432 dbname=telemetry user=postgres password=" + password);
            return;
        } catch (const std::exception& e) {
            Logger::get().logEvent(Logger::SevLevel::ERR,
                "database connect attempt " + std::to_string(attempt) + "/3 failed: " + e.what());
            std::cout << "Could not connect: " << e.what() << "\n";
        }
    }
    throw std::runtime_error("Could not connect to the database after 3 attempts");
}
void TelemtryRepsoitory::initializeSchema() {
    pqxx::work tx{*m_conn};
    tx.exec("CREATE EXTENSION IF NOT EXISTS timescaledb");
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS Car_Data (
           time     TIMESTAMPTZ     NOT NULL DEFAULT now(),
           device   TEXT            NOT NULL,
           boot_id  BIGINT          NOT NULL,
           signal   TEXT            NOT NULL,
           value    DECIMAL         NOT NULL,
           unit     TEXT            NOT NULL
        )WITH (
            tsdb.hypertable,
            tsdb.partition_column = 'time',
            tsdb.segmentby = 'device, signal',
            tsdb.chunk_interval = '1 day'
        )
    )");
    tx.commit();
}

void TelemtryRepsoitory::saveDatapoint(const std::vector<PacketHandler::datapoint>& points) {
    try {
        writeBatch(points);
    } catch (const std::exception& e) {
        Logger::get().logEvent(Logger::SevLevel::ERR,
            "database write failed, buffered " + std::to_string(points.size()) + " points: " + e.what());
        std::printf("[db] save failed, buffered %zu points: %s\n", points.size(), e.what());
        m_buffer.push(points);
        return;
    }
    while (!m_buffer.empty()) {
        try {
            writeBatch(m_buffer.top());
            m_buffer.pop();
        } catch (const std::exception&) {
            break;
        }
    }
}

void TelemtryRepsoitory::writeBatch(const std::vector<PacketHandler::datapoint>& points) {
    pqxx::work tx{*m_conn};
    auto stream = pqxx::stream_to::table(tx, {"car_data"},
                                            {"time", "device", "boot_id", "signal", "value", "unit"});
    for (const auto& p: points) {
        const ProcessedCanData& pData = p.procssedData;
        stream.write_values(toTimestamp(p.timeMs), p.device, pqxx::to_string(p.boot_id),
                            std::string{pData.name}, pqxx::to_string(pData.value), std::string{pData.unit});
    }
    stream.complete();
    tx.commit();
}

void TelemtryRepsoitory::updateLinkStatus(std::uint32_t ip, const PacketHandler::linkState& link) {

}